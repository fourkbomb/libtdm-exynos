#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "tdm_exynos.h"
#include <tdm_helper.h>

#define EXYNOS_DRM_NAME "i915"

static tdm_func_display exynos_func_display =
{
    exynos_display_get_capabilitiy,
    exynos_display_get_pp_capability,
    NULL,  //display_get_capture_capability
    exynos_display_get_outputs,
    exynos_display_get_fd,
    exynos_display_handle_events,
    exynos_display_create_pp,
    exynos_output_get_capability,
    exynos_output_get_layers,
    exynos_output_set_property,
    exynos_output_get_property,
    exynos_output_wait_vblank,
    exynos_output_set_vblank_handler,
    exynos_output_commit,
    exynos_output_set_commit_handler,
    exynos_output_set_dpms,
    exynos_output_get_dpms,
    exynos_output_set_mode,
    exynos_output_get_mode,
    NULL,   //output_create_capture
    exynos_layer_get_capability,
    exynos_layer_set_property,
    exynos_layer_get_property,
    exynos_layer_set_info,
    exynos_layer_get_info,
    exynos_layer_set_buffer,
    exynos_layer_unset_buffer,
    NULL,    //layer_create_capture
};

static tdm_func_pp exynos_func_pp =
{
    exynos_pp_destroy,
    exynos_pp_set_property,
    exynos_pp_get_property,
    exynos_pp_set_info,
    exynos_pp_attach,
    exynos_pp_commit,
    exynos_pp_set_done_handler,
};

static tdm_exynos_data *exynos_data;

static int
_tdm_exynos_open_drm(void)
{
    int fd = -1;

    fd = drmOpen(EXYNOS_DRM_NAME, NULL);
    if (fd < 0)
    {
        TDM_ERR("Cannot open '%s' drm", EXYNOS_DRM_NAME);
    }

#ifdef HAVE_UDEV
    if (fd < 0)
    {
        struct udev *udev;
        struct udev_enumerate *e;
        struct udev_list_entry *entry;
        struct udev_device *device, *drm_device, *device_parent;
        const char *filename;

        TDM_WRN("Cannot open drm device.. search by udev");
        udev = udev_new();
        if (!udev)
        {
            TDM_ERR("fail to initialize udev context\n");
            goto close_l;
        }

        /* Will try to find sys path /exynos-drm/drm/card0 */
        e = udev_enumerate_new(udev);
        udev_enumerate_add_match_subsystem(e, "drm");
        udev_enumerate_add_match_sysname(e, "card[0-9]*");
        udev_enumerate_scan_devices(e);

        drm_device = NULL;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e))
        {
            device = udev_device_new_from_syspath(udev_enumerate_get_udev(e),
                                                  udev_list_entry_get_name
                                                  (entry));
            device_parent = udev_device_get_parent(device);
            /* Not need unref device_parent. device_parent and device have same refcnt */
            if (device_parent)
            {
                if (strcmp(udev_device_get_sysname(device_parent), "exynos-drm") == 0)
                {
                    drm_device = device;
                    TDM_INFO("Found drm device: '%s' (%s)\n",
                             udev_device_get_syspath(drm_device),
                             udev_device_get_sysname(device_parent));
                    break;
                }
            }
            udev_device_unref(device);
        }

        if (drm_device == NULL)
        {
            TDM_ERR("fail to find drm device\n");
            udev_enumerate_unref(e);
            udev_unref(udev);
            goto close_l;
        }

        filename = udev_device_get_devnode(drm_device);

        fd = open(filename, O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            TDM_ERR("Cannot open drm device(%s)\n", filename);
        }
        udev_device_unref(drm_device);
        udev_enumerate_unref(e);
        udev_unref(udev);
    }
close_l:
#endif
    return fd;
}

void
tdm_exynos_deinit(tdm_backend_data *bdata)
{
    if (exynos_data != bdata)
        return;

    TDM_INFO("deinit");

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

tdm_backend_data*
tdm_exynos_init(tdm_display *dpy, tdm_error *error)
{
    tdm_error ret;

    if (!dpy)
    {
        TDM_ERR("display is null");
        if (error)
            *error = TDM_ERROR_INVALID_PARAMETER;
        return NULL;
    }

    if (exynos_data)
    {
        TDM_ERR("failed: init twice");
        if (error)
            *error = TDM_ERROR_BAD_REQUEST;
        return NULL;
    }

    exynos_data = calloc(1, sizeof(tdm_exynos_data));
    if (!exynos_data)
    {
        TDM_ERR("alloc failed");
        if (error)
            *error = TDM_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    LIST_INITHEAD(&exynos_data->output_list);
    LIST_INITHEAD(&exynos_data->buffer_list);

    ret = tdm_backend_register_func_display(dpy, &exynos_func_display);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    ret = tdm_backend_register_func_pp(dpy, &exynos_func_pp);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    exynos_data->dpy = dpy;

    exynos_data->drm_fd = -1;
    if (tdm_helper_drm_fd >= 0)
        exynos_data->drm_fd = dup(tdm_helper_drm_fd);

    if (exynos_data->drm_fd < 0)
        exynos_data->drm_fd = _tdm_exynos_open_drm();

    if (exynos_data->drm_fd < 0)
    {
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    if (drmSetClientCap(exynos_data->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0)
        TDM_WRN("Set DRM_CLIENT_CAP_UNIVERSAL_PLANES failed");

    exynos_data->mode_res = drmModeGetResources(exynos_data->drm_fd);
    if (!exynos_data->mode_res)
    {
        TDM_ERR("no drm resource: %m");
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    exynos_data->plane_res = drmModeGetPlaneResources(exynos_data->drm_fd);
    if (!exynos_data->plane_res)
    {
        TDM_ERR("no drm plane resource: %m");
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    if (exynos_data->plane_res->count_planes <= 0)
    {
        TDM_ERR("no drm plane resource");
        ret = TDM_ERROR_OPERATION_FAILED;
        goto failed;
    }

    ret = tdm_exynos_display_get_plane_prop_info(exynos_data, exynos_data->plane_res->planes[0],
            "zpos", NULL, &exynos_data->is_immutable_zpos);
    if (ret == TDM_ERROR_NONE)
    {
        exynos_data->has_zpos_info = 1;
        if (exynos_data->is_immutable_zpos)
            TDM_INFO("plane has immutable zpos info");
    }
    else
        TDM_INFO("plane doesn't have zpos info");

    ret = tdm_exynos_display_create_output_list(exynos_data);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    ret = tdm_exynos_display_create_layer_list(exynos_data);
    if (ret != TDM_ERROR_NONE)
        goto failed;

    if (error)
        *error = TDM_ERROR_NONE;

    TDM_INFO("init success!");

    return (tdm_backend_data*)exynos_data;
failed:
    if (error)
        *error = ret;

    tdm_exynos_deinit(exynos_data);

    TDM_ERR("init failed!");
    return NULL;
}

tdm_backend_module tdm_backend_module_data =
{
    "exynos",
    "Samsung",
    TDM_BACKEND_ABI_VERSION,
    tdm_exynos_init,
    tdm_exynos_deinit
};

