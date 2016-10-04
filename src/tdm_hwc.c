#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hardware/hwcomposer.h>
#include <hardware/fb.h>	/* Note: some devices may insist that the FB HAL be opened before HWC */
#include <sync/sync.h>

#include "tdm_hwc.h"

#define MAX_HW_LAYERS (1)
#define INCHES_TO_MM (25.4)
#define MAX_NUM_CONFIGS (32)
#define MAX_NUM_OUTPUTS (1)

/* TODO: we have the next problem:
 *       - hwcomposer doesn't differ vblank and commit events,
 *       - output_set_commit_handler is called before
 *         output_commit,
 */

/* TODO: yes this is dirty hack,
 *       but it will take some time to fix it,
 *       so, at least now, we have that we have :-) */
extern pthread_mutex_t tdm_mutex_check_lock;
extern int tdm_mutex_locked;

/* these macros were copied from tdm_private.h */
#define _pthread_mutex_unlock(l) \
	do { \
		pthread_mutex_lock(&tdm_mutex_check_lock); \
		tdm_mutex_locked = 0; \
		pthread_mutex_unlock(&tdm_mutex_check_lock); \
		pthread_mutex_unlock(l); \
	} while (0)
#define _pthread_mutex_lock(l) \
	do { \
		pthread_mutex_lock(l); \
		pthread_mutex_lock(&tdm_mutex_check_lock); \
		tdm_mutex_locked = 1; \
		pthread_mutex_unlock(&tdm_mutex_check_lock); \
	} while (0)


/* stores hwcomposer specific parameters */
struct _hwc_manager
{
	hw_module_t *hwc_module;
	hwc_composer_device_1_t *hwc_dev;

	framebuffer_device_t *fb_dev; /* Note: some devices may insist that the FB HAL be opened before HWC */

	hwc_procs_t hwc_callbacks;

	/* this array of pointers will be passed to prepare() and set() functions */
	hwc_display_contents_1_t **disps_list;

	/* layer with HWC_FRAMEBUFFER_TARGET compositionType, layer where final image will be.
	 * final image - all on screen apart contents which will be displayed via hw overlays.
	 * aka root window */
	hwc_layer_1_t *fb_target_layer;

	/* this callback will be called ONCE after a vblank event delivery */
	tdm_output_commit_handler commit_hndl;
	void *data;

	int max_num_outputs;
	int max_num_layers;
};

static void
_clean_leayers_list(hwc_manager_t hwc_manager)
{
	free(hwc_manager->disps_list[0]);
	free(hwc_manager->disps_list);
}

static int
_prepare_displays_content(hwc_manager_t hwc_manager)
{
	hwc_display_contents_1_t *list = NULL;
	hwc_layer_1_t *layer = NULL;
	hwc_rect_t rect   = { 0, 0, 0, 0 };
	hwc_frect_t frect = { 0.0, 0.0, 0.0, 0.0 };

	size_t size;
	int i;

	size = sizeof(hwc_display_contents_1_t) + MAX_HW_LAYERS * sizeof(hwc_layer_1_t);
	list = (hwc_display_contents_1_t *) calloc( 1, size );
	if (!list)
		return -1;

	hwc_manager->disps_list = (hwc_display_contents_1_t **) calloc(HWC_NUM_DISPLAY_TYPES, sizeof(hwc_display_contents_1_t *));
	if (!hwc_manager->disps_list) {
		free( list );
		return -1;
	}

	/* we will set same set of layers/buffers for all displays */
	for (i = 0; i < HWC_NUM_DISPLAY_TYPES; i++)
		hwc_manager->disps_list[i] = list;

	/* prepare framebuffer layer */

	/* framebuffer is the bottom layer */
	hwc_manager->fb_target_layer = &list->hwLayers[MAX_HW_LAYERS - 1];

	hwc_manager->fb_target_layer->compositionType = HWC_FRAMEBUFFER_TARGET;
	hwc_manager->fb_target_layer->hints = 0;
	hwc_manager->fb_target_layer->flags = 0;
	hwc_manager->fb_target_layer->handle = 0;
	hwc_manager->fb_target_layer->transform = 0;
	hwc_manager->fb_target_layer->blending = HWC_BLENDING_PREMULT;
	hwc_manager->fb_target_layer->sourceCropf = frect;
	hwc_manager->fb_target_layer->displayFrame = rect;
	hwc_manager->fb_target_layer->visibleRegionScreen.numRects = 1;
	hwc_manager->fb_target_layer->visibleRegionScreen.rects = &hwc_manager->fb_target_layer->displayFrame;
	hwc_manager->fb_target_layer->acquireFenceFd = -1;
	hwc_manager->fb_target_layer->releaseFenceFd = -1;
	hwc_manager->fb_target_layer->planeAlpha = 0xff;

	/* prepare overlay layers */

	for (i = 0; i < MAX_HW_LAYERS - 1;  i++) {
		layer = &list->hwLayers[i];

		layer->blending = HWC_BLENDING_NONE;
		layer->visibleRegionScreen.numRects = 1;
		layer->visibleRegionScreen.rects = &layer->displayFrame;
		layer->acquireFenceFd = -1;
		layer->releaseFenceFd = -1;
	}

	list->retireFenceFd = -1;
	list->flags = HWC_GEOMETRY_CHANGED;
	list->numHwLayers = MAX_HW_LAYERS;

	return 0;
}

/* callback */
static void
_hwc_invalidate_cb(const struct hwc_procs *procs)
{
	TDM_DBG("######################  hwc_invalidate callback: procs: %p.  ######################", procs);
}

/* callback */
static void
_hwc_vsync_cb(const struct hwc_procs *procs, int disp, int64_t ts)
{
	tdm_android_output_data *output_data;
	hwc_manager_t hwc_manager;
	pthread_mutex_t *tdm_mutex;
	int64_t tv_sec, tv_usec;

	hwc_manager = container_of(procs, hwc_manager, hwc_callbacks);

	output_data = hwc_manager->data;

	/* TODO: this check must be under some lock/unlock mechanism,
	 * 	     because commit_hndl is set from another thread */
	if (!hwc_manager->commit_hndl || !output_data || !output_data->commit_hndl_data)
		return;

	/* TODO: must be checked for compatibility with values drm provides which */
	tv_sec = ts/1000000000;
	tv_usec = (ts - tv_sec*10000000)/1000;

	/* look to the declaration of _tdm_private_display structure */
	tdm_mutex = (pthread_mutex_t *)output_data->android_data->dpy;

	/* as we don't use tdm_thread we gotta make lock/unlock here, yes it's dirty hack :-) */
	_pthread_mutex_lock(tdm_mutex);

	/* TODO: what about inter-thread synchronization ?*/
	/* TODO: what about sequence argument ? */
	/* TODO: it's temporary hack: we just bypass tdm_thread... write data to pipe by self... */
	hwc_manager->commit_hndl(output_data, 0, (unsigned int)tv_sec, (unsigned int)tv_usec,
			output_data->commit_hndl_data);

	_pthread_mutex_unlock(tdm_mutex);
}

/* callback */
static void
_hwc_hotplug_cb(const struct hwc_procs *procs, int disp, int conn)
{
	TDM_DBG("######################  hwc_hotplug callback: procs: %p, disp: %d, conn: %d.  ######################", procs, disp, conn);
}

static void
_unprepare_fb_device(hwc_manager_t hwc_manager)
{
	if (hwc_manager->fb_dev) {
		framebuffer_close(hwc_manager->fb_dev);
		hwc_manager->fb_dev = NULL;
	}
}

/**
 * @brief prepare framebuffer device
 *
 * backend uses hwc to output images, but some devices may insist that the FB HAL be opened before HWC
 */
static void
_prepare_fb_device(hwc_manager_t hwc_manager)
{
	const hw_module_t *module;
	int res;

	res = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
	if (res) {
		TDM_ERR("Error: %s module not found.", GRALLOC_HARDWARE_MODULE_ID);
		return;
	}

	res = framebuffer_open(module, &hwc_manager->fb_dev);
	if (res)
		TDM_ERR("Error: fail while trying to open framebuffer.");
	else
		TDM_INFO("framebuffer device has been successfully opened.");
}

static int
_prepare_hwc_device(hwc_manager_t hwc_manager)
{
	int value;
	int res;

	/* some devices may insist that the FB HAL be opened before HWC */
	_prepare_fb_device(hwc_manager);

	res = hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwc_manager->hwc_module);
	if (res || !hwc_manager->hwc_module) {
		TDM_ERR("Error: cannot obtain hw composer module info.");
		goto fail_1;
	}

	res = hwc_open_1((const hw_module_t *)hwc_manager->hwc_module, &hwc_manager->hwc_dev);
	if (res || !hwc_manager->hwc_dev) {
		TDM_ERR("Error: cannot open hwc device.");
		goto fail_1;
	}

	/* we're not interesting about framebuffer device, so we close it,
	 * of course if it exists on the platform. */
	_unprepare_fb_device(hwc_manager);

	TDM_DBG("turn on a screen...");
	res = hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, 0);
	if (res) {
		TDM_DBG("warning: cannot turn on a screen, obviously it has been turned on already.");
	}

	TDM_INFO("hwc version: %x.", hwc_manager->hwc_dev->common.version & 0xFFFF0000);
	TDM_INFO("hwc module api version: %hu.", hwc_manager->hwc_dev->common.module->module_api_version);

	/* always turn vsync off when we start */
	android_hwc_vsync_event_control(hwc_manager, HWC_DISPLAY_PRIMARY, 0);

	/* register hwc callback set */
	hwc_manager->hwc_callbacks.invalidate = _hwc_invalidate_cb;
	hwc_manager->hwc_callbacks.vsync = _hwc_vsync_cb;
	hwc_manager->hwc_callbacks.hotplug = _hwc_hotplug_cb;
	hwc_manager->hwc_dev->registerProcs(hwc_manager->hwc_dev, &hwc_manager->hwc_callbacks);

	res = hwc_manager->hwc_dev->query(hwc_manager->hwc_dev, HWC_BACKGROUND_LAYER_SUPPORTED, &value);
	if (res) {
		TDM_ERR("Error: cannot get query HWC_BACKGROUND_LAYER_SUPPORTED feature.");
		goto fail_2;
	}

	TDM_INFO("Hw composer does%s support HWC_BACKGROUND_LAYER feature.", value ? "":"n't");

	return 0;

fail_2:
	hwc_close_1(hwc_manager->hwc_dev);
fail_1:
	_unprepare_fb_device(hwc_manager);
	return -1;
}

/*
 * prepare hwcomposer
 * return 0 on success, -1 - otherwise
 */
tdm_error
android_hwc_init(hwc_manager_t *hwc_manager_)
{
	hwc_manager_t hwc_manager;
	int ret;

	hwc_manager = calloc(1, sizeof(struct _hwc_manager));
	if (!hwc_manager)
		return TDM_ERROR_OPERATION_FAILED;

	ret = _prepare_hwc_device(hwc_manager);
	if (ret)
		return TDM_ERROR_OPERATION_FAILED;

	/* prepare MAX_HW_LAYERS layers for each display */
	ret = _prepare_displays_content(hwc_manager);
	if (ret) {
		hwc_close_1(hwc_manager->hwc_dev);
		free(hwc_manager);
		return TDM_ERROR_OPERATION_FAILED;
	}

	/* turn on vsync event delivery */
	ret = android_hwc_vsync_event_control(hwc_manager, HWC_DISPLAY_PRIMARY, 1);
	if (ret) {
		_clean_leayers_list(hwc_manager);
		hwc_close_1(hwc_manager->hwc_dev);
		free(hwc_manager);
		return TDM_ERROR_OPERATION_FAILED;
	}

	hwc_manager->max_num_outputs = MAX_NUM_OUTPUTS;
	hwc_manager->max_num_layers = MAX_HW_LAYERS;

	*hwc_manager_ = hwc_manager;

	/* tdm requires set_dpms with TDM_OUTPUT_DPMS_ON arg to be called before commit call,
	 * so we can turn off screen here, it will be turned on request from set_dpms function */
	TDM_INFO("turn off a screen...");
	hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, 1);

	return TDM_ERROR_NONE;
}

void
android_hwc_deinit(hwc_manager_t hwc_manager)
{
	if (!hwc_manager)
		return;

	_clean_leayers_list(hwc_manager);
	hwc_close_1(hwc_manager->hwc_dev);
	free(hwc_manager);
}

/**
 * \brief control vsync event delivery
 *
 * \param[in] hwc_manager -
 * \param[in] state - turn on/off event delivery (1/0)
 * \return \c 0 on success, \c -1 otherwise
 */
tdm_error
android_hwc_vsync_event_control(hwc_manager_t hwc_manager, int output_idx, int state)
{
	int ret;

	ret = hwc_manager->hwc_dev->eventControl(hwc_manager->hwc_dev, output_idx, HWC_EVENT_VSYNC, state);
	if (ret) {
		TDM_ERR("Error: failed while turn %s VSYNC event.", state ? "on" : "off");
		return TDM_ERROR_OPERATION_FAILED;
	}

	TDM_DBG("VSYNC event delivery was turned %s.", state ? "on" : "off");

	return TDM_ERROR_NONE;
}

int
android_hwc_get_max_hw_layers(hwc_manager_t hwc_manager)
{
	return hwc_manager->max_num_layers;
}

int
android_hwc_get_max_num_outputs(hwc_manager_t hwc_manager)
{
	return hwc_manager->max_num_outputs;
}

static tdm_output_type
_android_hwc_get_output_type(int output_idx)
{
	switch (output_idx) {
	case HWC_DISPLAY_PRIMARY:
		return TDM_OUTPUT_TYPE_LVDS;
	case HWC_DISPLAY_EXTERNAL:
		return TDM_OUTPUT_TYPE_HDMIA;
	case HWC_DISPLAY_VIRTUAL:
		return TDM_OUTPUT_TYPE_VIRTUAL;
	}

	return TDM_OUTPUT_TYPE_Unknown;
}

tdm_error
android_hwc_get_output_capabilities(hwc_manager_t hwc_manager, int output_idx,
									tdm_caps_output *caps)
{
	uint32_t configs[MAX_NUM_CONFIGS];
	size_t num_configs = MAX_NUM_CONFIGS;
	int i;
	tdm_error ret;
	int res;
	const uint32_t disp_attrs[] =
	{
		HWC_DISPLAY_WIDTH,
		HWC_DISPLAY_HEIGHT,
		HWC_DISPLAY_VSYNC_PERIOD,
		HWC_DISPLAY_DPI_X,
		HWC_DISPLAY_DPI_Y,
		HWC_DISPLAY_NO_ATTRIBUTE,
	};
	const int vsize = sizeof(disp_attrs) / sizeof(uint32_t);
	int32_t values[vsize], dpi_x, dpi_y;

	RETURN_VAL_IF_FAIL(output_idx >= HWC_DISPLAY_PRIMARY &&
					   output_idx <= HWC_DISPLAY_VIRTUAL,
					   TDM_ERROR_INVALID_PARAMETER);

	memset(caps, 0, sizeof(tdm_caps_output));

	snprintf(caps->maker, TDM_NAME_LEN, "unknown");
	snprintf(caps->model, TDM_NAME_LEN, "unknown");
	snprintf(caps->name, TDM_NAME_LEN, "unknown");

	caps->type = _android_hwc_get_output_type(output_idx);

	/* get available display configurations */
	res = hwc_manager->hwc_dev->getDisplayConfigs(hwc_manager->hwc_dev,
												  output_idx,
												  configs, &num_configs);
	if (res || num_configs < 1) {
		TDM_ERR("Error: cannot get hwc's configs.");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto fail;
	}

	TDM_DBG("available amount of display configurations: %u.", num_configs);

	caps->mode_count = num_configs;
	caps->modes = calloc(caps->mode_count, sizeof(tdm_output_mode));
	if (!caps->modes) {
		ret = TDM_ERROR_OUT_OF_MEMORY;
		TDM_ERR("alloc failed\n");
		goto fail;
	}

	for (i = 0; i < num_configs; ++i) {
		res = hwc_manager->hwc_dev->getDisplayAttributes(hwc_manager->hwc_dev,
														 output_idx, configs[i],
														 disp_attrs, values);
		if (res) {
			TDM_ERR("Error: cannot get display attributes.");
			ret = TDM_ERROR_OPERATION_FAILED;
			goto fail;
		}
		caps->modes[i].hdisplay = values[0];
		caps->modes[i].vdisplay = values[1];
		caps->modes[i].vrefresh = values[2];
		/* flags is the index of display configuration */
		caps->modes[i].flags = i;

		TDM_DBG("display: %d configuration: %d attributes:", output_idx, i);
		TDM_DBG(" width:  %d", values[0]);
		TDM_DBG(" heitht: %d", values[1]);
		TDM_DBG(" vsync period: %f ms", values[2]/1000000.0);
		TDM_DBG(" x dpi:  %f", values[3]/1000.0);
		TDM_DBG(" y dpi:  %f", values[4]/1000.0);
	}

	caps->modes[0].type = TDM_OUTPUT_MODE_TYPE_PREFERRED;

	dpi_x = values[3];
	dpi_y = values[4];
	/* calculate the physical dimensions of the screen */
	caps->mmWidth = (caps->modes[i - 1].hdisplay / (dpi_x / 1000.0))
					* INCHES_TO_MM;
	caps->mmHeight = (caps->modes[i - 1].vdisplay / (dpi_y / 1000.0))
					 * INCHES_TO_MM;

	/* -1 because "not defined" */
	caps->max_w = -1;
	caps->max_h = -1;
	caps->min_w = -1;
	caps->min_h = -1;
	caps->preferred_align = -1;

	/* the main screen always connected */
	if (output_idx == HWC_DISPLAY_PRIMARY)
		caps->status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
	else
		caps->status = TDM_OUTPUT_CONN_STATUS_DISCONNECTED;

	return TDM_ERROR_NONE;
fail:
	free(caps->modes);
	memset(caps, 0, sizeof(tdm_caps_output));
	return ret;
}

tdm_error
android_hwc_get_layer_capabilities(hwc_manager_t hwc_manager, int layer_idx, tdm_caps_layer *caps)
{
	/* TODO: it's temporary code until we'll have figured out how to work
	 *       with several outputs and layers*/

	caps->capabilities = TDM_LAYER_CAPABILITY_PRIMARY | TDM_LAYER_CAPABILITY_GRAPHIC;
	caps->zpos = 0;

	/*
	 * TODO: find a way to know available formats, perhaps via tbm
	 */
	caps->format_count = 1;
	caps->formats = calloc(caps->format_count, sizeof(*caps->formats));
	if (!caps->formats)
		return TDM_ERROR_OUT_OF_MEMORY;

	caps->formats[0] = TBM_FORMAT_RGBA8888;

	caps->prop_count = 0;
	caps->props = NULL;

	return TDM_ERROR_NONE;
}

void
android_hwc_layer_set_info(hwc_manager_t hwc_manager, int output_idx, int layer_idx, tdm_info_layer *info)
{
	hwc_layer_1_t *curr_layer;

	curr_layer = &hwc_manager->disps_list[output_idx]->hwLayers[layer_idx];

	curr_layer->sourceCropf.left = (float)info->src_config.pos.x;
	curr_layer->sourceCropf.top = (float)info->src_config.pos.y;
	curr_layer->sourceCropf.right = (float)(info->src_config.pos.x + info->src_config.pos.w);
	curr_layer->sourceCropf.bottom = (float)(info->src_config.pos.y + info->src_config.pos.h);

	curr_layer->displayFrame.left = info->dst_pos.x;
	curr_layer->displayFrame.top = info->dst_pos.y;
	curr_layer->displayFrame.right = info->dst_pos.x + info->dst_pos.w;
	curr_layer->displayFrame.bottom = info->dst_pos.y + info->dst_pos.h;
}

void
android_hwc_layer_set_buff(hwc_manager_t hwc_manager, int output_idx, int layer_idx, buffer_handle_t buff)
{
	hwc_layer_1_t *curr_layer;

	curr_layer = &hwc_manager->disps_list[output_idx]->hwLayers[layer_idx];

	curr_layer->handle = buff;
}

void
android_hwc_output_set_commit_handler(hwc_manager_t hwc_manager, int output_idx, tdm_output_commit_handler hndl)
{
	hwc_manager->commit_hndl = hndl;
}

tdm_error
android_hwc_output_commit(hwc_manager_t hwc_manager, int output_idx, int sync, void *data)
{
	int old_retire_fd;
	int old_release_fd;
	int ret;

	/* TODO: some work for synchronize issues (hwc access to buffer) must be done */
	/* TODO: some work to make ability to use tdm_commit in sync modes (now only async mode is supplied) */
	/* TODO: vblank isn't synchronized with result of `set` call, it's very bad... */

	hwc_manager->data = data;

	/* fd for sync fence object signaled when composition is retired
	 * (after pageflip event) */
	old_retire_fd = hwc_manager->disps_list[output_idx]->retireFenceFd;

	/* fd for sync fence object signaled when hwc finished read from buffer */
	old_release_fd = hwc_manager->fb_target_layer->releaseFenceFd;

	/* must be -1 before prepare/set */
	hwc_manager->disps_list[output_idx]->retireFenceFd = -1;

	/* we've drawn yet everything */
	hwc_manager->fb_target_layer->acquireFenceFd = -1;

	ret = hwc_manager->hwc_dev->set(hwc_manager->hwc_dev,
									hwc_manager->max_num_outputs,
									hwc_manager->disps_list);

	/* wait until vblank event occurs
	 * we wait vblank event for frame which has been set by PREVIOUS set call !!! */
	if (old_retire_fd != -1) {
		sync_wait(old_retire_fd, -1);
		close(old_retire_fd);
	}

	if (old_release_fd != -1) {
		sync_wait(old_release_fd, -1);
		close(old_release_fd);
	}

	if (ret)
		return TDM_ERROR_OPERATION_FAILED;

	return TDM_ERROR_NONE;
}

tdm_error
android_hwc_output_set_mode(hwc_manager_t hwc_manager, int output_idx,
							unsigned int mode_idx)
{
	int res;
	uint32_t hwc_version;

	hwc_version = hwc_manager->hwc_dev->common.version & 0xFFFF0000;

	if (hwc_version >= 0x1040000) {
		res = hwc_manager->hwc_dev->setActiveConfig(hwc_manager->hwc_dev,
													output_idx,
													mode_idx);
		if (res) {
			TDM_ERR("Error: cannot set display configuration.");
			return TDM_ERROR_OPERATION_FAILED;
		}
	}

	return TDM_ERROR_NONE;
}

tdm_error
android_hwc_output_set_dpms(hwc_manager_t hwc_manager, int output_idx,
							tdm_output_dpms dpms_value)
{
	int ret;
	uint32_t hwc_version;

	hwc_version = hwc_manager->hwc_dev->common.version & 0xFFFF0000;

	switch (dpms_value) {
	case TDM_OUTPUT_DPMS_ON:
		if (hwc_version >= 0x1040000) {
			ret = hwc_manager->hwc_dev->setPowerMode(hwc_manager->hwc_dev,
													 output_idx,
													 HWC_POWER_MODE_NORMAL);
			if (ret)
				goto fail;
			break;
		}
		ret = hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, output_idx, 0);
		if (ret)
			goto fail;
		break;
	case TDM_OUTPUT_DPMS_STANDBY:
		if (hwc_version >= 0x1040000) {
			ret = hwc_manager->hwc_dev->setPowerMode(hwc_manager->hwc_dev,
													 output_idx,
													 HWC_POWER_MODE_DOZE);
			if (ret)
				goto fail;
			break;
		}
	case TDM_OUTPUT_DPMS_SUSPEND:
		if (hwc_version >= 0x1040000) {
			ret = hwc_manager->hwc_dev->setPowerMode(hwc_manager->hwc_dev,
													 output_idx,
													 HWC_POWER_MODE_DOZE_SUSPEND);
			if (ret)
				goto fail;
			break;
		}
	case TDM_OUTPUT_DPMS_OFF:
		if (hwc_version >= 0x1040000) {
			ret = hwc_manager->hwc_dev->setPowerMode(hwc_manager->hwc_dev,
													 output_idx,
													 HWC_POWER_MODE_OFF);
			if (ret)
				goto fail;
			break;
		}
		ret = hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, output_idx, 1);
		if (ret)
			goto fail;
		break;
	default:
		return TDM_ERROR_INVALID_PARAMETER;
	}

	return TDM_ERROR_NONE;

fail:
	TDM_ERR("Error: cannot set the display screen power state.");
	return TDM_ERROR_OPERATION_FAILED;
}
