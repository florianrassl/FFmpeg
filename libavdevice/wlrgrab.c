/*
 * XCB input grabber
 * Copyright (C) 2014 Luca Barbato <lu_zero@gentoo.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <libdrm/drm.h>
#include <libdrm/drm_fourcc.h>

/*
#ifndef _DRM_H_
#include <drm.h>
#include <drm_fourcc.h>
#endif
*/

#include <xf86drm.h>
#include <gbm.h>

#include "libavutil/internal.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/time.h"

#include "libavformat/avformat.h"
#include "libavformat/internal.h"

#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#define ENABLE_SCREENCOPY_DMABUF


struct format {
    enum wl_shm_format wl_format;
    uint8_t wl_is_bgr;
    enum AVPixelFormat av_format;
};

typedef struct{
    struct gbm_bo *bo;
    void *map_data;
    struct wl_buffer *wl_buffer;
    void *data;
    enum wl_shm_format format;
    uint32_t width, height, stride;
    int y_invert;
} buffer;

struct output_element{
    struct wl_list link;
    struct wl_output *output;
    uint32_t id;
    struct zxdg_output_v1* xdg_output;
    const char output_name[255];
};

typedef struct WLRGrabContext {
    const AVClass *class;

    struct zwlr_screencopy_manager_v1 *screencopy_manager;
    struct zwlr_screencopy_frame_v1 *frame;
    struct zxdg_output_manager_v1* xdg_output_manager;
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf;

    struct wl_shm *shm;

    struct wl_display * display;
    struct wl_list output_list;
    struct wl_output *output;
    int32_t output_id;
    const char *output_name;

    int drm_fd;
    struct gbm_device *gbm_device;
    struct gbm_bo *bo;
    int have_linux_dmabuf;

    int64_t frame_delay;
    int64_t frame_last;
    AVRational framerate;

    buffer *buffer;
    int bpp;
    int frame_size;
    enum AVPixelFormat av_format;

    int buffer_copy_done;

} WLRGrabContext;

struct wlrg_buffer_private{
    const char*         shm_name;
    int                 shm_fd;
    struct wl_shm_pool* shm_pool;
};

struct wlrg_buffer_private wlrg_buffer_private_data = {
    .shm_name   =   "/wlroots-screencopy_2",
    .shm_fd     =   -1,
    .shm_pool   =   NULL,
};

static int wlrg_buffer_init(struct wlrg_buffer_private *priv,  WLRGrabContext *ctx)
{
    av_log(ctx, AV_LOG_DEBUG, "init wlrig_buffer\n");

    priv->shm_fd = shm_open(priv->shm_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

    if (priv->shm_fd < 0) {
        av_log(ctx, AV_LOG_WARNING, "shm_open failed trying to unlink\n");
        shm_unlink(priv->shm_name);
        priv->shm_fd = shm_open(priv->shm_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    }

    if (priv->shm_fd < 0) {
        av_log(ctx, AV_LOG_ERROR, "shm_open failed\n");
        return -1;
    }
    return 0;
}

static int wlrg_buffer_resize(struct wlrg_buffer_private *priv, size_t new_size,  WLRGrabContext *ctx)
{
    int ret;
    if (priv->shm_pool == NULL){
        av_log(ctx, AV_LOG_DEBUG, "create shm_pool\n");
        priv->shm_pool = wl_shm_create_pool(ctx->shm, priv->shm_fd, new_size);
    }

    ctx->buffer->data = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, priv->shm_fd, 0);
    if (ctx->buffer->data == MAP_FAILED) {
        close(priv->shm_fd);
        av_log(ctx, AV_LOG_ERROR, "mmap failed\n");
        return -1;
    }

    wl_shm_pool_resize(priv->shm_pool, new_size);
    //close(fd);

    shm_unlink(priv->shm_name);
    while ((ret = ftruncate(priv->shm_fd, new_size)) == EINTR) {
        // No-op
    }

    if (ret < 0) {
        close(priv->shm_fd);
        av_log(ctx, AV_LOG_ERROR, "ftruncate failed\n");
        return -1 ;
    }
    return 0;
}

static buffer* wlrg_buffer_create_from_buffer(struct wlrg_buffer_private *priv, buffer *buffer,  WLRGrabContext *ctx)
{
    //av_log(ctx, AV_LOG_DEBUG, "create from buffer wlrig_buffer\n");

    buffer->wl_buffer = wl_shm_pool_create_buffer(priv->shm_pool, 0, buffer->width, buffer->height,
                                                  buffer->stride, buffer->format);
    if (ctx->buffer->wl_buffer == NULL) {
        av_log(ctx, AV_LOG_ERROR, "failed to create wl_shm_buffer\n");
        return NULL;
    }
    return buffer;
}

static void wlrg_buffer_close(struct wlrg_buffer_private *priv, WLRGrabContext *ctx)
{
    av_log(ctx, AV_LOG_DEBUG, "close wlrg_buffer\n");

    shm_unlink(priv->shm_name);
    if(priv->shm_pool){
        wl_shm_pool_destroy(priv->shm_pool);
        priv->shm_pool = NULL;
    }
    close(priv->shm_fd);
    priv->shm_fd = 0;
}

//#define TEST_BO

static void wlrgrab_free_wl_buffer(void *opaque, uint8_t *data)
{
    buffer *buffer = opaque;
    if (buffer->map_data){
        gbm_bo_unmap(buffer->bo, buffer->map_data);
#ifndef TEST_BO
        gbm_bo_destroy(buffer->bo);
#endif
    }
    else{
        munmap(data, buffer->stride * buffer->height);
    }

    wl_buffer_destroy(buffer->wl_buffer);
    av_free(buffer);
}

static void output_list_add_output(struct wl_output* output, uint32_t id, WLRGrabContext *ctx){
    struct output_element *oe;
    oe = av_malloc(sizeof (*oe));
    oe->id = id;
    oe->output = output;
    strcpy((char*)oe->output_name, "forgot my name");
    wl_list_insert(&ctx->output_list, &oe->link);
}

static struct wl_output* output_by_id(uint32_t id, WLRGrabContext *ctx){
    struct output_element* oe;
    wl_list_for_each(oe, &ctx->output_list, link){
        if(oe->id == id){
            av_log(ctx, AV_LOG_DEBUG, "using output: %d\n", id);
            return oe->output;
        }
    }
    return NULL;
}

static struct wl_output* output_by_name(const char *name, WLRGrabContext *ctx){
    struct output_element* oe;
    wl_list_for_each(oe, &ctx->output_list, link){
        if(strcmp(oe->output_name, name) == 0){
            av_log(ctx, AV_LOG_DEBUG, "using output: %s\n", name);
            return oe->output;
        }
    }
    return NULL;
}

static void output_logical_position(void* data, struct zxdg_output_v1* xdg_output,
                                    int32_t x, int32_t y)
{}
static void output_logical_size(void* data, struct zxdg_output_v1* xdg_output,
                                int32_t width, int32_t height)
{}
static void output_done(void *data,
                        struct zxdg_output_v1 *zxdg_output_v1)
{}

static void output_name(void* data, struct zxdg_output_v1* xdg_output,
                        const char* name)
{
    struct output_element *oe = data;
    strcpy((char*)oe->output_name, name);
}

static void output_description(void* data, struct zxdg_output_v1* xdg_output,
                               const char* description)
{}

static void output_list_set_names(WLRGrabContext *ctx){

    static const struct zxdg_output_v1_listener xdg_output_listener = {
        .logical_position =  output_logical_position,
                .logical_size =  output_logical_size,
                .done = output_done, /* Deprecated */
                .name = output_name,
                .description = output_description,
    };

    struct output_element* oe;
    wl_list_for_each(oe, &ctx->output_list, link){
        oe->xdg_output = zxdg_output_manager_v1_get_xdg_output(
                    ctx->xdg_output_manager, oe->output);
        if (oe->xdg_output == NULL){
            av_log(ctx, AV_LOG_WARNING, "xdg_output_manager is null. trying to continue\n");
            return;
        }
        zxdg_output_v1_add_listener(oe->xdg_output, &xdg_output_listener, oe);
        wl_display_dispatch(ctx->display);

        av_log(ctx, AV_LOG_DEBUG, "found output: id=%d, name=%s\n", oe->id, oe->output_name);
    }
}

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface, uint32_t version)
{
    WLRGrabContext *ctx = data;

    if (strcmp(interface, wl_output_interface.name) == 0) {
        ctx->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        output_list_add_output(ctx->output, name, ctx);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0) {
        ctx->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        ctx->zwp_linux_dmabuf = wl_registry_bind(registry, name,
                                                 &zwp_linux_dmabuf_v1_interface, 3);
    }
    else if (strcmp(interface,
                    zwlr_screencopy_manager_v1_interface.name) == 0) {
        ctx->screencopy_manager = wl_registry_bind(registry, name,
                                                   &zwlr_screencopy_manager_v1_interface, version);
    }
    else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        ctx->xdg_output_manager = wl_registry_bind(registry, name,
                                                   &zxdg_output_manager_v1_interface, 3);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                                 uint32_t id)
{
    WLRGrabContext* ctx = data;
    if (ctx->output)
        wl_output_destroy(ctx->output);
    return;
}

static inline buffer *create_buffer(enum wl_shm_format fmt,
                                    int width, int height, int stride, WLRGrabContext *ctx)
{
    buffer *buffer;
    buffer = av_mallocz(sizeof (*buffer));

    buffer->format = fmt;
    buffer->width = width;
    buffer->height = height;
    buffer->stride = stride;
    buffer->bo = NULL;
    return buffer;
}

static void frame_handle_buffer(void *data,
                                struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
                                uint32_t width, uint32_t height, uint32_t stride)
{
    WLRGrabContext *ctx = data;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK handle BUFFER\n");

    ctx->buffer = create_buffer(format, width, height, stride, ctx);
}

static void frame_handle_linux_dmabuf(void *data,
                                      struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
                                      uint32_t width, uint32_t height)
{
    WLRGrabContext *ctx = data;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK LINUX DMABUF\n");

    if (!ctx->drm_fd)
        return;

    ctx->have_linux_dmabuf = 1;

    //ctx->buffer = create_buffer(format, width, height, 0, ctx);

    ctx->buffer->format = format;
    ctx->buffer->width = width;
    ctx->buffer->height = height;
    ctx->buffer->stride = 0;

}

static void frame_handle_buffer_done(void *data,
                                     struct zwlr_screencopy_frame_v1 *frame)
{
    WLRGrabContext *ctx = data;
    uint32_t fd, off, bo_stride;
    uint64_t mod;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK BUFFER DONE\n");
    struct zwp_linux_buffer_params_v1 *params;

    if (!ctx->have_linux_dmabuf || !ctx->drm_fd) {
        //av_log(ctx, AV_LOG_DEBUG, "linux-dmabuf is not supported\n");
        wlrg_buffer_resize(&wlrg_buffer_private_data, ctx->buffer->stride * ctx->buffer->height, ctx);
        wlrg_buffer_create_from_buffer(&wlrg_buffer_private_data, ctx->buffer, ctx);
        zwlr_screencopy_frame_v1_copy(ctx->frame, ctx->buffer->wl_buffer);
        return;
    }

#ifdef TEST_BO
    if(!ctx->bo){
        ctx->bo = gbm_bo_create(ctx->gbm_device, ctx->buffer->width,
                                ctx->buffer->height, ctx->buffer->format,
                                GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
    }
    ctx->buffer->bo = ctx->bo;
#else
    assert(!ctx->buffer->bo);
    ctx->buffer->bo = gbm_bo_create(ctx->gbm_device, ctx->buffer->width,
                                    ctx->buffer->height, ctx->buffer->format,
                                    GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
#endif

    if (ctx->buffer->bo == NULL) {
        av_log(ctx, AV_LOG_ERROR, "failed to create GBM buffer object\n");
        exit(AVERROR(EIO));
    }

    params = zwp_linux_dmabuf_v1_create_params(ctx->zwp_linux_dmabuf);
    assert(params);

    fd = gbm_bo_get_fd(ctx->buffer->bo);
    off = gbm_bo_get_offset(ctx->buffer->bo, 0);
    bo_stride = gbm_bo_get_stride(ctx->buffer->bo);
    mod = gbm_bo_get_modifier(ctx->buffer->bo);

    zwp_linux_buffer_params_v1_add(params, fd, 0, off, bo_stride, mod >> 32,
                                   mod & 0xffffffff);

    ctx->buffer->wl_buffer = zwp_linux_buffer_params_v1_create_immed(params, ctx->buffer->width,
                                                                     ctx->buffer->height,
                                                                     ctx->buffer->format, /* flags */ 0);
    if (!ctx->buffer->wl_buffer)
        av_log(ctx, AV_LOG_ERROR, "failed to create_immed wl_buffer\n");

    zwp_linux_buffer_params_v1_destroy(params);
    close(fd);

    zwlr_screencopy_frame_v1_copy(ctx->frame, ctx->buffer->wl_buffer);
}

static void frame_handle_flags(void *data,
                               struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
    WLRGrabContext *ctx = data;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK FLAGS\n");
    ctx->buffer->y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

static void frame_handle_ready(void *data,
                               struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi,
                               uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    WLRGrabContext *ctx = data;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK READY\n");

    ctx->buffer_copy_done = 1;
    if(!ctx->have_linux_dmabuf)
        return;

    ctx->buffer->data = gbm_bo_map(ctx->buffer->bo, 0, 0, ctx->buffer->width,
                                   ctx->buffer->height, GBM_BO_TRANSFER_READ,
                                   &ctx->buffer->stride, &ctx->buffer->map_data);

    if (!ctx->buffer->data) {
        av_log(ctx, AV_LOG_ERROR, "Failed to map gbm bo\n");
        return;
    }
}

static void frame_handle_failed(void *data,
                                struct zwlr_screencopy_frame_v1 *frame)
{
    WLRGrabContext *ctx = data;
    av_log(ctx, AV_LOG_ERROR, "failed to copy frame\n");
    exit(AVERROR(EIO));
}

// wl_shm_format describes little-endian formats, libpng uses big-endian
// formats (so Wayland's ABGR is libpng's RGBA).
static const struct format formats[] = {
{DRM_FORMAT_XRGB8888,       1,  AV_PIX_FMT_BGR0},
{DRM_FORMAT_ARGB8888,       1,  AV_PIX_FMT_BGRA},
{DRM_FORMAT_XBGR8888,       0,  AV_PIX_FMT_RGB0},
{DRM_FORMAT_ABGR8888,       0,  AV_PIX_FMT_RGBA},
{WL_SHM_FORMAT_XRGB8888,    1,  AV_PIX_FMT_0BGR},
{WL_SHM_FORMAT_ARGB8888,    1,  AV_PIX_FMT_BGR0},
{WL_SHM_FORMAT_XBGR8888,    0,  AV_PIX_FMT_RGB0},
{WL_SHM_FORMAT_ABGR8888,    0,  AV_PIX_FMT_0RGB},
};

static int create_stream(AVFormatContext *avctx)
{
    WLRGrabContext *ctx = avctx->priv_data;
    //av_log(avctx, AV_LOG_DEBUG, "FUCK careate stream\n");

    int64_t frame_size_bits;

    AVStream *stream = avformat_new_stream(avctx, NULL);
    if (!stream){
        av_log(avctx, AV_LOG_ERROR, "Can't careate AVStream\n");
        return AVERROR(ENOMEM);
    }

    avpriv_set_pts_info(stream, 64, 1, 1000000);

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        if (formats[i].wl_format == ctx->buffer->format) {
            ctx->av_format = formats[i].av_format;
            if(formats[i].wl_is_bgr)
                ctx->av_format |= AV_HAVE_BIGENDIAN;
            av_log(ctx, AV_LOG_DEBUG, "Format:=%ld\n", i);
            break;
        }
    }
    ctx->bpp = 32; //bit per pixel

    frame_size_bits = (int64_t)ctx->buffer->width * ctx->buffer->height * ctx->bpp;
    av_log(ctx, AV_LOG_DEBUG, "FUCK frame_size_bits %ld\n", frame_size_bits);

    if (frame_size_bits / 8 + AV_INPUT_BUFFER_PADDING_SIZE > INT_MAX) {
        av_log(avctx, AV_LOG_ERROR, "Captured area is too large\n");
        return AVERROR_PATCHWELCOME;
    }

    ctx->frame_size = frame_size_bits / 8;

    stream->codecpar->format     = ctx->av_format;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id   = AV_CODEC_ID_RAWVIDEO;
    stream->codecpar->width      = ctx->buffer->width;
    stream->codecpar->height     = ctx->buffer->height;
    //stream->codecpar->bit_rate   = av_rescale(frame_size_bits, stream->avg_frame_rate.num, stream->avg_frame_rate.den);

    ctx->frame_delay = av_rescale_q(1, (AVRational) { ctx->framerate.den,
                                                      ctx->framerate.num }, AV_TIME_BASE_Q);
    return 0;
}

static int capture_frame(AVFormatContext *avctx)
{
    WLRGrabContext *ctx = avctx->priv_data;
    //av_log(ctx, AV_LOG_DEBUG, "FUCK capture frame\n");

    static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
        .buffer = frame_handle_buffer,
                .linux_dmabuf = frame_handle_linux_dmabuf,
                .buffer_done = frame_handle_buffer_done,
                .flags = frame_handle_flags,
                .ready = frame_handle_ready,
                .failed = frame_handle_failed,
    };

    //ctx->buffer = NULL;
    ctx->buffer_copy_done = 0;
    ctx->frame =
            zwlr_screencopy_manager_v1_capture_output(ctx->screencopy_manager, 0, ctx->output);
    zwlr_screencopy_frame_v1_add_listener(ctx->frame, &frame_listener, ctx);

    while (!ctx->buffer_copy_done && wl_display_dispatch(ctx->display) != -1) {}

    zwlr_screencopy_frame_v1_destroy(ctx->frame);

    return 0;
}

#ifdef ENABLE_SCREENCOPY_DMABUF
static int find_render_node(char *node, size_t maxlen)
{
    int ret = 0;
    drmDevice *devices[64];

    int n = drmGetDevices2(0, devices, sizeof(devices) / sizeof(devices[0]));
    for (int i = 0; i < n; ++i) {
        drmDevice *dev = devices[i];
        if (!(dev->available_nodes & (1 << DRM_NODE_RENDER))) {
            continue;
        }

        strncpy(node, dev->nodes[DRM_NODE_RENDER], maxlen - 1);
        node[maxlen - 1] = '\0';
        ret = 1;
        break;
    }

    drmFreeDevices(devices, n);
    return ret;
}
#endif

static av_cold int wlrgrab_read_header(AVFormatContext *avctx)
{
    WLRGrabContext *ctx = avctx->priv_data;
    struct wl_registry *registry;
    struct wl_output *wl_out_tmp = NULL;
    int ret;

    static const struct wl_registry_listener registry_listener = {
        .global = handle_global,
                .global_remove = handle_global_remove,
    };

#ifdef ENABLE_SCREENCOPY_DMABUF
    char render_node[256];
    if (!find_render_node(render_node, sizeof(render_node))) {
        ctx->drm_fd = 0;
        av_log(ctx, AV_LOG_WARNING, "Failed to find a DRM render node\n");
    }
    else{
        av_log(ctx, AV_LOG_DEBUG, "Using render node: %s\n", render_node);

        ctx->drm_fd = open(render_node, O_RDWR);
        if (ctx->drm_fd < 0) {
            av_log(ctx, AV_LOG_ERROR, "Failed to open drm render node");
            return AVERROR(EIO);
        }

        ctx->gbm_device = gbm_create_device(ctx->drm_fd);
        if (!ctx->gbm_device) {
            av_log(ctx, AV_LOG_ERROR, "Failed to create gbm device\n");
            return AVERROR(EIO);
        }
    }
#endif

    ctx->have_linux_dmabuf = 0;
    wl_list_init(&ctx->output_list);

    ctx->display = wl_display_connect(NULL);
    if (ctx->display == NULL) {
        av_log(ctx, AV_LOG_ERROR, "failed to create display\n");
        return AVERROR(EIO);
    }

    registry = wl_display_get_registry(ctx->display);
    if (registry == NULL){
        av_log(ctx, AV_LOG_ERROR, "wl_display_get_registry\n");
        return AVERROR(EIO);
    }

    wl_registry_add_listener(registry, &registry_listener, ctx);

    ret = wl_display_roundtrip(ctx->display);
    if (ret < 0){
        av_log(ctx, AV_LOG_ERROR, "wl_display_roundtrip\n");
        return AVERROR(EIO);
    }
    if (ctx->shm == NULL) {
        av_log(ctx, AV_LOG_ERROR, "compositor is missing wl_shm\n");
        return AVERROR(EIO);
    }
    if (wlrg_buffer_init(&wlrg_buffer_private_data, ctx))
        return AVERROR((EIO));

#ifdef ENABLE_SCREENCOPY_DMABUF
    if (ctx->zwp_linux_dmabuf == NULL) {
        av_log(ctx, AV_LOG_ERROR, "compositor is missing linux-dmabuf-unstable-v1\n");
        return AVERROR(EIO);
    }
#endif
    if (ctx->screencopy_manager == NULL) {
        av_log(ctx, AV_LOG_ERROR,  "compositor doesn't support wlr-screencopy-unstable-v1\n");
        return AVERROR(EIO);
    }
    if (ctx->xdg_output_manager == NULL) {
        av_log(ctx, AV_LOG_ERROR, "xdg_output_manager is NULL\n");
        return AVERROR(EIO);
    }

    output_list_set_names(ctx);
    if (ctx->output_id != -1)
        wl_out_tmp = output_by_id(ctx->output_id, ctx);
    else if(strlen(ctx->output_name))
        wl_out_tmp = output_by_name(ctx->output_name, ctx);
    else if(strlen(avctx->url))
        wl_out_tmp = output_by_name(avctx->url, ctx);
    if(wl_out_tmp == NULL)
        av_log(ctx, AV_LOG_WARNING, "output not found. using what ever\n");
    else
        ctx->output = wl_out_tmp;

    capture_frame(avctx);

    if (ctx->buffer->y_invert)
        av_log(ctx, AV_LOG_DEBUG, "y_invert\n");

    ret = create_stream(avctx);
    if (ret){
        av_log(ctx, AV_LOG_ERROR, "faild to create stream\n");
        //CLEAN UP
        return ret;
    }

    return 0;
}

static int wlrgrab_read_packet(AVFormatContext *avctx, AVPacket *pkt)
{
    WLRGrabContext *ctx = avctx->priv_data;
    int64_t now;
    int err;

    //av_log(ctx, AV_LOG_DEBUG, "FUCK PAK\n");

    now = av_gettime_relative();
    if (ctx->frame_last) {
        int64_t delay;
        while (1) {
            delay = ctx->frame_last + ctx->frame_delay - now;
            if (delay <= 0)
                break;
            av_usleep(delay);
            now = av_gettime_relative();
        }
    }
    ctx->frame_last = now;
    now = av_gettime();

    capture_frame(avctx);

    pkt->buf = av_buffer_create(ctx->buffer->data, ctx->frame_size,
                                &wlrgrab_free_wl_buffer, ctx->buffer, 0);

    if (!pkt->buf) {
        err = AVERROR(ENOMEM);
        goto fail;
    }

    pkt->data   = (uint8_t*)ctx->buffer->data;
    pkt->size   = ctx->frame_size;
    pkt->pts    = now;

    return 0;

fail:
    wlrg_buffer_close(&wlrg_buffer_private_data, ctx);
    return err;
}

static av_cold int wlrgrab_read_close(AVFormatContext *avctx)
{
    WLRGrabContext *ctx = avctx->priv_data;
    av_log(ctx, AV_LOG_DEBUG, "FUCK  read close\n");

    /*
    struct output_element* oe;
    wl_list_for_each(oe, &ctx->output_list, link){
        av_free(oe);
    }
    */

    wlrg_buffer_close(&wlrg_buffer_private_data, ctx);

    /*
     * NOPE!!
    if (ctx->bo)
        gbm_bo_destroy(ctx->bo);
    */

    /*
     * NOPE!!
    if(ctx->gbm_device)
        gbm_device_destroy(ctx->gbm_device);
    */

    if(ctx->drm_fd)
        close(ctx->drm_fd);

    if(ctx->screencopy_manager)
        zwlr_screencopy_manager_v1_destroy(ctx->screencopy_manager);
    if(ctx->zwp_linux_dmabuf)
        zwp_linux_dmabuf_v1_destroy(ctx->zwp_linux_dmabuf);
    if(ctx->shm)
        wl_shm_destroy(ctx->shm);

    return 0;
}

#define OFFSET(x) offsetof(WLRGrabContext, x)
#define FLAGS AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    { "name", "Window name to capture (same as -i).", OFFSET(output_name), AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, FLAGS },
    { "id", "Window id to capture.", OFFSET(output_id), AV_OPT_TYPE_INT, { .i64 = -1 }, -1, 10000, FLAGS },
    { "framerate", "Framerate to capture at", OFFSET(framerate), AV_OPT_TYPE_RATIONAL, { .dbl = 30.0 }, 0, 1000, FLAGS },
    { NULL },
};

static const AVClass wlrgrab_class = {
    .class_name = "wlrgrab indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
};


const AVInputFormat ff_wlrgrab_demuxer = {
    .name           = "wlrgrab",
    .long_name      = NULL_IF_CONFIG_SMALL("wlroots screen capture"),
    .priv_data_size = sizeof(WLRGrabContext),
    .read_header    = &wlrgrab_read_header,
    .read_packet    = &wlrgrab_read_packet,
    .read_close     = wlrgrab_read_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &wlrgrab_class,
};
