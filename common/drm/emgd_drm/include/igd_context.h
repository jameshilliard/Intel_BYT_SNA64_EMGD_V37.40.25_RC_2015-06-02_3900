/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_context.h
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

#ifndef _IGD_CONTEXT_H
#define _IGD_CONTEXT_H

#define MAX_VBLANK_STORE 5 
/*
 * IGD device context contains device information that should exist in
 * some form across all supported platforms.
 *
 * Device information that is specific to any individual platform/core
 * etc should be in the "platform_context" which is hardware specific.
 *
 * In the event that any given core has a large number of marketing
 * SKUs that change the PCI device ID, the device ID in this structure
 * may be overwritten with a single common ID and not truly reflect
 * the actual ID.  This prevents polluting the function tables with
 * dozens of logically identical chipsets.
 *
 */
typedef struct _device_context {
	unsigned long power_state;
	unsigned long long mmadr;		/* Primary mmio address */
	unsigned char *virt_mmadr;
	unsigned long gtt_size;     /* GTT Table size i.e. num of GTT entries */
	unsigned long long gttadr;		/* Primary mmio address */
	unsigned long *virt_gttadr; /* was gtt_mmap */
	unsigned long gatt_pages;   /* Number of pages addressable by GTT */
	unsigned long stolen_pages; /* Number of pages of stolen memory */
	unsigned long gmch_ctl;     /* GMCH control value */
	union {
		unsigned long long fb_adr;  	/* Video Memory address (GMADR for devices that work)*/
		unsigned long long gma_bus_addr;/*it is physical address*/
	};
	unsigned short did;         /* Device ID for main video device */
	unsigned long rid;          /* Device revision ID for main video device */
	unsigned short bid;			/* Device ID for Bridge */
	unsigned long max_dclk;     /* maximum dotclock of all the chipset */
	unsigned long hw_config;    /* HW Config parameter bits (see igd_init.h) */
	unsigned long hw_status_offset; /* Hw status page offset */
	unsigned short core_display_freq;	/* CDCLK Core DISPLAY Frequency, calculate GMBUS, PWM, MAX Pixel rate */
	unsigned short core_z_freq;   /* CZCLK */
	/* Added for SNB */
	union {
		unsigned char *gtt_mapping;/*it is physical address*/
	};

} device_context_t;

typedef void * platform_context_t;
typedef void * igd_context_h;

typedef enum _reg_state_id {
	REG_MODE_STATE = 1
} reg_state_id_t;

/* module_state handle */
typedef struct _module_state *module_state_h;

typedef void * igd_appcontext_h;
	/* need to eventually change this to a GEM client context */


/**
 * This holds information about a CRTC.
 */
typedef struct _emgd_crtc {
	struct drm_crtc         base;

	int                     crtc_id;

	igd_display_pipe_t     *igd_pipe;
	emgd_framebuffer_t     *fbdev_fb;
	igd_display_plane_t    *igd_plane;
	igd_cursor_t           *igd_cursor;
	igd_context_h           igd_context;

	int                     ports[IGD_MAX_PORTS];  /* Port numbers */
	struct drm_mode_set     mode_set;
	struct drm_display_mode saved_mode;
	struct drm_display_mode saved_adjusted_mode;
	unsigned char           lut_r[256];
	unsigned char           lut_g[256];
	unsigned char           lut_b[256];
	unsigned char           lut_a[256];
	unsigned char           default_lut_r[256];
	unsigned char           default_lut_g[256];
	unsigned char           default_lut_b[256];
	unsigned char           default_lut_a[256];

	/* Store vblank timestamp in crtc */
        int track_index; /* track index using update vblank timestamp */
        struct timeval vblank_timestamp[MAX_VBLANK_STORE];

	/* Flip request work task */
	emgd_page_flip_work_t  *flip_pending;

	/* Flip request work task for sprite */
   	/* Sprite flipping is mainly use for stereo 3D */
   	emgd_page_flip_work_t *sprite_flip_pending;

	/*
	 * Are we waiting for the next vblank event to perform flip cleanup
	 * on this CRTC?  Flip cleanup primarily involves sending a
	 * notification event back to userspace.
	 */
	unsigned char vblank_expected;

	/* Userspace event to send back upon flip completion. */
	struct drm_pending_vblank_event *flip_event;

	/* OTC members */
	enum pipe pipe;
	enum plane plane;
	int dpms_mode;
	bool active; /* is the crtc on? independent of the dpms mode */
	bool primary_disabled; /* is the crtc obscured by a plane? */
	bool lowfreq_avail;
	struct intel_overlay *overlay;
	struct intel_unpin_work *unpin_work;
	atomic_t unpin_work_count;
	int fdi_lanes;
	struct drm_i915_gem_object *cursor_bo;
	uint32_t cursor_addr;
	uint32_t cursor_handle;
	int16_t cursor_x, cursor_y;
	int16_t cursor_width, cursor_height;
	bool cursor_visible;
	unsigned int bpp;

	bool no_pll; /* tertiary pipe for IVB */
	bool use_pll_a;
	/* End of OTC members */

	/*Sprites and z-order (plus FB_BLEND_OVL)*/
	unsigned long zorder;
	uint32_t fb_blend_ovl_on;
	igd_ovl_plane_t *sprite1;
	igd_ovl_plane_t *sprite2;

	/* Information needed when locking plane access */
	int freeze_state;
	struct _emgd_crtc *freeze_crtc;
	emgd_framebuffer_t *freeze_fb;
	int unfreeze_xoffset;
	int unfreeze_yoffset;
	int freeze_pitch;
	int freezed_pitch;
	int freezed_tiled;
	struct drm_i915_gem_object *freeze_bo;

	struct emgd_obj *sysfs_obj;
	int palette_persistence;
	void *primary_regs;
#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	struct drm_flip_helper flip_helper;
#endif
	wait_queue_head_t vbl_wait;
	bool vbl_received;
	struct list_head pending_flips;

	/* To save the current s3d_mode in CRTC */
	unsigned long current_s3d_mode;

} emgd_crtc_t, igd_display_context_t;
typedef struct emgd_crtc_t * igd_display_h;


#include <igd_dispatch_module.h>
#include <igd_dispatch_drv.h>
#include <igd_dispatch_mode.h>
#include <igd_dispatch_pwr.h>

struct _igd_context {
	int igd_device_error_no; /* Values are already negative */
	igd_dispatch_t drv_dispatch;
	igd_mode_dispatch_t * mode_dispatch;
	igd_pwr_dispatch_t * pwr_dispatch;
	igd_module_dispatch_t module_dispatch;
	mode_context_t mode_context;
	device_context_t device_context;     /* Hardware independent */
	platform_context_t platform_context; /* Hardware dependent   */
	struct mutex gmbus_lock;
	void *drm_dev;
};

#endif /*_IGD_CONTEXT_H*/
