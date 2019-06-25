/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: ovl_dispatch.h
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
 *  
 *-----------------------------------------------------------------------------
 */

#ifndef _OVERLAY_DISPATCH_H
#define _OVERLAY_DISPATCH_H

#include <pci.h>
#include <dispatch.h>
#include <igd_core_structs.h>

#define MAX_OVLPLANE_NAME   64

typedef struct _igd_spr_dispatch {
	int (*calc_spr_regs)(igd_display_context_t *display, int engine, emgd_framebuffer_t *src_surf, 
		igd_rect_t *src_rect, igd_rect_t *dest_rect, igd_ovl_info_t *ovl_info, 
		unsigned int flags, void *regs);
	int (*commit_spr_regs)(igd_display_context_t *display, int engine, igd_ovl_info_t *ovl_info, 
		void *regs);
} igd_spr_dispatch_t;

typedef struct _ovl_dispatch {
	int plane_id; /* HAL DD rule - this must be 0-based
			index for total number of ovl/sprites for this HW*/
	char * plane_name;
	igd_display_pipe_t * target_pipe;
	int max_downscale;
	int max_upscale;
	int num_gamma;
	int has_ccorrect;
	int has_constalpha;
	unsigned long * supported_pfs;
	int (*blend_surf_needed)(igd_display_context_t *display,
		emgd_framebuffer_t  *src_surf,
		igd_rect_t          *src_rect,
		igd_rect_t          *dest_rect,
		unsigned int         flags,
		emgd_framebuffer_t  *blend_surf,
		igd_rect_t          *blend_rect);
	int (*alter_ovl)(igd_display_context_t *display,
		igd_appcontext_h     appctx,
		emgd_framebuffer_t  *src_surf,
		igd_rect_t          *src_rect,
		igd_rect_t          *dest_rect,
		igd_ovl_info_t      *ovl_info,
		unsigned int         flags);
	int (*query_ovl)(igd_display_context_t *display,
		unsigned int flags);
	int (*query_max_size_ovl)(igd_display_context_t *display,
		unsigned long pf,
		unsigned int *max_width,
		unsigned int *max_height);
	int (*alter_pipeblend)(igd_display_context_t *display,
		igd_ovl_pipeblend_t * pipeblend);
	int (*alter_colorcorrect)(igd_display_context_t *display,
		igd_ovl_color_correct_info_t * ccorrect);
	int (*alter_gamma)(igd_display_context_t *display,
		igd_ovl_gamma_info_t * gamma);
	igd_spr_dispatch_t *igd_spr_dispatch;
} ovl_dispatch_t;

#endif
