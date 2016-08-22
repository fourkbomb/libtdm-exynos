#ifndef _TDM_EXYNOS_DISPLAY_H_
#define _TDM_EXYNOS_DISPLAY_H_

#include "tdm_exynos.h"

void         tdm_exynos_display_update_output_status(tdm_exynos_data *exynos_data);
tdm_error    tdm_exynos_display_create_output_list(tdm_exynos_data *exynos_data);
void         tdm_exynos_display_destroy_output_list(tdm_exynos_data *exynos_data);
tdm_error    tdm_exynos_display_create_layer_list(tdm_exynos_data *exynos_data);
tdm_error    tdm_exynos_display_set_property(tdm_exynos_data *exynos_data,
													unsigned int obj_id, unsigned int obj_type,
													const char *name, unsigned int value);
tdm_error    tdm_exynos_display_get_property(tdm_exynos_data *exynos_data,
													unsigned int obj_id, unsigned int obj_type,
													const char *name, unsigned int *value, int *is_immutable);
tdm_exynos_display_buffer* tdm_exynos_display_find_buffer(tdm_exynos_data *exynos_data, tbm_surface_h buffer);
void         tdm_exynos_display_to_tdm_mode(drmModeModeInfoPtr drm_mode, tdm_output_mode *tdm_mode);


#endif /* _TDM_EXYNOS_DISPLAY_H_ */
