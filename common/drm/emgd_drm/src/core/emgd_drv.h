/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_drv.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *-----------------------------------------------------------------------------
 * Description:
 *
 *-----------------------------------------------------------------------------
 */
#ifndef _EMGD_DRV_H_
#define _EMGD_DRV_H_

#include "default_config.h"

#define DRIVER_AUTHOR     "Intel Corporation."
#define DRIVER_NAME       "emgd"
#define DRIVER_DESC       "Intel Embedded Media and Graphics Driver"
#define DRIVER_MAJOR      IGD_MAJOR_NUM
#define DRIVER_MINOR      IGD_MINOR_NUM
#define DRIVER_PATCHLEVEL IGD_BUILD_NUM

#define INTELFB_CONN_LIMIT 4

/**
 * CRTC Properties Mask
 */
#define EMGD_CRTC_UPDATE_ZORDER_PROPERTY        0x1
#define EMGD_CRTC_UPDATE_FB_BLEND_OVL_PROPERTY  0x2
#define EMGD_CRTC_UPDATE_ALL_PROPERTIES         (~0x0)

#define to_emgd_crtc(x) container_of(x, emgd_crtc_t, base)
#define to_emgd_connector(x) container_of(x, emgd_connector_t, base)
#define to_emgd_encoder(x) container_of(x, emgd_encoder_t, base)
#define to_emgd_framebuffer(x) container_of(x, emgd_framebuffer_t, base)
#define to_emgd_plane(x) container_of(x, igd_ovl_plane_t, base)

#define for_each_encoder_on_crtc(dev, __crtc, intel_encoder) \
	list_for_each_entry((intel_encoder), &(dev)->mode_config.encoder_list, base.head) \
		if ((intel_encoder)->base.crtc == (__crtc))

typedef struct drm_device drm_device_t;

/**
 * A pointer to a non-HAL-provided function that processes a VBlank interrupt.
 */
typedef int (*emgd_process_vblank_interrupt_t)(void *priv);

/**
 * This structure allows the HAL to track a non-HAL callback (and its
 *  parameter) to call when a VBlank interrupt occurs for a given port.  An
 *  opaque pointer to this structure serves as a unique identifier for the
 *  callback/port combination.
 */
typedef struct _emgd_vblank_callback {
	/** Non-HAL callback function to process a VBlank interrupt. */
	emgd_process_vblank_interrupt_t callback;
	/** An opaque pointer to a non-HAL data structure (passed to callback). */
	void *priv;
	/** Which HAL port number is associated with this interrupt callback. */
	uint32_t port_number;
} emgd_vblank_callback_t;

/**
 * An opaque pointer to a emgd_vblank_callback_t.  This pointer serves as a
 * unique identifier for the callback/port combination.
 */
typedef void *emgd_vblank_callback_h;

/**
 * A special value of a emgd_vblank_callback_h, meaning ALL devices/displays.
 */
#define ALL_PORT_CALLBACKS	((void *) 1001)

/*
 * IOCTL handler function prototypes:
 */
int emgd_get_param(struct drm_device *dev, void *arg,
	struct drm_file *file_priv);

 /*
  * Ioctl to query kernel params (EMGD)
  * I915 definition currently occupies from value 0 up to 17
  */
#define I915_PARAM_HAS_MULTIPLANE_DRM	30


#ifndef CONFIG_EMGD_DRM915_OVERLAY_KERNELSUPPORT
extern int emgd_ioctl_get_ovlplane_resources(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_get_ovlplane(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_set_ovlplane(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_mode_addfb2_ovlplane(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
#endif /*CONFIG_EMGD_DRM915_OVERLAY_KERNELSUPPORT*/

#ifdef SPLASH_VIDEO_SUPPORT /* *NOTE*: drm_mode_setplane() function symbol has to be exported in drm */
extern int emgd_ioctl_init_spvideo(struct drm_device *dev, void *data,
                                   struct drm_file *file_priv);
extern int emgd_ioctl_display_spvideo(struct drm_device *dev, void *data,
                                      struct drm_file *file_priv);
#endif

extern int emgd_ioctl_set_ovlplane_colorkey(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_get_ovlplane_colorkey(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_get_pipe_from_crtc_id(struct drm_device *dev, void *arg,
                 struct drm_file *file_priv);
#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
extern int emgd_ioctl_set_ovlplane_colorcorrection(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_get_ovlplane_colorcorrection(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
#endif
extern int emgd_ioctl_set_ovlplane_pipeblending(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_ioctl_get_ovlplane_pipeblending(struct drm_device *dev, void *arg,
		struct drm_file *file_priv);
extern int emgd_crtc_update_fb_and_calc(struct drm_crtc *crtc, int x, int y);

struct emgd_plane_coords;

extern int emgd_core_commit_sprite(struct drm_plane *_plane,
		const struct drm_crtc *crtc, void *regs);
extern int emgd_core_check_sprite(struct drm_plane *_plane,
		const struct drm_crtc *crtc, const struct drm_framebuffer *fb,
		struct emgd_plane_coords *coords);
extern int emgd_core_calc_sprite(struct drm_plane *_plane,
		const struct drm_crtc *crtc, const struct drm_framebuffer *fb,
		struct emgd_plane_coords *coords);

extern struct drm_encoder *emgd_connector_best_encoder(
		struct drm_connector *connector);
extern bool intel_encoder_crtc_ok(struct drm_encoder *encoder,
		struct drm_crtc *crtc);
extern void intel_update_watermarks(struct drm_device *dev);
extern int emgd_finish_fb(struct drm_framebuffer *old_fb);

extern void intel_modeset_commit_output_state(struct drm_device *dev);
extern void intel_modeset_update_staged_output_state(struct drm_device *dev);
extern void emgd_crtc_disable(struct drm_crtc *crtc);
extern void emgd_crtc_enable(struct drm_crtc *crtc);

extern int emgd_crtc_cursor_prepare(struct drm_crtc *crtc,
		struct drm_file *file_priv, uint32_t handle, uint32_t width,
		uint32_t height, struct drm_i915_gem_object **obj_ret,uint32_t *addr_ret);
extern void emgd_crtc_cursor_commit(struct drm_crtc *crtc, uint32_t handle,
		uint32_t width, uint32_t height, struct drm_i915_gem_object *obj, uint32_t addr);
extern void emgd_crtc_cursor_bo_unref(struct drm_crtc *crtc,
		struct drm_i915_gem_object *obj);

extern void intel_crtc_attach_properties(struct drm_crtc *crtc);
extern void intel_crtc_update_properties(struct drm_crtc *crtc);
extern void emgd_crtc_attach_properties(struct drm_crtc *crtc);
extern void emgd_crtc_update_properties(struct drm_crtc *crtc,
		uint32_t property_flag);

extern int emgd_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode,
		int x, int y, struct drm_framebuffer *old_fb);
extern int emgd_crtc_prepare_and_calc(struct drm_crtc *crtc, int x, int y,
		struct drm_framebuffer *old_fb);
extern void emgd_crtc_commit(struct drm_crtc *crtc);

struct emgd_plane_regs {
	unsigned long base_offset;
	unsigned long visible_offset;
	unsigned long tile_offset;
	unsigned long plane_control;
	unsigned long plane_reg;
	unsigned long stride;
	unsigned long stereo;

	bool is_mode_vga;

	unsigned char *mmio;
};

/**
 * convert_pf_drm_to_emgd()
 *
 * Converts a DRM pixel format to an EMGD pixel format.
 */
static inline unsigned long convert_pf_drm_to_emgd(int pf)
{
	switch(pf) {
	case DRM_FORMAT_RGB332:
		return IGD_PF_ARGB8_INDEXED;
	case DRM_FORMAT_RGB565:
		return IGD_PF_RGB16_565;
	case DRM_FORMAT_XRGB8888:
		return IGD_PF_xRGB32;
	case DRM_FORMAT_ARGB8888:
		return IGD_PF_ARGB32;
	case DRM_FORMAT_XBGR8888:
		return IGD_PF_xBGR32_8888;
	case DRM_FORMAT_ABGR8888:
		return IGD_PF_ABGR32_8888;
	case DRM_FORMAT_XRGB2101010:
		return IGD_PF_xRGB32_2101010;
	case DRM_FORMAT_ARGB2101010:
		return IGD_PF_ARGB32_2101010;
	case DRM_FORMAT_YUYV:
		return IGD_PF_YUV422_PACKED_YUY2;
	case DRM_FORMAT_UYVY:
		return IGD_PF_YUV422_PACKED_UYVY;
	case DRM_FORMAT_YVYU:
		return IGD_PF_YUV422_PACKED_YVYU;
	case DRM_FORMAT_VYUY:
		return IGD_PF_YUV422_PACKED_VYUY;
	default:
		EMGD_ERROR("Unknown pixel format 0x%x", pf);
		return IGD_PF_UNKNOWN;
	}
}


/**
 * convert_pf_emgd_to_drm()
 *
 * Converts a DRM pixel format to an EMGD pixel format.
 */
static inline int convert_pf_emgd_to_drm(unsigned long pf)
{
	switch(pf) {
	case IGD_PF_ARGB8_INDEXED:
		return DRM_FORMAT_RGB332;
	case IGD_PF_RGB16_565:
		return DRM_FORMAT_RGB565;
	case IGD_PF_xRGB32:
		return DRM_FORMAT_XRGB8888;
	case IGD_PF_ARGB32:
		return DRM_FORMAT_ARGB8888;
	case IGD_PF_xBGR32_8888:
		return DRM_FORMAT_XBGR8888;
	case IGD_PF_ABGR32_8888:
		return DRM_FORMAT_ABGR8888;
	/* 10 bits formats*/
	case IGD_PF_xRGB32_2101010:
		return DRM_FORMAT_XRGB2101010;
	case IGD_PF_xBGR32_2101010:
		return DRM_FORMAT_XBGR2101010;
	case IGD_PF_ARGB32_2101010:
		return DRM_FORMAT_ARGB2101010;
	case IGD_PF_ABGR32_2101010:
		return DRM_FORMAT_ABGR2101010;
	/* YUV422 - YUY packed formats*/
	case IGD_PF_YUV422_PACKED_YUY2:
		return DRM_FORMAT_YUYV;
	case IGD_PF_YUV422_PACKED_UYVY:
		return DRM_FORMAT_UYVY;
	case IGD_PF_YUV422_PACKED_YVYU:
		return DRM_FORMAT_YVYU;
	case IGD_PF_YUV422_PACKED_VYUY:
		return DRM_FORMAT_VYUY;
	/* unsupported formats */
	case IGD_PF_xBGR64_16161616F:
	case IGD_PF_ARGB64_16161616F:
	case IGD_PF_xRGB64_16161616F:
		EMGD_ERROR("64-bit deep color formats not supported by DRM yet");
		return -EINVAL;
	default:
		EMGD_ERROR("Unknown pixel format 0x%lx", pf);
		return -EINVAL;
	}
}

#endif
