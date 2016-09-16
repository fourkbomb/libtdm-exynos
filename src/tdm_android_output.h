#ifndef _TDM_ANDROID_OUTPUT_H_
#define _TDM_ANDROID_OUTPUT_H_

#include "tdm_android.h"

void
tdm_android_output_destroy_layer_list(tdm_android_output_data *output_data);

void tdm_android_output_cb_event(int fd, unsigned int sequence,
                                unsigned int tv_sec, unsigned int tv_usec,
                                void *user_data);
tdm_error
tdm_android_output_update_status(tdm_android_output_data *output_data,
                                tdm_output_conn_status status);

#endif /* _TDM_ANDROID_OUTPUT_H_ */
