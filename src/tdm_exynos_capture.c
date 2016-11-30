#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tdm_exynos.h"

typedef struct _tdm_exynos_capture_buffer {
	int index;
	tbm_surface_h ui_buffer;
	tbm_surface_h buffer;
	struct list_head link;
} tdm_exynos_capture_buffer;

typedef struct _tdm_exynos_pp_data {
	tdm_exynos_data *exynos_data;

	tdm_exynos_output_data *output_data;

	tdm_info_capture info;
	int info_changed;

	struct list_head pending_buffer_list;
	struct list_head buffer_list;

	struct {
		tdm_event_loop_source *timer_source;

		unsigned int prop_id;

		int startd;
		int first_event;
	} stream;

	tdm_capture_done_handler done_func;
	void *done_user_data;

	struct list_head link;
} tdm_exynos_capture_data;

static tbm_format capture_formats[] = {
	TBM_FORMAT_ARGB8888,
	TBM_FORMAT_XRGB8888,
};

#define NUM_CAPTURE_FORMAT   (sizeof(capture_formats) / sizeof(capture_formats[0]))

static int capture_list_init;
static struct list_head capture_list;

int
tdm_exynos_capture_find_prop_id(unsigned int prop_id)
{
	tdm_exynos_capture_data *capture_data = NULL;

	if (!capture_list_init) {
		capture_list_init = 1;
		LIST_INITHEAD(&capture_list);
	}

	if (LIST_IS_EMPTY(&capture_list))
		return 0;

	LIST_FOR_EACH_ENTRY(capture_data, &capture_list, link) {
		if (capture_data->stream.prop_id == prop_id)
			return 1;
	}

	return 0;
}

static int
_get_index(tdm_exynos_capture_data *capture_data)
{
	tdm_exynos_capture_buffer *b = NULL;
	int ret = 0;

	while (1) {
		int found = 0;
		LIST_FOR_EACH_ENTRY(b, &capture_data->pending_buffer_list, link) {
			if (ret == b->index) {
				found = 1;
				break;
			}
		}
		if (!found)
			LIST_FOR_EACH_ENTRY(b, &capture_data->buffer_list, link) {
			if (ret == b->index) {
				found = 1;
				break;
			}
		}
		if (!found)
			break;
		ret++;
	}

	return ret;
}

static tdm_error
_tdm_exynos_capture_stream_pp_set(tdm_exynos_capture_data *capture_data, tbm_surface_h ui_buffer)
{
	tdm_exynos_data *exynos_data = capture_data->exynos_data;
	tdm_info_capture *info = &capture_data->info;
	struct drm_exynos_ipp_property property;
	unsigned int width, height, stride = 0;
	int ret = 0;

	tbm_surface_internal_get_plane_data(ui_buffer, 0, NULL, NULL, &stride);
	width = tbm_surface_get_width(ui_buffer);
	height = tbm_surface_get_height(ui_buffer);

	CLEAR(property);

	property.config[0].ops_id = EXYNOS_DRM_OPS_SRC;
	property.config[0].fmt = tdm_exynos_format_to_drm_format(tbm_surface_get_format(ui_buffer));
	property.config[0].sz.hsize = stride >> 2;
	property.config[0].sz.vsize = height;
	property.config[0].pos.w = width;
	property.config[0].pos.h = height;

	property.config[1].ops_id = EXYNOS_DRM_OPS_DST;
	property.config[1].degree = info->transform % 4;
	property.config[1].flip = (info->transform > 3) ? EXYNOS_DRM_FLIP_HORIZONTAL : 0;
	property.config[1].fmt = tdm_exynos_format_to_drm_format(info->dst_config.format);
	memcpy(&property.config[1].sz, &info->dst_config.size, sizeof(tdm_size));
	memcpy(&property.config[1].pos, &info->dst_config.pos, sizeof(tdm_pos));
	property.cmd = IPP_CMD_M2M;
	property.prop_id = capture_data->stream.prop_id;

	TDM_DBG("src : flip(%x) deg(%d) fmt(%c%c%c%c) sz(%dx%d) pos(%d,%d %dx%d)  ",
			property.config[0].flip, property.config[0].degree,
			FOURCC_STR(property.config[0].fmt),
			property.config[0].sz.hsize, property.config[0].sz.vsize,
			property.config[0].pos.x, property.config[0].pos.y, property.config[0].pos.w,
			property.config[0].pos.h);
	TDM_DBG("dst : flip(%x) deg(%d) fmt(%c%c%c%c) sz(%dx%d) pos(%d,%d %dx%d)  ",
			property.config[1].flip, property.config[1].degree,
			FOURCC_STR(property.config[1].fmt),
			property.config[1].sz.hsize, property.config[1].sz.vsize,
			property.config[1].pos.x, property.config[1].pos.y, property.config[1].pos.w,
			property.config[1].pos.h);

	ret = ioctl(exynos_data->drm_fd, DRM_IOCTL_EXYNOS_IPP_SET_PROPERTY, &property);
	if (ret) {
		TDM_ERR("failed: %m");
		return TDM_ERROR_OPERATION_FAILED;
	}

	TDM_DBG("success. prop_id(%d) ", property.prop_id);
	capture_data->stream.prop_id = property.prop_id;
	return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_capture_stream_pp_queue(tdm_exynos_capture_data *capture_data,
									tdm_exynos_capture_buffer *b,
									enum drm_exynos_ipp_buf_type type)
{
	tdm_exynos_data *exynos_data = capture_data->exynos_data;
	struct drm_exynos_ipp_queue_buf buf;
	int i, bo_num, ret = 0;

	CLEAR(buf);
	buf.prop_id = capture_data->stream.prop_id;
	buf.ops_id = EXYNOS_DRM_OPS_SRC;
	buf.buf_type = type;
	buf.buf_id = b->index;
	buf.user_data = (__u64)(uintptr_t)capture_data;
	bo_num = tbm_surface_internal_get_num_bos(b->ui_buffer);
	for (i = 0; i < EXYNOS_DRM_PLANAR_MAX && i < bo_num; i++) {
		tbm_bo bo = tbm_surface_internal_get_bo(b->ui_buffer, i);
		buf.handle[i] = (__u32)tbm_bo_get_handle(bo, TBM_DEVICE_DEFAULT).u32;
	}

	TDM_DBG("prop_id(%d) ops_id(%d) ctrl(%d) id(%d) handles(%x %x %x). ",
			buf.prop_id, buf.ops_id, buf.buf_type, buf.buf_id,
			buf.handle[0], buf.handle[1], buf.handle[2]);

	ret = ioctl(exynos_data->drm_fd, DRM_IOCTL_EXYNOS_IPP_QUEUE_BUF, &buf);
	if (ret) {
		TDM_ERR("src failed. prop_id(%d) op(%d) buf(%d) id(%d). %m",
				buf.prop_id, buf.ops_id, buf.buf_type, buf.buf_id);
		return TDM_ERROR_OPERATION_FAILED;
	}

	CLEAR(buf);
	buf.prop_id = capture_data->stream.prop_id;
	buf.ops_id = EXYNOS_DRM_OPS_DST;
	buf.buf_type = type;
	buf.buf_id = b->index;
	buf.user_data = (__u64)(uintptr_t)capture_data;
	bo_num = tbm_surface_internal_get_num_bos(b->buffer);
	for (i = 0; i < EXYNOS_DRM_PLANAR_MAX && i < bo_num; i++) {
		tbm_bo bo = tbm_surface_internal_get_bo(b->buffer, i);
		buf.handle[i] = (__u32)tbm_bo_get_handle(bo, TBM_DEVICE_DEFAULT).u32;
	}

	TDM_DBG("prop_id(%d) ops_id(%d) ctrl(%d) id(%d) handles(%x %x %x). ",
			buf.prop_id, buf.ops_id, buf.buf_type, buf.buf_id,
			buf.handle[0], buf.handle[1], buf.handle[2]);

	ret = ioctl(exynos_data->drm_fd, DRM_IOCTL_EXYNOS_IPP_QUEUE_BUF, &buf);
	if (ret) {
		TDM_ERR("dst failed. prop_id(%d) op(%d) buf(%d) id(%d). %m",
				buf.prop_id, buf.ops_id, buf.buf_type, buf.buf_id);
		return TDM_ERROR_OPERATION_FAILED;
	}

	TDM_DBG("success. prop_id(%d)", buf.prop_id);

	return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_capture_stream_pp_cmd(tdm_exynos_capture_data *capture_data, enum drm_exynos_ipp_ctrl cmd)
{
	tdm_exynos_data *exynos_data = capture_data->exynos_data;
	struct drm_exynos_ipp_cmd_ctrl ctrl;
	int ret = 0;

	ctrl.prop_id = capture_data->stream.prop_id;
	ctrl.ctrl = cmd;

	TDM_DBG("prop_id(%d) ctrl(%d). ", ctrl.prop_id, ctrl.ctrl);

	ret = ioctl(exynos_data->drm_fd, DRM_IOCTL_EXYNOS_IPP_CMD_CTRL, &ctrl);
	if (ret) {
		TDM_ERR("failed. prop_id(%d) ctrl(%d). %m", ctrl.prop_id, ctrl.ctrl);
		return TDM_ERROR_OPERATION_FAILED;
	}

	TDM_DBG("success. prop_id(%d) ", ctrl.prop_id);

	return TDM_ERROR_NONE;
}

void
tdm_exynos_capture_stream_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
									 unsigned int tv_sec, unsigned int tv_usec, void *data)
{
	tdm_exynos_capture_data *found = NULL, *capture_data = data;
	tdm_exynos_capture_buffer *b = NULL, *bb = NULL, *dequeued_buffer = NULL;

	if (!capture_data || !buf_idx) {
		TDM_ERR("invalid params");
		return;
	}

	LIST_FOR_EACH_ENTRY(found, &capture_list, link) {
		if (found == capture_data)
			break;
	}
	if (!found)
		return;

	TDM_DBG("capture_data(%p) index(%d, %d)", capture_data, buf_idx[0], buf_idx[1]);

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &capture_data->buffer_list, link) {
		if (buf_idx[0] == b->index) {
			dequeued_buffer = b;
			LIST_DEL(&dequeued_buffer->link);
			TDM_DBG("dequeued: %d", dequeued_buffer->index);
			break;
		}
	}

	if (!dequeued_buffer) {
		TDM_ERR("not found buffer index: %d", buf_idx[0]);
		return;
	}

	if (!capture_data->stream.first_event) {
		TDM_DBG("capture(%p) got a first event. ", capture_data);
		capture_data->stream.first_event = 1;
	}

	if (capture_data->done_func)
		capture_data->done_func(capture_data,
								dequeued_buffer->buffer,
								capture_data->done_user_data);

	tdm_buffer_unref_backend(dequeued_buffer->ui_buffer);

	free(dequeued_buffer);
}

static tdm_error
_tdm_exynos_capture_stream_timer_handler(void *user_data)
{
	tdm_exynos_capture_data *capture_data = user_data;
	unsigned int ms = 1000 / capture_data->info.frequency;
	tdm_exynos_capture_buffer *b = NULL, *bb = NULL;
	tbm_surface_h ui_buffer = NULL;
	tdm_error ret;

	tdm_event_loop_source_timer_update(capture_data->stream.timer_source, ms);

	if (capture_data->output_data->primary_layer->display_buffer)
		ui_buffer = capture_data->output_data->primary_layer->display_buffer->buffer;

	if (!ui_buffer)
		return TDM_ERROR_NONE;

	if (capture_data->info_changed) {
		if (capture_data->stream.startd)
			_tdm_exynos_capture_stream_pp_cmd(capture_data, IPP_CTRL_PAUSE);

		ret = _tdm_exynos_capture_stream_pp_set(capture_data, ui_buffer);
		if (ret < 0)
			return TDM_ERROR_OPERATION_FAILED;
	}

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &capture_data->pending_buffer_list, link) {
		LIST_DEL(&b->link);
		b->ui_buffer = tdm_buffer_ref_backend(ui_buffer);
		_tdm_exynos_capture_stream_pp_queue(capture_data, b, IPP_BUF_ENQUEUE);
		TDM_DBG("queued: %d", b->index);
		LIST_ADDTAIL(&b->link, &capture_data->buffer_list);
		break;
	}

	if (capture_data->info_changed) {
		capture_data->info_changed = 0;

		if (!capture_data->stream.startd) {
			capture_data->stream.startd = 1;
			_tdm_exynos_capture_stream_pp_cmd(capture_data, IPP_CTRL_PLAY);
		} else
			_tdm_exynos_capture_stream_pp_cmd(capture_data, IPP_CTRL_RESUME);
	}


	return TDM_ERROR_NONE;
}


static tdm_error
_tdm_exynos_capture_commit_stream(tdm_exynos_capture_data *capture_data)
{
	unsigned int ms = 1000 / capture_data->info.frequency;

	if (!capture_data->stream.timer_source) {
		capture_data->stream.timer_source =
			tdm_event_loop_add_timer_handler(capture_data->exynos_data->dpy,
											 _tdm_exynos_capture_stream_timer_handler,
											 capture_data, NULL);
			RETURN_VAL_IF_FAIL(capture_data->stream.timer_source != NULL, TDM_ERROR_OUT_OF_MEMORY);
	}

	tdm_event_loop_source_timer_update(capture_data->stream.timer_source, ms);

	return TDM_ERROR_NONE;
}

static void
_tdm_exynos_capture_oneshot_center_rect(int src_w, int src_h, int dst_w, int dst_h, tdm_pos *fit)
{
	float rw = (float)src_w / dst_w;
	float rh = (float)src_h / dst_h;

	fit->x = fit->y = 0;

	if (rw > rh) {
		fit->w = dst_w;
		fit->h = src_h / rw;
		fit->y = (dst_h - fit->h) / 2;
	} else if (rw < rh) {
		fit->w = src_w / rh;
		fit->h = dst_h;
		fit->x = (dst_w - fit->w) / 2;
	} else {
		fit->w = dst_w;
		fit->h = dst_h;
	}

	fit->x = fit->x & ~0x1;
}

static void
_tdm_exynos_capture_oneshot_rect_scale(int src_w, int src_h, int dst_w, int dst_h, tdm_pos *scale)
{
	float ratio;
	tdm_pos center = {0,};

	_tdm_exynos_capture_oneshot_center_rect(src_w, src_h, dst_w, dst_h, &center);

	ratio = (float)center.w / src_w;
	scale->x = scale->x * ratio + center.x;
	scale->y = scale->y * ratio + center.y;
	scale->w = scale->w * ratio;
	scale->h = scale->h * ratio;
}

static void
_tdm_exynos_capture_oneshot_composite_layers_sw(tdm_exynos_capture_data *capture_data, tbm_surface_h buffer)
{
	tdm_exynos_output_data *output_data = capture_data->output_data;
	tdm_exynos_layer_data *layer_data = NULL;
	tbm_surface_info_s buf_info;
	int err;

	err = tbm_surface_map(buffer, TBM_OPTION_READ, &buf_info);
	RETURN_IF_FAIL(err == TBM_SURFACE_ERROR_NONE);

	LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link) {
		tbm_surface_h buf;
		tdm_pos dst_pos;

		if (!layer_data->display_buffer)
			continue;

		buf = layer_data->display_buffer->buffer;
		dst_pos = layer_data->info.dst_pos;

		_tdm_exynos_capture_oneshot_rect_scale(output_data->current_mode->hdisplay,
											   output_data->current_mode->vdisplay,
											   buf_info.width, buf_info.height, &dst_pos);

		tdm_helper_convert_buffer(buf, buffer,
								  &layer_data->info.src_config.pos, &dst_pos, 0, 1);
	}
}

static tdm_error
_tdm_exynos_capture_commit_oneshot(tdm_exynos_capture_data *capture_data)
{
	tdm_exynos_capture_buffer *b = NULL, *bb = NULL;

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &capture_data->pending_buffer_list, link) {
		LIST_DEL(&b->link);

		/* TODO: need to improve the performance with hardware */
		_tdm_exynos_capture_oneshot_composite_layers_sw(capture_data, b->buffer);

		if (capture_data->done_func)
			capture_data->done_func(capture_data,
									b->buffer,
									capture_data->done_user_data);
		free(b);
	}

	return TDM_ERROR_NONE;
}

tdm_error
tdm_exynos_capture_get_capability(tdm_exynos_data *exynos_data, tdm_caps_capture *caps)
{
	int i;

	if (!caps) {
		TDM_ERR("invalid params");
		return TDM_ERROR_INVALID_PARAMETER;
	}

	caps->capabilities = TDM_CAPTURE_CAPABILITY_OUTPUT|
						 TDM_CAPTURE_CAPABILITY_ONESHOT|
						 TDM_CAPTURE_CAPABILITY_STREAM;

	caps->format_count = NUM_CAPTURE_FORMAT;
	caps->formats = NULL;
	if (NUM_CAPTURE_FORMAT) {
		/* will be freed in frontend */
		caps->formats = calloc(1, sizeof capture_formats);
		if (!caps->formats) {
			TDM_ERR("alloc failed");
			return TDM_ERROR_OUT_OF_MEMORY;
		}
		for (i = 0; i < caps->format_count; i++)
			caps->formats[i] = capture_formats[i];
	}

	caps->min_w = 16;
	caps->min_h = 8;
	caps->preferred_align = 2;

	return TDM_ERROR_NONE;
}

tdm_capture*
tdm_exynos_capture_create_output(tdm_exynos_data *exynos_data, tdm_output *output, tdm_error *error)
{
	tdm_exynos_capture_data *capture_data = calloc(1, sizeof(tdm_exynos_capture_data));
	if (!capture_data) {
		TDM_ERR("alloc failed");
		if (error)
			*error = TDM_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	capture_data->exynos_data = exynos_data;
	capture_data->output_data = output;

	LIST_INITHEAD(&capture_data->pending_buffer_list);
	LIST_INITHEAD(&capture_data->buffer_list);

	if (!capture_list_init) {
		capture_list_init = 1;
		LIST_INITHEAD(&capture_list);
	}
	LIST_ADDTAIL(&capture_data->link, &capture_list);

	TDM_DBG("capture(%p) create", capture_data);

	return capture_data;
}

void
exynos_capture_destroy(tdm_capture *capture)
{
	tdm_exynos_capture_data *capture_data = capture;
	tdm_exynos_capture_buffer *b = NULL, *bb = NULL;

	if (!capture_data)
		return;

	TDM_DBG("capture(%p) destroy", capture_data);

	if (capture_data->stream.timer_source)
		tdm_event_loop_source_remove(capture_data->stream.timer_source);

	if (capture_data->stream.prop_id)
		_tdm_exynos_capture_stream_pp_cmd(capture_data, IPP_CTRL_STOP);

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &capture_data->pending_buffer_list, link) {
		LIST_DEL(&b->link);
		free(b);
	}

	LIST_FOR_EACH_ENTRY_SAFE(b, bb, &capture_data->buffer_list, link) {
		LIST_DEL(&b->link);
		tdm_buffer_unref_backend(b->ui_buffer);
		_tdm_exynos_capture_stream_pp_queue(capture_data, b, IPP_BUF_DEQUEUE);
		free(b);
	}

	LIST_DEL(&capture_data->link);

	free(capture_data);
}

tdm_error
exynos_capture_set_info(tdm_capture *capture, tdm_info_capture *info)
{
	tdm_exynos_capture_data *capture_data = capture;

	RETURN_VAL_IF_FAIL(capture_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(info, TDM_ERROR_INVALID_PARAMETER);

	capture_data->info = *info;
	capture_data->info_changed = 1;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_capture_attach(tdm_capture *capture, tbm_surface_h buffer)
{
	tdm_exynos_capture_data *capture_data = capture;
	tdm_exynos_capture_buffer *b;

	RETURN_VAL_IF_FAIL(capture_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(buffer, TDM_ERROR_INVALID_PARAMETER);

	b = calloc(1, sizeof(tdm_exynos_capture_buffer));
	if (!b) {
		TDM_ERR("alloc failed");
		return TDM_ERROR_NONE;
	}

	LIST_ADDTAIL(&b->link, &capture_data->pending_buffer_list);

	b->index = _get_index(capture_data);
	b->buffer = buffer;

	return TDM_ERROR_NONE;
}

tdm_error
exynos_capture_commit(tdm_capture *capture)
{
	tdm_exynos_capture_data *capture_data = capture;
	tdm_error ret;

	RETURN_VAL_IF_FAIL(capture_data, TDM_ERROR_INVALID_PARAMETER);

	if (capture_data->info.type == TDM_CAPTURE_TYPE_ONESHOT)
		ret = _tdm_exynos_capture_commit_oneshot(capture_data);
	else
		ret = _tdm_exynos_capture_commit_stream(capture_data);

	return ret;
}

tdm_error
exynos_capture_set_done_handler(tdm_capture *capture, tdm_capture_done_handler func, void *user_data)
{
	tdm_exynos_capture_data *capture_data = capture;

	RETURN_VAL_IF_FAIL(capture_data, TDM_ERROR_INVALID_PARAMETER);
	RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

	capture_data->done_func = func;
	capture_data->done_user_data = user_data;

	return TDM_ERROR_NONE;
}

