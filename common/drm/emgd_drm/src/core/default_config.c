/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: default_config.c
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
 *  A file that contains the initial display configuration information of the
 *  EMGD kernel module.  A user can edit this file in order to affect the way
 *  that the kernel initially configures the displays.  This file is compiled
 *  into the EMGD kernel module.
 *-----------------------------------------------------------------------------
 */
#include "default_config.h"
#include "image_data.h"


/*
 * Splash screen and splash video functionality is mutually exclusive.
 * If splash_video_data->immediate is enabled then splash screen will not
 * be called. To enable splash screen ensure width and height is non-zero.
 */
static emgd_drm_splash_screen_t splash_screen_data = {
#ifdef CONFIG_DEBUG
	0x000000FF, /* bg_color - blue in debug so we can see it */
#else
	0x00000000, /* bg_color */
#endif
	0,   /* x */
	0,   /* y */
	0, /* width */
	0, /* height */
	image_data,
	sizeof(image_data),
};

static emgd_drm_splash_video_t splash_video_data = {
	0, /* built_in_test */
	0, /* immediate (as opposed to using sysfs)*/
	0, /* phys_contig_dram_start */
	/*IGD_PF_xRGB32,*/ /* igd_pixel_format */
	IGD_PF_YUV422_PACKED_YUY2, /* igd_pixel_format */
	256, /* src_width */
	256, /* src_height */
	512, /* src_pitch */
	100, /* dst_x */
	100, /* dst_y */
	0, /* dst_width */
	0, /* dst_height */
};

/*
 * The igd_param_t structure contains many configuration values used by the
 * EMGD kernel module.
 */
igd_param_t config_params[] = {
	{ /* config 1 */
		0,			/* Limit maximum pages usable by GEM (0 = no limit) */
		0,			/* Max frame buffer size (0 = no limit) */
		1,			/* Preserve registers (should be 1, so VT switches work and so
					 * that the console will be restored after X server exits).
					 */
		IGD_DISPLAY_MULTI_DVO | IGD_DISPLAY_DETECT,     /* Display flags (bitfield, where:
														 * IGD_DISPLAY_HOTPLUG
														 * IGD_DISPLAY_MULTI_DVO
														 * IGD_DISPLAY_DETECT
														 * IGD_DISPLAY_FB_BLEND_OVL
														 */
		{  /* Display port order, corresponds to the "portorder" module parameter
			*   Valid port types are:
			*   IGD_PORT_TYPE_TV
			*   IGD_PORT_TYPE_SDVOB
			*   IGD_PORT_TYPE_SDVOC
			*   IGD_PORT_TYPE_LVDS
			*   IGD_PORT_TYPE_ANALOG
			*   IGD_PORT_TYPE_DPB
			*   IGD_PORT_TYPE_DPC
			*/
			IGD_PORT_TYPE_ANALOG,
			IGD_PORT_TYPE_SDVOC,
			IGD_PORT_TYPE_SDVOB,
			IGD_PORT_TYPE_DPB,
			IGD_PORT_TYPE_DPC,
			0,
			0
		},
		{		/* Display Params: */
			{     /* Port:2 */
				2,				/* Display port number (0 if not configured) */
				0x140,			/* Parameters present (see above) */
				0x1,			/* EDID flag */
				0x3,			/* Flags when EDID is available (see above) */
				0x1,			/* Flags when EDID is not available (see above) */
				0,				/* DDC GPIO pins */
				0,				/* DDC speed */
				0,				/* DDC DAB */
				0,				/* I2C GPIO pins */
				0,				/* I2C speed */
				0,				/* I2C DAB */
				{				/* Flat Panel Info: */
					0,			/* Flat Panel width */
					0,			/* Flat Panel height */
					0,			/* Flat Panel power method */
					0,			/* eDP T3 (EMGD T1): VDD active & DVO clock/data active */
					0,			/* eDP T7 (EMGD T2): DVO clock/data active & backlight enable */
					0,			/* eDP T9 (EMGD T3): backlight disable & DVO clock/data inactive */
					0,			/* eDP T10 (EMGD T4): DVO clock/data inactive & VDD inactive */
					0			/* eDP T12 (EMGD T5): VDD inactive & VDD active */
				},
				{		 /* DTD Info */
					0,   /* sizeof(dtd_config1_port2_dtdlist)/sizeof(igd_display_info_t), --> number of DTD's */
					NULL /* dtd_config1_port2_dtdlist --> DTD name */
				},
				{		 /* Attribute Info */
					0,   /* sizeof(attrs_config1_port2)/sizeof(igd_param_attr_t), --> number or attrs */
					NULL /* attrs_config1_port2 --> Attr name */
				}
			},
			{     /* Port:3 */
				3,	/* Display port number (0 if not configured) */
				0x140,
				0x1, 0x3, 0x1, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
			{     /* Port:5 */
				5,	/* Display port number (0 if not configured) */
				0x140,
				0x1, 0x3, 0x1, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
			{	/* Port: */
				0,				/* Display port number (0 if not configured) */
				0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
			{	/* Port: */
				0,				/* Display port number (0 if not configured) */
				0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
			{	/* Port: */
				0,				/* Display port number (0 if not configured) */
				0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
			{	/* Port: */
				0,				/* Display port number (0 if not configured) */
				0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
				{ 0, NULL },
				{ 0, NULL }
			},
		},
		0, /* Quickboot flags (refer to header or docs for more info) */
		0, /* Quickboot seamless (1 = enabled) */
		0, /* Quickboot video input (1 = enabled) */
		0, /* Quickboot splash (1 = enabled) */
	},
};


emgd_fb_config_t initial_fbs[] = {
	{   /* frame buffer 0 */
		1280,    /* frame buffer width */
		2048,    /* frame buffer height */
		24,   /* frame buffer depdth */
		32    /* frame buffer bpp */
	},
	{   /* frame buffer 1 */
		1280,    /* frame buffer width */
		1024,    /* frame buffer height */
		24,   /* frame buffer depdth */
		32    /* frame buffer bpp */
	},
};

emgd_ovl_config_t initial_ovl_config = {
	/* emgd_drm_ovl_plane_config_t array X 4 - known defaults*/
	{
		/* Sprite-A */
		{
			{
				/* igd_ovl_gamma_info          */
				/*******************************/
				0x100,/* unsigned int red;     */
				0x100,/* unsigned int green;   */
				0x100,/* unsigned int blue;    */
				IGD_OVL_GAMMA_DISABLE/* unsigned int flags;*/
			},
			{
				/* igd_ovl_color_correct_info_t                  */
				/*************************************************/
				MID_BRIGHTNESS_YUV,/* signed short brightness;   */
				MID_CONTRAST_YUV,  /* unsigned short contrast;   */
				MID_SATURATION_YUV,/* unsigned short saturation; */
				MID_HUE_YUV        /* unsigned short hue;        */
			}
		},
		/* Sprite-B */
		{
			{
				/* igd_ovl_gamma_info          */
				/*******************************/
				0x100,/* unsigned int red;     */
				0x100,/* unsigned int green;   */
				0x100,/* unsigned int blue;    */
				IGD_OVL_GAMMA_DISABLE/* unsigned int flags;*/
			},
			{
				/* igd_ovl_color_correct_info_t                  */
				/*************************************************/
				MID_BRIGHTNESS_YUV,/* signed short brightness;   */
				MID_CONTRAST_YUV,  /* unsigned short contrast;   */
				MID_SATURATION_YUV,/* unsigned short saturation; */
				MID_HUE_YUV        /* unsigned short hue;        */
			}
		},
		/* Sprite-C */
		{
			{
				/* igd_ovl_gamma_info          */
				/*******************************/
				0x100,/* unsigned int red;     */
				0x100,/* unsigned int green;   */
				0x100,/* unsigned int blue;    */
				IGD_OVL_GAMMA_DISABLE/* unsigned int flags;*/
			},
			{
				/* igd_ovl_color_correct_info_t                  */
				/*************************************************/
				MID_BRIGHTNESS_YUV,/* signed short brightness;   */
				MID_CONTRAST_YUV,  /* unsigned short contrast;   */
				MID_SATURATION_YUV,/* unsigned short saturation; */
				MID_HUE_YUV        /* unsigned short hue;        */
			}
		},
		/* Sprite-D */
		{
			{
				/* igd_ovl_gamma_info          */
				/*******************************/
				0x100,/* unsigned int red;     */
				0x100,/* unsigned int green;   */
				0x100,/* unsigned int blue;    */
				IGD_OVL_GAMMA_DISABLE/* unsigned int flags;*/
			},
			{
				/* igd_ovl_color_correct_info_t                  */
				/*************************************************/
				MID_BRIGHTNESS_YUV,/* signed short brightness;   */
				MID_CONTRAST_YUV,  /* unsigned short contrast;   */
				MID_SATURATION_YUV,/* unsigned short saturation; */
				MID_HUE_YUV        /* unsigned short hue;        */
			}
		}
	},
	/* igd_ovl_pipeblend_t X 1 - known defaults */
	{
		/* Pipe A */
		{
				IGD_OVL_ZORDER_DEFAULT, /* unsigned long zorder_combined*/
				0, /* unsigned long fb_blend_ovl*/
				0, /* unsigned long has_const_alpha*/
				0, /* unsigned long const_alpha*/
				(IGD_OVL_PIPEBLEND_SET_FBBLEND | IGD_OVL_PIPEBLEND_SET_ZORDER |
					IGD_OVL_PIPEBLEND_SET_CONSTALPHA) /* unsigned long enable_flags*/
		},
		/* Pipe B */
		{
				IGD_OVL_ZORDER_DEFAULT, /* unsigned long zorder_combined*/
				0, /* unsigned long fb_blend_ovl*/
				0, /* unsigned long has_const_alpha*/
				0, /* unsigned long const_alpha*/
				(IGD_OVL_PIPEBLEND_SET_FBBLEND | IGD_OVL_PIPEBLEND_SET_ZORDER |
					IGD_OVL_PIPEBLEND_SET_CONSTALPHA) /* unsigned long enable_flags*/
		}
	}
};

emgd_crtc_config_t crtcs[] = {
	{   /* CRTC 0 */
		1,  /* zero-based index into frame buffer array above to associate with this crtc */
		1280,  /* Display width used to find initial display timings */
		1024,  /* Display height used to find initial display timings */
		60, /* Display vertical refresh rate to use */
		0,  /* X offset */
		0,  /* Y offset */
	},
	{   /* CRTC 1 */
		1,  /* zero-based index into frame buffer array above to associate with this crtc */
		1024,  /* Display width used to find initial display timings */
		768,  /* Display height used to find initial display timings */
		60, /* Display vertical refresh rate to use */
		0,  /* X offset */
		0,  /* Y offset */
	},
};

/*
 * The emgd_drm_config_t structure is the main configuration structure
 * for the EMGD kernel module.
 */
emgd_drm_config_t default_config_drm = {
	2,
	crtcs,
	2,
	initial_fbs,
	&initial_ovl_config,/* overlay plane initial configuration (can be NULL) */
	&splash_screen_data,
	&splash_video_data,
	1,
	config_params,	/* driver parameters from above */
	0,
};

emgd_drm_config_t *config_drm;
