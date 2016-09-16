#ifndef _TDM_ANDROID_DISPLAY_H_
#define _TDM_ANDROID_DISPLAY_H_

#include "tdm_android.h"

void         tdm_android_display_update_output_status(tdm_android_data *android_data);
void         tdm_android_display_destroy_output_list(tdm_android_data *android_data);
tdm_error    tdm_android_display_set_property(tdm_android_data *android_data,
                                             unsigned int obj_id, unsigned int obj_type,
                                             const char *name, unsigned int value);
tdm_error    tdm_android_display_get_property(tdm_android_data *android_data,
                                             unsigned int obj_id, unsigned int obj_type,
                                             const char *name, unsigned int *value, int *is_immutable);
tdm_android_display_buffer* tdm_android_display_find_buffer(tdm_android_data *android_data, tbm_surface_h buffer);
//void         tdm_android_display_to_tdm_mode(drmModeModeInfoPtr drm_mode, tdm_output_mode *tdm_mode);


#endif /* _TDM_ANDROID_DISPLAY_H_ */
