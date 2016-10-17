#ifndef _TDM_HWC_H_
#define _TDM_HWC_H_

#include <system/window.h>

#include "tdm_android_types.h"


tdm_error android_hwc_init(hwc_manager_t *hwc_manager_);
void android_hwc_deinit(hwc_manager_t hwc_manager);

tdm_error android_hwc_vsync_event_control(hwc_manager_t hwc_manager, int output_idx, int state);

int android_hwc_get_max_hw_layers(hwc_manager_t hwc_manager);
int android_hwc_get_max_num_outputs(hwc_manager_t hwc_manager);

tdm_error android_hwc_get_output_capabilities(hwc_manager_t hwc_manager, int output_idx, tdm_caps_output *caps);
tdm_error android_hwc_get_layer_capabilities(hwc_manager_t hwc_manager, int layer_idx, tdm_caps_layer *caps);

void android_hwc_layer_set_info(hwc_manager_t hwc_manager, int output_idx, int layer_idx, tdm_info_layer *info);
void android_hwc_layer_set_buff(hwc_manager_t hwc_manager, int output_idx, int layer_idx, buffer_handle_t buff);

void android_hwc_output_set_commit_handler(hwc_manager_t hwc_manager, int output_idx, tdm_output_commit_handler hndl);
tdm_error android_hwc_output_commit(hwc_manager_t hwc_manager, int output_idx, int sync, tdm_output *output, void *user_data);

tdm_error
android_hwc_output_set_mode(hwc_manager_t hwc_manager, int output_idx,
							unsigned int mode_idx);

tdm_error
android_hwc_output_set_dpms(hwc_manager_t hwc_manager, int output_idx,
							tdm_output_dpms dpms_value);

#endif /* _TDM_HWC_H_ */
