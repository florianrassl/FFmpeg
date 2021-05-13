/* Generated by wayland-scanner 1.19.0 */

#ifndef WLR_SCREENCOPY_UNSTABLE_V1_SERVER_PROTOCOL_H
#define WLR_SCREENCOPY_UNSTABLE_V1_SERVER_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

/**
 * @page page_wlr_screencopy_unstable_v1 The wlr_screencopy_unstable_v1 protocol
 * screen content capturing on client buffers
 *
 * @section page_desc_wlr_screencopy_unstable_v1 Description
 *
 * This protocol allows clients to ask the compositor to copy part of the
 * screen content to a client buffer.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding interface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and interface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 *
 * @section page_ifaces_wlr_screencopy_unstable_v1 Interfaces
 * - @subpage page_iface_zwlr_screencopy_manager_v1 - manager to inform clients and begin capturing
 * - @subpage page_iface_zwlr_screencopy_frame_v1 - a frame ready for copy
 * @section page_copyright_wlr_screencopy_unstable_v1 Copyright
 * <pre>
 *
 * Copyright © 2018 Simon Ser
 * Copyright © 2019 Andri Yngvason
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * </pre>
 */
struct wl_buffer;
struct wl_output;
struct zwlr_screencopy_frame_v1;
struct zwlr_screencopy_manager_v1;

#ifndef ZWLR_SCREENCOPY_MANAGER_V1_INTERFACE
#define ZWLR_SCREENCOPY_MANAGER_V1_INTERFACE
/**
 * @page page_iface_zwlr_screencopy_manager_v1 zwlr_screencopy_manager_v1
 * @section page_iface_zwlr_screencopy_manager_v1_desc Description
 *
 * This object is a manager which offers requests to start capturing from a
 * source.
 * @section page_iface_zwlr_screencopy_manager_v1_api API
 * See @ref iface_zwlr_screencopy_manager_v1.
 */
/**
 * @defgroup iface_zwlr_screencopy_manager_v1 The zwlr_screencopy_manager_v1 interface
 *
 * This object is a manager which offers requests to start capturing from a
 * source.
 */
extern const struct wl_interface zwlr_screencopy_manager_v1_interface;
#endif
#ifndef ZWLR_SCREENCOPY_FRAME_V1_INTERFACE
#define ZWLR_SCREENCOPY_FRAME_V1_INTERFACE
/**
 * @page page_iface_zwlr_screencopy_frame_v1 zwlr_screencopy_frame_v1
 * @section page_iface_zwlr_screencopy_frame_v1_desc Description
 *
 * This object represents a single frame.
 *
 * When created, a series of buffer events will be sent, each representing a
 * supported buffer type. The "buffer_done" event is sent afterwards to
 * indicate that all supported buffer types have been enumerated. The client
 * will then be able to send a "copy" request. If the capture is successful,
 * the compositor will send a "flags" followed by a "ready" event.
 *
 * For objects version 2 or lower, wl_shm buffers are always supported, ie.
 * the "buffer" event is guaranteed to be sent.
 *
 * If the capture failed, the "failed" event is sent. This can happen anytime
 * before the "ready" event.
 *
 * Once either a "ready" or a "failed" event is received, the client should
 * destroy the frame.
 * @section page_iface_zwlr_screencopy_frame_v1_api API
 * See @ref iface_zwlr_screencopy_frame_v1.
 */
/**
 * @defgroup iface_zwlr_screencopy_frame_v1 The zwlr_screencopy_frame_v1 interface
 *
 * This object represents a single frame.
 *
 * When created, a series of buffer events will be sent, each representing a
 * supported buffer type. The "buffer_done" event is sent afterwards to
 * indicate that all supported buffer types have been enumerated. The client
 * will then be able to send a "copy" request. If the capture is successful,
 * the compositor will send a "flags" followed by a "ready" event.
 *
 * For objects version 2 or lower, wl_shm buffers are always supported, ie.
 * the "buffer" event is guaranteed to be sent.
 *
 * If the capture failed, the "failed" event is sent. This can happen anytime
 * before the "ready" event.
 *
 * Once either a "ready" or a "failed" event is received, the client should
 * destroy the frame.
 */
extern const struct wl_interface zwlr_screencopy_frame_v1_interface;
#endif

/**
 * @ingroup iface_zwlr_screencopy_manager_v1
 * @struct zwlr_screencopy_manager_v1_interface
 */
struct zwlr_screencopy_manager_v1_interface {
	/**
	 * capture an output
	 *
	 * Capture the next frame of an entire output.
	 * @param overlay_cursor composite cursor onto the frame
	 */
	void (*capture_output)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t frame,
			       int32_t overlay_cursor,
			       struct wl_resource *output);
	/**
	 * capture an output's region
	 *
	 * Capture the next frame of an output's region.
	 *
	 * The region is given in output logical coordinates, see
	 * xdg_output.logical_size. The region will be clipped to the
	 * output's extents.
	 * @param overlay_cursor composite cursor onto the frame
	 */
	void (*capture_output_region)(struct wl_client *client,
				      struct wl_resource *resource,
				      uint32_t frame,
				      int32_t overlay_cursor,
				      struct wl_resource *output,
				      int32_t x,
				      int32_t y,
				      int32_t width,
				      int32_t height);
	/**
	 * destroy the manager
	 *
	 * All objects created by the manager will still remain valid,
	 * until their appropriate destroy request has been called.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};


/**
 * @ingroup iface_zwlr_screencopy_manager_v1
 */
#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_manager_v1
 */
#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_manager_v1
 */
#define ZWLR_SCREENCOPY_MANAGER_V1_DESTROY_SINCE_VERSION 1

#ifndef ZWLR_SCREENCOPY_FRAME_V1_ERROR_ENUM
#define ZWLR_SCREENCOPY_FRAME_V1_ERROR_ENUM
enum zwlr_screencopy_frame_v1_error {
	/**
	 * the object has already been used to copy a wl_buffer
	 */
	ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED = 0,
	/**
	 * buffer attributes are invalid
	 */
	ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER = 1,
};
#endif /* ZWLR_SCREENCOPY_FRAME_V1_ERROR_ENUM */

#ifndef ZWLR_SCREENCOPY_FRAME_V1_FLAGS_ENUM
#define ZWLR_SCREENCOPY_FRAME_V1_FLAGS_ENUM
enum zwlr_screencopy_frame_v1_flags {
	/**
	 * contents are y-inverted
	 */
	ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT = 1,
};
#endif /* ZWLR_SCREENCOPY_FRAME_V1_FLAGS_ENUM */

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * @struct zwlr_screencopy_frame_v1_interface
 */
struct zwlr_screencopy_frame_v1_interface {
	/**
	 * copy the frame
	 *
	 * Copy the frame to the supplied buffer. The buffer must have a
	 * the correct size, see zwlr_screencopy_frame_v1.buffer and
	 * zwlr_screencopy_frame_v1.linux_dmabuf. The buffer needs to have
	 * a supported format.
	 *
	 * If the frame is successfully copied, a "flags" and a "ready"
	 * events are sent. Otherwise, a "failed" event is sent.
	 */
	void (*copy)(struct wl_client *client,
		     struct wl_resource *resource,
		     struct wl_resource *buffer);
	/**
	 * delete this object, used or not
	 *
	 * Destroys the frame. This request can be sent at any time by
	 * the client.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * copy the frame when it's damaged
	 *
	 * Same as copy, except it waits until there is damage to copy.
	 * @since 2
	 */
	void (*copy_with_damage)(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *buffer);
};

#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER 0
#define ZWLR_SCREENCOPY_FRAME_V1_FLAGS 1
#define ZWLR_SCREENCOPY_FRAME_V1_READY 2
#define ZWLR_SCREENCOPY_FRAME_V1_FAILED 3
#define ZWLR_SCREENCOPY_FRAME_V1_DAMAGE 4
#define ZWLR_SCREENCOPY_FRAME_V1_LINUX_DMABUF 5
#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER_DONE 6

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_FLAGS_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_READY_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_FAILED_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_DAMAGE_SINCE_VERSION 2
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_LINUX_DMABUF_SINCE_VERSION 3
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER_DONE_SINCE_VERSION 3

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_COPY_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_DESTROY_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 */
#define ZWLR_SCREENCOPY_FRAME_V1_COPY_WITH_DAMAGE_SINCE_VERSION 2

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an buffer event to the client owning the resource.
 * @param resource_ The client's resource
 * @param format buffer format
 * @param width buffer width
 * @param height buffer height
 * @param stride buffer stride
 */
static inline void
zwlr_screencopy_frame_v1_send_buffer(struct wl_resource *resource_, uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_BUFFER, format, width, height, stride);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an flags event to the client owning the resource.
 * @param resource_ The client's resource
 * @param flags frame flags
 */
static inline void
zwlr_screencopy_frame_v1_send_flags(struct wl_resource *resource_, uint32_t flags)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_FLAGS, flags);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an ready event to the client owning the resource.
 * @param resource_ The client's resource
 * @param tv_sec_hi high 32 bits of the seconds part of the timestamp
 * @param tv_sec_lo low 32 bits of the seconds part of the timestamp
 * @param tv_nsec nanoseconds part of the timestamp
 */
static inline void
zwlr_screencopy_frame_v1_send_ready(struct wl_resource *resource_, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_READY, tv_sec_hi, tv_sec_lo, tv_nsec);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an failed event to the client owning the resource.
 * @param resource_ The client's resource
 */
static inline void
zwlr_screencopy_frame_v1_send_failed(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_FAILED);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an damage event to the client owning the resource.
 * @param resource_ The client's resource
 * @param x damaged x coordinates
 * @param y damaged y coordinates
 * @param width current width
 * @param height current height
 */
static inline void
zwlr_screencopy_frame_v1_send_damage(struct wl_resource *resource_, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_DAMAGE, x, y, width, height);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an linux_dmabuf event to the client owning the resource.
 * @param resource_ The client's resource
 * @param format fourcc pixel format
 * @param width buffer width
 * @param height buffer height
 */
static inline void
zwlr_screencopy_frame_v1_send_linux_dmabuf(struct wl_resource *resource_, uint32_t format, uint32_t width, uint32_t height)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_LINUX_DMABUF, format, width, height);
}

/**
 * @ingroup iface_zwlr_screencopy_frame_v1
 * Sends an buffer_done event to the client owning the resource.
 * @param resource_ The client's resource
 */
static inline void
zwlr_screencopy_frame_v1_send_buffer_done(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, ZWLR_SCREENCOPY_FRAME_V1_BUFFER_DONE);
}

#ifdef  __cplusplus
}
#endif

#endif
