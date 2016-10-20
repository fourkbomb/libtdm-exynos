#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include "tdm_hwc.h"

#include "tdm_android.h"

void
tdm_android_output_destroy_layer_list(tdm_android_output_data *output_data)
{
	tdm_android_layer_data *l = NULL, *ll = NULL;

	if (LIST_IS_EMPTY(&output_data->layer_list)) {
		return;
	}

	LIST_FOR_EACH_ENTRY_SAFE(l, ll, &output_data->layer_list, link) {
		LIST_DEL(&l->link);
		free(l);
	}
}

static tdm_error
_tdm_android_output_create_layer_list(tdm_android_output_data *output_data)
{
	int i, num_layers;
	tdm_error ret;
	tdm_android_layer_data *layer_data;
	tdm_android_data *android_data = output_data->android_data;

	num_layers = android_hwc_get_max_hw_layers(android_data->hwc_manager);

	if (num_layers < 1) {
		TDM_ERR("invalid number of layers");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed_create;
	}

	for (i = 0; i < num_layers; ++i) {
		layer_data = calloc(1, sizeof(tdm_android_layer_data));
		if (!layer_data) {
			TDM_ERR("alloc failed");
			ret = TDM_ERROR_OUT_OF_MEMORY;
			goto failed_create;
		}
		layer_data->layer_idx = i;
		layer_data->output = output_data;

		LIST_ADDTAIL(&layer_data->link, &output_data->layer_list);
	}

	TDM_DBG("layers count: %d", num_layers);

	return TDM_ERROR_NONE;
failed_create:
	tdm_android_output_destroy_layer_list(output_data);
	return ret;
}

void
tdm_android_output_cb_event(int fd, unsigned int sequence,
                           unsigned int tv_sec, unsigned int tv_usec,
                           void *user_data)
{
}

tdm_error
tdm_android_output_update_status(tdm_android_output_data *output_data,
                                tdm_output_conn_status status)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_get_capability(tdm_output *output, tdm_caps_output *caps)
{
	tdm_error ret;
	tdm_android_output_data *output_data = output;
	tdm_android_data *android_data;

	RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

	android_data = output_data->android_data;

	ret = android_hwc_get_output_capabilities(android_data->hwc_manager,
											  output_data->otput_idx, caps);

	return ret;
}

tdm_layer **
android_output_get_layers(tdm_output *output,  int *count, tdm_error *error)
{
	tdm_android_output_data *output_data = output;
	tdm_android_layer_data *layer_data = NULL;
	tdm_layer **layers;
	tdm_error ret;
	int i;

	RETURN_VAL_IF_FAIL(output_data, NULL);
	RETURN_VAL_IF_FAIL(count, NULL);

	if (LIST_IS_EMPTY(&output_data->layer_list)) {
		ret = _tdm_android_output_create_layer_list(output_data);
		if (ret != TDM_ERROR_NONE) {
			*count = 0;
			goto failed_get;
		}
	}

	*count = 0;
	LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link)
	(*count)++;

	/* will be freed in frontend */
	layers = calloc(*count, sizeof(tdm_android_layer_data *));
	if (!layers) {
		TDM_ERR("failed: alloc memory");
		*count = 0;
		ret = TDM_ERROR_OUT_OF_MEMORY;
		goto failed_get;
	}

	i = 0;
	LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link)
	layers[i++] = layer_data;

	if (error)
		*error = TDM_ERROR_NONE;

	return layers;
failed_get:
	if (error)
		*error = ret;
	return NULL;
}

tdm_error
android_output_set_property(tdm_output *output, unsigned int id, tdm_value value)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_get_property(tdm_output *output, unsigned int id,
                           tdm_value *value)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_wait_vblank(tdm_output *output, int interval, int sync,
                          void *user_data)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_vblank_handler(tdm_output *output,
                                 tdm_output_vblank_handler func)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_commit(tdm_output *output, int sync, void *user_data)
{
	tdm_android_output_data *output_data;
	hwc_manager_t hwc_manager;

	RETURN_VAL_IF_FAIL(output, TDM_ERROR_INVALID_PARAMETER);

	output_data = output;
	hwc_manager = output_data->android_data->hwc_manager;

	return android_hwc_output_commit(hwc_manager, output_data->otput_idx, sync, output, user_data);
}

tdm_error
android_output_set_commit_handler(tdm_output *output,
                                 tdm_output_commit_handler hndl)
{
	tdm_android_output_data *output_data;
	hwc_manager_t hwc_manager;

	RETURN_VAL_IF_FAIL(output, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(hndl, TDM_ERROR_INVALID_PARAMETER);

	output_data = output;
	hwc_manager = output_data->android_data->hwc_manager;

	android_hwc_output_set_commit_handler(hwc_manager, output_data->otput_idx, hndl);

	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value)
{
	tdm_android_output_data *output_data = output;
	tdm_error ret;

	RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);

	ret = android_hwc_output_set_dpms(output_data->android_data->hwc_manager,
									  output_data->otput_idx, dpms_value);

	if (ret == TDM_ERROR_NONE)
		output_data->dpms_value = dpms_value;

	return ret;
}

tdm_error
android_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value)
{
	tdm_android_output_data *output_data = output;

	RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(dpms_value, TDM_ERROR_INVALID_PARAMETER);

	*dpms_value = output_data->dpms_value;

	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_mode(tdm_output *output, const tdm_output_mode *mode)
{
	tdm_error ret;
	tdm_android_output_data *output_data = output;
	hwc_manager_t hwc_manager;

	RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(mode, TDM_ERROR_INVALID_PARAMETER);

	hwc_manager = output_data->android_data->hwc_manager;

	ret = android_hwc_output_set_mode(hwc_manager, output_data->otput_idx,
									  mode->flags);

	if (ret == TDM_ERROR_NONE) {
		output_data->current_mode = mode;
	}

	return ret;
}

tdm_error
android_output_get_mode(tdm_output *output, const tdm_output_mode **mode)
{
	tdm_android_output_data *output_data = output;

	RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(mode, TDM_ERROR_INVALID_PARAMETER);

	*mode = output_data->current_mode;

	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_status_handler(tdm_output *output,
                                 tdm_output_status_handler func,
                                 void *user_data)
{
	return TDM_ERROR_NONE;
}
