/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: spr_snb.c
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
 *  This file contains function that actually programs the second
 *  overlay with the bits to properly configure the overlay
 *  Also includes functions to execute the second overlay flip
 *  instruction, and query the overlay flip status.
 *  Also contains some hardware capabilities querrying functions
 *  for upper overlay layer to get this chips second overlay
 *  capabilities
 *-----------------------------------------------------------------------------
 */
/* Referenced from Napa. Need change later */

#define MODULE_NAME hal.overlay

#include <io.h>
#include <memory.h>
#include <math_fix.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <gn6/regs.h>
#include "spr_snb.h"
#include "spr_regs_snb.h"
#include "../cmn/ovl_dispatch.h"
#include "../cmn/ovl_virt.h"

#include "emgd_drm.h"

extern unsigned long sprite_pixel_formats_snb[];
extern os_alarm_t spr_timeout[];

/*----------------------------------------------------------------------
 * Function: spr_check_pf_snb()
 * Parameters: unsigned int requested_pixel_format -
 *             according to definitions in igd_mode.h
 *
 * Description:
 *
 * Returns:
 *   TRUE on Success
 *   FALSE on The first pixel format that is supported
 *----------------------------------------------------------------------*/
static unsigned int spr_check_pf_snb(
	igd_display_context_t *display,
	unsigned int requested_pixel_format)
{
	unsigned long *spr_pf = sprite_pixel_formats_snb;
	int temp_loop = 0;

	while(spr_pf[temp_loop]) {
		if(spr_pf[temp_loop] == requested_pixel_format) {
			return TRUE;
		}
		++temp_loop;
	}

	return FALSE;
}

unsigned int spr_check_snb(igd_display_context_t *display,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	igd_timing_info_t *timing;

	EMGD_TRACE_ENTER;

	if (!display){
	    EMGD_ERROR_EXIT("display is null");
	    return -IGD_ERROR_INVAL;
	}
	if (!PIPE(display)){
	    EMGD_ERROR_EXIT("PIPE(display) is null");
	    return -IGD_ERROR_INVAL;
	}

	/* The following parameters are only valid if the overlay is on, so
	 * return success if the overlay is being turned off. */
	if ((flags & IGD_OVL_ALTER_ON) == IGD_OVL_ALTER_OFF) {
		EMGD_TRACE_EXIT;
		return IGD_SUCCESS;
	}

	timing = PIPE(display)->timing;
	if(!timing) {
	    EMGD_ERROR_EXIT("timing is null");
	    return -IGD_ERROR_INVAL;
	}

	/*************************************************************************
	 * Ensure the overlay surface is ok and can be properly displayed.
	 * This ensures the following is valid:
	 *    - Ensure x1, x2, y1, y2 are pixel aligned
	 *    - 2 pixels or greater in width and height
	 *    - Pixel format is supported by the overlay
	 *    - Pitch is <= 16KB
	 *    - Based on the pixel format, the width is supported
	 *************************************************************************/
	if (!src_surf){
	    EMGD_ERROR_EXIT("src_surf is null");
	    return -IGD_ERROR_INVAL;
	}
	if (!src_rect){
	    EMGD_ERROR_EXIT("src_rect is null");
	    return -IGD_ERROR_INVAL;
	}
	/* Get the minimum size of 1 pixel in width and height for y, u, and v.
	 */
	if (((src_rect->x2 - src_rect->x1) < 3) ||
		((src_rect->y2 - src_rect->y1) < 3)) {
		EMGD_ERROR_EXIT(
			"Sprite source width or height is < 3 pixels (%dx%d)",
			src_rect->x2 - src_rect->x1, src_rect->y2 - src_rect->y1);
		return -IGD_ERROR_INVAL;
	}

	if (FALSE == spr_check_pf_snb(display, src_surf->igd_pixel_format)) {
		EMGD_ERROR_EXIT("Overlay2 source pixel format unsupported (pf:0x%lx)",
			src_surf->igd_pixel_format);
		return -IGD_ERROR_HWERROR;
	}

	if (src_surf->base.DRMFB_PITCH > 32768) {
		EMGD_ERROR_EXIT("Overlay2 source pitch (%d) > 32KB",
			src_surf->base.DRMFB_PITCH);
		return -IGD_ERROR_HWERROR;
	}

	/*************************************************************************
	 * Ensure the location on the framebuffer is ok and can be properly
	 * displayed
	 * This ensures the following is valid:
	 *    - Greater than 1 pixel width and height
	 *    - Will be displayed on screen (not panned off)
	 *************************************************************************/
	if (!dest_rect){
	    EMGD_ERROR_EXIT("dest_rect is null");
	    return -IGD_ERROR_INVAL;
	}
	if (((dest_rect->x2 - dest_rect->x1) < 3) ||
		((dest_rect->y2 - dest_rect->y1) < 3)) {
		EMGD_ERROR_EXIT(
			"Sprite dest width or height is less than 3 (%dx%d)",
			dest_rect->x2 - dest_rect->x1, dest_rect->y2 - dest_rect->y1);
		return -IGD_ERROR_INVAL;
	}

	if ((dest_rect->x1 >= timing->width) ||
		(dest_rect->y1 >= timing->height)) {
		EMGD_ERROR_EXIT(
			"Sprite dest is panned off the screen (%d,%d)",
			dest_rect->x1, dest_rect->y1);
		return -IGD_ERROR_INVAL;
	}

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

int query_spr_snb(igd_display_context_t *display,
	unsigned int flags)
{
#ifndef HACKY_FLIP_PENDING
	inter_module_dispatch_t *md;
	int ret;
#endif
	os_alarm_t timeout;
	unsigned long sync;
	int pipe;

	//EMGD_TRACE_ENTER;

	switch (flags) {
	case IGD_OVL_QUERY_IS_HW_SUPPORTED:
		/* This is the second overlay, so HW overlay is not supported */
		break;

	case IGD_OVL_QUERY_IS_LAST_FLIP_DONE:
		/* For SNB, sprite-A is tied to pipe-A and sprite-B is tied to pipe-B*/
		pipe = (PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)?1:0;
		sync = ovl_context->sync[pipe];

		/* If there no sync to wait on, then the last flip is done, and the
		 * Register Update has occured, simply return TRUE (Flip done).
		 */
		if (!sync) {
			//EMGD_DEBUG("Overlay already synced");
			EMGD_TRACE_EXIT;
			return TRUE;
		}

		/*FIXME - our overlay flip pending isnt really working
		 * for SNB yet. Is there is a hacky way to guess if were done?
		 */
#ifdef HACKY_FLIP_PENDING
		if(OS_TEST_ALARM(spr_timeout[pipe])){
			sync = 0;
		} else{
			//EMGD_DEBUG("Overlay Sync-%d Hacky Check-Flip not done", pipe);
			return FALSE;
		}

#else
		/* Wait for vblank */
		/* Check to see if the plane C flip is pending.  If is is pending
		 * return FALSE (Flip not done). */
		if(display->context->dispatch.sync(
			display,
			IGD_PRIORITY_NORMAL,
			&sync,
			IGD_SYNC_NONBLOCK)) {
			//EMGD_DEBUG("Overlay Sync Check - Flip not done");
			return FALSE;
		}
		/*
		 * For SNB, we have seperate flip bits for both display
		 * and sprite planes, need to see if these check_flip_pending
		 * functions can work for multiple planes - else we need to
		 * have additional params at inter module level
		 */
		md = &display->context->mod_dispatch;
		platform = (platform_context_snb_t *)display->context->
						platform_context;
		if(md && md->check_flip_pending){
			if(md->check_flip_pending(MMIO(display), DEIIR)){
				//EMGD_DEBUG("Overlay Sync done but Flip not done");
				return FALSE;
			}
		}

#endif

		/* Now that we know the last flip is done and the register update is
		 * complete, set the sync to 0 and return TRUE (Flip done). */
		ovl_context->sync[pipe] = 0;
		break;
	case IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE:
		/* Wait for 200 milliseconds for the last flip to complete.  If not
		 * done in that time, there is likely a hardware problem so return
		 * FALSE. */
		timeout = OS_SET_ALARM(50);
		do {
			if (TRUE ==
				query_spr_snb(display, IGD_OVL_QUERY_IS_LAST_FLIP_DONE)) {
				//EMGD_TRACE_EXIT;
				return TRUE;
			}
		} while (!OS_TEST_ALARM(timeout));
		EMGD_ERROR_EXIT("Timeout waiting for last flip done");
		return FALSE;
		break;
	case IGD_OVL_QUERY_IS_GAMMA_SUPPORTED:
		return TRUE;
		break;
	case IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED:
		return FALSE;
		/* FIXME, SNB doesnt support brightness, contrast, saturation
		 * and hue - need to see if we can use gamma ramp to help fix this
		 * but for now we can simply return no since we dont support this.
		 */
		break;
	}

	//EMGD_TRACE_EXIT;
	return TRUE;
}



int query_max_size_spr_snb(
	igd_display_context_t *display,
	unsigned long pf,
	unsigned int *max_width,
	unsigned int *max_height)
{
	EMGD_TRACE_ENTER;

	/* FIXME: Should this be size of FB? */
	*max_width = 2048;
	*max_height = 2048;

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}
