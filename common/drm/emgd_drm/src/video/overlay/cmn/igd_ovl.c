/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_ovl.c
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
 *  
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.overlay

#include <memory.h>
#include <io.h>

#include <igd_core_structs.h>

#include <mode_access.h>

#include "ovl_dispatch.h"
#include "ovl_virt.h"

void _overlay_shutdown(igd_context_t *context);

extern ovl_dispatch_t ovl_dispatch_snb[];
extern ovl_dispatch_t ovl_dispatch_vlv[];

static dispatch_table_t ovl_dispatch_list[] = {

#ifdef CONFIG_SNB
	{PCI_DEVICE_ID_VGA_SNB, &ovl_dispatch_snb},
#endif
#ifdef CONFIG_IVB
	{PCI_DEVICE_ID_VGA_IVB, &ovl_dispatch_vlv},
#endif
#ifdef CONFIG_VLV
	{PCI_DEVICE_ID_VGA_VLV, &ovl_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV2, &ovl_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV3, &ovl_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV4, &ovl_dispatch_vlv},
#endif

	{0, NULL}
};

ovl_context_t ovl_context[1];

/* Description: Turns off video plane that was turned by fw/EFI. To be called by
 * alter_ovl if needed.
 *
 * Notes:Upon video splash being turned on, in order to reset it, need to call
 * secondary ovl dispatch function to turn off the right registers
 */
void igd_reset_fw_ovl(igd_display_context_t *display)
{
	ovl_dispatch_t	*ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;
	ovl_dispatch[OVL_SECONDARY].alter_ovl(display, NULL,
				NULL, NULL, NULL, NULL, IGD_OVL_ALTER_OFF);
}

static int igd_alter_ovl(igd_display_h display_h,
	unsigned long ovl_plane_id,
	igd_appcontext_h     appctx,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	igd_display_context_t *display =
		(igd_display_context_t *)display_h;
	ovl_dispatch_t    *ovl_dispatch =
		(ovl_dispatch_t *)ovl_context->dispatch;
	int ret = 0;

	EMGD_TRACE_ENTER;

	if(flags == IGD_FW_VIDEO_OFF)
	{
		igd_reset_fw_ovl(display);
		return IGD_SUCCESS;
	}

	/* Is the overlay is being turned off? */
	if ((flags & IGD_OVL_ALTER_ON) == IGD_OVL_ALTER_OFF) {
				
		/* Call family dependent overlay 
		 * function to alter the overlay. */
		ret = ovl_dispatch[ovl_plane_id-1].alter_ovl(display,
					  appctx,
					  src_surf,
					  src_rect,
					  dest_rect,
					  ovl_info,
					  flags);
		EMGD_TRACE_EXIT;
		return ret;
	}
	
	/* Call family dependent overlay function 
	 * to alter the overlay. */
	ret = ovl_dispatch[ovl_plane_id-1].alter_ovl(display,
				appctx,
				src_surf,
				src_rect,
				dest_rect,
				ovl_info,
				flags);

	if (ret != IGD_SUCCESS) {
		/* do we really need to turn off the overlay on a failure? */

		/* Turn the overlay off when there is an error */
		/*ovl_dispatch[ovl_plane_id-1].alter_ovl(display, NULL,
						NULL, NULL, NULL, NULL, 
						IGD_OVL_ALTER_OFF);*/
	}

	EMGD_TRACE_EXIT;
	return ret;
}

static int igd_calc_spr_regs(igd_display_h display_h,
	unsigned long ovl_plane_id,
	emgd_framebuffer_t  *src_surf,
	igd_rect_t          *src_rect,
	igd_rect_t          *dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags,
	void *regs)
{
	igd_display_context_t *display =
		(igd_display_context_t *)display_h;
	ovl_dispatch_t    *ovl_dispatch =
		(ovl_dispatch_t *)ovl_context->dispatch;
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* Call family dependent overlay function 
	 * to alter the overlay. */
	ret = ovl_dispatch[ovl_plane_id-1].igd_spr_dispatch->calc_spr_regs(display, 
				ovl_plane_id-1,
				src_surf,
				src_rect,
				dest_rect,
				ovl_info,
				flags,
				regs);

	EMGD_TRACE_EXIT;
	return ret;
}

static int igd_commit_spr_regs(igd_display_h display_h,
	unsigned long ovl_plane_id,
	igd_ovl_info_t      *ovl_info,
	void *regs)
{
	igd_display_context_t *display =
		(igd_display_context_t *)display_h;
	ovl_dispatch_t    *ovl_dispatch =
		(ovl_dispatch_t *)ovl_context->dispatch;
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* Call family dependent overlay function 
	 * to alter the overlay. */
	ret = ovl_dispatch[ovl_plane_id-1].igd_spr_dispatch->commit_spr_regs(display, 
				ovl_plane_id-1,
				ovl_info,
				regs);

	EMGD_TRACE_EXIT;
	return ret;
}

static int igd_query_ovl(igd_display_h display_h,
	unsigned long ovl_plane_id,
	unsigned int flags)
{
	igd_display_context_t *display = (igd_display_context_t *)display_h;
	ovl_dispatch_t    *ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;
	int ret = FALSE;

	if (flags == IGD_OVL_QUERY_NUM_PLANES) {
		return ovl_context->num_planes;
	}

	/* This is a bit of a short circuit (since the hardware dependent functions
	 * are not being called), but to determine if the hardware overlay is on,
	 * just check the state of the ovl_context. */
	if (flags == IGD_OVL_QUERY_IS_ON) {
		if (ovl_context->state[ovl_plane_id-1] == OVL_STATE_ON) {
			return TRUE;
		} else {
			return FALSE;
		}
	}

	ret = ovl_dispatch[ovl_plane_id-1].query_ovl(
			display, (flags & IGD_OVL_QUERY_MASK));
	if (ret == FALSE) {
		/* Can only return TRUE (event has occured and capability
		 * is available) if it is TRUE for all displays */
		return FALSE;
	}

	return ret;
}
static int igd_query_max_size_ovl(igd_display_h display_h,
	unsigned long ovl_plane_id,
	unsigned long pf,
	unsigned int *max_width,
	unsigned int *max_height)
{
	igd_display_context_t *display = (igd_display_context_t *)display_h;
	ovl_dispatch_t *ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;
	unsigned int tmp_max_width, tmp_max_height;
	int ret = -IGD_ERROR_INVAL;

	*max_width =  0x10000000;
	*max_height = 0x10000000;

	ret = ovl_dispatch[ovl_plane_id-1].query_max_size_ovl(
			display, pf,&tmp_max_width, &tmp_max_height);
	if (ret != IGD_SUCCESS) {
		/* Can only return IGD_SUCCESS (no error)
		 * if there is no error for all displays */
		return ret;
	}
	if (tmp_max_width < *max_width) {
		*max_width = tmp_max_width;
	}
	if (tmp_max_height < *max_height) {
		*max_height = tmp_max_height;
	}

	return ret;
}

/* This is a wrapper function that will call the device specific alter_ovl_osd
 * function. Currently this function is used by video driver to map the subpicture 
 * surface to second overlay so that it blends with the video surface on first 
 * overlay.
 */
static int igd_alter_ovl_osd(igd_display_h display_h,
	unsigned long ovl_plane_id,
	igd_appcontext_h appctx,
	emgd_framebuffer_t *sub_surface,
	igd_rect_t *sub_src_rect,
	igd_rect_t *sub_dest_rect,
	igd_ovl_info_t      *ovl_info,
	unsigned int         flags)
{
	ovl_dispatch_t  *ovl_dispatch =
		(ovl_dispatch_t *)ovl_context->dispatch;
	igd_display_context_t *display =
		(igd_display_context_t *)display_h;
	igd_display_context_t *primary, *secondary;
	igd_context_t * context;
	int cur_ovl, ret;
	unsigned long dc;

	/* Determine which display this overlay belongs to */
	/* In either case, for OSD, the 2nd overlay of this pipe is what's used */
	context = display->igd_context;
	context->module_dispatch.dsp_get_dc(&dc, &primary, &secondary);
	if(display == primary) {
		cur_ovl = 1;
	} else if (display == secondary) {
		if(ovl_context->num_planes>2){
			cur_ovl = 3;
		} else {
			cur_ovl = 1;
		}
	} else {
		/* shouldn't get here. */
		EMGD_ERROR("Invalid display for subpicture alter_ovl!");
		return  -IGD_ERROR_INVAL;
	}

	ret = 0;
	ret = ovl_dispatch[cur_ovl].alter_ovl(display, appctx,
			sub_surface, sub_src_rect, sub_dest_rect,
			ovl_info, (flags | IGD_OVL_SPRITE2_ON_PIPE));

	return ret;
}

/*----------------------------------------------------------------------
 * Function: igd_alter_ovl_pipeblend()
 * Description:
 *  igd_alter_ovl_pipeblend will probably be called by user
 *  space driver or compositor to set the over plane z-order.
 *  See the igd_ovl.h for details on this.
 *
 *----------------------------------------------------------------------*/
int igd_alter_ovl_attr(igd_display_h display_h,
		unsigned long ovl_plane_id,
		igd_ovl_pipeblend_t * pblend,
		int update_pipeblend, int query_pipeblend,
		igd_ovl_color_correct_info_t * ccorrect,
		int update_colorcorrect,
		igd_ovl_gamma_info_t * gamma,
		int update_gamma)
{
	int ret = IGD_SUCCESS, cur_pipe, ovl_master;
	ovl_dispatch_t  *ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;

	EMGD_TRACE_ENTER;

	/* Determine which display this overlay belongs to */
	if(ovl_dispatch[ovl_plane_id-1].target_pipe == ovl_dispatch[0].target_pipe) {
		cur_pipe = 0;
	} else {
		cur_pipe = 1;
	}

	if((update_pipeblend || query_pipeblend) && !pblend){
		EMGD_ERROR("invalid pblend ptr!");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}
	if(update_colorcorrect && !ccorrect){
		EMGD_ERROR("invalid colorcorrection ptr!");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}
	if(update_gamma && !gamma){
		EMGD_ERROR("invalid gamma ptr!");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	/*
	 * For querying Fb-Blend-Ovl and for Zorder, only need to call the first plane
	 * a.k.a. master plane for each pipe to handle this
	 */
	if(ovl_context->num_planes > 2) {
		/* first sprite of this display = idx 0/2,
		 * first sprite of other display = idx 2/0
		 */
		ovl_master = cur_pipe * 2;
		/*other_ovl = ((cur_pipe)?0:1)*2;*/
	} else {
		/* first sprite of this display = idx 0,
		 * first sprite of other display = idx 1
		 */
		ovl_master = cur_pipe;
		/*other_ovl = (cur_pipe)?0:1;*/
	}

	/* if its a query, just get the "combined" union version
	 * that we store in our global ovl_context (pipe specific)
	 */
	if(query_pipeblend) {
		pblend->zorder_combined = ((igd_display_context_t*)display_h)->zorder;
		pblend->fb_blend_ovl = ((igd_display_context_t*)display_h)->fb_blend_ovl_on;
	} else if(update_pipeblend) {
		/* not supported by the HAL DD layer*/
		pblend->zorder_combined = IGD_OVL_ZORDER_INVALID;
		pblend->fb_blend_ovl = 0x0;
	}

	if(update_pipeblend) {
		/* If we're here, we need to change the z-order
		 * ensure that we dont have anything invalid
		 * see bit definitions - mutually exclusive */
		if( (pblend->top & pblend->middle) ||
			(pblend->top & pblend->bottom) ||
			(pblend->middle & pblend->bottom) ){
			return  -IGD_ERROR_INVAL;
		}

		ret = IGD_SUCCESS;
		/* some DD layer implementation is not supported */
		if (ovl_dispatch[ovl_plane_id-1].alter_pipeblend) {
			ret = ovl_dispatch[ovl_plane_id-1].alter_pipeblend(
						(igd_display_context_t *)display_h, pblend);
			if (ret) {
				EMGD_ERROR("HAL DD layer for pipeblend failed!");
			}
		}
	}

	if(update_colorcorrect) {

		ret = IGD_SUCCESS;
		/* some DD layer implementation is not supported */
		if (ovl_dispatch[ovl_plane_id-1].alter_colorcorrect) {
			ret = ovl_dispatch[ovl_plane_id-1].alter_colorcorrect(
					(igd_display_context_t *)display_h, ccorrect);
			if (ret) {
				EMGD_ERROR("HAL DD layer for colorcorrection failed!");
			}
		}
	}

	if(update_gamma) {

		ret = IGD_SUCCESS;
		/* some DD layer implementation is not supported */
		if (ovl_dispatch[ovl_plane_id-1].alter_gamma) {
			ret = ovl_dispatch[ovl_plane_id-1].alter_gamma(
					(igd_display_context_t *)display_h, gamma);
			if (ret) {
				EMGD_ERROR("HAL DD layer for gamma failed!");
			}
		}
	}

	EMGD_TRACE_EXIT;
	return ret;
}

/*----------------------------------------------------------------------
 * Function: igd_get_ovl_init_params(ovl_um_context_t *ovl_um_context)
 * Description:
 *
 * Notes in Usage:
 * the user mode caller fills in the primary and secondary display handles.
 *
 *----------------------------------------------------------------------*/
int igd_get_ovl_init_params(igd_driver_h driver_handle,
		igd_ovl_caps_t * ovl_caps)
{
	int count = 0;

	ovl_dispatch_t  *ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;

	count = ovl_context->num_planes;
	if(!count){
		EMGD_ERROR("Unexpected overlay plane count - module uninitialized?");
		ovl_caps->num_planes = 0;
		return -IGD_ERROR_INVAL;
	}

	if(ovl_caps->ovl_planes && ovl_caps->num_planes >= count) {

		while(count){
			--count;
			ovl_caps->ovl_planes[count].plane_id =
					ovl_dispatch[count].plane_id;
			ovl_caps->ovl_planes[count].plane_name =
					ovl_dispatch[count].plane_name;
			ovl_caps->ovl_planes[count].target_igd_pipe_ptr =
					 (void *) ovl_dispatch[count].target_pipe;
			ovl_caps->ovl_planes[count].max_downscale =
					ovl_dispatch[count].max_downscale;
			ovl_caps->ovl_planes[count].max_upscale =
					ovl_dispatch[count].max_upscale;
			ovl_caps->ovl_planes[count].num_gamma =
					ovl_dispatch[count].num_gamma;
			ovl_caps->ovl_planes[count].has_ccorrect =
					ovl_dispatch[count].has_ccorrect;
			ovl_caps->ovl_planes[count].has_constalpha =
					ovl_dispatch[count].has_constalpha;
			ovl_caps->ovl_planes[count].supported_pixel_formats =
					ovl_dispatch[count].supported_pfs;
		}
	}

	ovl_caps->num_planes = ovl_context->num_planes;

	return IGD_SUCCESS;
}

int _overlay_full_init(igd_context_t *context,
			igd_param_t *params)
{
	ovl_dispatch_t * ovl_dispatch;
	int i;
	EMGD_TRACE_ENTER;

	/* Ensure the device context pointer is valid */
	if(!context){
		EMGD_ERROR_EXIT("Error: Null context");
		return -IGD_ERROR_INVAL;
	}

	/* Clear the allocated memory for overlay context */
	OS_MEMSET((void *)ovl_context, 0, sizeof(ovl_context_t));

	/* Get overlay's dispatch table */
	ovl_context->dispatch = (ovl_dispatch_t (*)[])dispatch_acquire(context,
		ovl_dispatch_list);
	if(!ovl_context->dispatch) {
		EMGD_ERROR_EXIT("Unsupported Device");
		return -IGD_ERROR_NODEV;
	}

	/* Hook up the IGD dispatch table entries for overlay
	 * Alter has a common function, query can call the family function
	 * directly */
	context->drv_dispatch.get_ovl_init_params = igd_get_ovl_init_params;
	context->drv_dispatch.alter_ovl           = igd_alter_ovl;
	context->drv_dispatch.query_ovl           = igd_query_ovl;
	context->drv_dispatch.query_max_size_ovl  = igd_query_max_size_ovl;
	context->drv_dispatch.alter_ovl_osd       = igd_alter_ovl_osd;
	context->drv_dispatch.alter_ovl_attr      = igd_alter_ovl_attr;

	context->drv_dispatch.calc_spr_regs       = igd_calc_spr_regs;
	context->drv_dispatch.commit_spr_regs     = igd_commit_spr_regs;

	/* Hook up optional inter-module functions */
	context->module_dispatch.overlay_shutdown = _overlay_shutdown;

	/* lets find out how many overlay or sprite planes this HW supports*/
	i=0;
	ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;
	while(ovl_dispatch[i].alter_ovl) {
		++i;
	}
	EMGD_DEBUG(" device dispatch count = %d planes",i);
	if(i > IGD_OVL_MAX_HW){
		i = IGD_OVL_MAX_HW;
		EMGD_ERROR("Ovl-Full-Init device dispatch count = %d planes!",i);
	}
	ovl_context->num_planes = i;

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}

void _overlay_shutdown(igd_context_t *context)
{

	EMGD_TRACE_ENTER;

	EMGD_TRACE_EXIT;

	return;
}

/*----------------------------------------------------------------------
 * Function: igd_overlay_pwr()
 * Description:
 *  igd_overlay_pwr will only be called from power module .
 *  It shuts down / powers up overlay output based on the power
 *  state transition requested.
 *
 * Notes in Usage:
 *
 *----------------------------------------------------------------------*/
int igd_overlay_pwr(
	igd_driver_h driver_handle,
	int power_state)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	ovl_dispatch_t  *ovl_dispatch = (ovl_dispatch_t *)ovl_context->dispatch;
	igd_display_context_t *primary, *secondary, *tmp;
	int count = 0;

	/* NOTE: The overlay will not be turned on by this method, but will be
	 * turned on with the next call to alter_ovl */

	/* Turn off the overlay for every display.  Most of the displays will
	 * likely not have the overlay off, but there should be no harm it turning
	 * it off for a display which does not have the overlay on. */

	EMGD_TRACE_ENTER;
	if(power_state != IGD_POWERSTATE_D0){

		context->module_dispatch.dsp_get_dc(NULL, &primary, &secondary);
		count = ovl_context->num_planes;
		if(!count){
			return IGD_SUCCESS;
		}
		while(count){
			--count;
			if(ovl_dispatch[count].target_pipe == primary->igd_pipe){
				tmp = primary;
			} else if(ovl_dispatch[count].target_pipe == secondary->igd_pipe){
				tmp = secondary;
			} else {
				continue;
			}
			ovl_dispatch[count].alter_ovl(tmp, NULL,
					NULL, NULL, NULL, NULL, IGD_OVL_ALTER_OFF);
		}
	}
	
	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}
