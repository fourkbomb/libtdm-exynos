#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <drm_fourcc.h>
#include <tdm_helper.h>
#include "tdm_exynos.h"

static void
_tdm_exynos_layer_cb_destroy_buffer(tbm_surface_h buffer, void *user_data)
{
	tdm_exynos_data *exynos_data;
	tdm_exynos_display_buffer *display_buffer;
	int ret;

	if (!user_data) {
		TDM_ERR("no user_data");
		return;
	}
	if (!buffer) {
		TDM_ERR("no buffer");
		return;
	}

	exynos_data = (tdm_exynos_data *)user_data;

	display_buffer = tdm_exynos_display_find_buffer(exynos_data, buffer);
	if (!display_buffer) {
		TDM_ERR("no display_buffer");
		return;
	}
	LIST_DEL(&display_buffer->link);

	if (display_buffer->fb_id > 0) {
		ret = drmModeRmFB(exynos_data->drm_fd, display_buffer->fb_id);
		if (ret < 0) {
			TDM_ERR("rm fb failed");
			return;
		}
		TDM_DBG("drmModeRmFB success!!! fb_id:%d", display_buffer->fb_id);
	} else
		TDM_DBG("drmModeRmFB not called fb_id:%d", display_buffer->fb_id);

	free(display_buffer);
}

tdm_error
exynos_layer_get_capability(tdm_layer *layer, tdm_caps_layer *caps)
{
	tdm_exynos_layer_data *layer_data = layer;
	tdm_exynos_data *exynos_data;
	drmModePlanePtr plane = NULL;
	drmModeObjectPropertiesPtr props = NULL;
	int i;
	tdm_error ret;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

	memset(caps, 0, sizeof(tdm_caps_layer));

	exynos_data = layer_data->exynos_data;
	plane = drmModeGetPlane(exynos_data->drm_fd, layer_data->plane_id);
	if (!plane) {
		TDM_ERR("get plane failed: %m");
		ret = TDM_ERROR_OPERATION_FAILED;
		goto failed_get;
	}

	caps->capabilities = layer_data->capabilities;
	caps->zpos = layer_data->zpos;  /* if VIDEO layer, zpos is -1 */

	caps->format_count = plane->count_formats;
	caps->formats = calloc(1, sizeof(tbm_format) * caps->format_count);
	if (!caps->formats) {
		ret = TDM_ERROR_OUT_OF_MEMORY;
		TDM_ERR("alloc failed\n");
		goto failed_get;
	}

	for (i = 0; i < caps->format_count; i++) {
		/* TODO: kernel reports wrong formats */
		if (plane->formats[i] != DRM_FORMAT_XRGB8888 &&
		    plane->formats[i] != DRM_FORMAT_ARGB8888)
			continue;
		caps->formats[i] = tdm_exynos_format_to_tbm_format(plane->formats[i]);
	}

	props = drmModeObjectGetProperties(exynos_data->drm_fd, layer_data->plane_id,
									   DRM_MODE_OBJECT_PLANE);
	if (!props) {
		ret = TDM_ERROR_OPERATION_FAILED;
		TDM_ERR("get plane properties failed: %m\n");
		goto failed_get;
	}

	caps->props = calloc(1, sizeof(tdm_prop) * props->count_props);
	if (!caps->props) {
		ret = TDM_ERROR_OUT_OF_MEMORY;
		TDM_ERR("alloc failed\n");
		goto failed_get;
	}

	caps->prop_count = 0;
	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(exynos_data->drm_fd,
													 props->props[i]);
		if (!prop)
			continue;
		if (!strncmp(prop->name, "type", TDM_NAME_LEN))
			continue;
		if (!strncmp(prop->name, "zpos", TDM_NAME_LEN))
			continue;
		snprintf(caps->props[i].name, TDM_NAME_LEN, "%s", prop->name);
		caps->props[i].id = props->props[i];
		caps->prop_count++;
		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
	drmModeFreePlane(plane);

	return TDM_ERROR_NONE;
failed_get:
	drmModeFreeObjectProperties(props);
	drmModeFreePlane(plane);
	free(caps->formats);
	free(caps->props);
	memset(caps, 0, sizeof(tdm_caps_layer));
	return ret;
}

tdm_error
exynos_layer_set_property(tdm_layer *layer, unsigned int id, tdm_value value)
{
	tdm_exynos_layer_data *layer_data = layer;
	tdm_exynos_data *exynos_data;
	int ret;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(layer_data->plane_id > 0, TDM_ERROR_INVALID_PARAMETER);

	exynos_data = layer_data->exynos_data;
	ret = drmModeObjectSetProperty(exynos_data->drm_fd,
								   layer_data->plane_id, DRM_MODE_OBJECT_PLANE,
								   id, value.u32);
	if (ret < 0) {
		TDM_ERR("set property failed: %m");
		return TDM_ERROR_OPERATION_FAILED;
	}

	return TDM_ERROR_NONE;
}

tdm_error
exynos_layer_get_property(tdm_layer *layer, unsigned int id, tdm_value *value)
{
	tdm_exynos_layer_data *layer_data = layer;
	tdm_exynos_data *exynos_data;
	drmModeObjectPropertiesPtr props;
	int i;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(layer_data->plane_id > 0, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(value, TDM_ERROR_INVALID_PARAMETER);

	exynos_data = layer_data->exynos_data;
	props = drmModeObjectGetProperties(exynos_data->drm_fd, layer_data->plane_id,
									   DRM_MODE_OBJECT_PLANE);
	if (props == NULL) {
		TDM_ERR("get property failed: %m");
		return TDM_ERROR_OPERATION_FAILED;
	}

	for (i = 0; i < props->count_props; i++)
		if (props->props[i] == id) {
			(*value).u32 = (uint)props->prop_values[i];
			break;
		}

	drmModeFreeObjectProperties(props);

	return TDM_ERROR_NONE;
}

tdm_error
exynos_layer_set_info(tdm_layer *layer, tdm_info_layer *info)
{
	tdm_exynos_layer_data *layer_data = layer;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	layer_data->info = *info;
	layer_data->info_changed = 1;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_layer_get_info(tdm_layer *layer, tdm_info_layer *info)
{
	tdm_exynos_layer_data *layer_data = layer;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	*info = layer_data->info;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_layer_set_buffer(tdm_layer *layer, tbm_surface_h buffer)
{
	tdm_exynos_layer_data *layer_data = layer;
	tdm_exynos_data *exynos_data;
	tdm_exynos_display_buffer *display_buffer;
	tdm_error err = TDM_ERROR_NONE;
	int ret, i, count;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(buffer, TDM_ERROR_INVALID_PARAMETER);

	exynos_data = layer_data->exynos_data;

	display_buffer = tdm_exynos_display_find_buffer(exynos_data, buffer);
	if (!display_buffer) {
		display_buffer = calloc(1, sizeof(tdm_exynos_display_buffer));
		if (!display_buffer) {
			TDM_ERR("alloc failed");
			return TDM_ERROR_OUT_OF_MEMORY;
		}
		display_buffer->buffer = buffer;

		err = tdm_buffer_add_destroy_handler(buffer, _tdm_exynos_layer_cb_destroy_buffer, exynos_data);
		if (err != TDM_ERROR_NONE) {
			TDM_ERR("add destroy handler fail");
			free(display_buffer);
			return TDM_ERROR_OPERATION_FAILED;
		}

		LIST_ADDTAIL(&display_buffer->link, &exynos_data->buffer_list);
	}

	if (display_buffer->fb_id == 0) {
		unsigned int width;
		unsigned int height;
		unsigned int format;
		unsigned int handles[4] = {0,};
		unsigned int pitches[4] = {0,};
		unsigned int offsets[4] = {0,};
		unsigned int size;

		width = tbm_surface_get_width(buffer);
		height = tbm_surface_get_height(buffer);
		format = tbm_surface_get_format(buffer);
		count = tbm_surface_internal_get_num_bos(buffer);
		for (i = 0; i < count; i++) {
			tbm_bo bo = tbm_surface_internal_get_bo(buffer, i);
			handles[i] = tbm_bo_get_handle(bo, TBM_DEVICE_DEFAULT).u32;
		}
		count = tbm_surface_internal_get_num_planes(format);
		for (i = 0; i < count; i++)
			tbm_surface_internal_get_plane_data(buffer, i, &size, &offsets[i], &pitches[i]);

		TDM_DBG("AddFB2: drm_fd(%d) size(%dx%d) format(%c%c%c%c) handles(%d,%d,%d) pitches(%d,%d,%d) offsets(%d,%d,%d) buffer(%p)",
				exynos_data->drm_fd, width, height, FOURCC_STR(format), handles[0], handles[1], handles[2],
				pitches[0], pitches[1], pitches[2], offsets[0], offsets[1], offsets[2], buffer);

		ret = drmModeAddFB2(exynos_data->drm_fd, width, height, format,
							handles, pitches, offsets, &display_buffer->fb_id, 0);
		if (ret < 0) {
			TDM_ERR("add fb failed: %m");
			return TDM_ERROR_OPERATION_FAILED;
		}
		TDM_DBG("exynos_data->drm_fd : %d, display_buffer->fb_id:%u",
				exynos_data->drm_fd, display_buffer->fb_id);

		if (IS_RGB(format))
			display_buffer->width = pitches[0] >> 2;
		else
			display_buffer->width = pitches[0];
	}

	layer_data->display_buffer = display_buffer;
	layer_data->display_buffer_changed = 1;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_layer_unset_buffer(tdm_layer *layer)
{
	tdm_exynos_layer_data *layer_data = layer;

	RETURN_VAL_IF_FAIL(layer_data, TDM_ERROR_INVALID_PARAMETER);

	layer_data->display_buffer = NULL;
	layer_data->display_buffer_changed = 1;

	return TDM_ERROR_NONE;
}
