#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include "tdm_android.h"


tdm_error
tdm_android_display_create_layer_list(tdm_android_data *android_data)
{
	return TDM_ERROR_NONE;
}

void
tdm_android_display_update_output_status(tdm_android_data *android_data)
{
}

void
tdm_android_display_destroy_output_list(tdm_android_output_data *outputs, int num)
{
	return;
}

static tdm_error
tdm_android_display_create_output_list(tdm_android_output_data **outputs,
									   int *num)
{
	tdm_android_output_data *outputs_data;
	int i;

	if (HWC_NUM_DISPLAY_TYPES < 1) {
		TDM_ERR("invalid number of outputs");
		return TDM_ERROR_OPERATION_FAILED;
	}

	outputs_data = calloc(HWC_NUM_DISPLAY_TYPES, sizeof(tdm_android_output_data));
	if (!outputs_data) {
		TDM_ERR("alloc failed");
		return TDM_ERROR_OUT_OF_MEMORY;
	}

	for (i = 0; i < HWC_NUM_DISPLAY_TYPES; i++) {

		outputs_data[i].display_type = i;

		if (outputs_data[i].display_type == HWC_DISPLAY_PRIMARY)
			outputs_data[i].status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
		else
			outputs_data[i].status = TDM_OUTPUT_CONN_STATUS_DISCONNECTED;

		TDM_DBG("output(%d) display_type(%d)", i, outputs_data[i].display_type);
	}

	TDM_DBG("output count: %d", HWC_NUM_DISPLAY_TYPES);

	*outputs = outputs_data;
	*num = HWC_NUM_DISPLAY_TYPES;

	return TDM_ERROR_NONE;
}

tdm_error
tdm_android_display_set_property(tdm_android_data *android_data,
                                unsigned int obj_id, unsigned int obj_type,
                                const char *name, unsigned int value)
{
	return TDM_ERROR_NONE;
}

tdm_error
tdm_android_display_get_property(tdm_android_data *android_data,
                                unsigned int obj_id, unsigned int obj_type,
                                const char *name, unsigned int *value, int *is_immutable)
{
	return TDM_ERROR_NONE;
}

tdm_android_display_buffer *
tdm_android_display_find_buffer(tdm_android_data *android_data,
                               tbm_surface_h buffer)
{
	return NULL;
}

tdm_error
android_display_get_capabilitiy(tdm_backend_data *bdata, tdm_caps_display *caps)
{
	return TDM_ERROR_NONE;
}

tdm_output **
android_display_get_outputs(tdm_backend_data *bdata, int *count,
                           tdm_error *error)
{
	tdm_android_data *android_data = bdata;
	tdm_android_output_data *outputs_data = NULL;
	tdm_output **outputs;
	tdm_error ret;
	int i;

	RETURN_VAL_IF_FAIL(android_data, NULL);
	RETURN_VAL_IF_FAIL(count, NULL);

	if (!android_data->num_outputs) {
		ret = tdm_android_display_create_output_list(&outputs_data, count);
		if (ret != TDM_ERROR_NONE)
			goto failed_get;

		android_data->num_outputs = *count;
		for (i = 0; i < *count; ++i) {
			android_data->outputs[i] = outputs_data[i];
		}
		free(outputs_data);
	}

	*count = android_data->num_outputs;

	/* will be freed in frontend */
	outputs = calloc(*count, sizeof(tdm_android_output_data *));
	if (!outputs) {
		TDM_ERR("failed: alloc memory");
		*count = 0;
		ret = TDM_ERROR_OUT_OF_MEMORY;
		goto failed_get;
	}

	for (i = 0; i < *count; ++i)
		outputs[i] = &android_data->outputs[i];

	if (error)
		*error = TDM_ERROR_NONE;

	return outputs;
failed_get:
	if (error)
		*error = ret;
	return NULL;
}

tdm_error
android_display_get_fd(tdm_backend_data *bdata, int *fd)
{
	*fd = -1;

	return TDM_ERROR_NONE;
}

tdm_error
android_display_handle_events(tdm_backend_data *bdata)
{
	return TDM_ERROR_NONE;
}
