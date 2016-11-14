#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include "tdm_hwc.h"

#include "tdm_android.h"

static tdm_android_data *android_data;

char *str_display_type[] = {
		"primary",
		"external",
		"virtual"
};

char *str_composit_type[] = {
		"HWC_FRAMEBUFFER",
		"HWC_OVERLAY",
		"HWC_BACKGROUND",
		"HWC_FRAMEBUFFER_TARGET",
		"HWC_SIDEBAND",
		"HWC_CURSOR_OVERLAY",
		"HWC_BLIT"
};

char *tdm_output_dpms_to_str(tdm_output_dpms dpms)
{
	if (dpms == TDM_OUTPUT_DPMS_ON) {
		return "TDM_OUTPUT_DPMS_ON";
	} else if (dpms == TDM_OUTPUT_DPMS_STANDBY) {
		return "TDM_OUTPUT_DPMS_STANDBY";
	} else if (dpms == TDM_OUTPUT_DPMS_SUSPEND) {
		return "TDM_OUTPUT_DPMS_SUSPEND";
	} else if (dpms == TDM_OUTPUT_DPMS_OFF) {
		return "TDM_OUTPUT_DPMS_OFF";
	}

	return "UNKNOWN";
}

void _tdm_dbg_layer_capability(tdm_layer_capability capa)
{
	if (capa & TDM_LAYER_CAPABILITY_CURSOR)
		TDM_DBG("TDM_LAYER_CAPABILITY_CURSOR");
	if (capa & TDM_LAYER_CAPABILITY_PRIMARY)
		TDM_DBG("TDM_LAYER_CAPABILITY_PRIMARY");
	if (capa & TDM_LAYER_CAPABILITY_OVERLAY)
		TDM_DBG("TDM_LAYER_CAPABILITY_OVERLAY");
	if (capa & TDM_LAYER_CAPABILITY_SCALE)
		TDM_DBG("TDM_LAYER_CAPABILITY_SCALE");
	if (capa & TDM_LAYER_CAPABILITY_TRANSFORM)
		TDM_DBG("TDM_LAYER_CAPABILITY_TRANSFORM");
	if (capa & TDM_LAYER_CAPABILITY_GRAPHIC)
		TDM_DBG("TDM_LAYER_CAPABILITY_GRAPHIC");
	if (capa & TDM_LAYER_CAPABILITY_VIDEO)
		TDM_DBG("TDM_LAYER_CAPABILITY_VIDEO");
}

static void
tdm_android_deinit(tdm_backend_data *bdata)
{
	if (android_data != bdata)
		return;

	TDM_INFO("deinit");

	tdm_android_display_destroy_output_list(android_data);

	android_hwc_deinit(android_data->hwc_manager);

	TDM_DBG("data:%p", android_data);

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

	LIST_INITHEAD(&android_data->output_list);

	memset(&android_func_display, 0, sizeof(android_func_display));
	android_func_display.display_get_capabilitiy = android_display_get_capabilitiy;
	android_func_display.display_get_outputs = android_display_get_outputs;
	android_func_display.display_get_fd = android_display_get_fd;
	android_func_display.display_handle_events = android_display_handle_events;

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

	TDM_DBG("display_get_capabilitiy:%p\n"
			"		display_get_outputs:%p\n"
			"		display_get_fd:%p\n"
			"		display_handle_events:%p\n"
			"\n		output_get_capability:%p\n"
			"		output_get_layers:%p\n"
			"		output_set_property:%p\n"
			"		output_get_property:%p\n"
			"		output_wait_vblank:%p\n"
			"		output_set_vblank_handler:%p\n"
			"		output_commit:%p\n"
			"		output_set_commit_handler:%p\n"
			"		output_set_dpms:%p\n"
			"		output_get_dpms:%p\n"
			"		output_set_mode:%p\n"
			"		output_get_mode:%p\n"
			"		output_set_status_handler:%p\n"
			"\n		layer_get_capability:%p\n"
			"		layer_set_property:%p\n"
			"		layer_get_property:%p\n"
			"		layer_set_info:%p\n"
			"		layer_get_info:%p\n"
			"		layer_set_buffer:%p\n"
			"		layer_unset_buffer:%p",
			"		display_get_capabilitiy:%p\n",
		android_func_display.display_get_outputs,
		android_func_display.display_get_fd,
		android_func_display.display_handle_events,
		android_func_output.output_get_capability,
		android_func_output.output_get_layers,
		android_func_output.output_set_property,
		android_func_output.output_get_property,
		android_func_output.output_wait_vblank,
		android_func_output.output_set_vblank_handler,
		android_func_output.output_commit,
		android_func_output.output_set_commit_handler,
		android_func_output.output_set_dpms,
		android_func_output.output_get_dpms,
		android_func_output.output_set_mode,
		android_func_output.output_get_mode,
		android_func_output.output_set_status_handler,
		android_func_layer.layer_get_capability,
		android_func_layer.layer_set_property,
		android_func_layer.layer_get_property,
		android_func_layer.layer_set_info,
		android_func_layer.layer_get_info,
		android_func_layer.layer_set_buffer,
		android_func_layer.layer_unset_buffer);

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

	android_hwc_init(&android_data->hwc_manager);

	if (error)
		*error = TDM_ERROR_NONE;

	TDM_DBG("dpy:%p, data:p, hwc_manager:%p", dpy, android_data,
			android_data->hwc_manager);

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

