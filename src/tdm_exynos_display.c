#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <drm_fourcc.h>
#include <tdm_helper.h>
#include "tdm_exynos.h"

#define LAYER_COUNT_PER_OUTPUT   2

static tdm_error
_tdm_exynos_display_create_layer_list_type(tdm_exynos_data *exynos_data)
{
	tdm_error ret;
	int i;

	for (i = 0; i < exynos_data->plane_res->count_planes; i++) {
		tdm_exynos_output_data *output_data = NULL;
		tdm_exynos_layer_data *layer_data;
		drmModePlanePtr plane;
		unsigned int type = 0;
		int output_find = 0;

		plane = drmModeGetPlane(exynos_data->drm_fd, exynos_data->plane_res->planes[i]);
		if (!plane) {
			TDM_ERR("no plane");
			continue;
		}

		ret = tdm_exynos_display_get_property(exynos_data,
											  exynos_data->plane_res->planes[i],
											  DRM_MODE_OBJECT_PLANE, "type", &type, NULL);
		if (ret != TDM_ERROR_NONE) {
			TDM_ERR("plane(%d) doesn't have 'type' info",
					exynos_data->plane_res->planes[i]);
			drmModeFreePlane(plane);
			continue;
		}

		layer_data = calloc(1, sizeof(tdm_exynos_layer_data));
		if (!layer_data) {
			TDM_ERR("alloc failed");
			drmModeFreePlane(plane);
			continue;
		}

		LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link) {
			if (plane->possible_crtcs & (1 << output_data->pipe)) {
				output_find = 1;
				break;
			}
		}

		if (!output_find) {
			TDM_ERR("plane(%d) couldn't found proper output", plane->plane_id);
			drmModeFreePlane(plane);
			free(layer_data);
			continue;
		}

		layer_data->exynos_data = exynos_data;
		layer_data->output_data = output_data;
		layer_data->plane_id = exynos_data->plane_res->planes[i];

		if (type == DRM_PLANE_TYPE_CURSOR) {
			layer_data->capabilities = TDM_LAYER_CAPABILITY_CURSOR |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
			layer_data->zpos = 2;
		} else if (type == DRM_PLANE_TYPE_OVERLAY) {
			layer_data->capabilities = TDM_LAYER_CAPABILITY_OVERLAY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
			layer_data->zpos = 1;
		} else if (type == DRM_PLANE_TYPE_PRIMARY) {
#ifdef _F_WEARABLE_PROFILE_
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC |
									   TDM_LAYER_CAPABILITY_RESEVED_MEMORY;
#else
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
#endif
			layer_data->zpos = 0;
			output_data->primary_layer = layer_data;
		} else {
			drmModeFreePlane(plane);
			free(layer_data);
			continue;
		}

		TDM_DBG("layer_data(%p) plane_id(%d) crtc_id(%d) zpos(%d) capabilities(%x)",
				layer_data, layer_data->plane_id, layer_data->output_data->crtc_id,
				layer_data->zpos, layer_data->capabilities);

		LIST_ADDTAIL(&layer_data->link, &output_data->layer_list);

		drmModeFreePlane(plane);
	}

	return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_display_create_layer_list_immutable_zpos(tdm_exynos_data *exynos_data)
{
	tdm_error ret;
	int i;

	for (i = 0; i < exynos_data->plane_res->count_planes; i++) {
		tdm_exynos_output_data *output_data = NULL;
		tdm_exynos_layer_data *layer_data;
		drmModePlanePtr plane;
		unsigned int type = 0, zpos = 0;
		int output_find = 0;

		plane = drmModeGetPlane(exynos_data->drm_fd, exynos_data->plane_res->planes[i]);
		if (!plane) {
			TDM_ERR("no plane");
			continue;
		}

		ret = tdm_exynos_display_get_property(exynos_data,
											  exynos_data->plane_res->planes[i],
											  DRM_MODE_OBJECT_PLANE, "type", &type, NULL);
		if (ret != TDM_ERROR_NONE) {
			TDM_ERR("plane(%d) doesn't have 'type' info",
					exynos_data->plane_res->planes[i]);
			drmModeFreePlane(plane);
			continue;
		}

		ret = tdm_exynos_display_get_property(exynos_data,
											  exynos_data->plane_res->planes[i],
											  DRM_MODE_OBJECT_PLANE, "zpos", &zpos, NULL);
		if (ret != TDM_ERROR_NONE) {
			TDM_ERR("plane(%d) doesn't have 'zpos' info",
					exynos_data->plane_res->planes[i]);
			drmModeFreePlane(plane);
			continue;
		}

		layer_data = calloc(1, sizeof(tdm_exynos_layer_data));
		if (!layer_data) {
			TDM_ERR("alloc failed");
			drmModeFreePlane(plane);
			continue;
		}

		LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link) {
			if (plane->possible_crtcs & (1 << output_data->pipe)) {
				output_find = 1;
				break;
			}
		}

		if (!output_find) {
			TDM_ERR("plane(%d) couldn't found proper output", plane->plane_id);
			drmModeFreePlane(plane);
			free(layer_data);
			continue;
		}

		layer_data->exynos_data = exynos_data;
		layer_data->output_data = output_data;
		layer_data->plane_id = exynos_data->plane_res->planes[i];
		layer_data->zpos = zpos;

		if (type == DRM_PLANE_TYPE_CURSOR)
			layer_data->capabilities = TDM_LAYER_CAPABILITY_CURSOR |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
		else if (type == DRM_PLANE_TYPE_OVERLAY)
			layer_data->capabilities = TDM_LAYER_CAPABILITY_OVERLAY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
		else if (type == DRM_PLANE_TYPE_PRIMARY) {
#ifdef _F_WEARABLE_PROFILE_
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC |
									   TDM_LAYER_CAPABILITY_RESEVED_MEMORY;
#else
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
#endif
			output_data->primary_layer = layer_data;
		} else {
			drmModeFreePlane(plane);
			free(layer_data);
			continue;
		}

		TDM_DBG("layer_data(%p) plane_id(%d) crtc_id(%d) zpos(%d) capabilities(%x)",
				layer_data, layer_data->plane_id, layer_data->output_data->crtc_id,
				layer_data->zpos, layer_data->capabilities);

		LIST_ADDTAIL(&layer_data->link, &output_data->layer_list);

		drmModeFreePlane(plane);
	}

	return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_display_create_layer_list_not_fixed(tdm_exynos_data *exynos_data)
{
	int i, find_pipe = -1;

	if (exynos_data->mode_res->count_connectors * LAYER_COUNT_PER_OUTPUT >
	    exynos_data->plane_res->count_planes) {
		TDM_ERR("not enough layers");
		return TDM_ERROR_OPERATION_FAILED;
	}

	for (i = 0; i < exynos_data->plane_res->count_planes; i++) {
		tdm_exynos_output_data *output_data = NULL;
		tdm_exynos_layer_data *layer_data;
		drmModePlanePtr plane;

		plane = drmModeGetPlane(exynos_data->drm_fd, exynos_data->plane_res->planes[i]);
		if (!plane) {
			TDM_ERR("no plane");
			continue;
		}

		layer_data = calloc(1, sizeof(tdm_exynos_layer_data));
		if (!layer_data) {
			TDM_ERR("alloc failed");
			drmModeFreePlane(plane);
			continue;
		}

		/* TODO
		 * Currently, kernel doesn give us the correct device infomation.
		 * Primary connector type is invalid. plane's count is not correct.
		 * So we need to fix all of them with kernel.
		 * Temporarily we dedicate only 2 plane to each output.
		 * First plane is primary layer. Second plane's zpos is 1.
		 */
		if (i % LAYER_COUNT_PER_OUTPUT == 0)
			find_pipe++;

		LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link) {
			if (output_data->pipe == find_pipe)
				break;
		}

		if (i == exynos_data->mode_res->count_connectors * LAYER_COUNT_PER_OUTPUT) {
			TDM_DBG("no more plane(%d) need for outputs", plane->plane_id);
			drmModeFreePlane(plane);
			free(layer_data);
			return TDM_ERROR_NONE;
		}

		layer_data->exynos_data = exynos_data;
		layer_data->output_data = output_data;
		layer_data->plane_id = exynos_data->plane_res->planes[i];

		layer_data->zpos = i % 2;
		if (layer_data->zpos == 0) {
#ifdef _F_WEARABLE_PROFILE_
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC |
									   TDM_LAYER_CAPABILITY_RESEVED_MEMORY;
#else
			layer_data->capabilities = TDM_LAYER_CAPABILITY_PRIMARY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;
#endif
			output_data->primary_layer = layer_data;
		} else {
			tdm_error ret;

			layer_data->capabilities = TDM_LAYER_CAPABILITY_OVERLAY |
									   TDM_LAYER_CAPABILITY_GRAPHIC;

			ret = tdm_exynos_display_set_property(exynos_data, layer_data->plane_id,
												  DRM_MODE_OBJECT_PLANE, "zpos", layer_data->zpos);
			if (ret != TDM_ERROR_NONE) {
				drmModeFreePlane(plane);
				free(layer_data);
				return TDM_ERROR_OPERATION_FAILED;
			}
		}

		TDM_DBG("layer_data(%p) plane_id(%d) crtc_id(%d) zpos(%d) capabilities(%x)",
				layer_data, layer_data->plane_id, layer_data->output_data->crtc_id,
				layer_data->zpos, layer_data->capabilities);

		LIST_ADDTAIL(&layer_data->link, &output_data->layer_list);

		drmModeFreePlane(plane);
	}

	return TDM_ERROR_NONE;
}

void
tdm_exynos_display_to_tdm_mode(drmModeModeInfoPtr drm_mode,
										tdm_output_mode *tdm_mode)
{
	tdm_mode->clock = drm_mode->clock;
	tdm_mode->hdisplay = drm_mode->hdisplay;
	tdm_mode->hsync_start = drm_mode->hsync_start;
	tdm_mode->hsync_end = drm_mode->hsync_end;
	tdm_mode->htotal = drm_mode->htotal;
	tdm_mode->hskew = drm_mode->hskew;
	tdm_mode->vdisplay = drm_mode->vdisplay;
	tdm_mode->vsync_start = drm_mode->vsync_start;
	tdm_mode->vsync_end = drm_mode->vsync_end;
	tdm_mode->vtotal = drm_mode->vtotal;
	tdm_mode->vscan = drm_mode->vscan;
	tdm_mode->vrefresh = drm_mode->vrefresh;
	tdm_mode->flags = drm_mode->flags;
	tdm_mode->type = drm_mode->type;
	snprintf(tdm_mode->name, TDM_NAME_LEN, "%s", drm_mode->name);
}

tdm_error
tdm_exynos_display_create_layer_list(tdm_exynos_data *exynos_data)
{
	tdm_exynos_output_data *output_data = NULL;
	tdm_error ret;

	if (!exynos_data->has_zpos_info)
		ret = _tdm_exynos_display_create_layer_list_type(exynos_data);
	else if (exynos_data->is_immutable_zpos)
		ret = _tdm_exynos_display_create_layer_list_immutable_zpos(exynos_data);
	else
		ret = _tdm_exynos_display_create_layer_list_not_fixed(exynos_data);

	if (ret != TDM_ERROR_NONE)
		return ret;

	LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link) {
		if (!output_data->primary_layer) {
			TDM_ERR("output(%d) no primary layer", output_data->pipe);
			return TDM_ERROR_OPERATION_FAILED;
		}
	}

	return TDM_ERROR_NONE;
}

void
tdm_exynos_display_destroy_output_list(tdm_exynos_data *exynos_data)
{
	tdm_exynos_output_data *o = NULL, *oo = NULL;

	if (LIST_IS_EMPTY(&exynos_data->output_list))
		return;

	LIST_FOR_EACH_ENTRY_SAFE(o, oo, &exynos_data->output_list, link) {
		LIST_DEL(&o->link);
		if (!LIST_IS_EMPTY(&o->layer_list)) {
			tdm_exynos_layer_data *l = NULL, *ll = NULL;
			LIST_FOR_EACH_ENTRY_SAFE(l, ll, &o->layer_list, link) {
				LIST_DEL(&l->link);
				free(l);
			}
		}
		free(o->drm_modes);
		free(o->output_modes);
		free(o);
	}
}

void
tdm_exynos_display_update_output_status(tdm_exynos_data *exynos_data)
{
	tdm_exynos_output_data *output_data = NULL;

	if (LIST_IS_EMPTY(&exynos_data->output_list))
		return;

	LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link) {
		drmModeConnectorPtr connector;
		tdm_output_conn_status new_status;

		connector = drmModeGetConnector(exynos_data->drm_fd,
										output_data->connector_id);
		if (!connector) {
			TDM_ERR("no connector: %d", output_data->connector_id);
			continue;
		}

		if (connector->connection == DRM_MODE_CONNECTED)
			new_status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
		else
			new_status = TDM_OUTPUT_CONN_STATUS_DISCONNECTED;

		tdm_exynos_output_update_status(output_data, new_status);

		drmModeFreeConnector(connector);
	}
}

tdm_error
tdm_exynos_display_create_output_list(tdm_exynos_data *exynos_data)
{
	tdm_exynos_output_data *output_data;
	int i;
	tdm_error ret;
	int allocated = 0;

	RETURN_VAL_IF_FAIL(LIST_IS_EMPTY(&exynos_data->output_list),
					   TDM_ERROR_OPERATION_FAILED);

	for (i = 0; i < exynos_data->mode_res->count_connectors; i++) {
		drmModeConnectorPtr connector;
		drmModeEncoderPtr encoder;
		int crtc_id = 0, c, j;

		connector = drmModeGetConnector(exynos_data->drm_fd,
										exynos_data->mode_res->connectors[i]);
		if (!connector) {
			TDM_ERR("no connector");
			ret = TDM_ERROR_OPERATION_FAILED;
			goto failed_create;
		}

		if (connector->count_encoders != 1) {
			TDM_ERR("too many encoders: %d", connector->count_encoders);
			drmModeFreeConnector(connector);
			ret = TDM_ERROR_OPERATION_FAILED;
			goto failed_create;
		}

		encoder = drmModeGetEncoder(exynos_data->drm_fd, connector->encoders[0]);
		if (!encoder) {
			TDM_ERR("no encoder");
			drmModeFreeConnector(connector);
			ret = TDM_ERROR_OPERATION_FAILED;
			goto failed_create;
		}

		for (c = 0; c < exynos_data->mode_res->count_crtcs; c++) {
			if (allocated & (1 << c))
				continue;

			if ((encoder->possible_crtcs & (1 << c)) == 0)
				continue;

			crtc_id = exynos_data->mode_res->crtcs[c];
			allocated |= (1 << c);
			break;
		}

		if (crtc_id == 0) {
			TDM_ERR("no possible crtc");
			drmModeFreeConnector(connector);
			ret = TDM_ERROR_OPERATION_FAILED;
			goto failed_create;
		}

		output_data = calloc(1, sizeof(tdm_exynos_output_data));
		if (!output_data) {
			TDM_ERR("alloc failed");
			drmModeFreeConnector(connector);
			drmModeFreeEncoder(encoder);
			ret = TDM_ERROR_OUT_OF_MEMORY;
			goto failed_create;
		}

		LIST_INITHEAD(&output_data->layer_list);

		output_data->exynos_data = exynos_data;
		output_data->connector_id = exynos_data->mode_res->connectors[i];
		output_data->encoder_id = encoder->encoder_id;
		output_data->crtc_id = crtc_id;
		output_data->pipe = c;
		output_data->connector_type = connector->connector_type;
		output_data->connector_type_id = connector->connector_type_id;

		if (connector->connection == DRM_MODE_CONNECTED)
			output_data->status = TDM_OUTPUT_CONN_STATUS_CONNECTED;
		else
			output_data->status = TDM_OUTPUT_CONN_STATUS_DISCONNECTED;

		for (j = 0; j < connector->count_props; j++) {
			drmModePropertyPtr prop = drmModeGetProperty(exynos_data->drm_fd,
														 connector->props[j]);
			if (!prop)
				continue;
			if (!strcmp(prop->name, "DPMS")) {
				output_data->dpms_prop_id = connector->props[j];
				drmModeFreeProperty(prop);
				break;
			}
			drmModeFreeProperty(prop);
		}

		output_data->count_modes = connector->count_modes;
		output_data->drm_modes = calloc(connector->count_modes, sizeof(drmModeModeInfo));
		if (!output_data->drm_modes) {
			TDM_ERR("alloc failed");
			free(output_data);
			drmModeFreeConnector(connector);
			drmModeFreeEncoder(encoder);
			ret = TDM_ERROR_OUT_OF_MEMORY;
			goto failed_create;
		}
		output_data->output_modes = calloc(connector->count_modes, sizeof(tdm_output_mode));
		if (!output_data->output_modes) {
			TDM_ERR("alloc failed");
			free(output_data->drm_modes);
			free(output_data);
			drmModeFreeConnector(connector);
			drmModeFreeEncoder(encoder);
			ret = TDM_ERROR_OUT_OF_MEMORY;
			goto failed_create;
		}
		for (j = 0; j < connector->count_modes; j++) {
			output_data->drm_modes[j] = connector->modes[j];
			tdm_exynos_display_to_tdm_mode(&output_data->drm_modes[j],
										   &output_data->output_modes[j]);
		}

		LIST_ADDTAIL(&output_data->link, &exynos_data->output_list);

		TDM_DBG("output_data(%p) connector_id(%d:%d:%d-%d) encoder_id(%d) crtc_id(%d) pipe(%d) dpms_id(%d)",
				output_data, output_data->connector_id, output_data->status,
				output_data->connector_type,
				output_data->connector_type_id, output_data->encoder_id, output_data->crtc_id,
				output_data->pipe, output_data->dpms_prop_id);

		drmModeFreeEncoder(encoder);
		drmModeFreeConnector(connector);
	}

	TDM_DBG("output count: %d", exynos_data->mode_res->count_connectors);

	return TDM_ERROR_NONE;
failed_create:
	tdm_exynos_display_destroy_output_list(exynos_data);
	return ret;
}

tdm_error
tdm_exynos_display_set_property(tdm_exynos_data *exynos_data,
									   unsigned int obj_id, unsigned int obj_type,
									   const char *name, unsigned int value)
{
	drmModeObjectPropertiesPtr props = NULL;
	unsigned int i;

	props = drmModeObjectGetProperties(exynos_data->drm_fd, obj_id, obj_type);
	if (!props) {
		TDM_ERR("drmModeObjectGetProperties failed: %m");
		return TDM_ERROR_OPERATION_FAILED;
	}
	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(exynos_data->drm_fd,
													 props->props[i]);
		int ret;
		if (!prop) {
			TDM_ERR("drmModeGetProperty failed: %m");
			drmModeFreeObjectProperties(props);
			return TDM_ERROR_OPERATION_FAILED;
		}
		if (!strcmp(prop->name, name)) {
			ret = drmModeObjectSetProperty(exynos_data->drm_fd, obj_id, obj_type,
										   prop->prop_id, value);
			if (ret < 0) {
				TDM_ERR("drmModeObjectSetProperty failed: %m");
				drmModeFreeProperty(prop);
				drmModeFreeObjectProperties(props);
				return TDM_ERROR_OPERATION_FAILED;
			}
			drmModeFreeProperty(prop);
			drmModeFreeObjectProperties(props);
			return TDM_ERROR_NONE;
		}
		drmModeFreeProperty(prop);
	}

	TDM_ERR("not found '%s' property", name);

	drmModeFreeObjectProperties(props);
	/* TODO
	* kernel info error
	* it must be changed to 'return TDM_ERROR_OPERATION_FAILED' after kernel fix.
	*/
	return TDM_ERROR_NONE;
}

tdm_error
tdm_exynos_display_get_property(tdm_exynos_data *exynos_data,
									   unsigned int obj_id, unsigned int obj_type,
									   const char *name, unsigned int *value, int *is_immutable)
{
	drmModeObjectPropertiesPtr props = NULL;
	int i;

	props = drmModeObjectGetProperties(exynos_data->drm_fd, obj_id, obj_type);
	if (!props)
		return TDM_ERROR_OPERATION_FAILED;

	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(exynos_data->drm_fd,
													 props->props[i]);

		if (!prop)
			continue;

		if (!strcmp(prop->name, name)) {
			if (is_immutable)
				*is_immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
			if (value)
				*value = (unsigned int)props->prop_values[i];
			drmModeFreeProperty(prop);
			drmModeFreeObjectProperties(props);
			return TDM_ERROR_NONE;
		}

		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);
	TDM_DBG("coundn't find '%s' property", name);
	return TDM_ERROR_OPERATION_FAILED;
}

tdm_exynos_display_buffer *
tdm_exynos_display_find_buffer(tdm_exynos_data *exynos_data,
									 tbm_surface_h buffer)
{
	tdm_exynos_display_buffer *display_buffer = NULL;

	LIST_FOR_EACH_ENTRY(display_buffer, &exynos_data->buffer_list, link) {
		if (display_buffer->buffer == buffer)
			return display_buffer;
	}

	return NULL;
}

tdm_error
exynos_display_get_capability(tdm_backend_data *bdata, tdm_caps_display *caps)
{
	RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

	caps->max_layer_count = -1; /* not defined */

	return TDM_ERROR_NONE;
}

tdm_error
exynos_display_get_pp_capability(tdm_backend_data *bdata, tdm_caps_pp *caps)
{
	return tdm_exynos_pp_get_capability(bdata, caps);
}

tdm_output **
exynos_display_get_outputs(tdm_backend_data *bdata, int *count,
								 tdm_error *error)
{
	tdm_exynos_data *exynos_data = bdata;
	tdm_exynos_output_data *output_data = NULL;
	tdm_output **outputs;
	tdm_error ret;
	int i;

	RETURN_VAL_IF_FAIL(exynos_data, NULL);
	RETURN_VAL_IF_FAIL(count, NULL);

	*count = 0;
	LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link)
	(*count)++;

	if (*count == 0) {
		ret = TDM_ERROR_NONE;
		goto failed_get;
	}

	/* will be freed in frontend */
	outputs = calloc(*count, sizeof(tdm_exynos_output_data *));
	if (!outputs) {
		TDM_ERR("failed: alloc memory");
		*count = 0;
		ret = TDM_ERROR_OUT_OF_MEMORY;
		goto failed_get;
	}

	i = 0;
	LIST_FOR_EACH_ENTRY(output_data, &exynos_data->output_list, link)
	outputs[i++] = output_data;

	if (error)
		*error = TDM_ERROR_NONE;

	return outputs;
failed_get:
	if (error)
		*error = ret;
	return NULL;
}

tdm_error
exynos_display_get_fd(tdm_backend_data *bdata, int *fd)
{
	tdm_exynos_data *exynos_data = bdata;

	RETURN_VAL_IF_FAIL(exynos_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(fd, TDM_ERROR_INVALID_PARAMETER);

	*fd = exynos_data->drm_fd;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_display_handle_events(tdm_backend_data *bdata)
{
	tdm_exynos_data *exynos_data = bdata;
	drmEventContext ctx;

	RETURN_VAL_IF_FAIL(exynos_data, TDM_ERROR_INVALID_PARAMETER);

	memset(&ctx, 0, sizeof(drmEventContext));

	ctx.version = DRM_EVENT_CONTEXT_VERSION;
	ctx.page_flip_handler = tdm_exynos_output_cb_event;
	ctx.vblank_handler = tdm_exynos_output_cb_event;

	drmHandleEvent(exynos_data->drm_fd, &ctx);

	return TDM_ERROR_NONE;
}

tdm_pp *
exynos_display_create_pp(tdm_backend_data *bdata, tdm_error *error)
{
	tdm_exynos_data *exynos_data = bdata;

	RETURN_VAL_IF_FAIL(exynos_data, NULL);

	return tdm_exynos_pp_create(exynos_data, error);
}
