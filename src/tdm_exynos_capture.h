#ifndef _TDM_EXYNOS_CAPTURE_H_
#define _TDM_EXYNOS_CAPTURE_H_

#include "tdm_exynos.h"

tdm_error    tdm_exynos_capture_get_capability(tdm_exynos_data *exynos_data, tdm_caps_capture *caps);
tdm_pp*      tdm_exynos_capture_create_output(tdm_exynos_data *exynos_data, tdm_output *output, tdm_error *error);
void         tdm_exynos_capture_stream_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
												  unsigned int tv_sec, unsigned int tv_usec, void *data);
int          tdm_exynos_capture_find_prop_id(unsigned int prop_id);

#endif /* _TDM_EXYNOS_CAPTURE_H_ */
