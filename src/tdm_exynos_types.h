#ifndef _TDM_EXYNOS_TYPES_H_
#define _TDM_EXYNOS_TYPES_H_

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

#if HAVE_UDEV
#include <libudev.h>
#endif

/* exynos module internal macros, structures */
#define NEVER_GET_HERE() TDM_ERR("** NEVER GET HERE **")

#define C(b, m)                (((b) >> (m)) & 0xFF)
#define B(c, s)                ((((unsigned int)(c)) & 0xff) << (s))
#define FOURCC(a, b, c, d)     (B(d, 24) | B(c, 16) | B(b, 8) | B(a, 0))
#define FOURCC_STR(id)         C(id, 0), C(id, 8), C(id, 16), C(id, 24)

#define IS_RGB(format)      (format == TBM_FORMAT_XRGB8888 || format == TBM_FORMAT_ARGB8888 || \
							format == TBM_FORMAT_XBGR8888 || format == TBM_FORMAT_ABGR8888)

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SWAP(a, b)  ({ \
						int t; \
						t = a; \
						a = b; \
						b = t; \
					})
#define ROUNDUP(x)  (ceil(floor((float)(height) / 4)))

#define ALIGN_TO_16B(x)    ((((x) + (1 <<  4) - 1) >>  4) <<  4)
#define ALIGN_TO_32B(x)    ((((x) + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)   ((((x) + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_2KB(x)    ((((x) + (1 << 11) - 1) >> 11) << 11)
#define ALIGN_TO_8KB(x)    ((((x) + (1 << 13) - 1) >> 13) << 13)
#define ALIGN_TO_64KB(x)   ((((x) + (1 << 16) - 1) >> 16) << 16)

#define RETURN_VAL_IF_FAIL(cond, val) { \
	if (!(cond)) { \
		TDM_ERR("'%s' failed", #cond); \
		return val; \
	} \
}

typedef enum {
	TDM_EXYNOS_EVENT_TYPE_WAIT,
	TDM_EXYNOS_EVENT_TYPE_COMMIT,
	TDM_EXYNOS_EVENT_TYPE_PAGEFLIP,
} tdm_exynos_event_type;

typedef struct _tdm_exynos_data tdm_exynos_data;
typedef struct _tdm_exynos_output_data tdm_exynos_output_data;
typedef struct _tdm_exynos_layer_data tdm_exynos_layer_data;
typedef struct _tdm_exynos_event_data tdm_exynos_event_data;
typedef struct _tdm_exynos_display_buffer tdm_exynos_display_buffer;

struct _tdm_exynos_data {
	tdm_display *dpy;

	int drm_fd;

#if HAVE_UDEV
	struct udev_monitor *uevent_monitor;
	tdm_event_loop_source *uevent_source;
#endif

	/* If true, it means that the device has many planes for one crtc. If false,
	 * planes are dedicated to specific crtc.
	 */
	int has_zpos_info;

	/* If has_zpos_info is false and is_immutable_zpos is true, it means that
	 * planes are dedicated to specific crtc.
	 */
	int is_immutable_zpos;

	drmModeResPtr mode_res;
	drmModePlaneResPtr plane_res;

	struct list_head output_list;
	struct list_head buffer_list;
};

struct _tdm_exynos_output_data {
	struct list_head link;

	/* data which are fixed at initializing */
	tdm_exynos_data *exynos_data;
	uint32_t connector_id;
	uint32_t encoder_id;
	uint32_t crtc_id;
	uint32_t pipe;
	uint32_t dpms_prop_id;
	int count_modes;
	drmModeModeInfoPtr drm_modes;
	tdm_output_mode *output_modes;
	tdm_output_type connector_type;
	unsigned int connector_type_id;
	struct list_head layer_list;
	tdm_exynos_layer_data *primary_layer;

	/* not fixed data below */
	tdm_output_vblank_handler vblank_func;
	tdm_output_commit_handler commit_func;

	tdm_output_conn_status status;
	tdm_output_status_handler status_func;
	void *status_user_data;

	int mode_changed;
	const tdm_output_mode *current_mode;

	int crtc_set;
};

struct _tdm_exynos_layer_data {
	struct list_head link;

	/* data which are fixed at initializing */
	tdm_exynos_data *exynos_data;
	tdm_exynos_output_data *output_data;
	uint32_t plane_id;
	tdm_layer_capability capabilities;
	int zpos;

	/* not fixed data below */
	tdm_info_layer info;
	int info_changed;

	tdm_exynos_display_buffer *display_buffer;
	int display_buffer_changed;
};

struct _tdm_exynos_display_buffer {
	struct list_head link;

	unsigned int fb_id;
	tbm_surface_h buffer;
	int width;
};

struct _tdm_exynos_event_data {
	tdm_exynos_event_type type;
	tdm_exynos_output_data *output_data;
	void *user_data;
};

typedef struct _Drm_Event_Context {
	void (*pageflip_handler)(int fd, unsigned int sequence, unsigned int tv_sec,
							 unsigned int tv_usec, void *user_data);
	void (*vblank_handler)(int fd, unsigned int sequence, unsigned int tv_sec,
						   unsigned int tv_usec, void *user_data);
	void (*pp_handler)(int fd, unsigned int  prop_id, unsigned int *buf_idx,
					   unsigned int  tv_sec, unsigned int  tv_usec, void *user_data);
} Drm_Event_Context;

#endif /* _TDM_EXYNOS_TYPES_H_ */
