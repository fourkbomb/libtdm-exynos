#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include <hardware/hwcomposer.h>
#include <hardware/fb.h>	/* Note: some devices may insist that the FB HAL be opened before HWC */

#include "tdm_android.h"

#define MAX_NUM_HW_PLANES (4)

static tdm_android_data *android_data;

/* stores hwcomposer specific parameters */
struct _hwc_manager
{
	hw_module_t *hwc_module;
	hwc_composer_device_1_t *hwc_dev;

	framebuffer_device_t *fb_dev; /* Note: some devices may insist that the FB HAL be opened before HWC */

	/* this array of pointers will be passed to prepare() and set() functions */
	hwc_display_contents_1_t **disps_list;

	/* layer with HWC_FRAMEBUFFER_TARGET compositionType, layer where weston, via gles2, will render final image.
	 * final image - all on screen apart contents which will be displayed via hw overlays. */
	hwc_layer_1_t *fb_target_layer;

	/* this geometry declared by hwcomposer (system base geometry) */
	uint32_t screen_width;
	uint32_t screen_height;
	uint32_t vsyns_period; /* in nanoseconds */
	uint32_t x_dpi; /* in dot per kinch */
	uint32_t y_dpi;
};

static
void clean_leayers_list(hwc_manager_t hwc_manager)
{
	free(hwc_manager->disps_list[0]);
	free(hwc_manager->disps_list);
}

static
int prepare_displays_content(hwc_manager_t hwc_manager)
{
	hwc_display_contents_1_t *list = NULL;
	hwc_layer_1_t *layer = NULL;
	hwc_rect_t rect   = { 0, 0, 0, 0 };
	hwc_frect_t frect = { 0.0, 0.0, 0.0, 0.0 };

	size_t size;
	int i;

	size = sizeof(hwc_display_contents_1_t) + MAX_NUM_HW_PLANES * sizeof(hwc_layer_1_t);
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
	hwc_manager->fb_target_layer = &list->hwLayers[MAX_NUM_HW_PLANES - 1];
	memset(hwc_manager->fb_target_layer, 0, sizeof(hwc_layer_1_t));

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

	for( i = 0; i < MAX_NUM_HW_PLANES - 1;  i++ )
	{
		layer = &list->hwLayers[i];

		layer->blending = HWC_BLENDING_NONE;
		layer->visibleRegionScreen.numRects = 1;
		layer->visibleRegionScreen.rects = &layer->displayFrame;
		layer->acquireFenceFd = -1;
		layer->releaseFenceFd = -1;
	}

	list->retireFenceFd = -1;
	list->flags = HWC_GEOMETRY_CHANGED;
	list->numHwLayers = MAX_NUM_HW_PLANES;

	return 0;
}

/**
 * \brief control vsync event delivery
 *
 * \param[in] hwc_manager -
 * \param[in] state - turn on/off event delivery (1/0)
 * \return \c 0 on success, \c -1 otherwise
 */
int
vsync_event_control(hwc_manager_t hwc_manager, int state)
{
	int ret;

	ret = hwc_manager->hwc_dev->eventControl(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, state);
	if (ret) {
		TDM_ERR("Error: failed while turn %s VSYNC event.", state ? "on" : "off");
		return -1;
	}

	TDM_INFO("VSYNC event delivery was turned %s.", state ? "on" : "off");

	return 0;
}

/* callback */
static
void hwc_invalidate(const struct hwc_procs *procs)
{
	TDM_INFO("######################  hwc_invalidate callback: procs: %p.  ######################", procs);
}

/* callback */
static
void hwc_vsync(const struct hwc_procs *procs, int disp, int64_t ts)
{
	TDM_INFO("######################  hwc_vsync callback: procs: %p.  ######################", procs);
}

/* callback */
static
void hwc_hotplug(const struct hwc_procs *procs, int disp, int conn)
{
	TDM_INFO("######################  hwc_hotplug callback: procs: %p, disp: %d, conn: %d.  ######################", procs, disp, conn);
}

static hwc_procs_t hwc_callbacks =
{
	.invalidate = hwc_invalidate,
	.vsync = hwc_vsync,
	.hotplug = hwc_hotplug,
};

/**
 * @brief prepare framebuffer device
 *
 * backend uses hwc to output images, but some devices may insist that the FB HAL be opened before HWC
 */
static void
prepare_fb_device(hwc_manager_t hwc_manager)
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

static
int prepare_hwc_device(hwc_manager_t hwc_manager)
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
	prepare_fb_device(hwc_manager);

	res = hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwc_manager->hwc_module);
	if (res || !hwc_manager->hwc_module) {
		TDM_ERR("Error: cannot obtain hw composer module info.");
		return -1;
	}

	res = hwc_open_1((const hw_module_t *)hwc_manager->hwc_module, &hwc_manager->hwc_dev);
	if (res || !hwc_manager->hwc_dev) {
		TDM_ERR("Error: cannot open hwc device.");
		return -1;
	}

	/* we are not interesting for framebuffer device, so we close it, of course if it exists on platform. */
	if (hwc_manager->fb_dev)	{
		framebuffer_close(hwc_manager->fb_dev);
		hwc_manager->fb_dev = NULL;
	}

	TDM_INFO("turn on a screen...");
	res = hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, 0);
	if (res) {
		TDM_INFO("warning: cannot turn on a screen, obviously it has been turned on already.");
	}

	TDM_INFO("hwc version: %x.", hwc_manager->hwc_dev->common.version & 0xFFFF0000);
	TDM_INFO("hwc module api version: %hu.", hwc_manager->hwc_dev->common.module->module_api_version);

	/* register hwc callback set */
	hwc_manager->hwc_dev->registerProcs(hwc_manager->hwc_dev, &hwc_callbacks);

	/* always turn vsync off when we start */
	vsync_event_control(hwc_manager, 0);

	// get available display configurations
	res = hwc_manager->hwc_dev->getDisplayConfigs(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, configs, &num_configs);
	if (res) {
		TDM_ERR("Error: cannot get hwc's configs.");
		goto fail;
	}

	TDM_INFO("available amount of display configurations: %u.", num_configs);

	res = hwc_manager->hwc_dev->getDisplayAttributes(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, configs[0], disp_attrs, values);
	if (res) {
		TDM_ERR("Error: cannot get display attributes.");
		goto fail;
	}

	hwc_manager->screen_width = values[0];
	hwc_manager->screen_height = values[1];
	hwc_manager->vsyns_period = values[2];
	hwc_manager->x_dpi = values[3];
	hwc_manager->y_dpi = values[4];

	TDM_INFO("display: %d configuration: 0 attributes:", HWC_DISPLAY_PRIMARY);
	TDM_INFO(" width:  %d", values[0]);
	TDM_INFO(" heitht: %d", values[1]);
	TDM_INFO(" vsync period: %f ms", values[2]/1000000.0);
	TDM_INFO(" x dpi:  %f", values[3]/1000.0);
	TDM_INFO(" y dpi:  %f", values[4]/1000.0);
	TDM_INFO(" is secure: %s.", values[5] ? "yes" : "no");

	res = hwc_manager->hwc_dev->query(hwc_manager->hwc_dev, HWC_BACKGROUND_LAYER_SUPPORTED, values);
	if (res) {
		TDM_ERR("Error: cannot get query HWC_BACKGROUND_LAYER_SUPPORTED feature.");
		goto fail;
	}

	TDM_INFO("Hw composer does%s support HWC_BACKGROUND_LAYER feature.", values[0] ? "":"n't");

	return 0;

fail:
	hwc_close_1(hwc_manager->hwc_dev);
	return -1;
}

/*
 * prepare hwcomposer
 * return 0 on success, -1 - otherwise
 */
static int
init_hwc(hwc_manager_t *hwc_manager_)
{
	hwc_manager_t hwc_manager;
	int ret;

	hwc_manager = calloc(1, sizeof(struct _hwc_manager));
	if (!hwc_manager)
		return -1;

	ret = prepare_hwc_device(hwc_manager);
	if (ret)
		return -1;

	/* prepare MAX_NUM_HW_PLANES layers for each display */
	ret = prepare_displays_content(hwc_manager);
	if (ret) {
		hwc_close_1(hwc_manager->hwc_dev);
		free(hwc_manager);
		return -1;
	}

	/* turn on vsync event delivery */
	ret = vsync_event_control(hwc_manager, 1);
	if (ret) {
		clean_leayers_list(hwc_manager);
		hwc_close_1(hwc_manager->hwc_dev);
		free(hwc_manager);
		return -1;
	}

	*hwc_manager_ = hwc_manager;

	TDM_INFO("turn off a screen...");
	hwc_manager->hwc_dev->blank(hwc_manager->hwc_dev, HWC_DISPLAY_PRIMARY, 1);

	return 0;
}

static void
tdm_android_deinit(tdm_backend_data *bdata)
{
	if (android_data != bdata)
		return;

	TDM_INFO("deinit");

	tdm_android_display_destroy_output_list(android_data->outputs,
											android_data->num_outputs);

	if (android_data->hwc_manager)
		clean_leayers_list(android_data->hwc_manager);
	if (android_data->hwc_manager->hwc_dev)
		hwc_close_1(android_data->hwc_manager->hwc_dev);
	if (android_data->hwc_manager)
		free(android_data->hwc_manager);

	free(android_data);
	android_data = NULL;
}

static tdm_backend_data *
tdm_android_init(tdm_display *dpy, tdm_error *error)
{
	tdm_func_display android_func_display;
	tdm_func_output android_func_output;
	tdm_func_layer android_func_layer;
	tdm_error ret;

	if (!dpy) {
		TDM_ERR("display is null");
		if (error)
			*error = TDM_ERROR_INVALID_PARAMETER;
		return NULL;
	}

	if (android_data) {
		TDM_ERR("failed: init twice");
		if (error)
			*error = TDM_ERROR_BAD_REQUEST;
		return NULL;
	}

	android_data = calloc(1, sizeof(tdm_android_data));
	if (!android_data) {
		TDM_ERR("alloc failed");
		if (error)
			*error = TDM_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	memset(&android_func_display, 0, sizeof(android_func_display));
	android_func_display.display_get_capabilitiy = android_display_get_capabilitiy;
	android_func_display.display_get_pp_capability = android_display_get_pp_capability;
	android_func_display.display_get_outputs = android_display_get_outputs;
	android_func_display.display_get_fd = android_display_get_fd;
	android_func_display.display_handle_events = android_display_handle_events;
	android_func_display.display_create_pp = android_display_create_pp;

	memset(&android_func_output, 0, sizeof(android_func_output));
	android_func_output.output_get_capability = android_output_get_capability;
	android_func_output.output_get_layers = android_output_get_layers;
	android_func_output.output_set_property = android_output_set_property;
	android_func_output.output_get_property = android_output_get_property;
	android_func_output.output_wait_vblank = android_output_wait_vblank;
	android_func_output.output_set_vblank_handler = android_output_set_vblank_handler;
	android_func_output.output_commit = android_output_commit;
	android_func_output.output_set_commit_handler = android_output_set_commit_handler;
	android_func_output.output_set_dpms = android_output_set_dpms;
	android_func_output.output_get_dpms = android_output_get_dpms;
	android_func_output.output_set_mode = android_output_set_mode;
	android_func_output.output_get_mode = android_output_get_mode;
	android_func_output.output_set_status_handler = android_output_set_status_handler;

	memset(&android_func_layer, 0, sizeof(android_func_layer));
	android_func_layer.layer_get_capability = android_layer_get_capability;
	android_func_layer.layer_set_property = android_layer_set_property;
	android_func_layer.layer_get_property = android_layer_get_property;
	android_func_layer.layer_set_info = android_layer_set_info;
	android_func_layer.layer_get_info = android_layer_get_info;
	android_func_layer.layer_set_buffer = android_layer_set_buffer;
	android_func_layer.layer_unset_buffer = android_layer_unset_buffer;

	ret = tdm_backend_register_func_display(dpy, &android_func_display);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_output(dpy, &android_func_output);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_layer(dpy, &android_func_layer);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	android_data->dpy = dpy;

	init_hwc(&android_data->hwc_manager);

	if (error)
		*error = TDM_ERROR_NONE;

	TDM_INFO("init success!");

	return (tdm_backend_data *)android_data;
failed:
	if (error)
		*error = ret;

	tdm_android_deinit(android_data);

	TDM_ERR("init failed!");
	return NULL;
}

tdm_backend_module tdm_backend_module_data = {
	"android",
	"Samsung",
	TDM_BACKEND_ABI_VERSION,
	tdm_android_init,
	tdm_android_deinit
};

