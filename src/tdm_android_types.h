#ifndef _TDM_ANDROID_TYPES_H_
#define _TDM_ANDROID_TYPES_H_

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

#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tdm_backend.h>
#include <tdm_log.h>
#include <tdm_list.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

/* android module internal macros, structures */
#define NEVER_GET_HERE() TDM_ERR("** NEVER GET HERE **")

#define RETURN_VAL_IF_FAIL(cond, val) {\
    if (!(cond)) {\
        TDM_ERR("'%s' failed", #cond);\
        return val;\
    }\
}

typedef struct _tdm_android_data tdm_android_data;
typedef struct _tdm_android_output_data tdm_android_output_data;
typedef struct _tdm_android_layer_data tdm_android_layer_data;
typedef struct _tdm_android_display_buffer tdm_android_display_buffer;

typedef struct _hwc_manager* hwc_manager_t;

struct _tdm_android_data
{
    tdm_display *dpy;

    hwc_manager_t hwc_manager;

    struct list_head output_list;
};

struct _tdm_android_output_data
{
	struct list_head link;

	tdm_android_data *android_data;

	/* to identify output in hwcomposer world */
	int otput_idx;

	struct list_head layer_list;

	const tdm_output_mode *current_mode;
	tdm_output_dpms dpms_value;
};

struct _tdm_android_layer_data
{
    struct list_head link;

    /* output this layer is associated with */
    tdm_android_output_data *output;

    /* to identify layer in hwcomposer world
     * 0 - is the top layer
     * max_hw_layers - 1 - is the bottom layer
     * Note: for tbm layer with TDM_LAYER_CAPABILITY_PRIMARY capability
     *       this backend maps layer with HWC_FRAMEBUFFER_TARGET capability
     *       and it has layer_idx == max_hw_layers - 1 */
    int layer_idx;

    tdm_info_layer info;
};

struct _tdm_android_display_buffer
{
    struct list_head link;
};

#endif /* _TDM_ANDROID_TYPES_H_ */
