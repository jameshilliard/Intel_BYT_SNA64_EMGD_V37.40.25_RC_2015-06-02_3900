/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: dsp_vlv.c
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
 *  This file contains all the necessary functions for display resource
 *  manager. This module abstracts all hardware resources and manages them.
 *-----------------------------------------------------------------------------
 */

#include <io.h>
#include <memory.h>

#include <igd_core_structs.h>

#include <gn7/regs.h>
#include <gn7/context.h>
#include "regs_common.h"
#include "drm_emgd_private.h"

#include "../cmn/dsp_dispatch.h"

/* Macros to selectively remove pixel formats ONLY for VBIOS
 * but available in driver. This is done because
 * 1. No USAGE for these pixel fomats in VBIOS
 * 2. Save some size in VBIOS where we are running out of space
 */
#ifdef CONFIG_MICRO   /* VBIOS */

#define SPRITE_PIXEL_FORMAT_VLV            NULL /* No use for these tables */
#define CURSOR_PIXEL_FORMAT_VLV            NULL
#define RENDER_PIXEL_FORMAT_VLV            NULL
#define TEXTURE_PIXEL_FORMAT_VLV           NULL
#define OVERLAY_PIXEL_FORMAT_VLV           NULL

#else  /* driver */

#define SPRITE_PIXEL_FORMAT_VLV            sprite_pixel_formats_vlv
#define CURSOR_PIXEL_FORMAT_VLV            cursor_pixel_formats_vlv
#define RENDER_PIXEL_FORMAT_VLV            render_pixel_formats_vlv
#define TEXTURE_PIXEL_FORMAT_VLV           texture_pixel_formats_vlv
#define OVERLAY_PIXEL_FORMAT_VLV           overlay_pixel_formats_vlv

#endif

#if defined(CONFIG_VLV)
static int dsp_init_vlv(igd_context_t *context);
#endif

/*
 * NOTE: Some of these format lists are shared with GMM. For this reason
 * they cannot be static.
 */
/* FIXME(VLV): Value based on GN4, need to be checked for VLV */
unsigned long fb_pixel_formats_vlv[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ABGR32,
	IGD_PF_ARGB32_2101010,
	IGD_PF_RGB16_565,
	IGD_PF_ARGB8_INDEXED,
	0
};

unsigned long vga_pixel_formats_vlv[] = {
	IGD_PF_ARGB8_INDEXED,
	0
};

#ifndef CONFIG_MICRO /* Not needed for VBIOS */
unsigned long sprite_pixel_formats_vlv[] = {
	IGD_PF_ARGB32,
	IGD_PF_xRGB32,
	IGD_PF_ABGR32,
	IGD_PF_xBGR32,
	IGD_PF_ABGR32_2101010,
	IGD_PF_xBGR32_2101010,
	IGD_PF_RGB16_565,
	IGD_PF_ARGB8_INDEXED,
	IGD_PF_YUV422_PACKED_YUY2,
	IGD_PF_YUV422_PACKED_UYVY,
	IGD_PF_YUV422_PACKED_YVYU,
	IGD_PF_YUV422_PACKED_VYUY,
	0
};

unsigned long render_pixel_formats_vlv[] = {
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

unsigned long texture_pixel_formats_vlv[] = {
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

unsigned long depth_pixel_formats_vlv[] = {
	IGD_PF_Z16,
	IGD_PF_Z24,
	IGD_PF_S8Z24,
	0
};


unsigned long cursor_pixel_formats_vlv[] = {
	IGD_PF_ARGB32,
	IGD_PF_RGB_2,
	IGD_PF_RGB_XOR_2,
	IGD_PF_RGB_T_2,
	0
};

/* No Overlay in VLV */
/*
unsigned long overlay_pixel_formats_vlv[] = {
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

unsigned long blt_pixel_formats_vlv[] = {
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
 * Plane Definitions for Gen7 family.
 */
#if defined(CONFIG_VLV)
static igd_display_plane_t planea_vlv = {
	DSPACNTR,
	IGD_PLANE_IS_PLANEA | IGD_PLANE_DISPLAY | IGD_PLANE_DOUBLE | IGD_PLANE_DIH,
	0, 0,
	fb_pixel_formats_vlv, NULL, NULL
};

static igd_display_plane_t planeb_vlv = {
	DSPBCNTR,
	IGD_PLANE_IS_PLANEB | IGD_PLANE_DISPLAY | IGD_PLANE_SPRITE | IGD_PLANE_DIH,
	0, 0,
	fb_pixel_formats_vlv, NULL, NULL
};

static igd_display_plane_t plane_vga_vlv = {
	VGACNTRL, IGD_PLANE_VGA, 0, 0,
	vga_pixel_formats_vlv, NULL, NULL
};
#endif

#if defined(CONFIG_VLV)
static igd_display_plane_t plane_cursora_vlv = {
	CUR_A_CNTR, IGD_PLANE_CURSOR|IGD_CURSOR_USE_PIPEA, 0, 0,
	CURSOR_PIXEL_FORMAT_VLV, NULL, NULL
};

static igd_display_plane_t plane_cursorb_vlv = {
	CUR_B_CNTR, IGD_PLANE_CURSOR|IGD_CURSOR_USE_PIPEB, 0, 0,
	CURSOR_PIXEL_FORMAT_VLV, NULL, NULL
};
#endif

#if defined(CONFIG_VLV)
	/* VLV has 4 Video Sprite
	 * A and B with no scaling and packed pixel formats
	 * C and D with no scaling and packed pixel formats */
static igd_display_plane_t videoa_vlv = {
	SPACNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_VLV, NULL, NULL
};
static igd_display_plane_t videob_vlv = {
	SPBCNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_VLV, NULL, NULL
};
static igd_display_plane_t videoc_vlv = {
	SPCCNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_VLV, NULL, NULL
};
static igd_display_plane_t videod_vlv = {
	SPDCNTR, IGD_PLANE_SPRITE, 0, 0,
	SPRITE_PIXEL_FORMAT_VLV, NULL, NULL
};
#endif

/*
 * Plane lists for VLV family members.
 */
#if defined(CONFIG_VLV)
/* Two Main Plane, Four Video Sprite, One VGA, Two Cursor */
static igd_display_plane_t *plane_table_vlv[] = {
	&planea_vlv,
	&planeb_vlv,
	&plane_vga_vlv,
	&videoa_vlv,
	&videob_vlv,
	&videoc_vlv,
	&videod_vlv,
	&plane_cursora_vlv,
	&plane_cursorb_vlv,
	NULL
};
#endif

#if defined(CONFIG_VLV)
static igd_clock_t clock_a_vlv = {
	DPLLACNTR, FPA0, 16
};
#endif

#if defined(CONFIG_VLV)
static igd_clock_t clock_b_vlv = {
	DPLLBCNTR, FPB0, 16
};
#endif

/*
 * Pipe definitions for Gen7 family.
 * Note: Gen7 does not support Double Wide modes.
 */
#if defined(CONFIG_VLV)
igd_display_pipe_t pipea_vlv = {
	0, _PIPEACONF, PIPEA_TIMINGS, DPALETTE_A, &clock_a_vlv,
	(IGD_PIPE_IS_PIPEA | IGD_PORT_SHARE_ANALOG | IGD_PORT_SHARE_DIGITAL),
	0, 0, NULL, NULL, NULL, 0
};

igd_display_pipe_t pipeb_vlv = {
	1, _PIPEBCONF, PIPEB_TIMINGS, DPALETTE_B, &clock_b_vlv,
	(IGD_PIPE_IS_PIPEB | IGD_PORT_SHARE_ANALOG | IGD_PORT_SHARE_DIGITAL),
	0, 0, NULL, NULL, NULL, 0
};
#endif

#if defined(CONFIG_VLV)
static igd_display_pipe_t *pipe_table_vlv[] = {
	&pipea_vlv,
	&pipeb_vlv,
	NULL
};
#endif

/*
 * Port definitions for Gen7 family.
 */

/* Port number: Port number is from 1-number of available ports on any hardware.
 * Here are the definitions:
 *
 * On Gen7:
 * =======
 * Port mappings:
 *   1 - Int-TV?? (depending on sku)
 *   2 - SDVOB/HDMIB/DPB port
 *   3 -       HDMIC/DPC port
 *   4 - Int-LVDS?? (depending on sku)
 *   5 - Analog port
 *   New additions???-->
 *   -------------------
 *   6 - DPB/eDPB
 *   7 - DPC/eDPC
 *
 * Note: Port number should match with port numbers in port parameters.
 *       See igd_init.h for more information.
 */


/*
 * These are the port attributes that the VLV core support.
 * Note that currently it only contains color correction attributes.
 * Eventually, this will include all the attributes.
 */
igd_attr_t port_attrib_vlv[5][5] = {
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
	{ /* Config for port 2:  SDVO/HDMI B */
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
	{ /* Config for port 3:  SDVO/HDMI C */
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
	{ /* Config for port 4: LVDS */
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
	{ /* Config for port 5: ANALOG */
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
};

igd_attr_t port_attrib_dp_vlv[6] = {
	 /* Config for port 6: DPB */
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
			PD_ATTR_ID_DP_USER_BPC, 
			PD_ATTR_TYPE_RANGE,
			"DP/eDP User Bits Per Color", 
			PD_ATTR_FLAG_PD_INVISIBLE, 
			IGD_DISPLAY_BPC_6, /*Safe fallback for DP/eDP.*/
			0, 
			IGD_DISPLAY_BPC_6, /* Minimum */
			IGD_DISPLAY_BPC_16, /* Maximum */
			2), /* 6, 8, 10, 12, 14, 16  */
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
};

static igd_display_port_t analog_port_vlv = {
	IGD_PORT_ANALOG, 5, "ANALOG", 0x61100,
	0, 0, IGD_PARAM_GPIO_PAIR_ANALOG, 0xA0,
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_TV | IGD_PORT_SHARE_DIGITAL |
		IGD_VGA_COMPRESS),
	DREFCLK, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 0, DDC_DEFAULT_SPEED,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_vlv[5 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

static igd_display_port_t hdmib_port_vlv = {
	IGD_PORT_DIGITAL, 2, "HDMI-B", 0x61140,
	IGD_PARAM_GPIO_PAIR_DIGITALB, 0xA0, IGD_PARAM_GPIO_PAIR_DIGITALB, 0xA0,
	/* Actually, for HDMI-B, we only use the DDC reg, slave-address and speed
	 * We are filling in the I2c reg, slave-addr and speed just for completeness
	 * This is actually decided by the implementation on the port driver,
	 * which could differ from one chipset to another - but its VLV only for now
	 */
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_DIGITAL | IGD_PORT_SHARE_ANALOG |
		IGD_VGA_COMPRESS | IGD_RGBA_COLOR | IGD_PORT_GANG),
	TVCLKINBC, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 100, 100,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_vlv[2 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

static igd_display_port_t hdmic_port_vlv = {
	IGD_PORT_DIGITAL, 3, "HDMI-C", 0x61160,
	IGD_PARAM_GPIO_PAIR_DIGITALC, 0xA0, IGD_PARAM_GPIO_PAIR_DIGITALC, 0xA0,
	/* Actually, for HDMI-C, we only use the DDC reg, slave-address and speed
	 * We are filling in the I2c reg, slave-addr and speed just for completeness
	 * This is actually decided by the implementation on the port driver,
	 * which could differ from one chipset to another - but VLV now is DDC only
	 */
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
		IGD_PORT_SHARE_DIGITAL | IGD_PORT_SHARE_ANALOG |
		IGD_VGA_COMPRESS | IGD_RGBA_ALPHA | IGD_PORT_GANG),
	TVCLKINBC, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 100, 100,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_vlv[3 - 1], /* Port Number - 1 */
	0,
	{ NULL },
	0,0,0,
};

igd_display_port_t dpb_port_vlv = {
	IGD_PORT_DIGITAL, 6, "DP-B", DPBCNTR, 
	IGD_PARAM_GPIO_PAIR_NONE, 0, DPBAUXCNTR, 0xA0,
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
	IGD_VGA_COMPRESS | IGD_RGBA_COLOR | IGD_PORT_GANG),
	TVCLKINBC, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 400, 400,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_dp_vlv ,
	0,
	{ NULL },
	0,0,0,

};

igd_display_port_t dpc_port_vlv = {
	IGD_PORT_DIGITAL, 7, "DP-C", DPCCNTR,
	IGD_PARAM_GPIO_PAIR_NONE, 0, DPCAUXCNTR, 0xA0,
	(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB |
	IGD_VGA_COMPRESS | IGD_RGBA_COLOR | IGD_PORT_GANG),
	TVCLKINBC, 0, IGD_POWERSTATE_D0, IGD_POWERSTATE_D0,
	NULL, NULL,
	NULL, NULL, NULL, 0, NULL, 400, 400,
	NULL, NULL, NULL, NULL, 0, NULL, 0, 0,
	IGD_POWERSTATE_UNDEFINED,
	port_attrib_dp_vlv,
	0,
	{ NULL },
	0,0,0,

};

/*
 * NOTE: Port B should come before Port C because the first sDVO device
 * is usually connected to Port B.
 */
#ifdef CONFIG_VLV
static igd_display_port_t *port_table_vlv[] = {
	&analog_port_vlv,
	&hdmib_port_vlv,
	&hdmic_port_vlv,
	&dpb_port_vlv,
	&dpc_port_vlv,
	NULL
};
#endif

#if defined(CONFIG_VLV)
static igd_fb_caps_t caps_table_vlv[] = {
	{IGD_PF_ARGB32, 0},
	{IGD_PF_xRGB32, 0},
	{IGD_PF_ABGR32, 0},
	{IGD_PF_xBGR32, 0},
	{IGD_PF_ARGB32, 0},
	{IGD_PF_RGB16_565, 0},
	{IGD_PF_ARGB8_INDEXED, 0},
	{0, 0}
};
#endif

#if defined(CONFIG_VLV)
dsp_dispatch_t dsp_dispatch_vlv = {
	plane_table_vlv, pipe_table_vlv, port_table_vlv, caps_table_vlv,
	SPRITE_PIXEL_FORMAT_VLV, RENDER_PIXEL_FORMAT_VLV,
	TEXTURE_PIXEL_FORMAT_VLV,
	1, /* FIXME - need to change to zero if GMBUS in VLV is broken */
	dsp_init_vlv
};
#endif

#if defined(CONFIG_VLV)
static int dsp_init_vlv(igd_context_t *context)
{
	if ((context->device_context.did == PCI_DEVICE_ID_VGA_CDV0) ||
		(context->device_context.did == PCI_DEVICE_ID_VGA_CDV1)||
		(context->device_context.did == PCI_DEVICE_ID_VGA_CDV2)||
		(context->device_context.did == PCI_DEVICE_ID_VGA_CDV3)){
		dsp_dispatch_vlv.can_use_gmbus = 0;
	} else {
		/* not sure about VLV yet - need to find out from the silicon folks*/
		/* FIXME: CDV cant use the GMBUS but we havent decided on VLV yet */
	}

	return 0;
}
#endif

