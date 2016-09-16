#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>

#include "tdm_hwc.h"

#include "tdm_android.h"

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

	output_data->commit_hndl_data = user_data;

	return android_hwc_output_commit(hwc_manager, output_data->otput_idx, sync, output);
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
	return TDM_ERROR_NONE;
}

tdm_error
android_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_mode(tdm_output *output, const tdm_output_mode *mode)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_get_mode(tdm_output *output, const tdm_output_mode **mode)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_output_set_status_handler(tdm_output *output,
                                 tdm_output_status_handler func,
                                 void *user_data)
{
	return TDM_ERROR_NONE;
}
