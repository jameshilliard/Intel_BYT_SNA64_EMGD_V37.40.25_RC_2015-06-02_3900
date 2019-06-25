/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: micro_spr_vlv.c
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

#include <gn7/regs.h>
#include <gn7/instr.h>
#include "instr_common.h"
#include "regs_common.h"

#include <i915/intel_bios.h>
#include <i915/intel_ringbuffer.h>
#include "drm_emgd_private.h"
#include "emgd_drm.h"

#include "spr_cache_vlv.h"
#include "spr_vlv.h"
#include "spr_regs_vlv.h"
#include "../cmn/ovl_dispatch.h"
#include "../cmn/ovl_virt.h"

unsigned int get_zorder_control_values(unsigned long zorder_combined, int plane_id);

bool isvalid_zorder_value(unsigned long zorder);

extern unsigned int spr_check_vlv(
	igd_display_context_t *display, int engine,
	emgd_framebuffer_t *src_surf,  igd_rect_t      *src_rect,
	igd_rect_t    *dest_rect, igd_ovl_info_t  *ovl_info,
	unsigned int   flags);

int micro_alter_sprite_vlv(igd_display_context_t *display, int engine,
	igd_appcontext_h   appctx,   emgd_framebuffer_t *src_surf,
	igd_rect_t        *src_rect, igd_rect_t       *dest_rect,
	igd_ovl_info_t    *ovl_info, unsigned int      flags);

int micro_alter_sprite_vlv1(igd_display_context_t *display,
	igd_appcontext_h   appctx,   emgd_framebuffer_t *src_surf,
	igd_rect_t        *src_rect, igd_rect_t       *dest_rect,
	igd_ovl_info_t    *ovl_info, unsigned int      flags)
{
	return micro_alter_sprite_vlv(display, 0, appctx, src_surf,
		src_rect, dest_rect, ovl_info, flags);
}
int micro_alter_sprite_vlv2(igd_display_context_t *display,
	igd_appcontext_h   appctx,   emgd_framebuffer_t *src_surf,
	igd_rect_t        *src_rect, igd_rect_t       *dest_rect,
	igd_ovl_info_t    *ovl_info, unsigned int      flags)
{
	return micro_alter_sprite_vlv(display, 1, appctx, src_surf,
		src_rect, dest_rect, ovl_info, flags);
}
int micro_alter_sprite_vlv3(igd_display_context_t *display,
	igd_appcontext_h   appctx,   emgd_framebuffer_t *src_surf,
	igd_rect_t        *src_rect, igd_rect_t       *dest_rect,
	igd_ovl_info_t    *ovl_info, unsigned int      flags)
{
	return micro_alter_sprite_vlv(display, 2, appctx, src_surf,
		src_rect, dest_rect, ovl_info, flags);
}
int micro_alter_sprite_vlv4(igd_display_context_t *display,
	igd_appcontext_h   appctx,   emgd_framebuffer_t *src_surf,
	igd_rect_t        *src_rect, igd_rect_t       *dest_rect,
	igd_ovl_info_t    *ovl_info, unsigned int      flags)
{
	return micro_alter_sprite_vlv(display, 3, appctx, src_surf,
		src_rect, dest_rect, ovl_info, flags);
}

int query_max_size_spr_vlv(igd_display_context_t * display,
	unsigned long pf, unsigned int *max_width, unsigned int *max_height);

int query_spr_vlv(igd_display_context_t * display, int engine, unsigned int flags);


/*
 *  Comment out for increase kernel coverage for sprite. In future those function
 *  needed then only will be turn on back. Currrently we didn't need now.
 */
#if 0 // Code Coverage
int query_spr_vlv1(igd_display_context_t * display, unsigned int flags)
{
	return query_spr_vlv(display, 0, flags);
}
int query_spr_vlv2(igd_display_context_t * display, unsigned int flags)
{
	return query_spr_vlv(display, 1, flags);
}
int query_spr_vlv3(igd_display_context_t * display, unsigned int flags)
{
	return query_spr_vlv(display, 2, flags);
}
int query_spr_vlv4(igd_display_context_t * display, unsigned int flags)
{
	return query_spr_vlv(display, 3, flags);
}
#endif // Code Coverage

int set_spr_const_alpha_vlv(igd_display_context_t * display, int engine,
	igd_ovl_pipeblend_t * pipeblend, int immediate_flush);

int set_spr_const_alpha_vlv1(igd_display_context_t * display,
	igd_ovl_pipeblend_t * pipeblend){
	return set_spr_const_alpha_vlv(display, 0, pipeblend, 1);
}
int set_spr_const_alpha_vlv2(igd_display_context_t * display,
	igd_ovl_pipeblend_t * pipeblend){
	return set_spr_const_alpha_vlv(display, 1, pipeblend, 1);
}
int set_spr_const_alpha_vlv3(igd_display_context_t * display,
	igd_ovl_pipeblend_t * pipeblend){
	return set_spr_const_alpha_vlv(display, 2, pipeblend, 1);
}
int set_spr_const_alpha_vlv4(igd_display_context_t * display,
	igd_ovl_pipeblend_t * pipeblend){
	return set_spr_const_alpha_vlv(display, 3, pipeblend, 1);
}

static int micro_sprite_update_color_correct_vlv(
	igd_display_context_t *display, int engine,
	emgd_framebuffer_t *src_surf, igd_ovl_color_correct_info_t *color_correct,
	int immediate_flush, spr_reg_cache_vlv_t *spr_flip_regs);


/*
 * Comment out for increase kernel coverage for sprite. In future those function
 * needed then only will be turn on back. Currrently we didn't need now.
 */
#if 0 // Code Coverage
int set_spr_colorcorrect_vlv1(igd_display_context_t * display,
	igd_ovl_color_correct_info_t * ccorrect)
{
	return micro_sprite_update_color_correct_vlv(
			display, 0, NULL, ccorrect, 1);
}
int set_spr_colorcorrect_vlv2(igd_display_context_t * display,
	igd_ovl_color_correct_info_t * ccorrect)
{
	return micro_sprite_update_color_correct_vlv(
			display, 1, NULL, ccorrect, 1);
}
int set_spr_colorcorrect_vlv3(igd_display_context_t * display,
	igd_ovl_color_correct_info_t * ccorrect)
{
	return micro_sprite_update_color_correct_vlv(
			display, 2, NULL, ccorrect, 1);
}
int set_spr_colorcorrect_vlv4(igd_display_context_t * display,
	igd_ovl_color_correct_info_t * ccorrect)
{
	return micro_sprite_update_color_correct_vlv(
			display, 3,NULL, ccorrect, 1);
}
#endif // Code Coverage

static int micro_sprite_update_gamma_vlv(
	igd_display_context_t *display, int engine,
	igd_ovl_gamma_info_t * gamma,
	int immediate_flush, spr_reg_cache_vlv_t *spr_flip_regs);

int sprite_calc_regs_vlv(
	igd_display_context_t *display,
	int                  engine,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags,
	void *regs);

int sprite_commit_regs_vlv(igd_display_context_t *display,
	int                  engine,
	igd_ovl_info_t      *ovl_info,
	void *regs);

/*
 * This code is needed as sysfs set gamma sprite will pass through this path.
 */
int set_spr_gamma_vlv1(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_vlv(
			display, 0, gamma, 1, NULL);
}
int set_spr_gamma_vlv2(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_vlv(
			display, 1, gamma, 1, NULL);
}
int set_spr_gamma_vlv3(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_vlv(
			display, 2, gamma, 1, NULL);
}
int set_spr_gamma_vlv4(igd_display_context_t * display,
	igd_ovl_gamma_info_t * gamma)
{
	return micro_sprite_update_gamma_vlv(
			display, 3, gamma, 1, NULL);
}

extern igd_display_pipe_t pipea_vlv;
extern igd_display_pipe_t pipeb_vlv;
extern unsigned long sprite_pixel_formats_vlv[];

igd_spr_dispatch_t _igd_spr_dispatch = {
	.calc_spr_regs			= sprite_calc_regs_vlv,
	.commit_spr_regs		= sprite_commit_regs_vlv,
};

static char spr_a[] = "Sprite A\0";
static char spr_b[] = "Sprite B\0";
static char spr_c[] = "Sprite C\0";
static char spr_d[] = "Sprite D\0";
ovl_dispatch_t ovl_dispatch_vlv[] = {
	/* Dispatch for the hardware overlay */
	{
		1,
		spr_a,
		&pipea_vlv,
		1 /*max upscale*/,1 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 1,1,
		sprite_pixel_formats_vlv,
		NULL, /*blend_surf_needed_vlv,*/
		micro_alter_sprite_vlv1,
		NULL, /*query_spr_vlv1,*/
		query_max_size_spr_vlv,
		set_spr_const_alpha_vlv1,
		NULL, /*set_spr_colorcorrect_vlv1,*/
		set_spr_gamma_vlv1,
		&_igd_spr_dispatch
	},
	/* Dispatch for the software overlay */
	{
		2,
		spr_b,
		&pipea_vlv,
		1 /*max upscale*/,1 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 1,1,
		sprite_pixel_formats_vlv,
		NULL, /*blend_surf_needed_vlv,*/
		micro_alter_sprite_vlv2,
		NULL, /*query_spr_vlv2,*/
		query_max_size_spr_vlv,
		set_spr_const_alpha_vlv2,
		NULL, /*set_spr_colorcorrect_vlv2, */
		set_spr_gamma_vlv2,
		&_igd_spr_dispatch
	},
	{
		3,
		spr_c,
		&pipeb_vlv,
		1 /*max upscale*/,1 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 1,1,
		sprite_pixel_formats_vlv,
		NULL, /*blend_surf_needed_vlv,*/
		micro_alter_sprite_vlv3,
		NULL, /*query_spr_vlv3,*/
		query_max_size_spr_vlv,
		set_spr_const_alpha_vlv3,
		NULL, /*set_spr_colorcorrect_vlv3,*/
		set_spr_gamma_vlv3,
		&_igd_spr_dispatch
	},
	{
		4,
		spr_d,
		&pipeb_vlv,
		1 /*max upscale*/,1 /*max downscale*/,
		SPR_REG_GAMMA_NUM, 1,1,
		sprite_pixel_formats_vlv,
		NULL, /*blend_surf_needed_vlv,*/
		micro_alter_sprite_vlv4,
		NULL, /*query_spr_vlv4, */
		query_max_size_spr_vlv,
		set_spr_const_alpha_vlv4,
		NULL, /*set_spr_colorcorrect_vlv4,*/
		set_spr_gamma_vlv4,
		&_igd_spr_dispatch
	},
	{0, NULL, NULL, 0,0,0,0,0, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL}
		/* end marker for the DI layer to count how
		 * many planes is supported by current HW */
};

unsigned int micro_sprite_flush_instr_vlv(
	igd_display_context_t    *display, int engine,
	unsigned int              flags);

/*
 * ****************************************************
 * PLANE Zorder HW specific definitions
 * ****************************************************
 */
/* The following are all the plane zorder definition
 * permutations from bottom to top 3! = 3x2x1 = 6 combo's*/
#define SPR_BOTM    BIT2
#define SPR_ZORDER  BIT0

/*
 *used B-spec notation
 * Notation DISP_SPR1_SPR2
 * 	          PA   SA   SB
 * 	          01   02   04
 * code's values for PA SA SB, Alpha channel and cursor are ignored
 */
/* This is default configuration */
/*0 - PA SA SB CA*/
#define DISP_SPR1_SPR2       0x00010204 /* this is per igd_ovl.h*/
#define REG1_DISP_SPR1_SPR2  (0)
#define REG2_DISP_SPR1_SPR2  (0)
/*1 - PA SB SA CA*/
#define DISP_SPR2_SPR1       0x00010402 /* this is per igd_ovl.h*/
#define REG1_DISP_SPR2_SPR1  (SPR_ZORDER)
#define REG2_DISP_SPR2_SPR1  (0)
/*2&(3) - SB PA SA CA*/
#define SPR2_DISP_SPR1       0x00040102 /* this is per igd_ovl.h*/
#define REG1_SPR2_DISP_SPR1  (0)
#define REG2_SPR2_DISP_SPR1  (SPR_BOTM)
/*4&(5) - SB SA PA CA*/
#define SPR2_SPR1_DISP       0x00040201 /* this is per igd_ovl.h*/
#define REG1_SPR2_SPR1_DISP  (SPR_ZORDER)
#define REG2_SPR2_SPR1_DISP  (SPR_BOTM)
/*6&(7) - SA PA SB CA*/
#define SPR1_DISP_SPR2       0x00020104 /* this is per igd_ovl.h*/
#define REG1_SPR1_DISP_SPR2  (SPR_BOTM)
#define REG2_SPR1_DISP_SPR2  (0)
/*8&(9) - SA SB PA CA*/
#define SPR1_SPR2_DISP       0x00020401 /* this is per igd_ovl.h*/
#define REG1_SPR1_SPR2_DISP  (SPR_BOTM)
#define REG2_SPR1_SPR2_DISP  (SPR_ZORDER)

typedef struct _hw_zorder_type_map_vlv{
	unsigned long igd_zorder_combined;
	unsigned char reg1bits;
	unsigned char reg2bits;
	unsigned char S1_above_S2;
	unsigned char D_above_S1;
	unsigned char D_above_S2;
}hw_zorder_type_map_vlv_t;

#define MAX_VLV_ZORDER_TYPES 8
hw_zorder_type_map_vlv_t zorder_types[MAX_VLV_ZORDER_TYPES] = {
		{SPR2_SPR1_DISP, REG1_SPR2_SPR1_DISP, REG2_SPR2_SPR1_DISP,
		1, 1, 1},
		{SPR1_SPR2_DISP, REG1_SPR1_SPR2_DISP, REG2_SPR1_SPR2_DISP,
		0, 1, 1},
		{SPR2_DISP_SPR1, REG1_SPR2_DISP_SPR1, REG2_SPR2_DISP_SPR1,
		1, 0, 1},
		{SPR2_DISP_SPR1, REG1_SPR2_DISP_SPR1, REG2_SPR2_DISP_SPR1,
		1, 0, 1},
		{SPR1_DISP_SPR2, REG1_SPR1_DISP_SPR2, REG2_SPR1_DISP_SPR2,
		0, 1, 0},
		{SPR1_DISP_SPR2, REG1_SPR1_DISP_SPR2, REG2_SPR1_DISP_SPR2,
		0, 1, 0},
		{DISP_SPR2_SPR1, REG1_DISP_SPR2_SPR1, REG2_DISP_SPR2_SPR1,
		1, 0, 0},
		{DISP_SPR1_SPR2, REG1_DISP_SPR1_SPR2, REG2_DISP_SPR1_SPR2,
		0, 0, 0},
};

bool isvalid_zorder_value(unsigned long zorder)
{
	return ((zorder == DISP_SPR1_SPR2)|| (zorder == DISP_SPR2_SPR1) ||
			(zorder == SPR2_DISP_SPR1) || (zorder == SPR2_SPR1_DISP) ||
			(zorder == SPR1_DISP_SPR2)|| (zorder == SPR1_SPR2_DISP));
}

unsigned int
get_zorder_control_values(unsigned long zorder_combined, int plane_id)
{
	/* plane id possiable values - 1,2-(A&B); 3,4 - (C&D)*/
	/* but we need only even or add*/
	unsigned int zorder_reg_values = 0;
	int  i = 0;

	EMGD_TRACE_ENTER;

	for ( i = 0; i < ARRAY_SIZE(zorder_types); i++)
	{
		if (zorder_types[i].igd_zorder_combined == zorder_combined)
		{
			if ((1 == plane_id) || (3 == plane_id))
			{
				zorder_reg_values = zorder_types[i].reg1bits;
			}
			if ((2 == plane_id) || (4 == plane_id))
			{
				zorder_reg_values = zorder_types[i].reg2bits;
			}
		}
	}

	EMGD_DEBUG("zorder_reg_values = 0x%ux", zorder_reg_values);

	EMGD_TRACE_EXIT;

	return zorder_reg_values;
} /**/


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

/* ValleyView overlay cache structure */
static spr_cache_vlv_t spr_cache[4];

/* Flag to signal the cache is invalid and needs
 * to be re-initialized */
static int spr_cache_needs_init[4] = { TRUE };
os_alarm_t spr_timeout_vlv[4];


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_dest_vlv()
 *
 * Description:
 *   This function updates the destination position and size
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_dest_vlv(int engine,
		 emgd_framebuffer_t *src_surf, igd_rect_t *dest_rect)
{
	EMGD_TRACE_ENTER;

	/* dest position and size */
	spr_cache[engine].spr_cache_regs.dest_pos_x1y1 =
		( (dest_rect->y1 << 16) | dest_rect->x1 );
	spr_cache[engine].spr_cache_regs.dest_size_w_h =
		( (dest_rect->y2 - dest_rect->y1 - 1) << 16) |
		  (dest_rect->x2 - dest_rect->x1 - 1);

	if (src_surf->igd_pixel_format & PF_TYPE_YUV) {
		if((dest_rect->x2 - dest_rect->x1)%2){
			--dest_rect->x2;
			EMGD_DEBUG("We're capping the dest width to an even value - HW limit!");
		}
	}
	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_update_src_ptr_vlv()
 *
 * Description:
 *   This function updates the source offset
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_src_ptr_vlv(
	int engine, 
	emgd_framebuffer_t *src_surf,
	igd_rect_t    *src_rect,
	unsigned int flags, 
	igd_ovl_info_t * ovl_info)
{

	EMGD_TRACE_ENTER;

	/* src surface */
	spr_cache[engine].spr_cache_regs.src_base = (i915_gem_obj_ggtt_offset(src_surf->bo) & ~0xfff);;

#ifdef CONFIG_PAVP_VIA_FIRMWARE
	/* SEC firmware handles PAVP decryption,
	 * so lets preserve this decryption bit throughout the flips
	 */
	spr_cache[engine].spr_cache_regs.src_base |= (READ_MMIO_REG_VLV(MMIO(display),
			SP_SURF(engine)) & BIT2);
#else /*CONFIG_PAVP_VIA_FIRMWARE*/
	/* Driver handles PAVP decryption,
	 * so lets set decryption bits accordingly
	 */
	if(flags & IGD_OVL_SURFACE_ENCRYPTED){
		EMGD_ERROR("Not handling encrypted surface in kernel!");
	}
#endif

	/* odd vs even field selection */
	if(flags & IGD_OVL_ALTER_INTERLEAVED) {
		spr_cache[engine].spr_cache_regs.control |= BIT21;
		if(flags & IGD_OVL_ALTER_FLIP_ODD) {
			/*
			 * FIXME: for ovl deinterlacing (DMA2OVL), need to fix offset
			 * start for ODD interleave - check other location in this file
			 * for same flag
			 */
			if (0 == (src_rect->y1 & 1))
                                src_rect->y1 += 1;
		}
		else{
			if (0 != (src_rect->y1 & 1)) 
                                src_rect->y1 += 1;
		}
	} else {
		spr_cache[engine].spr_cache_regs.control &= ~BIT21;
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_update_src_vlv()
 *
 * Description:
 *   This function updates the source pitch and pixel format
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
unsigned int micro_sprite_update_src_vlv(
	igd_display_context_t *display,
	int            engine,
	emgd_framebuffer_t *src_surf,
	igd_rect_t    *src_rect,
	unsigned int   flags)
{
	EMGD_TRACE_ENTER;

	spr_cache[engine].spr_cache_regs.stride = src_surf->base.DRMFB_PITCH;
	if(src_surf->igd_flags & IGD_SURFACE_TILED){
		spr_cache[engine].spr_cache_regs.tiled_offset = ((src_rect->y1)<<16) | (src_rect->x1);
		spr_cache[engine].spr_cache_regs.linear_offset = 0;
		spr_cache[engine].spr_cache_regs.control |= BIT10;
	} else {
		spr_cache[engine].spr_cache_regs.linear_offset =
			(src_rect->y1 * src_surf->base.DRMFB_PITCH) +
			(src_rect->x1 * (IGD_PF_BPP(src_surf->igd_pixel_format)/8));
		spr_cache[engine].spr_cache_regs.tiled_offset = 0;
		spr_cache[engine].spr_cache_regs.control &= ~BIT10;
	}

	spr_cache[engine].spr_cache_regs.control &= ~SPR_CMD_SRCFMTMASK;
	/* src pixel format */
	switch(src_surf->igd_pixel_format){
		case IGD_PF_YUV422_PACKED_YUY2:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
		case IGD_PF_YUV422_PACKED_UYVY:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_UYVY);
			break;
		case IGD_PF_YUV422_PACKED_YVYU:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YVYU);
			break;
		case IGD_PF_YUV422_PACKED_VYUY:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_VYUY);
			break;
		case IGD_PF_RGB16_565:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_RGB565);
			break;
		case IGD_PF_ARGB8_INDEXED:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ARGB8IDX);
			break;
		case IGD_PF_ARGB32_8888:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ARGB32);
			break;
		case IGD_PF_xRGB32_8888:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xRGB32);
			break;
		case IGD_PF_ABGR32_8888:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ABGR32);
			break;
		case IGD_PF_xBGR32_8888:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xBGR32);
			break;
		case IGD_PF_ABGR32_2101010:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ABGR32_2101010);
			break;
		case IGD_PF_xBGR32_2101010:
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xBGR32_2101010);
			break;
		default:
			EMGD_ERROR("Invalid pixel format: 0x%lx", src_surf->igd_pixel_format);
			/* treat like IGD_PF_YUV422_PACKED_YUY2 */
			spr_cache[engine].spr_cache_regs.control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
	}

	/* Turn off YUV to RGB conversion if the src is RGB */
	if (!(src_surf->igd_pixel_format & PF_TYPE_YUV)) {
		spr_cache[engine].spr_cache_regs.control |= BIT19;
	} else {
		spr_cache[engine].spr_cache_regs.control &= ~BIT19;
	}

	/* odd vs even field selection */
	if(flags & IGD_OVL_ALTER_INTERLEAVED) {
		spr_cache[engine].spr_cache_regs.control |= BIT21;
		if(flags & IGD_OVL_ALTER_FLIP_ODD) {
			if (0 == (src_rect->y1 & 1))
                                src_rect->y1 += 1;
		}
		else{
			if (0 != (src_rect->y1 & 1))
                                src_rect->y1 += 1;
		}	
	} else {
		spr_cache[engine].spr_cache_regs.control &= ~BIT21;
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_color_correct_vlv()
 *
 * Description:
 *   This function updates the contrast, brightness, and saturation of
 *   the overlay using the values specified in overlay_info.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
static int micro_sprite_update_color_correct_vlv(
	igd_display_context_t        *display,
	int                           engine,
	emgd_framebuffer_t           *src_surf,
	igd_ovl_color_correct_info_t *color_correct,
	int                           immediate_flush,
	spr_reg_cache_vlv_t 		 *spr_flip_regs)
{
	int                          calc_brightness     = 0;
	unsigned int                 calc_contrast       = 0;
	unsigned int                 calc_saturation     = 0;
	unsigned int				 calc_hue			 = 0;
	unsigned char              * mmio = MMIO(display);

	EMGD_TRACE_ENTER;

	if(!src_surf){
		/* this happens when its an ioctl with an immediate flush*/
		src_surf = &spr_cache[engine].src_surf;
	}

	/* If the src_surf pixel format is RGB, then brightness, contrast,
	 * and saturation should all be set to the exact default */
	if (src_surf->igd_pixel_format & PF_TYPE_RGB) {
		EMGD_DEBUG("HW cant set color correction in RGB format!");

		if (color_correct->brightness != MID_BRIGHTNESS_YUV) {
			EMGD_DEBUG("RGB surfaces must set brightness to default");
		}
		if (color_correct->contrast != MID_CONTRAST_YUV) {
			EMGD_DEBUG("RGB surfaces must set contrast to default");
		}
		if (color_correct->saturation != MID_SATURATION_YUV) {
			EMGD_DEBUG("RGB surfaces must set saturation to default");
		}
		if (color_correct->hue != MID_HUE_YUV) {
			EMGD_DEBUG("RGB surfaces must set hue to default");
		}
		if (!spr_flip_regs) {
			spr_cache[engine].spr_cache_regs.cont_bright = SPR_RGB_COLOR_DEF_CONT_BRGHT;
			spr_cache[engine].spr_cache_regs.satn_hue =  SPR_RGB_COLOR_DEF_SATN_HUE;
		} else {
			spr_flip_regs->cont_bright = SPR_RGB_COLOR_DEF_CONT_BRGHT;
			spr_flip_regs->satn_hue =  SPR_RGB_COLOR_DEF_SATN_HUE;
		}

		if(immediate_flush) {
			/* Actually nothing additional like flip or reprogramming surface address
			 * Based on B-Specs these registers are not double buffered and will
			 * take immediate effect on VLV
			 */
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.cont_bright,
			   mmio, SP_CLRC0(engine));
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.satn_hue,
			   mmio, SP_CLRC1(engine));
		}

		EMGD_TRACE_EXIT;
		return IGD_SUCCESS;
	}

	/*************************************************************************
	 * Brightness
	 *************************************************************************/

	calc_brightness = color_correct->brightness;
	if (calc_brightness  > 127) {
		calc_brightness = 127;
	}
	if (calc_brightness  < -128) {
		calc_brightness = -128;
	}

	if (!spr_flip_regs) {
		spr_cache[engine].spr_cache_regs.cont_bright =
			(spr_cache[engine].spr_cache_regs.cont_bright & 0xFFFFFF00) |
			(calc_brightness & 0xFF);
	} else {
		spr_flip_regs->cont_bright =
			(spr_flip_regs->cont_bright & 0xFFFFFF00) | (calc_brightness & 0xFF);
	}


	/*************************************************************************
	 * Contrast
	 *************************************************************************/

	calc_contrast = color_correct->contrast;
	if (calc_contrast > 0x1FF) {
		calc_contrast = 0x1FF;
	}

	if (!spr_flip_regs) {
		spr_cache[engine].spr_cache_regs.cont_bright =
			(spr_cache[engine].spr_cache_regs.cont_bright & 0xF803FFFF ) |
			((calc_contrast & 0x1FF) << 18);
	} else {
		spr_flip_regs->cont_bright =
			(spr_flip_regs->cont_bright & 0xF803FFFF ) | ((calc_contrast & 0x1FF) << 18);
	}


	/*************************************************************************
	 * Saturation
	 *************************************************************************/

	calc_saturation  = color_correct->saturation;
	if (calc_saturation > 0x3FF) {
		calc_saturation = 0x3FF;
	}

	if (!spr_flip_regs) {
		spr_cache[engine].spr_cache_regs.satn_hue =
			(spr_cache[engine].spr_cache_regs.satn_hue & 0xFFFFFC00 ) |
			(calc_saturation & 0x3FF);
	} else {
		spr_flip_regs->satn_hue =
			(spr_flip_regs->satn_hue & 0xFFFFFC00 ) | (calc_saturation & 0x3FF);
	}

	/*************************************************************************
	 * Hue
	 *************************************************************************/

	calc_hue = color_correct->hue;
	if (calc_hue > 0x3FF) {
		calc_hue = 0x3FF;
	}

	if (!spr_flip_regs) {
		spr_cache[engine].spr_cache_regs.satn_hue =
			(spr_cache[engine].spr_cache_regs.satn_hue & 0xF800FFFF) | 
			((calc_hue & 0x7FF) << 16);
	} else {
		spr_flip_regs->satn_hue = (spr_flip_regs->satn_hue & 0xF800FFFF) |
			((calc_hue & 0x7FF) << 16);
	}

	if(immediate_flush) {
		/* Actually nothing additional like flip or reprogramming surface address
		 * Based on B-Specs these registers are not double buffered and will
		 * take immediate effect on VLV
		 */
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.cont_bright,
		   mmio, SP_CLRC0(engine));
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.satn_hue,
		   mmio, SP_CLRC1(engine));
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*-----------------------------------------------------------------------------
 * Function: check_gamma_vlv()
 *
 * Description:
 *    This function ensure the gamma are in the acceptable range.
 *
 *---------------------------------------------------------------------------*/
static void check_gamma_vlv(unsigned int *gamma)
{

	if (*gamma < IGD_OVL_GAMMA_MIN) {
		EMGD_ERROR("Gamma to small (0x%x in 24i.8f format), "
			"changing to Min Gamma (0.6)",
			*gamma);
		*gamma = IGD_OVL_GAMMA_MIN;
	}
	if (*gamma > IGD_OVL_GAMMA_MAX) {
		EMGD_ERROR("Gamma to large (0x%x in 24i.8f format), "
			"changing to Max Gamma (6.0)",
			*gamma);
		*gamma = IGD_OVL_GAMMA_MAX;
	}

	return;
}

static const int gamma_reg_input[SPR_REG_GAMMA_NUM] = {8, 16, 32, 64, 128, 192};
static const unsigned int gamma_def[SPR_REG_GAMMA_NUM] = {
		0x00080808,
		0x00101010,
		0x00202020,
		0x00404040,
		0x00808080,
		0x00c0c0c0
};

/*-----------------------------------------------------------------------------
 * Function: sprite_update_gamma_vlv()
 *
 * Description:
 *    This function sets the gamma correction values for the overlays.
 *
 * Returns:
 *   != 0 on Error
 *   IGD_SUCCESS on Success
 *---------------------------------------------------------------------------*/
static int micro_sprite_update_gamma_vlv(
	igd_display_context_t *display, int engine,
	igd_ovl_gamma_info_t *ovl_gamma, int immediate_flush, spr_reg_cache_vlv_t *spr_flip_regs)
{
	unsigned int          new_gamma_red_24i_8f, new_gamma_green_24i_8f;
	unsigned int          new_gamma_blue_24i_8f;
	unsigned int          gamma_normal_r_24i_8f;
	unsigned int          gamma_normal_g_24i_8f;
	unsigned int          gamma_normal_b_24i_8f;
	unsigned int          gamma_reg, gamma_reg_24i_8f;
	unsigned int          i;

	EMGD_TRACE_ENTER;

	/* Check the gamma value first whether the gamma is on/off */
	check_gamma_vlv(&ovl_gamma->red);
	check_gamma_vlv(&ovl_gamma->green);
	check_gamma_vlv(&ovl_gamma->blue);

	/* If there is no display plane or fb_info, this is happen during module
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
	if (((ovl_gamma->flags & IGD_OVL_GAMMA_ENABLE) == IGD_OVL_GAMMA_DISABLE) ||
			(PLANE(display)->fb_info->igd_pixel_format == IGD_PF_ARGB8_INDEXED)) {

		spr_cache[engine].ovl_info.gamma.red   = IGD_OVL_GAMMA_DEFAULT;
		spr_cache[engine].ovl_info.gamma.blue  = IGD_OVL_GAMMA_DEFAULT;
		spr_cache[engine].ovl_info.gamma.green = IGD_OVL_GAMMA_DEFAULT;

		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* program register */
			if (!spr_flip_regs) {
				spr_cache[engine].spr_cache_regs.gamma_regs[i] = gamma_def[i];
			} else {
				spr_flip_regs->gamma_regs[i] = gamma_def[i];
			}
		}

		if(immediate_flush) {
			/* Actually nothing additional like flip or reprogramming surface address
			 * Based on B-Specs these registers are not double buffered and will
			 * take immediate effect on VLV
			 */
			for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
				/* TAKE NOTE! The register offsets are in opposite direction
				 * Of how the values were stored. We programmed the values
				 * of GAM-0 in the gamma_regs[0] and GAM-5 in gamma_regs[]5.
				 * However, the register offset in B-Spec is the opposite
				 * direction as GAM-5 has the lowest register offset addr
				 */
				WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.gamma_regs[i],
					MMIO(display), SP_GAMC5(engine) + ((SPR_REG_GAMMA_NUM - 1 - i)*4));
			}
		}

		EMGD_TRACE_EXIT;
		return IGD_SUCCESS;
	}

	/* It is assumed that the input value is a 24-bit24568 number */
	new_gamma_red_24i_8f   = ovl_gamma->red;
	new_gamma_green_24i_8f = ovl_gamma->green;
	new_gamma_blue_24i_8f  = ovl_gamma->blue;


	spr_cache[engine].ovl_info.gamma.red   = ovl_gamma->red;
	spr_cache[engine].ovl_info.gamma.blue  = ovl_gamma->blue;
	spr_cache[engine].ovl_info.gamma.green = ovl_gamma->green;

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

		if (!spr_flip_regs) {
		/* program register */
			spr_cache[engine].spr_cache_regs.gamma_regs[i] = gamma_reg;

		} else {
			spr_flip_regs->gamma_regs[i] = gamma_reg;
		}
	}

	if(immediate_flush) {
		/* Actually nothing additional like flip or reprogramming surface address
		 * Based on B-Specs these registers are not double buffered and will
		 * take immediate effect on VLV
		 */
		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* TAKE NOTE! The register offsets are in opposite direction
			 * Of how the values were stored. We programmed the values
			 * of GAM-0 in the gamma_regs[0] and GAM-5 in gamma_regs[]5.
			 * However, the register offset in B-Spec is the opposite
			 * direction as GAM-5 has the lowest register offset addr
			 */
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.gamma_regs[i],
				MMIO(display), SP_GAMC5(engine) + ((SPR_REG_GAMMA_NUM - 1 - i)*4));
		}
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_disable_vlv()
 *
 * Description:
 * Write the registers needed to turn off the overlay.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/
static unsigned int micro_sprite_disable_vlv(
	igd_display_context_t *display, int engine,
	unsigned int flags)
{
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	unsigned long temp;
	unsigned long plane_control;
	int is_alpha_in_display_plane = 0;
	int other_engine_map[4] = {1,0,3,2};
	int other_engine = other_engine_map[engine];
	igd_context_t *context = NULL;
	int ret = 0;
#ifdef USE_RING_TRIGGER
	uint32_t dword0, dword1, dword2, dword3 = 0;
#endif

	EMGD_TRACE_ENTER;

	dev = ((struct drm_crtc *)(&display->base))->dev;
	dev_priv = (drm_emgd_private_t *) dev->dev_private;
	context = dev_priv->context;

	/* Turn OFF this sprite control -
	 * but maintain the z-order bits in registers for the debug purpose only*/
	temp = READ_MMIO_REG_VLV(MMIO(display), SP_CNTR(engine));
	temp &= 0x00000007;
	WRITE_MMIO_REG_VLV(temp, MMIO(display), SP_CNTR(engine));

	/* This code work well only for one sprite,
	 * WRITE_MMIO_REG_VLV(0x0, MMIO(display), SP_SURF(engine));
	 * but VLV has 4 sprite(s) ...
	 * So, have to use MI_DISPLAY_FLIP_I915
	 */

	if ((flags & IGD_OVL_ALTER_ON) == IGD_OVL_ALTER_OFF)
	{
#ifdef USE_RING_TRIGGER
		/* Set up ring to trigger the respecitve sprite flip*/
		if (engine < 2) {
			dword0 = (MI_DISPLAY_FLIP_I915 | MI_DISPLAY_FLIP_PLANE(engine + 2));
		} else {
			/* Sprite C = 0x5 while Sprite D = 0x4 for bit 21:19 */
			dword0 = (MI_DISPLAY_FLIP_I915 | MI_DISPLAY_FLIP_PLANE((engine == 0x2)?5:4));
		}
		dword1 = ((READ_MMIO_REG_VLV(MMIO(display), SP_STRIDE(engine)) & 0x0000FFC0) |
				((READ_MMIO_REG_VLV(MMIO(display), SP_CNTR(engine)) & 0x00000400) >> 10));
		/* we must to write 0 into SP_SURF register
		 * else sprite surface will be alive after flip
		 * (and it is potential memory leak) */
		dword2 = 0;
		dword3 = MI_NOOP;

		ret = BEGIN_LP_RING(4);
		if (ret) {
			EMGD_ERROR("Error allocating ring buffer");
			return ret;
		}

		OUT_RING(dword0);
		OUT_RING(dword1);
		OUT_RING(dword2);
		OUT_RING(dword3);
		ADVANCE_LP_RING();
#else
		WRITE_MMIO_REG_VLV(0, MMIO(display), SP_SURF(engine));
#endif
	}
	
	spr_cache[engine].spr_cache_regs.enabled = 0;
	spr_cache[engine].spr_cache_regs.src_base = 0;
	spr_cache[engine].spr_cache_regs.control &= 0x7;

	/* If no other sprite on the same crtc is active,
	 * turn off the plane control key enable
	 */ 	 
	if (!spr_cache[other_engine].spr_cache_regs.enabled) {
		plane_control = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg);
		plane_control &= ~KEY_ENABLE;
		WRITE_MMIO_REG_VLV(plane_control, MMIO(display), PLANE(display)->plane_reg);
	}
	/* check is alpha channel valid in display plane or not*/
	is_alpha_in_display_plane = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg);
	/* Bit 26 is indicate about alpha channel */
	is_alpha_in_display_plane = (is_alpha_in_display_plane & DSP_ALPHA) >> 26;
	/* Complicated case:
	 * 1 .If FB_Blend mode is on, before disabling sprite plane,
	 * we needs to disable alpha channel in display plane as well,
	 * 2. But we have 2 sprites... so, need to check is another sprite enabled or not
	 * */
	if ((display->fb_blend_ovl_on == 1) && (is_alpha_in_display_plane != 0))
	{
		if (1 != spr_cache[other_engine].spr_cache_regs.enabled)
		{
			ret = context->mode_dispatch->kms_change_plane_color_format((emgd_crtc_t *) display, 0);
			if (ret != 0)
			{
				EMGD_ERROR("kms_change_plane_color_format Failed!");
				return ret;
			}
		}
	}
	/* not sure what we need to trig display plane here*/
	/*
	 * CHECKME: updating plane start address register as trigger, B-Spec
	 * not clear about which start address register - DSPACNTR or DPSASURF
	 */
	temp = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg + 0x1c);
	WRITE_MMIO_REG_VLV(temp, MMIO(display), PLANE(display)->plane_reg + 0x1c);

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_clear_cache_vlv()
 *
 * Description:
 *
 *----------------------------------------------------------------------*/
static void micro_sprite_clear_cache_vlv(
	igd_display_context_t *display,
	int                    engine,
	unsigned int           flags)
{

	int tmp_dstck = 0;
	spr_cache_vlv_t * p_spr_cash = NULL;

	/* Actually Dst color key belong to display plane, we need to save it
	 */
	tmp_dstck = spr_cache[engine].spr_cache_regs.dest_ck_on;

	/* Force every cache check to miss */
	p_spr_cash = &(spr_cache[engine]);
	memset(p_spr_cash, 0, sizeof(spr_cache_vlv_t));

	/* We just set our cached flags to 0, which might accidently
	 * match up with "OFF" for some important incoming flag
	 * bits, causing us to think we already handled them when
	 * we didn't.  So set our cached flags to the exact
	 * opposite of the incoming flags, which will force
	 * us to test and handle every single bit, regardless
	 * of whether it is on or off. */
	spr_cache[engine].flags = ~flags;
	spr_cache[engine].spr_cache_regs.dest_ck_on = tmp_dstck;

	/* initialization complete */
	spr_cache_needs_init[engine] = FALSE;

}

/*-----------------------------------------------------------------------------
 * Function: micro_sprite_update_colorkey_vlv()
 *
 * Description:
 *    This function sets the colorkey values for the overlays.
 *
 * Returns:
 *   != 0 on Error
 *   IGD_SUCCESS on Success
 *---------------------------------------------------------------------------*/
static void micro_sprite_update_colorkey_vlv(
	igd_display_context_t *display,
	int                    engine,
	emgd_framebuffer_t    *src_surf,
	igd_ovl_info_t        *ovl_info,
	unsigned long          flags)
{
	unsigned int ckey_low, ckey_high;

	/* Destination color key */
	EMGD_DEBUG("Color key.flags: 0x%08lx", ovl_info->color_key.flags);

	if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {

		EMGD_DEBUG("Overlay Enable Dest Color Key");
		/* The mask and color key are different for the
		 * different pixel formats */
		spr_cache[engine].spr_cache_regs.dstckey_value =
			convert_color_key_to_hw
			(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest);

		spr_cache[engine].spr_cache_regs.dstckey_mask =
			convert_color_key_to_mask
			(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest);

	} else {
		EMGD_DEBUG("Overlay Disable Dest Color Key");
	}

	/* Source Color key */
	if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
		EMGD_DEBUG("Overlay Enable Src Color Key");

		ckey_high = (convert_color_key_to_hw
			(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_hi) & 0x00FFFFFF);
		spr_cache[engine].spr_cache_regs.ckey_max = yuv_to_uvy(src_surf->igd_pixel_format,
				       ckey_high);

		ckey_low = (convert_color_key_to_hw
			(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_lo) & 0x00FFFFFF);
		spr_cache[engine].spr_cache_regs.ckey_value = yuv_to_uvy(src_surf->igd_pixel_format,
				      ckey_low);

		spr_cache[engine].spr_cache_regs.ckey_mask = 0x7;
		spr_cache[engine].spr_cache_regs.control |= BIT22;

	} else {
		EMGD_DEBUG("Overlay Disable Src Color Key");
		spr_cache[engine].spr_cache_regs.control &= ~BIT22;
		spr_cache[engine].spr_cache_regs.ckey_mask = 0;

	}

}

/*-----------------------------------------------------------------------------
 * Function: set_spr_const_alpha_vlv()
 *
 * Description:
 *    This function sets overlay capabilities that include:
 *           - CONST ALPHA  - (impacts only this plane)
 *           - and nothing more now
 * Returns:

 *   != 0 on Error
 *   IGD_SUCCESS on Success
 *---------------------------------------------------------------------------*/
int set_spr_const_alpha_vlv(igd_display_context_t *display, int engine,
	igd_ovl_pipeblend_t * pipeblend, int immediate_flush)
{
	EMGD_TRACE_ENTER;

	/* Check if user requested to update the constant alpha value */
	if(pipeblend->enable_flags & IGD_OVL_PIPEBLEND_SET_CONSTALPHA) {
		if(pipeblend->has_const_alpha) {
			spr_cache[engine].spr_cache_regs.const_alpha = BIT31;
			spr_cache[engine].spr_cache_regs.const_alpha |=
						pipeblend->const_alpha;
		} else {
			spr_cache[engine].spr_cache_regs.const_alpha = 0;
		}
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: micro_sprite_program_cached_regs_vlv()
 *
 * Description:
 *
 *----------------------------------------------------------------------*/
static void micro_sprite_program_cached_regs_vlv(
	igd_display_context_t *display,
	int                    engine,
	igd_ovl_info_t        *ovl_info,
	int                    cache_changed,
	unsigned int           flags)
{
	int i;
	unsigned char * mmio = MMIO(display);
	unsigned long plane_control;

	/*
	 * Now write all the changed registers to the HW
	*/

	/* Write dest rect information */
    if (cache_changed & OVL_UPDATE_DEST) {
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.dest_pos_x1y1,
			    mmio, SP_POS(engine));
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.dest_size_w_h,
			    mmio, SP_SIZE(engine));
		EMGD_DEBUG("SP_POS = 0x%08lx", spr_cache[engine].spr_cache_regs.dest_pos_x1y1);
   		EMGD_DEBUG("SP_SIZE = 0x%08lx", spr_cache[engine].spr_cache_regs.dest_size_w_h);
	}

	/* Write source information */
    if (cache_changed & (OVL_UPDATE_SURF |
                             OVL_UPDATE_SRC  ) ) {
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.linear_offset,
			   mmio, SP_LINOFF(engine));
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.stride,
			   mmio, SP_STRIDE(engine));
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.tiled_offset,
			   mmio, SP_TILEOFF(engine));
		EMGD_DEBUG("SP_LINOFF = 0x%08lx", spr_cache[engine].spr_cache_regs.linear_offset);
		EMGD_DEBUG("SP_STRIDE = 0x%08lx", spr_cache[engine].spr_cache_regs.stride);
		EMGD_DEBUG("SP_TILEOFF = 0x%08lx", spr_cache[engine].spr_cache_regs.tiled_offset);
	}

    /* Write the color correction registers */
    if (cache_changed & OVL_UPDATE_CC) {
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.cont_bright,
		   mmio, SP_CLRC0(engine));
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.satn_hue,
		   mmio, SP_CLRC1(engine));
    }

	/* Write the gamma */
    if (cache_changed & OVL_UPDATE_GAMMA) {
		for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
			/* program register */
			/* TAKE NOTE! The register offsets are in opposite direction
			 * Of how the values were stored. We programmed the values
			 * of GAM-0 in the gamma_regs[0] and GAM-5 in gamma_regs[]5.
			 * However, the register offset in B-Spec is the opposite
			 * direction as GAM-5 has the lowest register offset addr
			 */
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.gamma_regs[i],
			   mmio, SP_GAMC5(engine) + ((SPR_REG_GAMMA_NUM - 1 - i)*4));
		}
	}

	/* Write the colorkey data */
    if (cache_changed & OVL_UPDATE_COLORKEY) {
		/* Dest color key */
		if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {
			/* Write the regs needed to turn it on */
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.dstckey_value,
				mmio, PLANE(display)->plane_reg + 0x14);
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.dstckey_mask,
				mmio, PLANE(display)->plane_reg + 0x18);

			/* Turn on the src key enable for Plane */
			plane_control = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg);
			plane_control |= KEY_ENABLE;
			WRITE_MMIO_REG_VLV(plane_control, MMIO(display), PLANE(display)->plane_reg);

			/* Need to trigger so that the plan control reg is updated */
			spr_cache[engine].spr_cache_regs.trigger_plane = 1;
		}
		/* Source Color key */
		if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.ckey_value,
				   mmio, SP_KEYMINVAL(engine));
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.ckey_max,
				   mmio, SP_KEYMAXVAL(engine));
			WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.ckey_mask,
				   mmio, SP_KEYMASK(engine));
		}
	}

    if (cache_changed & OVL_UPDATE_CONST_ALPHA)
    {
    	/* one more checking level */
    	if (0 != spr_cache[engine].spr_cache_regs.const_alpha)
    	{
    		WRITE_MMIO_REG_VLV((spr_cache[engine].spr_cache_regs.const_alpha),
    				   mmio, SP_CONSTALPHA(engine));
    	}
    }


	/* Write the control register first */
	WRITE_MMIO_REG_VLV((spr_cache[engine].spr_cache_regs.control | BIT31),
		   mmio, SP_CNTR(engine));
	EMGD_DEBUG("SP_CNTR = 0x%08lx", spr_cache[engine].spr_cache_regs.control);

}

int sprite_calc_regs_vlv(
	igd_display_context_t *display,
	int                  engine,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags,
	void *regs)
{
	unsigned int ckey_low, ckey_high;
	emgd_crtc_t *emgd_crtc = (emgd_crtc_t *) display;
	spr_reg_cache_vlv_t *spr_flip_regs = (spr_reg_cache_vlv_t *)regs;

	EMGD_TRACE_ENTER;

	spr_flip_regs->dest_pos_x1y1 = ( (dest_rect->y1 << 16) | dest_rect->x1 );
	spr_flip_regs->dest_size_w_h = ( (dest_rect->y2 - dest_rect->y1 - 1) << 16) |
							  (dest_rect->x2 - dest_rect->x1 - 1);

	if (src_surf->igd_pixel_format & PF_TYPE_YUV) {
               if((dest_rect->x2 - dest_rect->x1)%2){
                       --dest_rect->x2;
                       EMGD_DEBUG("We're capping the dest width to an even value - HW limit!");
               }
	}

	spr_flip_regs->src_base = (i915_gem_obj_ggtt_offset(src_surf->bo) & ~0xfff);

#ifdef CONFIG_PAVP_VIA_FIRMWARE
	spr_flip_regs->src_base |= (READ_MMIO_REG_VLV(MMIO(display), SP_SURF(engine)) & BIT2);
#else /*CONFIG_PAVP_VIA_FIRMWARE*/
	if(flags & IGD_OVL_SURFACE_ENCRYPTED){
		EMGD_DEBUG("Not handling encrypted surface in kernel!");
	}
#endif

	spr_flip_regs->stride = src_surf->base.DRMFB_PITCH;
	if(src_surf->igd_flags & IGD_SURFACE_TILED){
		spr_flip_regs->tiled_offset = ((src_rect->y1)<<16) | (src_rect->x1);
		spr_flip_regs->linear_offset = 0;
		spr_flip_regs->control |= BIT10;
	} else {
		spr_flip_regs->linear_offset = (src_rect->y1 * src_surf->base.DRMFB_PITCH) +
			(src_rect->x1 * (IGD_PF_BPP(src_surf->igd_pixel_format)/8));
		spr_flip_regs->tiled_offset = 0;
		spr_flip_regs->control &= ~BIT10;
	}

	spr_flip_regs->control &= ~SPR_CMD_SRCFMTMASK;

	/* src pixel format */
	switch(src_surf->igd_pixel_format){
		case IGD_PF_YUV422_PACKED_YUY2:
			spr_flip_regs->control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
		case IGD_PF_YUV422_PACKED_UYVY:
			spr_flip_regs->control |= (SPR_CMD_YUV422 | SPR_CMD_UYVY);
			break;
		case IGD_PF_YUV422_PACKED_YVYU:
			spr_flip_regs->control |= (SPR_CMD_YUV422 | SPR_CMD_YVYU);
			break;
		case IGD_PF_YUV422_PACKED_VYUY:
			spr_flip_regs->control |= (SPR_CMD_YUV422 | SPR_CMD_VYUY);
			break;
		case IGD_PF_RGB16_565:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_RGB565);
			break;
		case IGD_PF_ARGB8_INDEXED:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ARGB8IDX);
			break;
		case IGD_PF_ARGB32_8888:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ARGB32);
			break;
		case IGD_PF_xRGB32_8888:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xRGB32);
			break;
		case IGD_PF_ABGR32_8888:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ABGR32);
			break;
		case IGD_PF_xBGR32_8888:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xBGR32);
			break;
		case IGD_PF_ABGR32_2101010:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_ABGR32_2101010);
			break;
		case IGD_PF_xBGR32_2101010:
			spr_flip_regs->control |= (SPR_CMD_RGB_CONVERSION | SPR_CMD_xBGR32_2101010);
			break;
		default:
			EMGD_ERROR("Invalid pixel format: 0x%lx", src_surf->igd_pixel_format);
			/* treat like IGD_PF_YUV422_PACKED_YUY2 */
			spr_flip_regs->control |= (SPR_CMD_YUV422 | SPR_CMD_YUYV);
			break;
	}

	/* Turn off YUV to RGB conversion if the src is RGB */
	if (!(src_surf->igd_pixel_format & PF_TYPE_YUV)) {
		spr_flip_regs->control |= BIT19;
	} else {
		spr_flip_regs->control &= ~BIT19;
	}

	if(flags & IGD_OVL_ALTER_INTERLEAVED) {
		spr_flip_regs->control |= BIT21;
		if(flags & IGD_OVL_ALTER_FLIP_ODD) {
			if (0 == (src_rect->y1 & 1))
            	src_rect->y1 += 1;
		}
		else{
			if (0 != (src_rect->y1 & 1)) 
                src_rect->y1 += 1;
		}
	} else {
		spr_flip_regs->control &= ~BIT21;
	}

	micro_sprite_update_color_correct_vlv(display,
			engine, src_surf, &ovl_info->color_correct, 0, spr_flip_regs);

	micro_sprite_update_gamma_vlv(display,
			engine, &ovl_info->gamma, 0, spr_flip_regs);

	EMGD_DEBUG("Color key.flags: 0x%08lx", ovl_info->color_key.flags);
	/* Update the Destination colorkey */
	if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {
		EMGD_DEBUG("Overlay Enable Dest Color Key");
		spr_flip_regs->dstckey_value =
			convert_color_key_to_hw(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest);

		spr_flip_regs->dstckey_mask =
			convert_color_key_to_mask(PLANE(display)->fb_info->igd_pixel_format,
			 ovl_info->color_key.dest);

	} else {
		EMGD_DEBUG("Overlay Disable Dest Color Key");
	}

	/* Source Color key */
	if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
		EMGD_DEBUG("Overlay Enable Src Color Key");

		ckey_high = (convert_color_key_to_hw(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_hi) & 0x00FFFFFF);
		spr_flip_regs->ckey_max = yuv_to_uvy(src_surf->igd_pixel_format,
				       ckey_high);

		ckey_low = (convert_color_key_to_hw(src_surf->igd_pixel_format,
			 ovl_info->color_key.src_lo) & 0x00FFFFFF);
		spr_flip_regs->ckey_value = yuv_to_uvy(src_surf->igd_pixel_format,
				      ckey_low);

		spr_flip_regs->ckey_mask = 0x7;
		spr_flip_regs->control |= BIT22;

	} else {
		EMGD_DEBUG("Overlay Disable Src Color Key");
		spr_flip_regs->control &= ~BIT22;
		spr_flip_regs->ckey_mask = 0;
	}

	/* Check if user requested to update the constant alpha value */
	if(ovl_info->pipeblend.has_const_alpha) {
		spr_flip_regs->const_alpha = BIT31;
		spr_flip_regs->const_alpha |= ovl_info->pipeblend.const_alpha;
	}

	/* Z-ORDER configuration
     * To take current z-order configuration bits from crtc */
    if ((0 == engine) || (2 == engine))
    {
		spr_flip_regs->control &= ~(0x00000007);
		spr_flip_regs->control |= emgd_crtc->sprite1->zorder_control_bits;
		spr_flip_regs->trigger_other_ovl = 1;
	}
    else if ((1 == engine) || (3 == engine))
    {
    	spr_flip_regs->control &= ~(0x00000007);
    	spr_flip_regs->control |= emgd_crtc->sprite2->zorder_control_bits;
       	spr_flip_regs->trigger_other_ovl = 1;
    }

	spr_flip_regs->control |= BIT31;

	EMGD_TRACE_EXIT;
	return 0;
}


/*----------------------------------------------------------------------
 * Function: micro_sprite_update_regs_vlv()
 *
 * Description:
 * Examine the incoming sprite parameters, and update the sprite hardware
 * regs according to what changed.
 *
 * Returns:
 *   != 0 on Error
 *   0 on Success
 *----------------------------------------------------------------------*/

static unsigned int micro_sprite_update_regs_vlv(
	igd_display_context_t *display,
	int                  engine,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	int ret;
	int cache_changed;
	emgd_crtc_t *emgd_crtc = (emgd_crtc_t *) display;

	EMGD_TRACE_ENTER;

	/* Fast path for turning off overlay. No need for cache */
	if ((flags & IGD_OVL_ALTER_ON) == IGD_OVL_ALTER_OFF) {
		ret = micro_sprite_disable_vlv(display, engine, flags);

		/* Reset the cache */
		spr_cache_needs_init[engine] = TRUE;

		EMGD_TRACE_EXIT;
		return ret;
	}

	/* Init the cache if needed */
	if (spr_cache_needs_init[engine]) {
		micro_sprite_clear_cache_vlv(display, engine, flags);
	}

	/* See what has changed in the cache */
	cache_changed = get_cache_changes_vlv(
		src_surf, src_rect, dest_rect, ovl_info,
		flags, &spr_cache[engine], engine);

	/* ----------------------------------------------------------*/
	/* Has our destination rectangle changed? */
	if (cache_changed & OVL_UPDATE_DEST) {
		micro_sprite_update_dest_vlv(engine, src_surf, dest_rect);
	}

	/* ----------------------------------------------------------*/
	/* Always update the source pointers every frame. */
	ret = micro_sprite_update_src_ptr_vlv(engine, src_surf, src_rect,
				flags, ovl_info);
	if (ret) {
		EMGD_ERROR("Sprite updating src failed");
		EMGD_TRACE_EXIT;
		return ret;
	}

	/* ----------------------------------------------------------*/
	/* Did either the src rect or surface change? */
    if (cache_changed & (OVL_UPDATE_SURF |
                             OVL_UPDATE_SRC  ) ) {
		ret = micro_sprite_update_src_vlv(display,
					engine, src_surf, src_rect, flags);
		if (ret) {
			EMGD_ERROR("Sprite updating src failed");
			EMGD_TRACE_EXIT;
			return ret;
		}
	}

	/* ----------------------------------------------------------*/
	/* Did the color correction information change? */
    if (cache_changed & (OVL_UPDATE_CC) ) {
		ret = micro_sprite_update_color_correct_vlv(display,
					engine, src_surf, &ovl_info->color_correct, 0, NULL);
		if (ret) {
			EMGD_ERROR("Sprite video quality failed");
			EMGD_TRACE_EXIT;
			return ret;
		}
	}

	/* ----------------------------------------------------------*/
	/* Did the gamma change? */
    if (cache_changed & OVL_UPDATE_GAMMA) {
		ret = micro_sprite_update_gamma_vlv(display,
					engine, &ovl_info->gamma, 0, NULL);
		if (ret) {
			EMGD_ERROR("Sprite gamma failed");
			EMGD_TRACE_EXIT;
			return ret;
		}

	}

	/* ----------------------------------------------------------*/
	/* Did the color key change? */
    if (cache_changed & OVL_UPDATE_COLORKEY) {
		micro_sprite_update_colorkey_vlv(display,
				engine, src_surf, ovl_info, flags);
	}

    /* ----------------------------------------------------------*/
    /* Did the const alpha  configuration change? */
	if (cache_changed & OVL_UPDATE_CONST_ALPHA)
	{
		set_spr_const_alpha_vlv(display, engine,
			&ovl_info->pipeblend, 0);
	}

    /* Z-ORDER configuration
     * To take current z-order configuration bits from crtc */
    if ((0 == engine) || (2 == engine))
    {
    	if (spr_cache[engine].spr_cache_regs.control != emgd_crtc->sprite1->zorder_control_bits)
    	{
    		spr_cache[engine].spr_cache_regs.control &= ~(0x00000007);
    		spr_cache[engine].spr_cache_regs.control |=
    					emgd_crtc->sprite1->zorder_control_bits;
        	spr_cache[engine].spr_cache_regs.trigger_other_ovl = 1;
    	}
    }
    else if ((1 == engine) || (3 == engine))
    {
    	if (spr_cache[engine].spr_cache_regs.control != emgd_crtc->sprite2->zorder_control_bits)
    	{
    		spr_cache[engine].spr_cache_regs.control &= ~(0x00000007);
    		spr_cache[engine].spr_cache_regs.control |=
    					emgd_crtc->sprite2->zorder_control_bits;
        	spr_cache[engine].spr_cache_regs.trigger_other_ovl = 1;
    	}
    }

	/* Now write all the changes to the part */
	micro_sprite_program_cached_regs_vlv(display, engine,
			ovl_info, cache_changed, flags);

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

unsigned int micro_sprite_flush_instr_vlv(
	igd_display_context_t     *display, int engine,
	unsigned int               flags)
{
	int temp = 0;
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	int is_alpha_in_display_plane = 0;
	igd_context_t *context = NULL;
	int other_engine_map[4] = {1,0,3,2};
	int other_engine = other_engine_map[engine];
	int other_sprite_control = 0;
	int other_sprite_surface = 0;
	int zorder_control_bits = 0;
	int ret = 0;
#ifdef USE_RING_TRIGGER
	uint32_t dword0, dword1, dword2, dword3 = 0;
#endif
	
	EMGD_TRACE_ENTER;

	dev = ((struct drm_crtc *)(&display->base))->dev;
	dev_priv = (drm_emgd_private_t *) dev->dev_private;
	context = dev_priv->context;
	/* Take current display's plane control register and check color format
	 * NOTE: You can not use here (display->igd_plane->fb_info->igd_pixel_format) variable,
	 * because in clone mode one display's surface belongs to both (2, two) crtcs,
	 * and when you changing value for crtc 0 in (display->igd_plane->fb_info->igd_pixel_format),
	 * it becomes incorrect for crtc 1 yet, and vice versa.
	 * So, better to read display's control register. */
	is_alpha_in_display_plane = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg);
	/* Bit 26 is indicate about alpha channel */
	is_alpha_in_display_plane = (is_alpha_in_display_plane & DSP_ALPHA) >> 26;

	/* if FB_Blend mode is on and display plane does not have alpha channel enabled yet,
	 * lets enable it*/
	if ((display->fb_blend_ovl_on == 1) && (is_alpha_in_display_plane == 0) )
	{
		ret = context->mode_dispatch->kms_change_plane_color_format((emgd_crtc_t *) display, 1);
		if (ret != 0)
		{
			EMGD_ERROR("kms_change_plane_color_format Failed!");
			return ret;
		}
	}

	/* if fb_blend_ovl flag was changed dynamically within runtime:
	 * from 1 to 0, it's was disabled*/
	if ((display->fb_blend_ovl_on == 0) && (is_alpha_in_display_plane == 1) )
	{
		ret = context->mode_dispatch->kms_change_plane_color_format((emgd_crtc_t *) display, 0);
		if (ret != 0)
		{
			EMGD_ERROR("kms_change_plane_color_format Failed!");
			return ret;
		}
	}

	/* FIXME: Is it desired to use ring to trigger this? */
	if(spr_cache[engine].spr_cache_regs.trigger_plane) {
		/*
		 * CHECKME: updating plane start address register as trigger, B-Spec
		 * not clear about which start address register - DSPACNTR or DPSASURF
		 */
		temp = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg + 0x1c);
		WRITE_MMIO_REG_VLV(temp, MMIO(display), PLANE(display)->plane_reg + 0x1c);
		spr_cache[engine].spr_cache_regs.trigger_plane = 0;
	}

	/* Sprite enablement, so write the control register first! 
	 * The control register is NOT the trigger register on 2nd flip onwards!  
	 * Also, dont forget that we DONT cache the enable bit!
	 * */
	/* Enable sprite only if surface available - src_base is not NULL(0)  */
	if ( spr_cache[engine].spr_cache_regs.src_base != 0)
	{
		EMGD_DEBUG("spr_cache[engine].spr_cache_regs.src_base %lx",
								spr_cache[engine].spr_cache_regs.src_base);
		WRITE_MMIO_REG_VLV((spr_cache[engine].spr_cache_regs.control | BIT31),
					/* we dont cache the enable bit though*/
					MMIO(display), SP_CNTR(engine));
#ifdef USE_RING_TRIGGER
		/* Set up ring to trigger the respective sprite */
		if (engine < 2) {
			dword0 = (MI_DISPLAY_FLIP_I915 | MI_DISPLAY_FLIP_PLANE(engine + 2));
		} else {
			/* Sprite C = 0x5 while Sprite D = 0x4 for bit 21:19 */
			dword0 = (MI_DISPLAY_FLIP_I915 | MI_DISPLAY_FLIP_PLANE((engine == 0x2)?5:4));
		}
		dword1 = ( (spr_cache[engine].spr_cache_regs.stride & 0x0000FFC0) |
			   ((spr_cache[engine].spr_cache_regs.control & 0x00000400) >> 10) );
		dword2 = spr_cache[engine].spr_cache_regs.src_base;
		dword3 = MI_NOOP;

		ret = BEGIN_LP_RING(4);
		if (ret) {
			EMGD_ERROR("Error allocating ring buffer");
			return ret;
		}

		OUT_RING(dword0);
		OUT_RING(dword1);
		OUT_RING(dword2);
		OUT_RING(dword3);
		ADVANCE_LP_RING();
#else
		/* For micro, this is NOT an instruction, so it happens immediately,
		 * we do not have to poll waiting for it. */
		WRITE_MMIO_REG_VLV(spr_cache[engine].spr_cache_regs.src_base,
			MMIO(display), SP_SURF(engine));
#endif

		spr_cache[engine].spr_cache_regs.enabled = 1;
	}

	/* if another one sprite enabled on exactly this crtc and...
	 * if z-order value for this crtc was changes,
	 * so we needs to update second control register */
	if (1 == spr_cache[engine].spr_cache_regs.trigger_other_ovl)
	{
		other_sprite_control = READ_MMIO_REG_VLV(MMIO(display), SP_CNTR(other_engine));
		other_sprite_surface = READ_MMIO_REG_VLV(MMIO(display), SP_SURF(other_engine));
		other_sprite_control &= ~(0x00000007);
		/* get control bit from crtc
		 * engine 0 & 2 mean sprites with plane_id 2 and 3.
		 * Keep it in mind! */
		if ((0 == other_engine) || (2 == other_engine))
			zorder_control_bits = display->sprite1->zorder_control_bits;
		else
			zorder_control_bits = display->sprite2->zorder_control_bits;

		/* update cash and control register */
		other_sprite_control |= zorder_control_bits;
		spr_cache[other_engine].spr_cache_regs.control = other_sprite_control;
		WRITE_MMIO_REG_VLV(other_sprite_control, MMIO(display), SP_CNTR(other_engine));
		/* trig other_engine sprite plane */
		WRITE_MMIO_REG_VLV(other_sprite_surface, MMIO(display), SP_SURF(other_engine));
	}
	/* don't needs to trig other sprite plane any more,
	 * so, erase flag */
	spr_cache[engine].spr_cache_regs.trigger_other_ovl = 0;

	/* FIXME - once we have GEM and ring buffer, we should use command instruction
	 * to dispatch the overlay flip above and also get a sync for the sprite flip
	 */
	ovl_context->sync[engine] = 0;
	
	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

int micro_prepare_spr_vlv(
	igd_display_context_t *display,
	int                  engine,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* Check to ensure the overlay can be used given the current mode as
	 * well as what the IAL is asking for.  If not return an error. */
	ret = spr_check_vlv(display, engine, src_surf, src_rect,
					dest_rect, ovl_info,flags);
	if (ret) {
		EMGD_ERROR("SPRITE_CHECK_VLV_RET failed");
		return ret;
	}

	/* Check if last flip is still pending.
	 * This is necessary for the following reasons:
	 *    - If the previous instructions have not been processed, then the
	 *      spritec_regs_vlv is still in use and can not be overwritten.
	 */
	ret = query_spr_vlv(display, engine, IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE);
	if ((FALSE == ret) &&
		(flags & IGD_OVL_ALTER_ON)) {
		EMGD_ERROR("SPRITE_QUERY_VLV_RET failed");
		/* Only return an error if the overlay is on.  If turning it off,
		* allow it to continue, since something may have failed and we
		* should try our best to turn the overlay off. */
		return -IGD_ERROR_HWERROR;
	}

	/* Update all Overlay Update Registers */
	ret = micro_sprite_update_regs_vlv(display, engine,
			src_surf, src_rect, dest_rect, ovl_info, flags);
	if (ret) {
		EMGD_ERROR("Sprite update Registers failed");
		return ret;
	}

	EMGD_TRACE_EXIT;
	return ret;
}

int sprite_commit_regs_vlv(igd_display_context_t *display,
	int                  engine,
	igd_ovl_info_t      *ovl_info,
	void *regs)
{
	unsigned char * mmio = MMIO(display);
	unsigned int temp = 0;
	unsigned long plane_control;
	int i;
	int other_sprite_control = 0;
	int other_sprite_surface = 0;
	int other_engine_map[4] = {1,0,3,2};
	int other_engine = other_engine_map[engine];
	int zorder_control_bits = 0;

	spr_reg_cache_vlv_t *spr_flip_regs = (spr_reg_cache_vlv_t *)regs;

	/*
	 * Now write all the changed registers to the HW
	*/

	/* Write dest rect information */
	WRITE_MMIO_REG_VLV(spr_flip_regs->dest_pos_x1y1,
			mmio, SP_POS(engine));
	WRITE_MMIO_REG_VLV(spr_flip_regs->dest_size_w_h,
			mmio, SP_SIZE(engine));
	EMGD_DEBUG("SP_ENGINE = %d", engine);
	EMGD_DEBUG("SP_POS = 0x%08lx", spr_flip_regs->dest_pos_x1y1);
	EMGD_DEBUG("SP_SIZE = 0x%08lx", spr_flip_regs->dest_size_w_h);

	/* Write source information */
	WRITE_MMIO_REG_VLV(spr_flip_regs->linear_offset,
		   mmio, SP_LINOFF(engine));
	WRITE_MMIO_REG_VLV(spr_flip_regs->stride,
		   mmio, SP_STRIDE(engine));
	WRITE_MMIO_REG_VLV(spr_flip_regs->tiled_offset,
		   mmio, SP_TILEOFF(engine));
	EMGD_DEBUG("SP_LINOFF = 0x%08lx", spr_flip_regs->linear_offset);
	EMGD_DEBUG("SP_STRIDE = 0x%08lx", spr_flip_regs->stride);
	EMGD_DEBUG("SP_TILEOFF = 0x%08lx", spr_flip_regs->tiled_offset);

	WRITE_MMIO_REG_VLV(spr_flip_regs->cont_bright,
	   mmio, SP_CLRC0(engine));
	WRITE_MMIO_REG_VLV(spr_flip_regs->satn_hue,
	   mmio, SP_CLRC1(engine));

	/* Write the gamma */
	for (i = 0; i < SPR_REG_GAMMA_NUM; i++) {
		/* program register */
		/* TAKE NOTE! The register offsets are in opposite direction
		 * Of how the values were stored. We programmed the values
		 * of GAM-0 in the gamma_regs[0] and GAM-5 in gamma_regs[]5.
		 * However, the register offset in B-Spec is the opposite
		 * direction as GAM-5 has the lowest register offset addr
		 */
		WRITE_MMIO_REG_VLV(spr_flip_regs->gamma_regs[i],
		   mmio, SP_GAMC5(engine) + ((SPR_REG_GAMMA_NUM - 1 - i)*4));
	}

	/* Write the colorkey data */
	/* Dest color key */
	if (ovl_info->color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE) {
		/* Write the regs needed to turn it on */
		WRITE_MMIO_REG_VLV(spr_flip_regs->dstckey_value,
			mmio, PLANE(display)->plane_reg + 0x14);
		WRITE_MMIO_REG_VLV(spr_flip_regs->dstckey_mask,
			mmio, PLANE(display)->plane_reg + 0x18);

		/* Turn on the src key enable for Plane */
		plane_control = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg);
		plane_control |= KEY_ENABLE;
		WRITE_MMIO_REG_VLV(plane_control, MMIO(display), PLANE(display)->plane_reg);

		/* Need to trigger so that the plan control reg is updated */
		spr_flip_regs->trigger_plane = 1;
	}
	/* Source Color key */
	if (ovl_info->color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE) {
		WRITE_MMIO_REG_VLV(spr_flip_regs->ckey_value,
			   mmio, SP_KEYMINVAL(engine));
		WRITE_MMIO_REG_VLV(spr_flip_regs->ckey_max,
			   mmio, SP_KEYMAXVAL(engine));
		WRITE_MMIO_REG_VLV(spr_flip_regs->ckey_mask,
			   mmio, SP_KEYMASK(engine));
	}

	if (ovl_info->pipeblend.has_const_alpha)
	{
		WRITE_MMIO_REG_VLV((spr_flip_regs->const_alpha),
				   mmio, SP_CONSTALPHA(engine));
	} else {
		if (spr_flip_regs->const_alpha) {
			WRITE_MMIO_REG_VLV(0x00, mmio, SP_CONSTALPHA(engine));

			spr_flip_regs->const_alpha = 0x00;
		}
	}

	/* Write the control register first */
	WRITE_MMIO_REG_VLV(spr_flip_regs->control, mmio, SP_CNTR(engine));
	WRITE_MMIO_REG_VLV(spr_flip_regs->control, mmio, SP_CNTR(engine));
	EMGD_DEBUG("SP_CNTR = 0x%08lx", spr_flip_regs->control);

	temp = READ_MMIO_REG_VLV(MMIO(display), PLANE(display)->plane_reg + 0x1c);
	WRITE_MMIO_REG_VLV(temp, MMIO(display), PLANE(display)->plane_reg + 0x1c);

	WRITE_MMIO_REG_VLV(spr_flip_regs->src_base,
		MMIO(display), SP_SURF(engine));
	EMGD_DEBUG("SP_SURF = 0x%08lx", spr_flip_regs->src_base);

	spr_flip_regs->enabled = 1;

	/* if another one sprite enabled on exactly this crtc and...
	 * if z-order value for this crtc was changes,
	 * so we needs to update second control register */
	if (1 == spr_flip_regs->trigger_other_ovl)
	{
		other_sprite_control = READ_MMIO_REG_VLV(MMIO(display), SP_CNTR(other_engine));
		other_sprite_surface = READ_MMIO_REG_VLV(MMIO(display), SP_SURF(other_engine));
		other_sprite_control &= ~(0x00000007);

		if ((0 == other_engine) || (2 == other_engine))
			zorder_control_bits = display->sprite1->zorder_control_bits;
		else
			zorder_control_bits = display->sprite2->zorder_control_bits;

		other_sprite_control |= zorder_control_bits;
		if (spr_flip_regs->other_sprite_control != other_sprite_control) {
			spr_flip_regs->other_sprite_control = other_sprite_control;

			WRITE_MMIO_REG_VLV(other_sprite_control, MMIO(display), SP_CNTR(other_engine));
			WRITE_MMIO_REG_VLV(other_sprite_surface, MMIO(display), SP_SURF(other_engine));
		}
	}

	return 0;
}

int micro_alter_sprite_vlv(igd_display_context_t *display,
	int                  engine,
	igd_appcontext_h     appctx,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	int ret=0;

	EMGD_TRACE_ENTER;

	if (micro_prepare_spr_vlv(display, engine, src_surf,
			src_rect, dest_rect, ovl_info, flags)) {
		return -IGD_ERROR_HWERROR;
	}

	/* micro_prepare_spr_vlv would have done a fast path to 
	 * directly turn off the sprite planes. No RB instruction
	 * necessary after that - so skip this is we're turning
	 * it OFF!
	 */
	if(flags & IGD_OVL_ALTER_ON){
		ret = micro_sprite_flush_instr_vlv(display, engine, flags);
	}

	EMGD_TRACE_EXIT;
	return ret;
}

