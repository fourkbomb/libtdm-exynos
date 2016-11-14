#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include "tdm_hwc.h"

#include "tdm_android.h"


tdm_error
android_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps)
{
	tdm_android_layer_data *layer_data = layer;
	tdm_android_data *android_data;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

	android_data = layer_data->output->android_data;

	return android_hwc_get_layer_capabilities(android_data->hwc_manager,
			layer_data->layer_idx, caps);
}

tdm_error
android_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_layer_set_info(tdm_layer *layer, tdm_info_layer *info)
{
	tdm_android_output_data *output_data;
	tdm_android_layer_data *layer_data;
	hwc_manager_t hwc_manager;

	RETURN_VAL_IF_FAIL(layer, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	layer_data = layer;
	output_data = layer_data->output;
	hwc_manager = output_data->android_data->hwc_manager;

	layer_data->info = *info;

	android_hwc_layer_set_info(hwc_manager, output_data->otput_idx, layer_data->layer_idx, info);

	return TDM_ERROR_NONE;
}

tdm_error
android_layer_get_info(tdm_layer *layer, tdm_info_layer *info)
{
	tdm_android_layer_data *layer_data;

	RETURN_VAL_IF_FAIL(layer, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	layer_data = layer;

	*info = layer_data->info;

	TDM_DBG("layer:%p, info:%p", layer_data, info);

	return TDM_ERROR_NONE;
}

tdm_error
android_layer_set_buffer(tdm_layer *layer, tbm_surface_h surface)
{
	tdm_android_output_data *output_data;
	tdm_android_layer_data *layer_data;
	hwc_manager_t hwc_manager;
	buffer_handle_t buff;
	tbm_bo bo;

	RETURN_VAL_IF_FAIL(layer, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(surface, TDM_ERROR_INVALID_PARAMETER);

	layer_data = layer;
	output_data = layer_data->output;
	hwc_manager = output_data->android_data->hwc_manager;

	/* now we support only 'bo_idx == 1' case */
	bo = tbm_surface_internal_get_bo(surface, 0);
	RETURN_VAL_IF_FAIL(bo, TDM_ERROR_OPERATION_FAILED);

	buff = (buffer_handle_t)(tbm_bo_get_handle(bo, TBM_DEVICE_DEFAULT).u32);
	RETURN_VAL_IF_FAIL(buff, TDM_ERROR_OPERATION_FAILED);

	android_hwc_layer_set_buff(hwc_manager, output_data->otput_idx, layer_data->layer_idx, buff);

	return TDM_ERROR_NONE;
}

tdm_error
android_layer_unset_buffer(tdm_layer *layer)
{
	tdm_android_output_data *output_data;
	tdm_android_layer_data *layer_data;
	hwc_manager_t hwc_manager;

	RETURN_VAL_IF_FAIL(layer, TDM_ERROR_INVALID_PARAMETER);

	layer_data = layer;
	output_data = layer_data->output;
	hwc_manager = output_data->android_data->hwc_manager;

	android_hwc_layer_unset_buff(hwc_manager, output_data->otput_idx,
								 layer_data->layer_idx);

	return TDM_ERROR_NONE;
}
