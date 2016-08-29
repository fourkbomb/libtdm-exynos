#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include "tdm_android.h"

static tdm_android_data *android_data;

void
tdm_android_deinit(tdm_backend_data *bdata)
{
	if (android_data != bdata)
		return;

	TDM_INFO("deinit");

	if (android_data->hwc_dev)
		hwc_close_1(android_data->hwc_dev);

	free(android_data);
	android_data = NULL;
}

tdm_backend_data *
tdm_android_init(tdm_display *dpy, tdm_error *error)
{
	tdm_func_display android_func_display;
	tdm_func_output android_func_output;
	tdm_func_layer android_func_layer;
	tdm_func_pp android_func_pp;
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

	ret = hw_get_module(HWC_HARDWARE_MODULE_ID,
						(const hw_module_t **)&android_data->hwc_module);
	if (ret || !android_data->hwc_module) {
		TDM_ERR("cannot get hwc module");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	ret = hwc_open_1(android_data->hwc_module, &android_data->hwc_dev);
	if (ret || !android_data->hwc_dev) {
		TDM_ERR("cannot open hwc device");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
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

	memset(&android_func_pp, 0, sizeof(android_func_pp));
	android_func_pp.pp_destroy = android_pp_destroy;
	android_func_pp.pp_set_info = android_pp_set_info;
	android_func_pp.pp_attach = android_pp_attach;
	android_func_pp.pp_commit = android_pp_commit;
	android_func_pp.pp_set_done_handler = android_pp_set_done_handler;

	ret = tdm_backend_register_func_display(dpy, &android_func_display);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_output(dpy, &android_func_output);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_layer(dpy, &android_func_layer);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_pp(dpy, &android_func_pp);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	android_data->dpy = dpy;

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

