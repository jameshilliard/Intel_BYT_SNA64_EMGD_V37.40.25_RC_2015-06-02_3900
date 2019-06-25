/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: spr_snb_cache.c
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
 *  Support functions for overlay caching.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.overlay

#include <igd_core_structs.h>
#include "spr_cache_snb.h"


/* Checks to see what, if anything has changed.
 * Clears bits in the command and config register that are invalid.
 * Returns a set of flags telling what changed */
unsigned int get_cache_changes_snb(
	emgd_framebuffer_t *src_surf,
	igd_rect_t     *src_rect,
	igd_rect_t     *dest_rect,
	igd_ovl_info_t *ovl_info,
	unsigned int    flags,
	spr_cache_snb_t * spr_cache,
	int pipe)
{
	unsigned int cache_changed = 0;

	/* Have the flags changed? */
	if (spr_cache->flags != flags) {
		spr_cache->flags = flags;
		cache_changed |= OVL_UPDATE_FLAGS;
	}

	/* Do a comparison to source surface */
	if (is_changed_surf(&spr_cache->src_surf, src_surf)) {
		cache_changed |= OVL_UPDATE_SURF;
		copy_or_clear_surf(&spr_cache->src_surf, src_surf);
	}

	/* Has our scaling ratio changed? */
	if (is_changed_scaling_ratio(&spr_cache->src_rect,  src_rect,
			 &spr_cache->dest_rect, dest_rect)) {
		cache_changed |= OVL_UPDATE_SCALING;
		/* no data to copy since below checks will catch it */
	}

	/* Do a comparison to source rectangle */
	if (is_changed_rect(&spr_cache->src_rect, src_rect)) {
		cache_changed |= OVL_UPDATE_SRC;
		copy_or_clear_rect(&spr_cache->src_rect, src_rect);
	}

	/* Has our destination rectangle changed? */
	if (is_changed_rect(&spr_cache->dest_rect, dest_rect)) {
		cache_changed |= OVL_UPDATE_DEST;
		copy_or_clear_rect(&spr_cache->dest_rect, dest_rect);
	}

	/* Do a comparison to overlay info color key */
	if (is_changed_color_key(&spr_cache->ovl_info, ovl_info)) {
		cache_changed |= OVL_UPDATE_COLORKEY;
		copy_or_clear_color_key(&spr_cache->ovl_info, ovl_info);
	}

	/* Do a comparison to overlay info video quality */
	if (is_changed_colorcorrect(&spr_cache->ovl_info, ovl_info)) {
		cache_changed |= OVL_UPDATE_CC;
		copy_or_clear_colorcorrect(&spr_cache->ovl_info, ovl_info);
	}

	/* Do a comparison to overlay info gamma */
	if (is_changed_gamma(&spr_cache->ovl_info, ovl_info)) {
		cache_changed |= OVL_UPDATE_GAMMA;
		copy_or_clear_gamma(&spr_cache->ovl_info, ovl_info);
	}

	return cache_changed;
}

