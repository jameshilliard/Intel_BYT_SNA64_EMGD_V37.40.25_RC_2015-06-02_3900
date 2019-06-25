/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: default_config.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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
 *  Data structure containing the initial display configuration information of
 *  the EMGD kernel module.
 *-----------------------------------------------------------------------------
 */
#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CONFIG_VERSION 1
#define USING_OLD_CONFIG 1

#include "igd_display_pipeline.h"
#include "igd_display_info.h"
#include "igd_init.h"
#include "igd_ovl.h"
#include "igd_pd.h"
#include "igd_mode.h"


/*
 * Splash Screen data provided by the user.
 */
typedef struct _emgd_drm_splash_screen {
	unsigned long bg_color;
	unsigned long x;
	unsigned long y;
	unsigned long width;
	unsigned long height;
} emgd_drm_splash_screen_t;

/*
 * Splash Video data provided by the user.
 */
typedef struct _emgd_drm_splash_video {
	unsigned long built_in_test;
	unsigned long immediate;
		/* if "immediate" is set, then the splash video
		 * gets turned on immediately, else it will be
		 * turned on later via a sysfs attribute store function
		 * for "splashvideo"
		 */
	long long     phys_contig_dram_start;
		/* we'll get GEM to map in these contiguous DRAM phys
		 * pages into the GTT for us and that means the gfx offset
		 * can vary - this is NOT required for "built_in_test" above
		 */
	unsigned long igd_pixel_format;
		/* this is an EMGD pixel format - take note for built_in_test,
		 * we only support either YUY2 or XRGB32 for now */
	unsigned long src_width;
	unsigned long src_height;
	unsigned long src_pitch;
		/* for splash video, we assume this will be for external camera
		 * DMA-ing some data, in which case it should be linear, not tiled.
		 * Take NOTE: Our hardware has 64byte alignment requirement on sprite
		 * The external FPGA DMA-ing needs to accomodate for that in scanline
		 * size and the phys_contig_dram_start needs to ensure the size
		 * of the allocated pages are also in accordance
		 */
	unsigned long dst_x;
	unsigned long dst_y;
	unsigned long dst_width;
	unsigned long dst_height;
} emgd_drm_splash_video_t;


/**
 * User-configurable parameters for a single CRTC. This configurs the
 * frame buffer settings and the timings for a CRTC. Setting width and
 * height values to zero will cause the system to auto detect the best
 * fit values.
 */
typedef struct _emgd_crtc_config {
	int framebuffer_id; /* frame buffer index */
	int display_width;  /* horizontal active */
	int display_height; /* vertical active */
	int refresh;        /* vertical refresh rate (Hz) */
	int x_offset;       /* X offset of display into the frame buffer */
	int y_offset;       /* X offset of display into the frame buffer */
} emgd_crtc_config_t;

/**
 * User-configurable parameters for a frame buffer.
 */
typedef struct _emgd_fb_config {
	int width;     /* frame buffer width */
	int height;    /* frame buffer height */
	int depth;     /* frame buffer color depth */
	int bpp;       /* frame buffer bits per pixel */
} emgd_fb_config_t;

/**
 * User-configurable parameters for the sprite planes. Depending on the
 * hardware in use, there can be 2 or 4 sprite planes. For SNB, there
 * are 2 sprites and on VLV there are 4 sprites. SNB doesnt have color
 * correction control in the form of brightness, contrast, saturation
 * and hue. VLV does have those. SNB does however have gamma correction
 * capability which could be used to configure some level of brightness.
 * contrast and saturation. However, at the moment, EMGD stack doesnt
 * support any form of color correction on SNB even via gamma correction.
 * On VLV however, there are dedicated controls for brightness, contrast,
 * saturation and hue. VLV also has gamma correction support. VLV has
 * additional pipe-blending controls like inter-plane alpha blending (a.k.a.
 * FB-Blend-Ovl), constant alpha for the plane and sprite-display planes
 * Z-Order settings.
 */
typedef struct _emgd_ovl_plane_config {
	igd_ovl_gamma_info_t gamma;
	igd_ovl_color_correct_info_t color_correct;
	/* Refer to egd_drm/include/igd_ovl.h for these 2 structure definition
	 * along with the various members and flags. It allows the control
	 * of one ore more of the following config attrs:
	 *  - brightness, contrast, saturation, hue
	 *  - gamma correction (per channel: RGB)
	 */
} emgd_ovl_plane_config_t;

typedef struct _emgd_ovl_config {
	emgd_ovl_plane_config_t ovl_configs[4];
		/* one for each plane, for plane_config[x],
		 * x = 0 --> SpriteA for all chips
		 * x = 1 --> SpriteB for all chips
		 * x = 2 --> SpriteC for VLV only
		 * x = 3 --> SpriteD for VLV only
		 */
	igd_ovl_pipeblend_forinit_t ovl_pipeblend[2];
		/* one for each pipe, for ovl_pipeblend[x],
		 * x = 0 --> PipeA
		 * x = 1 --> PipeB
		 */
		/* Refer to egd_drm/include/igd_ovl.h for this structure definition
		 * along with the various members and flags. It allows the control
		 * of one ore more of the following config attrs:
		 *  - Z-Order of sprite and display planes for all pipes
		 *  - Enablement of inter-plane alpha blending (Fb-Blend-Ovl)
		 *  - Enablement / setting for constant alpha pre-mult of sprite
		 */
} emgd_ovl_config_t;


/**
 * User-configurable parameters.  This structure is the basis for the
 * user_config.c" file, which allows compile-time customization of the EMGD DRM
 * module.
 *
 * Besides the igd_param_t values, the other options in this structure
 * correspond to EMGD module parameters of the same name.  Most are only
 * applicable if the init option is non-zero.  There is one additional module
 * parameter ("portorder") that corresponds to the port_order member of the
 * igd_param_t structure.
 */
typedef struct _emgd_drm_config {
	/** The CRTC configurations to use. */
	emgd_crtc_config_t crtcs[2];
	/** The Overlay plane configurations to use. */
	emgd_ovl_config_t * ovl_config;
	/** The splash screen data if specified by the user. */
	emgd_drm_splash_screen_t *ss_data;
	/** The splash video data if specified by the user. */
	emgd_drm_splash_video_t *sv_data;
	/** Array of other parameters (one per configid), used by the hardware
	 * abstraction layer code.
	 */
	igd_param_t **hal_params;
} emgd_drm_config_t;

#endif
