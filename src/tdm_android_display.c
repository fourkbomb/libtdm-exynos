#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include "tdm_android.h"

#include "tdm_hwc.h"

void
tdm_android_display_update_output_status(tdm_android_data *android_data)
{
}

void
tdm_android_display_destroy_output_list(tdm_android_data *android_data)
{
	tdm_android_output_data *o = NULL, *oo = NULL;

	if (LIST_IS_EMPTY(&android_data->output_list))
		return;

	LIST_FOR_EACH_ENTRY_SAFE(o, oo, &android_data->output_list, link) {
		TDM_DBG("data:%p, output_list:%p, output:%p",
				android_data, &android_data->output_list, o);
		LIST_DEL(&o->link);
		tdm_android_output_destroy_layer_list(o);
		free(o);
	}

	return;
}

static tdm_error
tdm_android_display_create_output_list(tdm_android_data *android_data)
{
	tdm_android_output_data *output_data;
	int i, num_outputs;
	tdm_error ret;

	num_outputs = android_hwc_get_max_num_outputs(android_data->hwc_manager);

	if (num_outputs < 1) {
		TDM_ERR("invalid number of outputs");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed_create;
	}

	for (i = 0; i < num_outputs; ++i) {
		output_data = calloc(1, sizeof(tdm_android_output_data));
		if (!output_data) {
			TDM_ERR("alloc failed");
			ret = TDM_ERROR_OUT_OF_MEMORY;
			goto failed_create;
		}
		output_data->otput_idx = i;
		output_data->android_data = android_data;
		output_data->dpms_value = TDM_OUTPUT_DPMS_OFF;

		LIST_INITHEAD(&output_data->layer_list);

		TDM_DBG("output:%p, otput_idx:d, display_type:%s, dpms_value:%s",
				output_data, i, str_display_type[i],
				tdm_output_dpms_to_str(output_data->dpms_value));

		LIST_ADDTAIL(&output_data->link, &android_data->output_list);
	}

	TDM_DBG("output count: %d", num_outputs);

	return TDM_ERROR_NONE;
failed_create:
	tdm_android_display_destroy_output_list(android_data);
	return ret;
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
	RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

	/* hwc does not provide the ability to determine max layer count */
	caps->max_layer_count = -1; /* not defined */

	return TDM_ERROR_NONE;
}

tdm_output **
android_display_get_outputs(tdm_backend_data *bdata, int *count,
                           tdm_error *error)
{
	tdm_android_data *android_data = bdata;
	tdm_android_output_data *output_data = NULL;
	tdm_output **outputs;
	tdm_error ret;
	int i;

	RETURN_VAL_IF_FAIL(android_data, NULL);
	RETURN_VAL_IF_FAIL(count, NULL);

	if (LIST_IS_EMPTY(&android_data->output_list)) {
		ret = tdm_android_display_create_output_list(android_data);
		if (ret != TDM_ERROR_NONE) {
			*count = 0;
			goto failed_get;
		}
	}

	*count = 0;
	LIST_FOR_EACH_ENTRY(output_data, &android_data->output_list, link)
	(*count)++;

	/* will be freed in frontend */
	outputs = calloc(*count, sizeof(tdm_android_output_data *));
	if (!outputs) {
		TDM_ERR("failed: alloc memory");
		*count = 0;
		ret = TDM_ERROR_OUT_OF_MEMORY;
		goto failed_get;
	}

	i = 0;
	LIST_FOR_EACH_ENTRY(output_data, &android_data->output_list, link)
	outputs[i++] = output_data;

	if (error)
		*error = TDM_ERROR_NONE;

	TDM_DBG("outputs:%p, count:%d", outputs, *count);

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
