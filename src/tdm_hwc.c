#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hardware/hwcomposer.h>
#include <hardware/fb.h>	/* Note: some devices may insist that the FB HAL be opened before HWC */

#include "tdm_hwc.h"

#define MAX_HW_LAYERS (4)

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

	/* this geometry declared by hwcomposer (system base geometry) */
	uint32_t screen_width;
	uint32_t screen_height;
	uint32_t vsyns_period; /* in nanoseconds */
	uint32_t x_dpi; /* in dot per kinch */
	uint32_t y_dpi;
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
	uint32_t configs[32];
	size_t num_configs = 32;
	int32_t values[6];
	int res;

	const uint32_t disp_attrs[] =
	{
		HWC_DISPLAY_WIDTH,
		HWC_DISPLAY_HEIGHT,
		HWC_DISPLAY_VSYNC_PERIOD,
		HWC_DISPLAY_DPI_X,
		HWC_DISPLAY_DPI_Y,
		HWC_DISPLAY_SECURE,
		HWC_DISPLAY_NO_ATTRIBUTE,
	};

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

	/* register hwc callback set */
	hwc_manager->hwc_callbacks.invalidate = _hwc_invalidate_cb;
	hwc_manager->hwc_callbacks.vsync = _hwc_vsync_cb;
	hwc_manager->hwc_callbacks.hotplug = _hwc_hotplug_cb;
	hwc_manager->hwc_dev->registerProcs(hwc_manager->hwc_dev, &hwc_manager->hwc_callbacks);

	/* always turn vsync off when we start */
	android_hwc_vsync_event_control(hwc_manager, 0);

	// get available display configurations
	res = hwc_manager->hwc_dev->getDisplayConfigs(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, configs, &num_configs);
	if (res) {
		TDM_ERR("Error: cannot get hwc's configs.");
		goto fail_2;
	}

	TDM_DBG("available amount of display configurations: %u.", num_configs);

	res = hwc_manager->hwc_dev->getDisplayAttributes(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, configs[0], disp_attrs, values);
	if (res) {
		TDM_ERR("Error: cannot get display attributes.");
		goto fail_2;
	}

	hwc_manager->screen_width = values[0];
	hwc_manager->screen_height = values[1];
	hwc_manager->vsyns_period = values[2];
	hwc_manager->x_dpi = values[3];
	hwc_manager->y_dpi = values[4];

	TDM_DBG("display: %d configuration: 0 attributes:", HWC_DISPLAY_PRIMARY);
	TDM_DBG(" width:  %d", values[0]);
	TDM_DBG(" heitht: %d", values[1]);
	TDM_DBG(" vsync period: %f ms", values[2]/1000000.0);
	TDM_DBG(" x dpi:  %f", values[3]/1000.0);
	TDM_DBG(" y dpi:  %f", values[4]/1000.0);
	TDM_DBG(" is secure: %s.", values[5] ? "yes" : "no");

	res = hwc_manager->hwc_dev->query(hwc_manager->hwc_dev, HWC_BACKGROUND_LAYER_SUPPORTED, values);
	if (res) {
		TDM_ERR("Error: cannot get query HWC_BACKGROUND_LAYER_SUPPORTED feature.");
		goto fail_2;
	}

	TDM_INFO("Hw composer does%s support HWC_BACKGROUND_LAYER feature.", values[0] ? "":"n't");

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
	ret = android_hwc_vsync_event_control(hwc_manager, 1);
	if (ret) {
		_clean_leayers_list(hwc_manager);
		hwc_close_1(hwc_manager->hwc_dev);
		free(hwc_manager);
		return TDM_ERROR_OPERATION_FAILED;
	}

	*hwc_manager_ = hwc_manager;

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
	return 0;
}

int
android_hwc_get_max_outputs(hwc_manager_t hwc_manager)
{
	return 0;
}

tdm_error
android_hwc_get_output_capabilities(hwc_manager_t hwc_manager, int output_idx, tdm_caps_output *caps)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_hwc_get_layer_capabilities(hwc_manager_t hwc_manager, int layer_idx, tdm_caps_layer *caps)
{
	return TDM_ERROR_NONE;
}

void
android_hwc_layer_set_info(hwc_manager_t hwc_manager, int output_idx, int layer_idx, tdm_info_layer *info)
{
}

void
android_hwc_layer_set_buff(hwc_manager_t hwc_manager, int output_idx, int layer_idx, buffer_handle_t buff)
{
}

void
android_hwc_primary_layer_set_info(hwc_manager_t hwc_manager, int output_idx, tdm_info_layer *info)
{
}

void
android_hwc_primary_layer_set_buff(hwc_manager_t hwc_manager, int output_idx, buffer_handle_t buff)
{
}

void
android_hwc_output_set_commit_handler(hwc_manager_t hwc_manager, int output_idx, tdm_output_commit_handler hndl)
{
}

tdm_error
android_hwc_output_commit(hwc_manager_t hwc_manager, int output_idx, int sync, void *data)
{
	return TDM_ERROR_NONE;
}
