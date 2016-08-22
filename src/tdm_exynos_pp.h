#ifndef _TDM_EXYNOS_PP_H_
#define _TDM_EXYNOS_PP_H_

#include "tdm_exynos.h"

tdm_error    tdm_exynos_pp_get_capability(tdm_exynos_data *exynos_data, tdm_caps_pp *caps);
tdm_pp*      tdm_exynos_pp_create(tdm_exynos_data *exynos_data, tdm_error *error);
void         tdm_exynos_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
										 unsigned int tv_sec, unsigned int tv_usec, void *data);
void         tdm_exynos_pp_cb(int fd, unsigned int prop_id, unsigned int *buf_idx,
									unsigned int tv_sec, unsigned int tv_usec,
									void *user_data);
#endif /* _TDM_EXYNOS_PP_H_ */
