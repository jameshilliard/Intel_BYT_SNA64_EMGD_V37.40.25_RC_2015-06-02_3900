/* -*- pse-c -*-
 *----------------------------------------------------------------------------
 * Filename: spr_snb_cache.h
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
 *-----------------------------------------------------------------------------
 * Description:
 *  This is the internal header file for overlay caching. It should be not be
 *  by any other module besides the overlay module itself.
 *-----------------------------------------------------------------------------
 */
#ifndef _SPRITE_SNB_CACHE_H
#define _SPRITE_SNB_CACHE_H

#include "spr_regs_snb.h"
#include "../cmn/ovl_cache_util.h"
/*
 * In order to cache the overlay registers we need a structure
 * that defines and wraps them.  We already have one for the primary
 * display, called spr_reg_image_snb_t.
 * Here we define a structure for the secondary display
 */

typedef struct _spr_reg_cache_snb{
	unsigned long control;
	unsigned long linear_offset;
	unsigned long stride;
	unsigned long dest_pos_x1y1;
	unsigned long dest_size_w_h;
	unsigned long ckey_value;
	unsigned long ckey_mask;
	unsigned long src_base;
	unsigned long ckey_max;
	unsigned long tiled_offset;
	unsigned long scaler_ctl;
	unsigned long gamma_regs[SPR_REG_GAMMA_NUM];
	unsigned long gamma_max[3];
} spr_reg_cache_snb_t;

/*
 * This stucture caches the overlay state, so we don't have to
 * re-program everything for every single frame
 */
typedef struct _spr_cache_snb {
	emgd_framebuffer_t   src_surf;
	igd_rect_t           src_rect;
	igd_rect_t           dest_rect;
	igd_ovl_info_t       ovl_info;
	unsigned long        flags;
	spr_reg_cache_snb_t  spr_cache_regs;
} spr_cache_snb_t, *pspr_cache_snb_t;


/* Checks to see what, if anything has changed.
 * Clears bits in the command and config register that are invalid.
 * Returns a set of flags telling what changed */
unsigned int get_cache_changes_snb(
	emgd_framebuffer_t  *src_surf,
	igd_rect_t     *src_rect,
	igd_rect_t     *dest_rect,
	igd_ovl_info_t *ovl_info,
	unsigned int    flags,
	pspr_cache_snb_t spr_cache,
	int pipe);


#endif /* _SPRITE_SNB_CACHE_H */