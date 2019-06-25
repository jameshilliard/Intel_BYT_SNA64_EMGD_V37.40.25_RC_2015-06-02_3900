/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: dsp_snb.c
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
 *  This file contains all the necessary functions for display resource
 *  manager. This module abstracts all hardware resources and manages them.
 *-----------------------------------------------------------------------------
 */

#include <io.h>
#include <memory.h>

#include <igd_core_structs.h>

#include <gn6/regs.h>
#include <gn6/context.h>
#include "regs_common.h"
#include "drm_emgd_private.h"

#include "../cmn/dsp_dispatch.h"

/*
 * The kernel DRM code does not support 64-bit deep color formats at this
 * time, so we don't want to advertise them as valid overlay/sprite formats.
 *
 * Switch the following to a #define to start advertising them again.
 */
#undef ADVERTISE_64BIT_DEEPCOLOR

/* Macros to selectively remove pixel formats ONLY for VBIOS
 * but available in driver. This is done because
 * 1. No USAGE for these pixel fomats in VBIOS
 * 2. Save some size in VBIOS where we are running out of space
 */
#ifdef CONFIG_MICRO   /* VBIOS */

#define SPRITE_PIXEL_FORMAT_SNB            NULL /* No use for these tables */
#define CURSOR_PIXEL_FORMAT_SNB            NULL
#define RENDER_PIXEL_FORMAT_SNB            NULL
#define TEXTURE_PIXEL_FORMAT_SNB           NULL
#define OVERLAY_PIXEL_FORMAT_SNB           NULL

#else  /* driver */

#define SPRITE_PIXEL_FORMAT_SNB            sprite_pixel_formats_snb
#define CURSOR_PIXEL_FORMAT_SNB            cursor_pixel_formats_snb
#define RENDER_PIXEL_FORMAT_SNB            render_pixel_formats_snb
#define TEXTURE_PIXEL_FORMAT_SNB           texture_pixel_formats_snb
#define OVERLAY_PIXEL_FORMAT_SNB           overlay_pixel_formats_snb

#endif

#if defined(CONFIG_SNB)
static int dsp_init_snb(igd_context_t *context);
#endif

/*
 * NOTE: Some of these format lists are shared with GMM. For this reason
 * they cannot be static.
 */
/* FIXME(SNB): Value based on GN4, need to be checked for SNB */
unsigned long fb_pixel_formats_snb[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ABGR32,
	IGD_PF_ARGB32_2101010,
	IGD_PF_RGB16_565,
	IGD_PF_ARGB8_INDEXED,
	0
};

unsigned long vga_pixel_formats_snb[] = {
	IGD_PF_ARGB8_INDEXED,
	0
};

#ifndef CONFIG_MICRO /* Not needed for VBIOS */
unsigned long sprite_pixel_formats_snb[] = {
	IGD_PF_xRGB32,
	IGD_PF_xBGR32,
	IGD_PF_xRGB32_2101010,
	IGD_PF_xBGR32_2101010,
#ifdef ADVERTISE_64BIT_DEEPCOLOR
	IGD_PF_xRGB64_16161616F,
	IGD_PF_xBGR64_16161616F,
#endif
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	0
};

unsigned long render_pixel_formats_snb[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ARGB32_2101010,
	IGD_PF_RGB16_565,
	IGD_PF_xRGB16_555,
	IGD_PF_ARGB16_1555,
	IGD_PF_ARGB16_4444,
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	IGD_PF_GR32_1616,
	IGD_PF_R16F,/* Adding Floating-Point formats */
	IGD_PF_GR32_1616F,
	IGD_PF_R32F,
	IGD_PF_ABGR64_16161616F,
	0
};

unsigned long texture_pixel_formats_snb[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ABGR32,
	IGD_PF_xBGR32,
	IGD_PF_RGB16_565,
	IGD_PF_xRGB16_555,
	IGD_PF_ARGB16_1555,
	IGD_PF_ARGB16_4444,
	IGD_PF_ARGB8_INDEXED,
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	IGD_PF_YUV420_PLANAR_I420,
	IGD_PF_YUV420_PLANAR_IYUV,
	IGD_PF_YUV420_PLANAR_YV12,
	IGD_PF_YUV410_PLANAR_YVU9,
	IGD_PF_DVDU_88,      /* Bump Pixel Formats*/
	IGD_PF_LDVDU_655,
	IGD_PF_xLDVDU_8888,
	IGD_PF_DXT1,
	IGD_PF_DXT2,
	IGD_PF_DXT3,
	IGD_PF_DXT4,
	IGD_PF_DXT5,
	IGD_PF_L8,
	IGD_PF_A8,    /*adding A8*/
	IGD_PF_AL88,
	IGD_PF_AI44,
	IGD_PF_L16,
	IGD_PF_ARGB32_2101010,
	IGD_PF_AWVU32_2101010,
	IGD_PF_QWVU32_8888,
	IGD_PF_GR32_1616,
	IGD_PF_VU32_1616,
	IGD_PF_R16F, /* Adding Floating-Point formats */
	IGD_PF_GR32_1616F,
	IGD_PF_R32F,
	IGD_PF_ABGR64_16161616F, /* 64-bit FP format for textures */
	0
};

unsigned long depth_pixel_formats_snb[] = {
	IGD_PF_Z16,
	IGD_PF_Z24,
	IGD_PF_S8Z24,
	0
};


unsigned long cursor_pixel_formats_snb[] = {
	IGD_PF_ARGB32,
	IGD_PF_RGB_2,
	IGD_PF_RGB_XOR_2,
	IGD_PF_RGB_T_2,
	0
};

/* No Overlay in SNB */
/*
unsigned long overlay_pixel_formats_snb[] = {
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	IGD_PF_YUV420_PLANAR_I420,
	IGD_PF_YUV420_PLANAR_IYUV,
	IGD_PF_YUV420_PLANAR_YV12,
	IGD_PF_YUV410_PLANAR_YVU9,
	0
};
*/

unsigned long blt_pixel_formats_snb[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ABGR32,
	IGD_PF_xBGR32,
	IGD_PF_RGB16_565,
	IGD_PF_xRGB16_555,
	IGD_PF_ARGB16_1555,
	IGD_PF_ARGB16_4444,
	IGD_PF_ARGB8_INDEXED,
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	IGD_PF_YUV420_PLANAR_I420,
	IGD_PF_YUV420_PLANAR_IYUV,
	IGD_PF_YUV420_PLANAR_YV12,
	IGD_PF_YUV410_PLANAR_YVU9,
	IGD_PF_DVDU_88,      /*Bump Pixel Formats*/
	IGD_PF_LDVDU_655,
	IGD_PF_xLDVDU_8888,
	IGD_PF_DXT1,
	IGD_PF_DXT2,
	IGD_PF_DXT3,
	IGD_PF_DXT4,
	IGD_PF_DXT5,
	IGD_PF_Z16,
	IGD_PF_Z24,
	IGD_PF_S8Z24,
	IGD_PF_RGB_2,
	IGD_PF_RGB_XOR_2,
	IGD_PF_RGB_T_2,
	IGD_PF_L8,
	IGD_PF_A8,	/*Adding A8 */
	IGD_PF_AL88,
	IGD_PF_AI44,
	IGD_PF_L16,
	IGD_PF_ARGB32_2101010,
	IGD_PF_AWVU32_2101010,
	IGD_PF_QWVU32_8888,
	IGD_PF_GR32_1616,
	IGD_PF_VU32_1616,
	IGD_PF_R16F, /* Adding Floating-Point formats */
	IGD_PF_GR32_1616F,
	IGD_PF_R32F,
	IGD_PF_ABGR64_16161616F,
	0
};

#endif /*  ifndef CONFIG_MICRO */

/*
 * Plane Definitions for Gen4 family.
 */
#if defined(CONFIG_SNB)
static igd_display_plane_t planea_snb = {
	DSPACNTR, 
	IGD_PLANE_IS_PLANEA | IGD_PLANE_DISPLAY | IGD_PLANE_DOUBLE | IGD_PLANE_DIH, 0, 0,
	fb_pixel_formats_snb, NULL, NULL
};

static igd_display_plane_t planeb_snb = {
	DSPBCNTR,
	IGD_PLANE_IS_PLANEB | IGD_PLANE_DISPLAY | IGD_PLANE_SPRITE | IGD_PLANE_DIH, 0, 0,
	fb_pixel_formats_snb, NULL, NULL
};

static igd_display_plane_t plane_vga_snb = {
	VGACNTRL, IGD_PLANE_VGA, 0, 0,
	vga_pixel_formats_snb, NULL, NULL
};
#endif

#if defined(CONFIG_SNB)
static igd_display_plane_t plane_cursora_snb = {
	CUR_A_CNTR, IGD_PLANE_CURSOR|IGD_CURSOR_USE_PIPEA, 0, 0,
	CURSOR_PIXEL_FORMAT_SNB, NULL, NULL
};

static igd_display_plane_t plane_cursorb_snb = {
	CUR_B_CNTR, IGD_PLANE_CURSOR|IGD_CURSOR_USE_PIPEB, 0, 0,
	CURSOR_PIXEL_FORMAT_SNB, NULL, NULL
};
#endif

#if defined(CONFIG_SNB)
	/* SNB has 2 Video Sprite
	 * A and B with scaling capabilities and limited pixel formats */
static igd_display_plane_t videoa_snb = {
	DVSACNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_SNB, NULL, NULL
};
static igd_display_plane_t videob_snb = {
	DVSBCNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_SNB, NULL, NULL
};
#endif

/*
 * Plane lists for SNB family members.
 */
#if defined(CONFIG_SNB)
/* Two Main Plane, Two Video Sprite, One VGA, Two Cursor */
static igd_display_plane_t *plane_table_snb[] = {
	&planea_snb,
	&planeb_snb,
	&plane_vga_snb,
	&videoa_snb,
	&videob_snb,
	&plane_cursora_snb,
	&plane_cursorb_snb,
	NULL
};
#endif

#if defined(CONFIG_SNB)
static igd_clock_t clock_a_snb = {
	PCH_DPLL_A, PCH_FPA0, 16
};
#endif

#if defined(CONFIG_SNB)
static igd_clock_t clock_b_snb = {
	PCH_DPLL_B, PCH_FPB0, 16
};
#endif

/*
 * Pipe definitions for Gen4 family.
 * Note: Gen4 does not support Double Wide modes.
 */
#if defined(CONFIG_SNB)
igd_display_pipe_t pipea_snb = {
	0, _PIPEACONF, PIPEA_TIMINGS, LGC_PALETTE_A, &clock_a_snb,
	(IGD_PIPE_IS_PIPEA | IGD_PORT_SHARE_ANALOG | IGD_PORT_SHARE_DIGITAL),
	0, 0, NULL, NULL, NULL, 0
};

igd_display_pipe_t pipeb_snb = {
	1, _PIPEBCONF, PIPEB_TIMINGS, LGC_PALETTE_B, &clock_b_snb,
	(IGD_PIPE_IS_PIPEB | IGD_PORT_SHARE_ANALOG | IGD_PORT_SHARE_DIGITAL),
	0, 0, NULL, NULL, NULL, 0
};
#endif

#if defined(CONFIG_SNB)
static igd_display_pipe_t *pipe_table_snb[] = {
	&pipea_snb,
	&pipeb_snb,
	NULL
};
#endif

/*
 * Port definitions for Gen4 family.
 */

/* Port number: Port number is from 1-number of available ports on any hardware.
 * Here are the definitions:
 *
 * On Gen4:
 * =======
 * Port mappings:
 *   1 - Int-TV (depending on sku)
 *   2 - SDVOB/HDMIB/DPB port
 *   3 -       HDMIC/DPC port
 *   4 - Int-LVDS (depending on sku)
 *   5 - Analog port
 *   New additions???-->
 *   -------------------
 *   6 - eDP (not on PCH- on CPU side)
 *
 * Note: Port number should match with port numbers in port parameters.
 *       See igd_init.h for more information.
 */


/*
 * These are the port attributes that the SNB core support.
 * Note that currently it only contains color correction attributes.
 * Eventually, this will include all the attributes.
 */
igd_attr_t port_attrib_snb[IGD_MAX_PORTS][5] = {
	{ /* Config for port 1:  Integrated TV Encoder (Alviso only) */
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_GAMMA,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Gamma",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x202020,  /* default */
			0x202020,  /* current */
			0x131313,  /* Min:  ~0.6 in 3i.5f format for R-G-B*/
			0xC0C0C0,  /* Max:  6 in 3i.5f format for R-G-B   */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_BRIGHTNESS,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Brightness",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_CONTRAST,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Contrast",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_EXTENSION,
			0,
			"",
			PD_ATTR_FLAG_PD_INVISIBLE|PD_ATTR_FLAG_USER_INVISIBLE,
			0,
			0,
			0,
			0,
			0),
		PD_MAKE_ATTR(
			PD_ATTR_LIST_END,
			0,
			"",
			0,
			0,
			0,
			0,
			0,
			0)
	},
	{ /* Config for port 2:  SDVO-B / HDMI-DVI-B */
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_GAMMA,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Gamma",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x202020,  /* default */
			0x202020,  /* current */
			0x131313,  /* Min:  ~0.6 in 3i.5f format for R-G-B*/
			0xC0C0C0,  /* Max:  6 in 3i.5f format for R-G-B   */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_BRIGHTNESS,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Brightness",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_CONTRAST,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Contrast",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_EXTENSION,
			0,
			"",
			PD_ATTR_FLAG_PD_INVISIBLE|PD_ATTR_FLAG_USER_INVISIBLE,
			0,
			0,
			0,
			0,
			0),
		PD_MAKE_ATTR(
			PD_ATTR_LIST_END,
			0,
			"",
			0,
			0,
			0,
			0,
			0,
			0)
	},
	{ /* Config for port 3: HDMI-DVI-C (No SDVO on port C)*/
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_GAMMA,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Gamma",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x202020,  /* default */
			0x202020,  /* current */
			0x131313,  /* Min:  ~0.6 in 3i.5f format for R-G-B*/
			0xC0C0C0,  /* Max:  6 in 3i.5f format for R-G-B   */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_BRIGHTNESS,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Brightness",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_CONTRAST,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Contrast",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_EXTENSION,
			0,
			"",
			PD_ATTR_FLAG_PD_INVISIBLE|PD_ATTR_FLAG_USER_INVISIBLE,
			0,
			0,
			0,
			0,
			0),
		PD_MAKE_ATTR(
			PD_ATTR_LIST_END,
			0,
			"",
			0,
			0,
			0,
			0,
			0,
			0)
	},
	{ /* Config for port 4:  Int-LVDS */
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_GAMMA,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Gamma",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x202020,  /* default */
			0x202020,  /* current */
			0x131313,  /* Min:  ~0.6 in 3i.5f format for R-G-B*/
			0xC0C0C0,  /* Max:  6 in 3i.5f format for R-G-B   */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_BRIGHTNESS,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Brightness",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_CONTRAST,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Contrast",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_EXTENSION,
			0,
			"",
			PD_ATTR_FLAG_PD_INVISIBLE|PD_ATTR_FLAG_USER_INVISIBLE,
			0,
			0,
			0,
			0,
			0),
		PD_MAKE_ATTR(
			PD_ATTR_LIST_END,
			0,
			"",
			0,
			0,
			0,
			0,
			0,
			0)
	},
	{ /* Config for port 5:  Int-ANALOG */
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_GAMMA,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Gamma",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x202020,  /* default */
			0x202020,  /* current */
			0x131313,  /* Min:  ~0.6 in 3i.5f format for R-G-B*/
			0xC0C0C0,  /* Max:  6 in 3i.5f format for R-G-B   */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_BRIGHTNESS,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Brightness",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_FB_CONTRAST,
			PD_ATTR_TYPE_RANGE,
			"Frame Buffer Contrast",
			PD_ATTR_FLAG_PD_INVISIBLE,
			0x808080,
			0x808080,
			0x000000,    /* Min: */
			0xFFFFFF,    /* Max: */
			1),
		PD_MAKE_ATTR(
			PD_ATTR_ID_EXTENSION,
			0,
			"",
			PD_ATTR_FLAG_PD_INVISIBLE|PD_ATTR_FLAG_USER_INVISIBLE,
			0,
			0,
			0,
			0,
			0),
		PD_MAKE_ATTR(
			PD_ATTR_LIST_END,
			0,
			"",
			0,
			0,
			0,
			0,
			0,
			0)
	}
};


static igd_display_port_t analog_port_snb = {
	IGD_PORT_ANALOG, 5, "ANALOG", PCH_ADPA,
	0, 0, IGD_PARAM_GPIO_PAIR_ANALOG, 0xA0,
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_TV | IGD_PORT_SHARE_DIGITAL |
		IGD_VGA_COMPRESS | IGD_IS_PCH_PORT),
	DREFCLK, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 0,
	DDC_DEFAULT_SPEED, NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_snb[5 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};


/* take note that depending on SNB SKU, u may have CRT, Int-LVDS, Int-TV
 * or SDVO (B), HDMI-DVI (B/C/D) or DisplayPort (B/C/D) where these are all
 * sitting on PCH side (the SDVO can only operate on DigitalPortB, whereas
 * HDMI-DVI can operation on DigitalPortC/D. Also, take note that DisplayPort
 * uses the same physical lines as SDVO / HDMI-DVI ... so mutual exclusion.
 * There is also an eDP port - but that sits on CPU side, not PCH.
 */
static igd_display_port_t hdmib_port_snb = {
	IGD_PORT_DIGITAL, 2, "HDMI-B", HDMIB,
	IGD_PARAM_GPIO_PAIR_DIGITALB, 0xA0,
	IGD_PARAM_GPIO_PAIR_DIGITALB, 0xA0,
        /* Actually, for HDMI-B, we only use the DDC reg, slave-address and speed
         * We are filling in the I2c reg, slave-addr and speed just for completeness
         * This is actually decided by the implementation on the port driver,
         * which could differ from one chipset to another - but SNB now is DDC only
         */
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_DIGITAL | IGD_PORT_SHARE_ANALOG |
		IGD_VGA_COMPRESS | IGD_RGBA_COLOR | IGD_PORT_GANG |
		IGD_IS_PCH_PORT),
	DREFCLK /*SNB NO ext clk?*/, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 0, DDC_DEFAULT_SPEED,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_snb[2 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

static igd_display_port_t hdmic_port_snb = {
	IGD_PORT_DIGITAL, 3, "HDMI-C", HDMIC,
	IGD_PARAM_GPIO_PAIR_DIGITALC, 0xA0,
	IGD_PARAM_GPIO_PAIR_DIGITALC, 0xA0,
        /* Actually, for HDMI-C, we only use the DDC reg, slave-address and speed
         * We are filling in the I2c reg, slave-addr and speed just for completeness
         * This is actually decided by the implementation on the port driver,
         * which could differ from one chipset to another - but SNB now is DDC only
         */
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_DIGITAL | IGD_PORT_SHARE_ANALOG |
		IGD_VGA_COMPRESS | IGD_RGBA_ALPHA | IGD_PORT_GANG |
		IGD_IS_PCH_PORT),
	DREFCLK /*SNB NO ext clk?*/, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 0, DDC_DEFAULT_SPEED,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_snb[3 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

igd_display_port_t hdmid_port_snb = {
	IGD_PORT_DIGITAL, 1, "HDMI-D", HDMID,
	IGD_PARAM_GPIO_PAIR_DIGITALD, 0xA0,
	IGD_PARAM_GPIO_PAIR_DIGITALD, 0xA0,
        /* Actually, for HDMI-D, we only use the DDC reg, slave-address and speed
         * We are filling in the I2c reg, slave-addr and speed just for completeness
         * This is actually decided by the implementation on the port driver,
         * which could differ from one chipset to another - but SNB now is DDC only
         */
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_DIGITAL | IGD_PORT_SHARE_ANALOG |
		IGD_VGA_COMPRESS | IGD_RGBA_ALPHA | IGD_PORT_GANG |
		IGD_IS_PCH_PORT),
	DREFCLK /*SNB NO ext clk?*/, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 0, DDC_DEFAULT_SPEED,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_snb[1 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

/*
 * NOTE: Port B should come before Port C because the first sDVO device
 * is usually connected to Port B.
 */
#ifdef CONFIG_SNB
static igd_display_port_t *port_table_snb[] = {
	&analog_port_snb,
	&hdmib_port_snb,
	&hdmic_port_snb,	
	&hdmid_port_snb,	
	NULL
};
#endif


#if defined(CONFIG_SNB)
static igd_fb_caps_t caps_table_snb[] = {
	{IGD_PF_ARGB32, 0},
	{IGD_PF_xRGB32, 0},
	{IGD_PF_ABGR32, 0},
	{IGD_PF_xBGR32, 0},
	{IGD_PF_ARGB32_2101010, 0},
	{IGD_PF_RGB16_565, 0},
	{IGD_PF_ARGB8_INDEXED, 0},
	{0, 0}
};
#endif


#if defined(CONFIG_SNB)
dsp_dispatch_t dsp_dispatch_snb = {
	plane_table_snb, pipe_table_snb, port_table_snb, caps_table_snb,
	SPRITE_PIXEL_FORMAT_SNB, RENDER_PIXEL_FORMAT_SNB,
	TEXTURE_PIXEL_FORMAT_SNB,
	1/*gmbus works*/,
	dsp_init_snb
};
#endif


#if defined(CONFIG_SNB)
static int dsp_init_snb(igd_context_t *context)
{
	return 0;
}
#endif
