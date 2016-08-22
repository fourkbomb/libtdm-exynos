#ifndef _TDM_EXYNOS_OUTPUT_H_
#define _TDM_EXYNOS_OUTPUT_H_

#include "tdm_exynos.h"

void tdm_exynos_output_cb_event(int fd, unsigned int sequence,
									   unsigned int tv_sec, unsigned int tv_usec,
									   void *user_data);
tdm_error
tdm_exynos_output_update_status(tdm_exynos_output_data *output_data,
										tdm_output_conn_status status);

#endif /* _TDM_EXYNOS_OUTPUT_H_ */
