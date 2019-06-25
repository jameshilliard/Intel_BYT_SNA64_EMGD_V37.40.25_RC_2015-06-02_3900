/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: micro_spr_snb.c
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
 *  This file contains function that actually programs either SpriteA
 *  overlay with the bits to properly configure the overlay
 *  Also includes functions to execute the second overlay flip
 *  instruction, and query the overlay flip status.
 *  Also contains some hardware capabilities querrying functions
 *  for upper overlay layer to get this chips second overlay
 *  capabilities
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.overlay

#include <io.h>
#include <memory.h>
#include <math_fix.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <gn6/regs.h>
#include "instr_common.h"
#include "regs_common.h"

#include <i915/intel_bios.h>
#include <i915/intel_ringbuffer.h>
#include "drm_emgd_private.h"
#include "emgd_drm.h"

#include "spr_cache_snb.h"
#include "spr_snb.h"
#include "spr_regs_snb.h"
#include "../cmn/ovl_dispatch.h"
#include "../cmn/ovl_virt.h"


extern unsigned int spr_check_snb(igd_display_context_t *display,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags);

int micro_alter_sprite_snb(igd_display_context_t *display,
	igd_appcontext_h     appctx,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags);

unsigned int micro_sprite_flush_instr_snb(
	igd_display_context_t *display);

int query_max_size_spr_snb(
	igd_display_context_t *display,
	unsigned long pf,
	unsigned int *max_width,
	unsigned int *max_height);

int query_spr_snb(igd_display_context_t * display,
	unsigned int flags);

void get_spr_zorder_fbblend_snb(igd_display_context_t * display,
	unsigned long * zorder_combined,
	unsigned long * fbblendovl);

static int micro_sprite_update_gamma_snb(
	igd_display_context_t *display, int engine,
	igd_ovl_gamma_info_t * gamma,
	int immediate_flush);

int set_spr_gamma_snb1(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_snb(
			display, 0, gamma, 1);
}
int set_spr_gamma_snb2(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_snb(
			display, 1, gamma, 1);
}


#define	SPRITE_CHECK_SNB_RET(ret,a, b, c, d, e,f) ret = spr_check_snb(a ,b ,c ,d ,e ,f)
#define SPRITE_QUERY_SNB_RET(ret,a, b) ret = query_spr_snb(a, b)

extern igd_display_pipe_t pipea_snb;
extern igd_display_pipe_t pipeb_snb;
extern unsigned long sprite_pixel_formats_snb[];

static char spr_a[] = "Sprite A\0";
static char spr_b[] = "Sprite B\0";
ovl_dispatch_t ovl_dispatch_snb[] = {
	/* Dispatch for the hardware overlay */
	{
		1, /* plane_id */
		spr_a,
		&pipea_snb,
		0/*max upscale*/,16 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 0,0,
		sprite_pixel_formats_snb,
		NULL, /*blend_surf_needed_snb,*/
		micro_alter_sprite_snb,
		query_spr_snb,
		query_max_size_spr_snb,
		NULL,
		NULL,
		set_spr_gamma_snb1
	},
	/* Dispatch for the software overlay */
	{
		2, /* plane_id */
		spr_b,
		&pipeb_snb,
		0/*max upscale*/,16 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 0,0,
		sprite_pixel_formats_snb,
		NULL, /*blend_surf_needed_snb,*/
		micro_alter_sprite_snb,
		query_spr_snb,
		query_max_size_spr_snb,
		NULL,
		NULL,
		set_spr_gamma_snb2
	},
	{0, NULL, NULL, 0,0,0,0,0, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL}
		/* end marker for the DI layer to count how
		 * many planes is supported by current HW */
};

static unsigned int convert_color_key_to_hw (
	unsigned long pf,
	unsigned int input)
{
	unsigned int output;

	switch (pf) {
		case IGD_PF_ARGB32:
		case IGD_PF_xRGB32:
			output = input;
			break;
		default:
			EMGD_ERROR("Sprite color key conversion unsupported format");
			output = input;
			break;
	}

	return output;
}
static unsigned int convert_color_key_to_mask (
	unsigned long pf,
	unsigned int input)
{
	unsigned int output;

	switch (pf) {
		case IGD_PF_ARGB32:
		case IGD_PF_xRGB32:
			output = 0x00ffffff;
			break;
		default:
			EMGD_ERROR("Sprite color key mask unsupported format");
			output = 0x00ffffff;
			break;
	}

	return output;
}

/* Convert YUV to UVY for YUV pixel formats.
 * Do not convert RGB pixel formats */
static unsigned int yuv_to_uvy(
	unsigned long pf,
	unsigned int input)
{
	unsigned int output;

	if (IGD_PF_TYPE(pf) & PF_TYPE_YUV) {
		output =
			((input & 0x00ff0000) >> 8) |
			((input & 0x0000ff00) >> 8)  |
			((input & 0x000000ff) << 16);
	} else {
		output = input;
	}

	return output;
}

/* SandyBridge overlay cache structure */
static spr_cache_snb_t spr_cache[2];

/* Flag to signal the cache is invalid and needs
 * to be re-initialized */
static int spr_cache_needs_init = TRUE;
os_alarm_t spr_timeout[2];


void get_spr_zorder_fbblend_snb(igd_display_context_t * display,
	unsigned long * zorder_combined,
	unsigned long * fbblendovl)
{
	*zorder_combined = IGD_OVL_ZORDER_INVALID;
	*fbblendovl      = 0;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_update_dest_snb()
 *
 * Description:
 *   This function updates the destination position and size
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_dest_snb(int pipe, igd_rect_t *dest_rect)
{
	EMGD_TRACE_ENTER;

	/* dest position and size */
	spr_cache[pipe].spr_cache_regs.dest_pos_x1y1 =
		( (dest_rect->y1 << 16) | dest_rect->x1 );
	spr_cache[pipe].spr_cache_regs.dest_size_w_h =
		( (dest_rect->y2 - dest_rect->y1 - 1) << 16) |
		  (dest_rect->x2 - dest_rect->x1 - 1);

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_update_scaler_snb()
 *
 * Description:
 *   This function updates the scaler register bits
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_scaler_snb(int pipe, igd_rect_t *src_rect,
		igd_rect_t *dest_rect, emgd_framebuffer_t *src_surf)
{
	unsigned long src_width, src_height, dest_width, dest_height;
	int capwidth_downscale = 0, capheight_downscale = 0;

	EMGD_TRACE_ENTER;

	src_width = (src_rect->x2 - src_rect->x1);
	src_height = (src_rect->y2 - src_rect->y1);
	dest_width = (dest_rect->x2 - dest_rect->x1);
	dest_height = (dest_rect->y2 - dest_rect->y1);

	/* Step 1: see if scaling is even needed */
	if( (dest_width  == src_width) &&
		(dest_height == src_height)){
		/* we dont need any scaling */
		EMGD_DEBUG("No scaling detected for sprite - turning off!");
		spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~(BIT31 | BIT30 | BIT29);
		EMGD_TRACE_EXIT;
		return IGD_SUCCESS;
	}

	/* Step 2: Can we scale requested ratio? We have limits on downscale */
	/* look at width first */
	if( (src_width  > dest_width ) && ((src_width /dest_width ) >= 16) ){
		if((src_width/dest_width)==16){
			unsigned long balance = (src_width << 16) / dest_width;
			balance &= 0x0000FFFF;
			if(!balance){
				/* we're at the limit of a 16:1 downscale ratio, but can be done*/
				EMGD_DEBUG("Downscale ratio at the limit but carrying on!");
			} else {
				EMGD_ERROR("We've hit the width scaling ratio limitations1 - capping to 16!");
				capwidth_downscale= 1;
			}
		} else {
			EMGD_ERROR("We've hit the width scaling ratio limitations2 - capping to 16!");
			capwidth_downscale= 1;
		}
		if(capwidth_downscale){
			dest_width = src_width / 16;
			spr_cache[pipe].spr_cache_regs.dest_size_w_h &= ~0x00000FFF;
			spr_cache[pipe].spr_cache_regs.dest_size_w_h |= dest_width;
		}
	}
	/* look at height second */
	if( (src_height > dest_height ) && ((src_height /dest_height ) >= 16) ){
		if((src_height/dest_height)==16){
			unsigned long balance = (src_height << 16) / dest_height;
			balance &= 0x0000FFFF;
			if(!balance){
				/* we're at the limit of a 16:1 downscale ratio, but can be done*/
				EMGD_DEBUG("Downscale ratio at the limit but carrying on!");
			} else {
				EMGD_ERROR("We've hit the height scaling ratio limitations1 - capping to 16!");
				capheight_downscale= 1;
			}
		} else {
			EMGD_ERROR("We've hit the height scaling ratio limitations2 - capping to 16!");
			capheight_downscale= 1;
		}
		if(capheight_downscale){
			dest_height = src_height / 16;
			spr_cache[pipe].spr_cache_regs.dest_size_w_h &= ~0x0FFF0000;
			spr_cache[pipe].spr_cache_regs.dest_size_w_h |= (dest_height << 16);
		}
	}

	/* FIXME - i am not looking at the comparison of the downscale ratio vs the
	 * (pixel format+dot_clock+num_pipes) combo to determine if we have enough
	 * resources to downscale this. Currently this is not yet being handled but
	 * should be fixed especially for higher dot clocks more than 1 disp pipe.
	 */

	/* Step 3, if we're here, then lets enabling scaling, (dont forget to
	 * set the filtering and fix interleaved vertical phase offset)
	 */
	spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~BIT27;
	spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~(BIT30 | BIT29);
	spr_cache[pipe].spr_cache_regs.scaler_ctl |= (BIT29 | BIT31);

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_src_ptr_snb()
 *
 * Description:
 *   This function updates the source offset
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_src_ptr_snb(int pipe,
		emgd_framebuffer_t *src_surf, unsigned int flags)
{
	EMGD_TRACE_ENTER;

	/* src surface */
	spr_cache[pipe].spr_cache_regs.src_base = (i915_gem_obj_ggtt_offset(src_surf->bo) & ~0xfff);

	/* odd vs even field selection */
	if(flags & IGD_OVL_ALTER_INTERLEAVED){
		if(flags & IGD_OVL_ALTER_FLIP_ODD){
			spr_cache[pipe].spr_cache_regs.scaler_ctl |= BIT28;
		} else {
			spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~BIT28;
		}
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_update_src_snb()
 *
 * Description:
 *   This function updates the source pitch and pixel format
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_src_snb(
	igd_display_context_t *display,
	int pipe,
	emgd_framebuffer_t *src_surf,
	igd_rect_t    *src_rect,
	unsigned int flags)
{
	unsigned long width, height;
	EMGD_TRACE_ENTER;

	spr_cache[pipe].spr_cache_regs.stride = src_surf->base.DRMFB_PITCH;
	if(src_surf->igd_flags & IGD_SURFACE_TILED){
		spr_cache[pipe].spr_cache_regs.tiled_offset = ((src_rect->y1)<<16) | (src_rect->x1);
		spr_cache[pipe].spr_cache_regs.linear_offset = 0;
		spr_cache[pipe].spr_cache_regs.control |= BIT10;
	} else {
		spr_cache[pipe].spr_cache_regs.linear_offset =
			(src_rect->y1 * src_surf->base.DRMFB_PITCH) +
			(src_rect->x1 * (IGD_PF_BPP(src_surf->igd_pixel_format)/8));
		spr_cache[pipe].spr_cache_regs.tiled_offset = 0;
		spr_cache[pipe].spr_cache_regs.control &= ~BIT10;
	}

	spr_cache[pipe].spr_cache_regs.control &= ~SPR_CMD_SRCFMTMASK;
	/* src pixel format */
	switch(src_surf->igd_pixel_format){
		case IGD_PF_YUV422_PACKED_YUY2:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
		case IGD_PF_YUV422_PACKED_UYVY:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_UYVY);
			break;
		case IGD_PF_YUV422_PACKED_YVYU:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YVYU);
			break;
		case IGD_PF_YUV422_PACKED_VYUY:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_VYUY);
			break;
		case IGD_PF_xRGB32_8888:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB32 | SPR_CMD_RGB);
			break;
		case IGD_PF_xBGR32_8888:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB32 | SPR_CMD_BGR);
			break;
		case IGD_PF_xRGB32_2101010:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB32_2101010 | SPR_CMD_RGB);
			break;
		case IGD_PF_xBGR32_2101010:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB32_2101010 | SPR_CMD_BGR);
			break;
		case IGD_PF_xRGB64_16161616F:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB64_16161616F | SPR_CMD_RGB);
			break;
		case IGD_PF_xBGR64_16161616F:
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_xRGB64_16161616F | SPR_CMD_BGR);
			break;
		default:
			EMGD_ERROR("Invalid pixel format: 0x%lx", src_surf->igd_pixel_format);
			/* treat like IGD_PF_YUV422_PACKED_YUY2 */
			spr_cache[pipe].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
	}

	/* Turn off YUV to RGB conversion if the src is RGB */
	if (!(src_surf->igd_pixel_format & PF_TYPE_YUV)) {
		spr_cache[pipe].spr_cache_regs.control |= BIT19;
	} else {
		spr_cache[pipe].spr_cache_regs.control &= ~BIT19;
	}

	/* Update the src width and height here */
	width = src_rect->x2 - src_rect->x1;
	height = src_rect->y2 - src_rect->y1;

	spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~0x07FFFFFF;
	spr_cache[pipe].spr_cache_regs.scaler_ctl |= height;
	spr_cache[pipe].spr_cache_regs.scaler_ctl |= (width << 16);

	/* update if its interlaced src or progressive - although the spec
	 * doesnt say if these interlace related bits are used in non scaling
	 * scenarios
	 */
	spr_cache[pipe].spr_cache_regs.scaler_ctl &= ~(BIT27 | BIT28);
	if(flags & IGD_OVL_ALTER_INTERLEAVED){
		spr_cache[pipe].spr_cache_regs.scaler_ctl |= BIT27;
		if(flags & IGD_OVL_ALTER_FLIP_ODD){
			spr_cache[pipe].spr_cache_regs.scaler_ctl |= BIT28;
		}
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_color_correct_snb()
 *
 * Description:
 *   This function updates the contrast, brightness, and saturation of
 *   the overlay using the values specified in overlay_info.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
static int micro_sprite_update_color_correct_snb(
	igd_display_context_t        *display,
	int                           pipe,
	emgd_framebuffer_t           *src_surf,
	igd_ovl_color_correct_info_t *color_correct)
{
	EMGD_TRACE_ENTER;

	EMGD_ERROR("For now, SNB Sprite doesnt support brightness, constrast, sat/hue - need to use gamma!");
	/*FIXME - we probably should be using the gamma correction as a way to change
	 * brightness / contrast / saturation / hue - but for now we'll skip this on SNB
	 */
	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


static const int gamma_reg_input[SPR_REG_GAMMA_NUM] = {	0, 256, 512, 768, 1024, 1280,
														1536, 1792, 2048, 2304, 2560, 2816,
														3072, 3328, 3684, 3840};
static const unsigned int gamma_def[SPR_REG_GAMMA_NUM] = {
	0x00000000,
	0x04010040,
	0x08020080,
	0x0C0300C0,
	0x10040100,
	0x14050140,
	0x18060180,
	0x1C0701C0,
	0x20080200,
	0x24090240,
	0x280A0280,
	0x2C0B02C0,
	0x300C0300,
	0x340D0340,
	0x380E0380,
	0x3C0F03C0
};
static const unsigned int gamma_max_default[3] = {
		0x00000400,
		0x00000400,
		0x00000400
};

/*-----------------------------------------------------------------------------
 * Function: sprite_update_gamma_snb()
 *
 * Description:
 *    This function sets the gamma correction values for the overlays.
 *
 * Returns:
 *   != 0 on Error
 *   IGD_SUCCESS on Success
 *---------------------------------------------------------------------------*/
static int micro_sprite_update_gamma_snb(
	igd_display_context_t *display, int pipe,
	igd_ovl_gamma_info_t *ovl_gamma, int immediate_flush)
{
	unsigned int  new_gamma_red_24i_8f, new_gamma_green_24i_8f;
	unsigned int  new_gamma_blue_24i_8f;
	unsigned int  gamma_normal_r_24i_8f;
	unsigned int  gamma_normal_g_24i_8f;
	unsigned int  gamma_normal_b_24i_8f;
	unsigned int  gamma_reg, gamma_reg_24i_8f;
	unsigned int  i;

	EMGD_TRACE_ENTER;

	/* FIXME: we need to be using the gamma correction as a way to change
	 * brightness / contrast / saturation / hue - but for now we'll skip this on SNB
	 */

	/* If there is no display plane and fb_info, this is happen during module
	 * initialization, just return.
	 */
    if (!PLANE(display)) {
        EMGD_TRACE_EXIT;
        return IGD_SUCCESS;
    } else if (!PLANE(display)->fb_info) {
        EMGD_TRACE_EXIT;
        return IGD_SUCCESS;
    }

	/* If the sprite gamma is disabled set it to the default */
	if ((ovl_gamma->flags & IGD_OVL_GAMMA_ENABLE)==IGD_OVL_GAMMA_DISABLE) {

		spr_cache[pipe].ovl_info.gamma.red   = IGD_OVL_GAMMA_DEFAULT;
		spr_cache[pipe].ovl_info.gamma.blue  = IGD_OVL_GAMMA_DEFAULT;
		spr_cache[pipe].ovl_info.gamma.green = IGD_OVL_GAMMA_DEFAULT;

		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* program register */
			spr_cache[pipe].spr_cache_regs.gamma_regs[i] = gamma_def[i];
		}
		for (i = 0; i < 3; i++) {
			/* program register */
			spr_cache[pipe].spr_cache_regs.gamma_max[i] = gamma_max_default[i];
		}
		if(immediate_flush){
			/* Actually nothing additional like flip or reprogramming surface address
			 * Based on B-Specs these registers are not double buffered and will
			 * take immediate effect on SNB
			 */
			for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
				/* program register */
				EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_regs[i],
				   MMIO(display) + DVS_GAMCBASE(pipe) + i*4);
			}
			for (i = 0; i < 3; i++) {
				/* program register */
				EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_max[i],
				   MMIO(display) + DVS_GAMCBASE(pipe) + (SPR_REG_GAMMA_NUM+i)*4);
			}
		}
		EMGD_TRACE_EXIT;
		return IGD_SUCCESS;
	}

	/* It is assumed that the input value is a 24-bit24568 number */
	new_gamma_red_24i_8f   = ovl_gamma->red;
	new_gamma_green_24i_8f = ovl_gamma->green;
	new_gamma_blue_24i_8f  = ovl_gamma->blue;

	spr_cache[pipe].ovl_info.gamma.red   = ovl_gamma->red;
	spr_cache[pipe].ovl_info.gamma.blue  = ovl_gamma->green;
	spr_cache[pipe].ovl_info.gamma.green = ovl_gamma->blue;

	/*
	 * Program RGB for each of the 6 gamma registers
	 */

	/* Since the OS_POW_FIX function can only take an integer base,
	 * we need to normalize the result by gamma_normal_x
	 */
	gamma_normal_r_24i_8f =  OS_POW_FIX(255, (1<<16)/new_gamma_red_24i_8f);
	gamma_normal_g_24i_8f =  OS_POW_FIX(255, (1<<16)/new_gamma_green_24i_8f);
	gamma_normal_b_24i_8f =  OS_POW_FIX(255, (1<<16)/new_gamma_blue_24i_8f);

	for( i = 0; i < SPR_REG_GAMMA_NUM; i++ )
	{
		/* red */
		gamma_reg_24i_8f = OS_POW_FIX(gamma_reg_input[i],
					      (1<<16)/new_gamma_red_24i_8f);
		gamma_reg =
			((255 * gamma_reg_24i_8f) / gamma_normal_r_24i_8f) << 16;

		/* green */
		gamma_reg_24i_8f = OS_POW_FIX(gamma_reg_input[i],
					      (1<<16)/new_gamma_green_24i_8f);
		gamma_reg        |=
			((255 * gamma_reg_24i_8f) / gamma_normal_g_24i_8f) << 8;

		/* blue */
		gamma_reg_24i_8f = OS_POW_FIX(gamma_reg_input[i],
					      (1<<16)/new_gamma_blue_24i_8f);
		gamma_reg        |=
			((255 * gamma_reg_24i_8f) / gamma_normal_b_24i_8f);

		/* program register */
		spr_cache[pipe].spr_cache_regs.gamma_regs[i] = gamma_reg;

	}
	for (i = 0; i < 3; i++) {
		/* program register */
		spr_cache[pipe].spr_cache_regs.gamma_max[i] = gamma_max_default[i];
	}
	if(immediate_flush){
		/* Actually nothing additional like flip or reprogramming surface address
		 * Based on B-Specs these registers are not double buffered and will
		 * take immediate effect on SNB
		 */
		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* program register */
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_regs[i],
			   MMIO(display) + DVS_GAMCBASE(pipe) + i*4);
		}
		for (i = 0; i < 3; i++) {
			/* program register */
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_max[i],
			   MMIO(display) + DVS_GAMCBASE(pipe) + (SPR_REG_GAMMA_NUM+i)*4);
		}
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_disable_snb()
 *
 * Description:
 * Write the registers needed to turn off the overlay.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
static unsigned int micro_sprite_disable_snb(
	igd_display_context_t *display)
{
	unsigned int pipe;

	EMGD_TRACE_ENTER;

	/* Turn OFF this sprite.
	 * Ensure we are using the correct Pipe. */
	/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
	pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)? 1:0;

	EMGD_WRITE32(0, MMIO(display) + DVS_CNTR(pipe));

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_clear_cache_snb()
 *
 * Description:
 *
 *----------------------------------------------------------------------*/
static void micro_sprite_clear_cache_snb(
	igd_display_context_t *display,
	unsigned int           flags)
{

	int pipe;

	/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
	pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)?1:0;

	/* Force every cache check to miss */
	OS_MEMSET(&(spr_cache[pipe]), 0, sizeof(spr_cache_snb_t));

	/* We just set our cached flags to 0, which might accidently
	 * match up with "OFF" for some important incoming flag
	 * bits, causing us to think we already handled them when
	 * we didn't.  So set our cached flags to the exact
	 * opposite of the incoming flags, which will force
	 * us to test and handle every single bit, regardless
	 * of whether it is on or off. */
	spr_cache[pipe].flags = ~flags;

	/* initialization complete */
	spr_cache_needs_init = FALSE;

}


/*-----------------------------------------------------------------------------
 * Function: micro_sprite_update_colorkey_snb()
 *
 * Description:
 *    This function sets the colorkey values for the overlays.
 *
 * Returns:
 *   != 0 on Error
 *   IGD_SUCCESS on Success
 *---------------------------------------------------------------------------*/
static void micro_sprite_update_colorkey_snb(
	igd_display_context_t *display,
	int                    pipe,
	emgd_framebuffer_t    *src_surf,
	igd_ovl_info_t        *ovl_info)
{
	unsigned int ckey_low, ckey_high;

	/* Destination color key */
	EMGD_DEBUG("Color key.flags: 0x%lx", ovl_info->color_key.flags);

	if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {

		EMGD_DEBUG("Overlay Enable Dest Color Key");
		/* The mask and color key are different for the
		 * different pixel formats */
		spr_cache[pipe].spr_cache_regs.ckey_value =
			convert_color_key_to_hw
			(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest) & 0x00FFFFFF;

		spr_cache[pipe].spr_cache_regs.ckey_mask =
			convert_color_key_to_mask
			(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest);
		/*
		 * Sprite plane bit2 for dest color key
		 */
		spr_cache[pipe].spr_cache_regs.control |= BIT2;

	} else {
		EMGD_DEBUG("Overlay Disable Dest Color Key");
		spr_cache[pipe].spr_cache_regs.control &= ~BIT2;
	}

	if(ovl_info->pipeblend.enable_flags & IGD_OVL_PIPEBLEND_SET_FBBLEND) {
		/*
		 * If Overlay + FB Blend is requested and the FB is xRGB
		 * turn on the ARGB format.
		 */
		EMGD_ERROR("Request for HW Sprite+FB is not yet EMGD supported on SNB");
	}

	/* Source Color key */
	if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
		EMGD_DEBUG("Overlay Enable Src Color Key");

		ckey_high = (convert_color_key_to_hw
			(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_hi) & 0x00FFFFFF);
		spr_cache[pipe].spr_cache_regs.ckey_max = yuv_to_uvy(src_surf->igd_pixel_format,
				       ckey_high);

		ckey_low = (convert_color_key_to_hw
			(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_lo) & 0x00FFFFFF);
		spr_cache[pipe].spr_cache_regs.ckey_value = yuv_to_uvy(src_surf->igd_pixel_format,
				      ckey_low);

		spr_cache[pipe].spr_cache_regs.ckey_mask |= (0x7 << 24);
		spr_cache[pipe].spr_cache_regs.control |= BIT22;
	} else {
		EMGD_DEBUG("Overlay Disable Src Color Key");
		spr_cache[pipe].spr_cache_regs.control &= ~BIT22;
	}

}


/*----------------------------------------------------------------------
 * Function: micro_sprite_program_cached_regs_snb()
 *
 * Description:
 *
 *----------------------------------------------------------------------*/
static void micro_sprite_program_cached_regs_snb(
	igd_display_context_t *display,
	igd_ovl_info_t        *ovl_info,
	int                    cache_changed)
{
	int i, pipe;

	/*
	 * Now write all the changed registers to the HW
	*/

	/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
	pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)?1:0;

	/* Write dest rect information */
    if (cache_changed & OVL_UPDATE_DEST) {
		EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.dest_pos_x1y1,
			    MMIO(display) + DVS_POS(pipe));
		EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.dest_size_w_h,
			    MMIO(display) + DVS_SIZE(pipe));
		EMGD_DEBUG("DVS_POS = 0x%08lx", spr_cache[pipe].spr_cache_regs.dest_pos_x1y1);
   		EMGD_DEBUG("DVS_SIZE = 0x%08lx", spr_cache[pipe].spr_cache_regs.dest_size_w_h);
	}
	EMGD_DEBUG("DVS_POS = 0x%08lx", spr_cache[pipe].spr_cache_regs.dest_pos_x1y1);
   	EMGD_DEBUG("DVS_SIZE = 0x%08lx", spr_cache[pipe].spr_cache_regs.dest_size_w_h);

	/* Write source information */
    if (cache_changed & (OVL_UPDATE_SURF |
                             OVL_UPDATE_SRC  ) ) {
		EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.linear_offset,
			   MMIO(display) + DVS_LINOFF(pipe));
		EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.stride,
			   MMIO(display) + DVS_STRIDE(pipe));
		EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.tiled_offset,
			   MMIO(display) + DVS_TILEOFF(pipe));
		EMGD_DEBUG("DVS_LINOFF = 0x%08lx", spr_cache[pipe].spr_cache_regs.linear_offset);
		EMGD_DEBUG("DVS_STRIDE = 0x%08lx", spr_cache[pipe].spr_cache_regs.stride);
    		EMGD_DEBUG("DVS_TILEOFF = 0x%08lx", spr_cache[pipe].spr_cache_regs.tiled_offset);
	}
	EMGD_DEBUG("DVS_LINOFF = 0x%08lx", spr_cache[pipe].spr_cache_regs.linear_offset);
	EMGD_DEBUG("DVS_STRIDE = 0x%08lx", spr_cache[pipe].spr_cache_regs.stride);
    	EMGD_DEBUG("DVS_TILEOFF = 0x%08lx", spr_cache[pipe].spr_cache_regs.tiled_offset);

    /* Write the scaler control */
    if(cache_changed & OVL_UPDATE_SCALING) {
    	EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.scaler_ctl,
    			MMIO(display) + DVS_SCALE(pipe));
    	EMGD_DEBUG("DVS_SCALE = 0x%08lx", spr_cache[pipe].spr_cache_regs.scaler_ctl);
    }
	EMGD_DEBUG("DVS_SCALE = 0x%08lx", spr_cache[pipe].spr_cache_regs.scaler_ctl);

	/* Write the gamma */
    if (cache_changed & OVL_UPDATE_GAMMA) {
		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* program register */
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_regs[i],
			   MMIO(display) + DVS_GAMCBASE(pipe) + i*4);
		}
		for (i = 0; i < 3; i++) {
			/* program register */
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.gamma_max[i],
			   MMIO(display) + DVS_GAMCBASE(pipe) + (SPR_REG_GAMMA_NUM+i)*4);
		}
	}

	/* Write the colorkey data */
    if (cache_changed & OVL_UPDATE_COLORKEY) {
		/* Dest color key */
		if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {
			/* Write the regs needed to turn it on */
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.ckey_value,
				   MMIO(display) + DVS_KEYMINVAL(pipe));
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.ckey_mask,
				   MMIO(display) + DVS_KEYMASK(pipe));
		}
		/* Source Color key */
		if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.ckey_value,
				   MMIO(display) + DVS_KEYMINVAL(pipe));
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.ckey_max,
				   MMIO(display) + DVS_KEYMAXVAL(pipe));
			EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.ckey_mask,
				   MMIO(display) + DVS_KEYMASK(pipe));
		}
	}

	/* Write the control register first */
	EMGD_WRITE32((spr_cache[pipe].spr_cache_regs.control | BIT31),
		   MMIO(display) + DVS_CNTR(pipe));

	EMGD_DEBUG("DVS_CNTR = 0x%08lx", spr_cache[pipe].spr_cache_regs.control);

}


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_regs_snb()
 *
 * Description:
 * Examine the incoming sprite parameters, and update the sprite hardware
 * regs according to what changed.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/

static unsigned int micro_sprite_update_regs_snb(
	igd_display_context_t *display,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	int ret;
	int cache_changed, pipe;


	EMGD_TRACE_ENTER;

	/* Fast path for turning off overlay. No need for cache */
	if ((flags & IGD_OVL_ALTER_ON) == IGD_OVL_ALTER_OFF) {
		ret = micro_sprite_disable_snb(display);

		/* Reset the cache */
		spr_cache_needs_init = TRUE;

		EMGD_TRACE_EXIT;
		return ret;
	}

	/* Init the cache if needed */
	if (spr_cache_needs_init) {
		micro_sprite_clear_cache_snb(display, flags);
	}

	/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
	pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)?1:0;

	/* See what has changed in the cache */
	cache_changed = get_cache_changes_snb(
		src_surf, src_rect,
		dest_rect, ovl_info,
		flags, &spr_cache[pipe], pipe);

	/* Normally we would set interleave parameters here,
	 * but the secondary overlay does not support interleave.*/
	if (flags & IGD_OVL_ALTER_INTERLEAVED) {
		EMGD_ERROR("Sprites do not support Interleaved??");
	}

	/* ----------------------------------------------------------*/
	/* Has our destination rectangle changed? */
	if (cache_changed & OVL_UPDATE_DEST) {
		micro_sprite_update_dest_snb(pipe, dest_rect);
	}

	/* ----------------------------------------------------------*/
	/* Always update the source pointers every frame. */
	ret = micro_sprite_update_src_ptr_snb(pipe, src_surf, flags);
	if (ret) {
		EMGD_ERROR("Sprite updating src failed");
		EMGD_TRACE_EXIT;
		return ret;
	}


	/* ----------------------------------------------------------*/
	/* Did either the Src rect or surface change? */
    if (cache_changed & (OVL_UPDATE_SURF |
                             OVL_UPDATE_SRC  ) ) {
		ret = micro_sprite_update_src_snb(display,
					pipe, src_surf, src_rect, flags);
		if (ret) {
			EMGD_ERROR("Sprite updating src failed");
			EMGD_TRACE_EXIT;
			return ret;
		}
	}

	/* ----------------------------------------------------------*/
	/* Did the scaling ratio changes, does the scaler need change? */
    /* REMEMBER!!! - only do this AFTER updating src and dest above */
    if(cache_changed & OVL_UPDATE_SCALING) {
		ret = micro_sprite_update_scaler_snb(
				pipe, src_rect, dest_rect, src_surf);
		if (ret) {
			EMGD_ERROR("Sprite updating scaler failed!");
			EMGD_TRACE_EXIT;
			return ret;
		}
	}

	/* ----------------------------------------------------------*/
	/* Did the color correction information change? */
    if (cache_changed & (OVL_UPDATE_CC) ) {
		ret = micro_sprite_update_color_correct_snb(display,
					pipe, src_surf, &ovl_info->color_correct);
		if (ret) {
			EMGD_ERROR("Sprite video quality failed");
			EMGD_TRACE_EXIT;
			return ret;
		}
	}

	/* ----------------------------------------------------------*/
	/* Did the gamma change? */
    if (cache_changed & OVL_UPDATE_GAMMA) {
		ret = micro_sprite_update_gamma_snb(display,
					pipe, &ovl_info->gamma, 0);
		if (ret) {
			EMGD_ERROR("Sprite gamma failed");
			EMGD_TRACE_EXIT;
			return ret;
		}

	}

	/* ----------------------------------------------------------*/
	/* Did the color key change? */
    if (cache_changed & OVL_UPDATE_COLORKEY) {
		micro_sprite_update_colorkey_snb(display,
				pipe, src_surf, ovl_info);
	}

	/* Now write all the changes to the part */
	micro_sprite_program_cached_regs_snb(display,
			ovl_info, cache_changed);

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

unsigned int micro_sprite_flush_instr_snb(
	igd_display_context_t     *display)
{
	int pipe;
	
	EMGD_TRACE_ENTER;

	/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
	pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)?1:0;

	/* For micro, this is NOT an instruction, so it happens immediately,
	 * we do not have to poll waiting for it. */
	EMGD_WRITE32(spr_cache[pipe].spr_cache_regs.src_base,
			MMIO(display) + DVS_SURF(pipe));
	/* FIXME - once we have GEM and ring buffer, we should use command instruction
	 * to dispatch the overlay flip above and also get a sync for the next vblank
	 */
#ifdef HACKY_FLIP_PENDING
	ovl_context->sync[pipe] = 1;
	spr_timeout[pipe] = OS_SET_ALARM(30);
#else
	/* FIXME - once we have GEM and ring buffer, we should use command instruction
	 * to dispatch the overlay flip above and also get a sync for the sprite flip
	 */
	ovl_context->sync[pipe] = 0;
#endif

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

int micro_prepare_spr_snb(
	igd_display_context_t *display,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	spr_reg_cache_snb_t *sprite_regs_snb,
	unsigned int         flags)
{
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* Check to ensure the overlay can be used given the current mode as
	 * well as what the IAL is asking for.  If not return an error. */
	if( (SPRITE_CHECK_SNB_RET(ret, display, src_surf, src_rect,
									dest_rect, ovl_info,flags)) )
	{
		if (ret) {
			EMGD_ERROR("SPRITE_CHECK_SNB_RET failed");
			return ret;
		}
	}

	/* Check if last flip is still pending.
	 * This is necessary for the following reasons:
	 *    - If the previous instructions have not been processed, then the
	 *      spritec_regs_snb is still in use and can not be overwritten.
	 */
	if( (SPRITE_QUERY_SNB_RET(ret, display,
								IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE)) )
	{
		EMGD_ERROR("SPRITE_QUERY_SNB_RET failed");
		if ((FALSE == ret) &&
			(flags & IGD_OVL_ALTER_ON)) {
			/* Only return an error if the overlay is on.  If turning it off,
			* allow it to continue, since something may have failed and we
			* should try our best to turn the overlay off. */
			return -IGD_ERROR_HWERROR;
		}
	}
	/* Update all Overlay Update Registers */
	ret = micro_sprite_update_regs_snb(display,
			src_surf, src_rect, dest_rect, ovl_info, flags);
	if (ret) {
		EMGD_ERROR("Sprite update Registers failed");
		return ret;
	}

	EMGD_TRACE_EXIT;
	return ret;
}

int micro_alter_sprite_snb(igd_display_context_t *display,
	igd_appcontext_h     appctx,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	spr_reg_cache_snb_t sprite_regs_snb;
	int ret=0;

	EMGD_TRACE_ENTER;

	/* Initialize structure so compilers don't complain */
	OS_MEMSET(&sprite_regs_snb, 0, sizeof(spr_reg_cache_snb_t));

	if (micro_prepare_spr_snb(display, src_surf, src_rect, dest_rect,
		ovl_info, &sprite_regs_snb, flags)) {
		return -IGD_ERROR_HWERROR;
	}

	/* Directly write the register to update 2nd overlay */
	ret = micro_sprite_flush_instr_snb(display);

	EMGD_TRACE_EXIT;
	return ret;
}

