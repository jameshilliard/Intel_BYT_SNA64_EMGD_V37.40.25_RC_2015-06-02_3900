/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: ovl_cache_util.c
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
 *  Support functions for overlay caching.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.overlay

#include <memory.h>
#include <igd_core_structs.h>

#include "emgd_drm.h"

/*
 * Caching helper functions.  Can be used by any DD layer function as they see fit
 */

/* Tells if a rectangle has changed */
int is_changed_rect(
		    igd_rect_t *old_rect,
		    igd_rect_t *new_rect)
{
	return ((old_rect->x1 != new_rect->x1) ||
		(old_rect->x2 != new_rect->x2) ||
		(old_rect->y1 != new_rect->y1) ||
		(old_rect->y2 != new_rect->y2) );
}

/* Tells if a surface has changed */
int is_changed_surf(
		emgd_framebuffer_t *old_surf,
		emgd_framebuffer_t *new_surf)
{
	return( (old_surf->base.DRMFB_PITCH !=  new_surf->base.DRMFB_PITCH)  ||
			(old_surf->base.width       !=  new_surf->base.width)  ||
			(old_surf->base.height      !=  new_surf->base.height) ||
			(old_surf->igd_pixel_format !=  new_surf->igd_pixel_format) ||
			(old_surf->u_pitch      !=  new_surf->u_pitch)      ||
			(old_surf->v_pitch      !=  new_surf->v_pitch)      ||
			(old_surf->igd_flags    !=  new_surf->igd_flags)    );
}

/* Tells if the color key has changed */
int is_changed_color_key(
			igd_ovl_info_t *old_info,
			igd_ovl_info_t *new_info)
{
	return ( (old_info->color_key.src_lo != new_info->color_key.src_lo)  ||
		 (old_info->color_key.src_hi != new_info->color_key.src_hi)  ||
		 (old_info->color_key.dest   != new_info->color_key.dest)    ||
		 (old_info->color_key.flags  != new_info->color_key.flags)   );

}

/* Tells if the video quality has changed */
int is_changed_colorcorrect(
			igd_ovl_info_t *old_info,
			igd_ovl_info_t *new_info)
{
	return ( (old_info->color_correct.contrast !=
				new_info->color_correct.contrast)        ||
			 (old_info->color_correct.brightness !=
				new_info->color_correct.brightness)      ||
			 (old_info->color_correct.saturation !=
				new_info->color_correct.saturation)      ||
			 (old_info->color_correct.hue != 
				new_info->color_correct.hue));

}

/* Tells if the gamma has changed */
int is_changed_gamma(
			igd_ovl_info_t *old_info,
			igd_ovl_info_t *new_info)
{
	if(old_info->gamma.flags != new_info->gamma.flags){
		return 1;
	}
	if( (   (old_info->gamma.red   != new_info->gamma.red)   ||
			(old_info->gamma.green != new_info->gamma.green) ||
			(old_info->gamma.blue  != new_info->gamma.blue)  ) &&
		(new_info->gamma.flags == IGD_OVL_GAMMA_ENABLE) ) {
		return 1;
	}
	return 0;
}

/* Tells if the scaling ratio changed */
int is_changed_scaling_ratio(
		igd_rect_t * old_src_rect,
		igd_rect_t * new_src_rect,
		igd_rect_t * old_dst_rect,
		igd_rect_t * new_dst_rect)
{
	return (
			((old_src_rect->x2-old_src_rect->x1) != (new_src_rect->x2-new_src_rect->x1)) ||
			((old_src_rect->y2-old_src_rect->y1) != (new_src_rect->y2-new_src_rect->y1)) ||
			((old_dst_rect->x2-old_dst_rect->x1) != (new_dst_rect->x2-new_dst_rect->x1)) ||
			((old_dst_rect->y2-old_dst_rect->y1) != (new_dst_rect->y2-new_dst_rect->y1))  );
}

/* Tells if the pipe-blending config has changed */
int is_changed_pipeblend(
		igd_ovl_pipeblend_t * pipeblend)
{
	return (pipeblend->enable_flags);
}

/* Copies a source rectangle to the cache */
void copy_or_clear_rect(
			igd_rect_t *cache_rect,
			igd_rect_t * new_rect)
{
	if (new_rect) {
		OS_MEMCPY(cache_rect,
			  new_rect,
			  sizeof(igd_rect_t));
	} else {
		OS_MEMSET(cache_rect,
			  0,
			  sizeof(igd_rect_t));
	}
}

/* Copies a surface to the cache */
void copy_or_clear_surf(
		emgd_framebuffer_t * cache_surf,
		emgd_framebuffer_t * new_surf)
{
	if (new_surf) {
		OS_MEMCPY(cache_surf,
			  new_surf,
			  sizeof(emgd_framebuffer_t));
	} else {
		OS_MEMSET(cache_surf,
			  0,
			  sizeof(emgd_framebuffer_t));
	}
}

/* Copies the color key to the cache */
void copy_or_clear_color_key(
			igd_ovl_info_t *cache_info,
			igd_ovl_info_t * new_info)
{
	if (new_info) {
		OS_MEMCPY(&cache_info->color_key,
			  &new_info->color_key,
			  sizeof(igd_ovl_color_key_info_t));
	} else {
		OS_MEMSET(&cache_info->color_key,
			  0,
			  sizeof(igd_ovl_color_key_info_t));
	}
}

/* Copies the color correction info to the cache */
void copy_or_clear_colorcorrect(
			igd_ovl_info_t *cache_info,
			igd_ovl_info_t * new_info)
{
	if (new_info) {
		OS_MEMCPY(&cache_info->color_correct,
			  &new_info->color_correct,
			  sizeof(igd_ovl_color_correct_info_t));
	} else {
		OS_MEMSET(&cache_info->color_correct,
			  0,
			  sizeof(igd_ovl_color_correct_info_t));
	}
}

/* Copies the gamma to the cache */
void copy_or_clear_gamma(
			igd_ovl_info_t *cache_info,
			igd_ovl_info_t * new_info)
{
	if (new_info) {
		OS_MEMCPY(&cache_info->gamma,
			  &new_info->gamma,
			  sizeof(igd_ovl_gamma_info_t));
	} else {
		OS_MEMSET(&cache_info->gamma,
			  0,
			  sizeof(igd_ovl_gamma_info_t));
	}
}

/* Copies the pipe blending config info to the cache */
void copy_or_clear_pipeblend(
			igd_ovl_info_t *cache_info,
			igd_ovl_info_t * new_info)
{
	if (new_info) {
		OS_MEMCPY(&cache_info->pipeblend,
			  &new_info->pipeblend,
			  sizeof(igd_ovl_pipeblend_t));
	} else {
		OS_MEMSET(&cache_info->pipeblend,
			  0,
			  sizeof(igd_ovl_pipeblend_t));
	}
}

