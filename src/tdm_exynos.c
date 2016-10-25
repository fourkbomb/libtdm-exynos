#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_exynos.h"
#include <tdm_helper.h>

#define EXYNOS_DRM_NAME "exynos"

static tdm_exynos_data *exynos_data;
unsigned int exynos_screen_prerotation_hint;

#ifdef HAVE_UDEV
static tdm_error
_tdm_exynos_udev_fd_handler(int fd, tdm_event_loop_mask mask, void *user_data)
{
	tdm_exynos_data *edata = (tdm_exynos_data*)user_data;
	struct udev_device *dev;
	const char *hotplug;
	struct stat s;
	dev_t udev_devnum;
	int ret;

	dev = udev_monitor_receive_device(edata->uevent_monitor);
	if (!dev) {
		TDM_ERR("couldn't receive device");
		return TDM_ERROR_OPERATION_FAILED;
	}

	udev_devnum = udev_device_get_devnum(dev);

	ret = fstat(edata->drm_fd, &s);
	if (ret == -1) {
		TDM_ERR("fstat failed");
		return TDM_ERROR_OPERATION_FAILED;
	}

	hotplug = udev_device_get_property_value(dev, "HOTPLUG");

	if (memcmp(&s.st_rdev, &udev_devnum, sizeof(dev_t)) == 0 &&
			   hotplug && atoi(hotplug) == 1) {
		TDM_INFO("HotPlug");
		tdm_exynos_display_update_output_status(edata);
	}

	udev_device_unref(dev);

	return TDM_ERROR_NONE;
}

static void
_tdm_exynos_udev_init(tdm_exynos_data *edata)
{
	struct udev *u = NULL;
	struct udev_monitor *mon = NULL;

	u = udev_new();
	if (!u) {
		TDM_ERR("couldn't create udev");
		goto failed;
	}

	mon = udev_monitor_new_from_netlink(u, "udev");
	if (!mon) {
		TDM_ERR("couldn't create udev monitor");
		goto failed;
	}

	if (udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", "drm_minor") > 0 ||
	    udev_monitor_enable_receiving(mon) < 0) {
		TDM_ERR("add match subsystem failed");
		goto failed;
	}

	edata->uevent_source =
		tdm_event_loop_add_fd_handler(edata->dpy, udev_monitor_get_fd(mon),
									  TDM_EVENT_LOOP_READABLE,
									  _tdm_exynos_udev_fd_handler,
									  edata, NULL);
	if (!edata->uevent_source) {
		TDM_ERR("couldn't create udev event source");
		goto failed;
	}

	edata->uevent_monitor = mon;

	TDM_INFO("hotplug monitor created");

	return;
failed:
	if (mon)
		udev_monitor_unref(mon);
	if (u)
		udev_unref(u);
}

static void
_tdm_exynos_udev_deinit(tdm_exynos_data *edata)
{
	if (edata->uevent_source) {
		tdm_event_loop_source_remove(edata->uevent_source);
		edata->uevent_source = NULL;
	}

	if (edata->uevent_monitor) {
		struct udev *u = udev_monitor_get_udev(edata->uevent_monitor);
		udev_monitor_unref(edata->uevent_monitor);
		udev_unref(u);
		edata->uevent_monitor = NULL;
		TDM_INFO("hotplug monitor destroyed");
	}
}
#endif

static int
_tdm_exynos_open_drm(void)
{
	int fd = -1;

	fd = drmOpen(EXYNOS_DRM_NAME, NULL);
	if (fd < 0)
		TDM_ERR("Cannot open '%s' drm", EXYNOS_DRM_NAME);

#ifdef HAVE_UDEV
	if (fd < 0) {
		struct udev *udev;
		struct udev_enumerate *e;
		struct udev_list_entry *entry;
		struct udev_device *device, *drm_device, *device_parent;
		const char *filename;

		TDM_WRN("Cannot open drm device.. search by udev");
		udev = udev_new();
		if (!udev) {
			TDM_ERR("fail to initialize udev context\n");
			goto close_l;
		}

		/* Will try to find sys path /exynos-drm/drm/card0 */
		e = udev_enumerate_new(udev);
		udev_enumerate_add_match_subsystem(e, "drm");
		udev_enumerate_add_match_sysname(e, "card[0-9]*");
		udev_enumerate_scan_devices(e);

		drm_device = NULL;
		udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
			device = udev_device_new_from_syspath(udev_enumerate_get_udev(e),
												  udev_list_entry_get_name(entry));
			device_parent = udev_device_get_parent(device);
			/* Not need unref device_parent. device_parent and device have same refcnt */
			if (device_parent) {
				if (strcmp(udev_device_get_sysname(device_parent), "exynos-drm") == 0) {
					drm_device = device;
					TDM_DBG("Found drm device: '%s' (%s)\n",
							udev_device_get_syspath(drm_device),
							udev_device_get_sysname(device_parent));
					break;
				}
			}
			udev_device_unref(device);
		}

		if (drm_device == NULL) {
			TDM_ERR("fail to find drm device\n");
			udev_enumerate_unref(e);
			udev_unref(udev);
			goto close_l;
		}

		filename = udev_device_get_devnode(drm_device);

		fd = open(filename, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			TDM_ERR("Cannot open drm device(%s)\n", filename);
		udev_device_unref(drm_device);
		udev_enumerate_unref(e);
		udev_unref(udev);
	}
close_l:
#endif
	return fd;
}

static int
_tdm_exynos_drm_user_handler(struct drm_event *event)
{
	struct drm_exynos_ipp_event *ipp;

	if (event->type != DRM_EXYNOS_IPP_EVENT)
		return -1;

	TDM_DBG("got ipp event");

	ipp = (struct drm_exynos_ipp_event *)event;

	tdm_exynos_pp_handler(ipp->prop_id, ipp->buf_id, ipp->tv_sec, ipp->tv_usec,
						  (void *)(unsigned long)ipp->user_data);

	return 0;
}

void
tdm_exynos_deinit(tdm_backend_data *bdata)
{
	if (exynos_data != bdata)
		return;

	TDM_INFO("deinit");

#ifdef HAVE_UDEV
	_tdm_exynos_udev_deinit(exynos_data);
#endif

	drmRemoveUserHandler(exynos_data->drm_fd, _tdm_exynos_drm_user_handler);

	tdm_exynos_display_destroy_output_list(exynos_data);

	if (exynos_data->plane_res)
		drmModeFreePlaneResources(exynos_data->plane_res);
	if (exynos_data->mode_res)
		drmModeFreeResources(exynos_data->mode_res);
	if (exynos_data->drm_fd >= 0)
		close(exynos_data->drm_fd);

	free(exynos_data);
	exynos_data = NULL;
}

tdm_backend_data *
tdm_exynos_init(tdm_display *dpy, tdm_error *error)
{
	tdm_func_display exynos_func_display;
	tdm_func_output exynos_func_output;
	tdm_func_layer exynos_func_layer;
	tdm_func_pp exynos_func_pp;
	tdm_error ret;
	const char *value;

	value = (const char*)getenv("SCREEN_PREROTATION_HINT");
	if (value) {
		char *end;
		exynos_screen_prerotation_hint = strtol(value, &end, 10);
		TDM_INFO("SCREEN_PREROTATION_HINT = %d", exynos_screen_prerotation_hint);
	}

	if (!dpy) {
		TDM_ERR("display is null");
		if (error)
			*error = TDM_ERROR_INVALID_PARAMETER;
		return NULL;
	}

	if (exynos_data) {
		TDM_ERR("failed: init twice");
		if (error)
			*error = TDM_ERROR_BAD_REQUEST;
		return NULL;
	}

	exynos_data = calloc(1, sizeof(tdm_exynos_data));
	if (!exynos_data) {
		TDM_ERR("alloc failed");
		if (error)
			*error = TDM_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	LIST_INITHEAD(&exynos_data->output_list);
	LIST_INITHEAD(&exynos_data->buffer_list);

	memset(&exynos_func_display, 0, sizeof(exynos_func_display));
	exynos_func_display.display_get_capability = exynos_display_get_capability;
	exynos_func_display.display_get_pp_capability = exynos_display_get_pp_capability;
	exynos_func_display.display_get_outputs = exynos_display_get_outputs;
	exynos_func_display.display_get_fd = exynos_display_get_fd;
	exynos_func_display.display_handle_events = exynos_display_handle_events;
	exynos_func_display.display_create_pp = exynos_display_create_pp;

	memset(&exynos_func_output, 0, sizeof(exynos_func_output));
	exynos_func_output.output_get_capability = exynos_output_get_capability;
	exynos_func_output.output_get_layers = exynos_output_get_layers;
	exynos_func_output.output_set_property = exynos_output_set_property;
	exynos_func_output.output_get_property = exynos_output_get_property;
	exynos_func_output.output_wait_vblank = exynos_output_wait_vblank;
	exynos_func_output.output_set_vblank_handler = exynos_output_set_vblank_handler;
	exynos_func_output.output_commit = exynos_output_commit;
	exynos_func_output.output_set_commit_handler = exynos_output_set_commit_handler;
	exynos_func_output.output_set_dpms = exynos_output_set_dpms;
	exynos_func_output.output_get_dpms = exynos_output_get_dpms;
	exynos_func_output.output_set_mode = exynos_output_set_mode;
	exynos_func_output.output_get_mode = exynos_output_get_mode;
#ifdef HAVE_UDEV
	exynos_func_output.output_set_status_handler = exynos_output_set_status_handler;
#endif

	memset(&exynos_func_layer, 0, sizeof(exynos_func_layer));
	exynos_func_layer.layer_get_capability = exynos_layer_get_capability;
	exynos_func_layer.layer_set_property = exynos_layer_set_property;
	exynos_func_layer.layer_get_property = exynos_layer_get_property;
	exynos_func_layer.layer_set_info = exynos_layer_set_info;
	exynos_func_layer.layer_get_info = exynos_layer_get_info;
	exynos_func_layer.layer_set_buffer = exynos_layer_set_buffer;
	exynos_func_layer.layer_unset_buffer = exynos_layer_unset_buffer;

	memset(&exynos_func_pp, 0, sizeof(exynos_func_pp));
	exynos_func_pp.pp_destroy = exynos_pp_destroy;
	exynos_func_pp.pp_set_info = exynos_pp_set_info;
	exynos_func_pp.pp_attach = exynos_pp_attach;
	exynos_func_pp.pp_commit = exynos_pp_commit;
	exynos_func_pp.pp_set_done_handler = exynos_pp_set_done_handler;

	ret = tdm_backend_register_func_display(dpy, &exynos_func_display);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_output(dpy, &exynos_func_output);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_layer(dpy, &exynos_func_layer);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_backend_register_func_pp(dpy, &exynos_func_pp);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	exynos_data->dpy = dpy;

	/* The drm master fd can be opened by a tbm backend module in
	 * tbm_bufmgr_init() time. In this case, we just get it from
	 * TBM_DRM_MASTER_FD enviroment.
	 *
	 */
	exynos_data->drm_fd = tdm_helper_get_fd("TBM_DRM_MASTER_FD");
	if (exynos_data->drm_fd < 0)
		exynos_data->drm_fd = _tdm_exynos_open_drm();

	if (exynos_data->drm_fd < 0) {
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	/* To share the drm master fd with other modules in display server side. */
	tdm_helper_set_fd("TDM_DRM_MASTER_FD", exynos_data->drm_fd);

	TDM_DBG("drm_fd(%d)", exynos_data->drm_fd);

#ifdef HAVE_UDEV
	_tdm_exynos_udev_init(exynos_data);
#endif

	drmAddUserHandler(exynos_data->drm_fd, _tdm_exynos_drm_user_handler);

	if (drmSetClientCap(exynos_data->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0)
		TDM_WRN("Set DRM_CLIENT_CAP_UNIVERSAL_PLANES failed");

	exynos_data->mode_res = drmModeGetResources(exynos_data->drm_fd);
	if (!exynos_data->mode_res) {
		TDM_ERR("no drm resource: %m");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	exynos_data->plane_res = drmModeGetPlaneResources(exynos_data->drm_fd);
	if (!exynos_data->plane_res) {
		TDM_ERR("no drm plane resource: %m");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	if (exynos_data->plane_res->count_planes <= 0) {
		TDM_ERR("no drm plane resource");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed;
	}

	ret = tdm_exynos_display_get_property(exynos_data,
										  exynos_data->plane_res->planes[0],
										  DRM_MODE_OBJECT_PLANE, "zpos", NULL,
										  &exynos_data->is_immutable_zpos);
	if (ret == TDM_ERROR_NONE) {
		exynos_data->has_zpos_info = 1;
		if (exynos_data->is_immutable_zpos)
			TDM_DBG("plane has immutable zpos info");
	} else
		TDM_DBG("plane doesn't have zpos info");

	ret = tdm_exynos_display_create_output_list(exynos_data);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	ret = tdm_exynos_display_create_layer_list(exynos_data);
	if (ret != TDM_ERROR_NONE)
		goto failed;

	if (error)
		*error = TDM_ERROR_NONE;

	TDM_INFO("init success!");

	return (tdm_backend_data *)exynos_data;
failed:
	if (error)
		*error = ret;

	tdm_exynos_deinit(exynos_data);

	TDM_ERR("init failed!");
	return NULL;
}

tdm_backend_module tdm_backend_module_data = {
	"exynos",
	"Samsung",
	TDM_BACKEND_ABI_VERSION,
	tdm_exynos_init,
	tdm_exynos_deinit
};

