#ifndef _TDM_EXYNOS_H_
#define _TDM_EXYNOS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <exynos_drm.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tdm_backend.h>
#include <tdm_log.h>
#include <tdm_list.h>

#include "tdm_exynos_types.h"
#include "tdm_exynos_format.h"
#include "tdm_exynos_display.h"
#include "tdm_exynos_output.h"
#include "tdm_exynos_layer.h"
#include "tdm_exynos_pp.h"

extern unsigned int exynos_screen_prerotation_hint;

/* exynos backend functions */
tdm_error    exynos_display_get_capability(tdm_backend_data *bdata, tdm_caps_display *caps);
tdm_error    exynos_display_get_pp_capability(tdm_backend_data *bdata, tdm_caps_pp *caps);
tdm_output** exynos_display_get_outputs(tdm_backend_data *bdata, int *count, tdm_error *error);
tdm_error    exynos_display_get_fd(tdm_backend_data *bdata, int *fd);
tdm_error    exynos_display_handle_events(tdm_backend_data *bdata);
tdm_pp*      exynos_display_create_pp(tdm_backend_data *bdata, tdm_error *error);
tdm_error    exynos_output_get_capability(tdm_output *output, tdm_caps_output *caps);
tdm_layer**  exynos_output_get_layers(tdm_output *output, int *count, tdm_error *error);
tdm_error    exynos_output_set_property(tdm_output *output, unsigned int id, tdm_value value);
tdm_error    exynos_output_get_property(tdm_output *output, unsigned int id, tdm_value *value);
tdm_error    exynos_output_wait_vblank(tdm_output *output, int interval, int sync, void *user_data);
tdm_error    exynos_output_set_vblank_handler(tdm_output *output, tdm_output_vblank_handler func);
tdm_error    exynos_output_commit(tdm_output *output, int sync, void *user_data);
tdm_error    exynos_output_set_commit_handler(tdm_output *output, tdm_output_commit_handler func);
tdm_error    exynos_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value);
tdm_error    exynos_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value);
tdm_error    exynos_output_set_mode(tdm_output *output, const tdm_output_mode *mode);
tdm_error    exynos_output_get_mode(tdm_output *output, const tdm_output_mode **mode);
tdm_error    exynos_output_set_status_handler(tdm_output *output, tdm_output_status_handler func, void *user_data);
tdm_error    exynos_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps);
tdm_error    exynos_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value);
tdm_error    exynos_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value);
tdm_error    exynos_layer_set_info(tdm_layer *layer, tdm_info_layer *info);
tdm_error    exynos_layer_get_info(tdm_layer *layer, tdm_info_layer *info);
tdm_error    exynos_layer_set_buffer(tdm_layer *layer, tbm_surface_h buffer);
tdm_error    exynos_layer_unset_buffer(tdm_layer *layer);
void         exynos_pp_destroy(tdm_pp *pp);
tdm_error    exynos_pp_set_info(tdm_pp *pp, tdm_info_pp *info);
tdm_error    exynos_pp_attach(tdm_pp *pp, tbm_surface_h src, tbm_surface_h dst);
tdm_error    exynos_pp_commit(tdm_pp *pp);
tdm_error    exynos_pp_set_done_handler(tdm_pp *pp, tdm_pp_done_handler func, void *user_data);

#endif /* _TDM_EXYNOS_H_ */
