/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_ovl.c
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
 *  Overlay kernel module functions.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.overlay

#include <drmP.h>
#include <i915_drm.h>
#include <drm_crtc_helper.h>
#include <linux/version.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "i915/intel_drv.h"
#include "drm_emgd_private.h"
#include "emgd_drv.h"
#include "emgd_drm.h"
#include <igd_core_structs.h>
#include <igd_ovl.h>
#include <memory.h>
#include "splash_video.h"
#include "default_config.h"
#include "i915/intel_drv.h"
#include "../video/overlay/gn7/spr_cache_vlv.h"

/*
 *********************************************************************
 * External declarations from elsewhere
 *********************************************************************
 */
/* From GEM --> */
extern int intel_pin_and_fence_fb_obj(struct drm_device *dev,
				      struct drm_i915_gem_object *obj,
				      struct intel_ring_buffer *pipelined);
/* From Sysfs --> */
void emgd_sysfs_ovlplane_add(drm_emgd_private_t *priv,
		igd_ovl_plane_t * emgd_ovlplane);
void emgd_sysfs_ovlplane_remove(igd_ovl_plane_t * emgd_ovlplane);

/*
 *********************************************************************
 * "EMGD_CORE_...OVLPLANE" functions
 * Core Overlay Plane management functions
 * common to both the EMGD private ioctl mechanism as well as the
 * kernel supported DRM callback mechanism
 *********************************************************************
 */
int emgd_core_update_ovlplane(drm_emgd_private_t * dev_priv,
			igd_ovl_plane_t   * plane, emgd_crtc_t * emgd_crtc,
			struct drm_i915_gem_object * bufobj, int pin,
			int          crtc_x, int          crtc_y,
			unsigned int crtc_w, unsigned int crtc_h,
			uint32_t     src_x,  uint32_t     src_y,
			uint32_t     src_w,  uint32_t     src_h,
			uint32_t     src_pitch, uint32_t     drm_fmt);
int emgd_core_disable_ovlplane(drm_emgd_private_t * dev_priv,
			igd_ovl_plane_t  * plane, int unpin);
/*
 *********************************************************************
 * "EMGD_DRM_...OVLPLANE"  functions
 * Kernel DRM Overlay Plane management callbacks
 * These are wrappers to above "Core" functions
 * These functions are callbacks exported to the upper drm layer but
 * only for kernels that support multi overlay planes
 * (kernel 3.4 above?)
 *********************************************************************
 */
static int emgd_drm_update_ovlplane(struct drm_plane *plane, struct drm_crtc *crtc,
			   struct drm_framebuffer *fb, int crtc_x, int crtc_y,
			   unsigned int crtc_w, unsigned int crtc_h,
			   uint32_t src_x, uint32_t src_y,
			   uint32_t src_w, uint32_t src_h);
int emgd_drm_disable_ovlplane(struct drm_plane *plane);
static void emgd_drm_destroy_ovlplane(struct drm_plane *plane);

static int emgd_drm_set_property(struct drm_plane *plane,
		struct drm_property *property,
		uint64_t value);

static const struct drm_plane_funcs emgd_plane_funcs = {
	.update_plane  = emgd_drm_update_ovlplane,
	.disable_plane = emgd_drm_disable_ovlplane,
	.destroy       = emgd_drm_destroy_ovlplane,
	.set_property  = emgd_drm_set_property,
};

unsigned int
get_zorder_control_values(unsigned long zorder_combined, int plane_id);

int
emgd_overlay_planes_init(struct drm_device *dev, emgd_crtc_t *emgd_crtc,
	emgd_ovl_config_t * ovl_config)
{
	igd_ovl_caps_t    ovl_caps;
	drm_emgd_private_t * dev_priv = (drm_emgd_private_t *)dev->dev_private;
	igd_context_t * context = dev_priv->context;
	int ret = 0, i;
	unsigned long possible_crtcs;

	EMGD_TRACE_ENTER;

	memset(&ovl_caps, 0, sizeof(igd_ovl_caps_t));

	if(!context->drv_dispatch.alter_ovl){
		EMGD_ERROR("No HAL overlay support!?");
		return -ENODEV;
	}
	/* Allocation and parameters filling done ones for all plane and all crtc's */
	/* if active crtc's more than 1 allocation will be done for 2th too together
	 * with allocation for 1th crtc */


	if(!dev_priv->ovl_planes){
		ret = context->drv_dispatch.get_ovl_init_params(
				(igd_driver_h)context, &ovl_caps);
		if(ret || !ovl_caps.num_planes){
			EMGD_ERROR("Cant populate ovl_planes array!");
			EMGD_TRACE_EXIT;
			return -EINVAL;
		}

		dev_priv->num_ovlplanes = ovl_caps.num_planes;

		if(dev_priv->num_ovlplanes > IGD_OVL_MAX_HW) {
			EMGD_ERROR("OVL HAL caps report more than IGD_OVL_MAX_HW!");
			dev_priv->num_ovlplanes = IGD_OVL_MAX_HW;
			ovl_caps.num_planes     = IGD_OVL_MAX_HW;
		}

		ovl_caps.ovl_planes = kzalloc(sizeof(igd_ovl_plane_t) *
				dev_priv->num_ovlplanes, GFP_KERNEL);
		if(!ovl_caps.ovl_planes){
			EMGD_ERROR("Cant alloc ovl_formats array!");
			EMGD_TRACE_EXIT;
			return -ENOMEM;
		}

		ret = context->drv_dispatch.get_ovl_init_params(
				(igd_driver_h)context, &ovl_caps);
		if(ret){
			EMGD_ERROR("Cant populate ovl_planes - Catastrophic ERR!");
			dev_priv->num_ovlplanes = 0;
			EMGD_TRACE_EXIT;
			return -EINVAL;
		}

		dev_priv->ovl_planes = ovl_caps.ovl_planes;

		i = 0;
		while(dev_priv->ovl_planes[0].supported_pixel_formats[i++]);

		dev_priv->num_ovldrmformats = i;
		dev_priv->ovl_drmformats = kzalloc(sizeof(uint32_t) * i, GFP_KERNEL);
		if(!dev_priv->ovl_drmformats){
			dev_priv->num_ovldrmformats = 0;
			EMGD_ERROR("Cant alloc ovl_formats array!");
			EMGD_TRACE_EXIT;
			return -ENOMEM;
		}

		for(i = 0; i < dev_priv->num_ovldrmformats; ++i){
			dev_priv->ovl_drmformats[i] =
				convert_pf_emgd_to_drm(dev_priv->ovl_planes[0].supported_pixel_formats[i]);
			if (dev_priv->ovl_drmformats[i] == -EINVAL) {
				EMGD_ERROR("Unknown pixel format; defaulting to YUYV format");
				EMGD_DEBUG("dev_priv->ovl_drmformats[%d] = %d",i, dev_priv->ovl_drmformats[i]);
				EMGD_DEBUG("dev_priv->ovl_planes[0].supported_pixel_formats[%d] = %u",
						i, (unsigned int)dev_priv->ovl_planes[0].supported_pixel_formats[i]);

				dev_priv->ovl_drmformats[i] = DRM_FORMAT_YUYV;
			}
		}

		for(i = 0; i <dev_priv->num_ovlplanes; ++i) {
			igd_ovl_gamma_info_t *gamma_src = NULL;
			igd_ovl_gamma_info_t *gamma_dst = NULL;
			igd_ovl_color_correct_info_t * cc_src = NULL;
			igd_ovl_color_correct_info_t * cc_dst = NULL;

			if(ovl_config){
				/* Something wrong with mem copy...
				 * Let's do it step by step*/
				//memcpy(&(dev_priv->ovl_planes[i].ovl_info.gamma),
				//	 &(ovl_config->ovl_configs[i].gamma), sizeof(igd_ovl_gamma_info_t));
				//memcpy(&(dev_priv->ovl_planes[i].ovl_info.color_correct),
				//	 &(ovl_config->ovl_configs[i].color_correct), sizeof(igd_ovl_color_correct_info_t));
				/*memcpy(&(dev_priv->ovl_planes[i].ovl_info.pipeblend),
					 &(ovl_config->ovl_pipeblend[(i/2)]), sizeof(igd_ovl_pipeblend_t));*/
				gamma_src = &(ovl_config->ovl_configs[i].gamma);
				gamma_dst = &(dev_priv->ovl_planes[i].ovl_info.gamma);
				cc_src = &(ovl_config->ovl_configs[i].color_correct);
				cc_dst = &(dev_priv->ovl_planes[i].ovl_info.color_correct);
				EMGD_DEBUG("gamma_src = %p", gamma_src);
				EMGD_DEBUG("gamma_dst = %p", gamma_dst);
				EMGD_DEBUG("cc_src = %p", cc_src);
				EMGD_DEBUG("cc_dst = %p", cc_dst);
				memcpy(gamma_dst, gamma_src, sizeof(igd_ovl_gamma_info_t) );
				memcpy(cc_dst, cc_src,sizeof(igd_ovl_color_correct_info_t) );
				dev_priv->ovl_planes[i].ovl_info.pipeblend.zorder_combined =
					 ovl_config->ovl_pipeblend[(i/2)].zorder_combined;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.fb_blend_ovl    =
					 ovl_config->ovl_pipeblend[(i/2)].fb_blend_ovl;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.has_const_alpha =
					 ovl_config->ovl_pipeblend[(i/2)].has_const_alpha;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.const_alpha     =
					 ovl_config->ovl_pipeblend[(i/2)].const_alpha;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.enable_flags    =
					 ovl_config->ovl_pipeblend[(i/2)].enable_flags;

				/* Call alter_ovl_attr to check the gamma min/max cap. */
				dev_priv->context->drv_dispatch.alter_ovl_attr(
					(igd_display_h) emgd_crtc, i + 1, NULL, 0, 0,
					NULL, 0, gamma_dst, 1);

			} else {
				/* initialize the gamma correction to the defaults */
				dev_priv->ovl_planes[i].ovl_info.gamma.red   = 0x100;
				dev_priv->ovl_planes[i].ovl_info.gamma.green = 0x100;
				dev_priv->ovl_planes[i].ovl_info.gamma.blue  = 0x100;
				dev_priv->ovl_planes[i].ovl_info.gamma.flags = IGD_OVL_GAMMA_DISABLE;

				/* initialize the color correction to the defaults */
				dev_priv->ovl_planes[i].ovl_info.color_correct.brightness = MID_BRIGHTNESS_YUV;
				dev_priv->ovl_planes[i].ovl_info.color_correct.contrast   = MID_CONTRAST_YUV;
				dev_priv->ovl_planes[i].ovl_info.color_correct.saturation = MID_SATURATION_YUV;
				dev_priv->ovl_planes[i].ovl_info.color_correct.hue        = MID_HUE_YUV;

				/* initialize the pipe blending values to the defaults */
				dev_priv->ovl_planes[i].ovl_info.pipeblend.zorder_combined =
							IGD_OVL_ZORDER_DEFAULT;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.fb_blend_ovl    = 0;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.has_const_alpha = 0;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.const_alpha     = 0;
				dev_priv->ovl_planes[i].ovl_info.pipeblend.enable_flags    = 0;
			}
			/* initialize the color keying values to the defaults */
			dev_priv->ovl_planes[i].ovl_info.color_key.src_lo = 0;
			dev_priv->ovl_planes[i].ovl_info.color_key.src_hi = 0;
			dev_priv->ovl_planes[i].ovl_info.color_key.dest   = 0;
			dev_priv->ovl_planes[i].ovl_info.color_key.flags  = (
					IGD_OVL_SRC_COLOR_KEY_DISABLE |IGD_OVL_DST_COLOR_KEY_DISABLE);

			/* setup sysfs interfaces for this plane's attributes */
			emgd_sysfs_ovlplane_add(dev_priv, &dev_priv->ovl_planes[i]);
		}
	}

	for(i = 0; i <dev_priv->num_ovlplanes; ++i) {

		if(dev_priv->ovl_planes[i].target_igd_pipe_ptr == emgd_crtc->igd_pipe) {
			/* dev_priv->ovl_planes[i].emgd_crtc = emgd_crtc;*/
			possible_crtcs = (1 << emgd_crtc->pipe);
			ret = drm_plane_init(dev, &(dev_priv->ovl_planes[i].base),
					possible_crtcs,
					&emgd_plane_funcs, dev_priv->ovl_drmformats,
					dev_priv->num_ovldrmformats, false);
			if (ret){
				EMGD_ERROR("Cant drm_plane_init overlay plane id-%d",
						dev_priv->ovl_planes[i].plane_id)
			}

			/* Initialize overlay plane properties */
			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.brightness,
					dev_priv->ovl_planes[i].ovl_info.color_correct.brightness);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.contrast,
					dev_priv->ovl_planes[i].ovl_info.color_correct.contrast);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.saturation,
					dev_priv->ovl_planes[i].ovl_info.color_correct.saturation);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.hue,
					dev_priv->ovl_planes[i].ovl_info.color_correct.hue);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.gamma_red,
					dev_priv->ovl_planes[i].ovl_info.gamma.red);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.gamma_green,
					dev_priv->ovl_planes[i].ovl_info.gamma.green);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.gamma_blue,
					dev_priv->ovl_planes[i].ovl_info.gamma.blue);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.gamma_enable,
					dev_priv->ovl_planes[i].ovl_info.gamma.flags);

			drm_object_attach_property(&(dev_priv->ovl_planes[i].base.base),
					dev_priv->ovl_properties.const_alpha,
					dev_priv->ovl_planes[i].ovl_info.pipeblend.const_alpha);

			spin_lock_init(&dev_priv->ovl_planes[i].commit_lock_workers);

			dev_priv->ovl_planes[i].regs = kzalloc(sizeof(spr_reg_cache_vlv_t), GFP_KERNEL);
			if(!dev_priv->ovl_planes[i].regs){
				EMGD_ERROR("Cant alloc ovl_planes[i].regs!");
				EMGD_TRACE_EXIT;
				return -ENOMEM;
			}
		}
	}

	/* assign sprites pointer in crtc structure */
	if (1 == emgd_crtc->crtc_id)
	{
		emgd_crtc->sprite1 = &dev_priv->ovl_planes[0];
		emgd_crtc->sprite2 = &dev_priv->ovl_planes[1];
	}
	else if (2 == emgd_crtc->crtc_id) /*Max crtc's num is 2*/
	{
		emgd_crtc->sprite1 =  &dev_priv->ovl_planes[2];
		emgd_crtc->sprite2 =  &dev_priv->ovl_planes[3];
	}
	else
	{
		EMGD_ERROR("crtc_id wrong-%d", emgd_crtc->crtc_id);
		ret = 1;
		EMGD_TRACE_EXIT;
		return ret;
	}

	emgd_crtc->sprite1->other_sprplane = emgd_crtc->sprite2;
	emgd_crtc->sprite2->other_sprplane = emgd_crtc->sprite1;

	/* reverse assignment */
	/* Yes, notation is terrible */
	emgd_crtc->sprite1->emgd_crtc = emgd_crtc;
	emgd_crtc->sprite2->emgd_crtc = emgd_crtc;

	emgd_crtc->sprite1->pipe = emgd_crtc->pipe;
	emgd_crtc->sprite2->pipe = emgd_crtc->pipe;

	/* default values, in cause if there is nothing default_config.c */
	emgd_crtc->zorder = 0x00010204;
	emgd_crtc->fb_blend_ovl_on = 0;
	emgd_crtc->sprite1->zorder_control_bits = 0;
	emgd_crtc->sprite2->zorder_control_bits = 0;

	/* to pickup values from default_config.c if it is present*/
	if (ovl_config)
	{
		emgd_crtc->zorder = ovl_config->ovl_pipeblend[emgd_crtc->crtc_id/2].zorder_combined;
		emgd_crtc->fb_blend_ovl_on = ovl_config->ovl_pipeblend[emgd_crtc->crtc_id/2].fb_blend_ovl;
		emgd_crtc->sprite1->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite1->plane_id);
		emgd_crtc->sprite2->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite2->plane_id);
	}

	EMGD_TRACE_EXIT;
	return ret;
}

void emgd_overlay_planes_cleanup(struct drm_device *dev)
{
	int i;
	igd_ovl_plane_t * plane = NULL;
	drm_emgd_private_t * dev_priv = (drm_emgd_private_t *)dev->dev_private;

	EMGD_TRACE_ENTER;

	/* First, close / free splash video hardware state and allocations */
	destroy_splash_video(dev_priv);

	/* Second, lets loop thru all the overlay planes and disable them if they
	 * have a framebuffer connected to it (i.e. its probably enabled)
	 * Take note that this function could be called after all crtc's are freed
	 * Thus, we need to use a special dispatch (not alter_ovl) to disable
	 */
	for(i=0; i< dev_priv->num_ovlplanes; ++i){
		plane = &dev_priv->ovl_planes[i];
		/*
		 * FIXME: emgd drm needs to check if the overlay sprites are on
		 * and disable them - need new HAL for disable ovl without crtc
		 */
		emgd_sysfs_ovlplane_remove(plane);
	}

	EMGD_TRACE_EXIT;
}

int emgd_core_update_ovlplane(drm_emgd_private_t * dev_priv,
			igd_ovl_plane_t   * plane, emgd_crtc_t * emgd_crtc,
			struct drm_i915_gem_object * newbufobj, int pin,
			int          crtc_x, int          crtc_y,
			unsigned int crtc_w, unsigned int crtc_h,
			uint32_t     src_x,  uint32_t     src_y,
			uint32_t     src_w,  uint32_t     src_h,
			uint32_t     src_pitch, uint32_t     drm_fmt)
{
	int ret = 0;
	int primary_w = emgd_crtc->igd_pipe->timing->width;
	int primary_h = emgd_crtc->igd_pipe->timing->height;
	struct drm_i915_gem_object * oldbufobj = NULL;
	igd_display_port_t *port = NULL;
	uint32_t test_w = 0;
	uint32_t test_h = 0;

	EMGD_TRACE_ENTER;

	port = PORT(emgd_crtc);
	if(port == NULL) /* to prevent any sudden port disable that may occur */
	{
		EMGD_ERROR("port is NULL");
		return -EINVAL;
	}

	if (crtc_x >= primary_w || crtc_y >= primary_h) {
		EMGD_DEBUG("Ovl dest rect top left not inside display pipe's active");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	/* Don't modify another pipe's plane */
	if (plane->emgd_crtc && (plane->emgd_crtc != emgd_crtc)){
		EMGD_DEBUG("Ovl requested crtc id not matching what was set");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	/*
	 * Clamp the width & height into the visible area.  Note we don't
	 * try to scale the source if part of the visible region is offscreen.
	 * The caller must handle that by adjusting source offset and size.
	 */
	if (crtc_x < 0) {
		crtc_w += crtc_x;
		crtc_x = 0;
	}
	if (crtc_x + crtc_w > primary_w) {
		crtc_w = primary_w - crtc_x;
	}
	if (crtc_y < 0) {
		crtc_h += crtc_y;
		crtc_y = 0;
	}
	if (crtc_y + crtc_h > primary_h) {
		crtc_h = primary_h - crtc_y;
	}

	/*
	 * Validate for down- & up- scale compliance
	 */
	if((plane->max_downscale) || (plane->max_upscale) ){
		test_w = src_w >> 16;
		test_h = src_h >> 16;
		/* VLV does not support down- or up- scaling !!! */
		if ((test_w != crtc_w) || (test_h != crtc_h))
		{
			EMGD_ERROR(" src_w  = %d", src_w);
			EMGD_ERROR(" src_h  = %d", src_h);
			EMGD_ERROR(" test_w  = %d", test_w);
			EMGD_ERROR(" test_h  = %d", test_h);
			EMGD_ERROR(" dst_w  = %d", crtc_w);
			EMGD_ERROR(" dst_h  = %d", crtc_h);
			EMGD_ERROR("VLV does not support down- or up- scaling !!!");
			EMGD_TRACE_EXIT;
			return -EINVAL;
		}
	}

	/* In OTC driver, they might need to disable the display pipe
	 * if the OVL is gonna be full screen - but not for embedded
	 * We cant do this for embedded cases as we dont know if FB content dest
	 * keying is used for menu, or subtitle or FB-Blend-Ovl with other hmi pixels
	 */

	/* In OTC driver, they reject gem objects that are not X-tiled
	 * We cant do this for embedded cases as we know that we may get customers
	 * using the overlay planes for FPGA direct DMA from external camera. Those
	 * wont be tiled surfaces
	 */
	if(!(newbufobj)){
		EMGD_ERROR("Failed to newbufobj is NULL!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&(dev_priv->dev->struct_mutex));

	if(plane->curr_surf.bo) {
		oldbufobj = (struct drm_i915_gem_object *)plane->curr_surf.bo;
	}
	if (oldbufobj != newbufobj) {
		if(pin) {
			ret = intel_pin_and_fence_fb_obj(dev_priv->dev, newbufobj, NULL);
			if (ret) {
				EMGD_ERROR("Failed to pin object!");
				mutex_unlock(&(dev_priv->dev->struct_mutex));
				EMGD_TRACE_EXIT;
				return -EINVAL;
			}
			drm_gem_object_reference(&newbufobj->base);
		}
	}

	/* In OTC driver, they might need to turn back the display pipe on in case they
	 * disabled the display pipe if the OVL was full screen - but not for embedded
	 * We cant do this for embedded cases as we dont know if FB content dest
	 * keying is used for menu, or subtitle or FB-Blend-Ovl with other hmi pixels
	 */

	/* Update up the private device specific state */
	plane->emgd_crtc   = emgd_crtc;

	/* Prep from local validated params into igd hal structs */
	plane->curr_surf.base.width  = src_w;
	plane->curr_surf.base.height = src_h;
	plane->curr_surf.base.DRMFB_PITCH  = src_pitch;
	plane->curr_surf.bo          = newbufobj;

	plane->src_rect.x1 = src_x;
	plane->src_rect.x2 = src_x + src_w;
	plane->src_rect.y1 = src_y;
	plane->src_rect.y2 = src_y + src_h;
	plane->dst_rect.x1 = crtc_x;
	plane->dst_rect.x2 = crtc_x + crtc_w;
	plane->dst_rect.y1 = crtc_y;
	plane->dst_rect.y2 = crtc_y + crtc_h;

	plane->curr_surf.base.pixel_format = drm_fmt;
	plane->curr_surf.igd_pixel_format = convert_pf_drm_to_emgd(drm_fmt);
	if (plane->curr_surf.igd_pixel_format == IGD_PF_UNKNOWN) {
		EMGD_ERROR("unsupported pxl format conversion - default ARGB8888");
		plane->curr_surf.igd_pixel_format = IGD_PF_ARGB32;
	}
	plane->curr_surf.igd_flags |=  IGD_SURFACE_OVERLAY;

	/* Processing of Tiling...
	 * To unset/reset flag first */
	plane->curr_surf.igd_flags &= ~(IGD_SURFACE_TILED);
	/* to check if tiling enabled or not */
	if(newbufobj->tiling_mode != I915_TILING_NONE) {
		plane->curr_surf.igd_flags |= IGD_SURFACE_TILED;
	}

	ret = dev_priv->context->drv_dispatch.alter_ovl(
				(igd_display_h) emgd_crtc,
				plane->plane_id,
				NULL,
				&(plane->curr_surf),
				&(plane->src_rect),
				&(plane->dst_rect),
				&(plane->ovl_info),
				IGD_OVL_ALTER_ON);
	if(!ret){
		plane->enabled     = 1;
	} else {
		EMGD_ERROR("alter_ovl failes on plane id %d", plane->plane_id);
	}

	/* Unpin old obj after new one is active to avoid ugliness */
	if (oldbufobj && (oldbufobj != newbufobj)) {
		/*
		 * It's fairly common to simply update the position of
		 * an existing object.  In that case, we don't need to
		 * wait for vblank to avoid ugliness, we only need to
		 * do the pin & ref bookkeeping.
		 */
		/*
		mutex_unlock(&dev_priv->dev->struct_mutex);
		intel_wait_for_vblank(dev_priv->dev, emgd_crtc->pipe);
		mutex_lock(&dev_priv->dev->struct_mutex);
		*/
		intel_unpin_fb_obj(oldbufobj);
		drm_gem_object_unreference(&oldbufobj->base);
	}

	mutex_unlock(&(dev_priv->dev->struct_mutex));

	EMGD_TRACE_EXIT;

	return ret;

}

int emgd_core_disable_ovlplane( drm_emgd_private_t * dev_priv,
		igd_ovl_plane_t * plane, int unpin)
{
	EMGD_TRACE_ENTER;

	if(plane->enabled) {

		dev_priv->context->drv_dispatch.alter_ovl(
					plane->emgd_crtc, plane->plane_id,
					NULL, NULL, NULL, NULL, NULL,
					IGD_OVL_ALTER_OFF);

		if(plane->curr_surf.bo != NULL) {
			mutex_lock(&dev_priv->dev->struct_mutex);
			if(unpin) {
				intel_unpin_fb_obj(plane->curr_surf.bo);
				drm_gem_object_unreference(&plane->curr_surf.bo->base);
			}
			mutex_unlock(&dev_priv->dev->struct_mutex);
		}

		plane->enabled      = 0;
		plane->curr_surf.bo = NULL;
		plane->emgd_crtc    = NULL;

	}

	EMGD_TRACE_EXIT;

	return 0;
}

int emgd_core_check_sprite(struct drm_plane *_plane, const struct drm_crtc *crtc,
		const struct drm_framebuffer *fb, struct emgd_plane_coords *coords)
{
	igd_ovl_plane_t *plane = to_emgd_plane(_plane);
	emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);
	emgd_framebuffer_t *emgd_fb = container_of(fb, emgd_framebuffer_t, base);
	int primary_w = emgd_crtc->igd_pipe->timing->width;
	int primary_h = emgd_crtc->igd_pipe->timing->height;
	igd_display_port_t *port = NULL;
	uint32_t test_w, test_h;

	EMGD_TRACE_ENTER;
	if (!coords) {
		return 0;
	}

	coords->visible = false;

	port = PORT(emgd_crtc);
	if(port == NULL) /* to prevent any sudden port disable that may occur */
	{
		EMGD_ERROR("port == NULL");
		return -EINVAL;
	}

	if (!fb || !crtc || !crtc->enabled) {
		return 0;
	}

	if(!emgd_fb || !(emgd_fb->bo)){
		EMGD_ERROR("Either emgd_fb or emgd_fb->bo is NULL!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	if (coords->crtc_x >= primary_w || coords->crtc_y >= primary_h) {
		EMGD_DEBUG("Ovl dest rect top left not inside display pipe's active");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	/* Don't modify another pipe's plane */
	if (plane->emgd_crtc && (plane->emgd_crtc != emgd_crtc)){
		EMGD_DEBUG("Ovl requested crtc id not matching what was set");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	/*
	 * Validate for down- & up- scale compliance
	 */
	if((plane->max_downscale) || (plane->max_upscale) ){
		test_w = coords->src_w >> 16;
		test_h = coords->src_h >> 16;
		/* VLV does not support down- or up- scaling !!! */
		if ((test_w != coords->crtc_w) || (test_h != coords->crtc_h))
		{
			EMGD_ERROR(" src_w  = %d", coords->src_w);
			EMGD_ERROR(" src_h  = %d", coords->src_h);
			EMGD_ERROR(" test_w  = %d", test_w);
			EMGD_ERROR(" test_h  = %d", test_h);
			EMGD_ERROR(" dst_w  = %d", coords->crtc_w);
			EMGD_ERROR(" dst_h  = %d", coords->crtc_h);
			EMGD_ERROR("VLV does not support down- or up- scaling !!!");
			EMGD_TRACE_EXIT;
			return -EINVAL;
		}
	}

	coords->visible = true;
	EMGD_TRACE_EXIT;
	return 0;
}

int emgd_core_calc_sprite(struct drm_plane *_plane, const struct drm_crtc *crtc,
		const struct drm_framebuffer *fb, struct emgd_plane_coords *coords)
{
	struct drm_device *dev = _plane->dev;
	drm_emgd_private_t *dev_priv = dev->dev_private;
	igd_ovl_plane_t *plane = to_emgd_plane(_plane);
	emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);
	emgd_framebuffer_t *emgd_fb = container_of(fb, emgd_framebuffer_t, base);
	struct drm_i915_gem_object *newbufobj;
	int primary_w = emgd_crtc->igd_pipe->timing->width;
	int primary_h = emgd_crtc->igd_pipe->timing->height;
	igd_display_port_t *port = NULL;
	int drm_fmt;
	int ret;

	EMGD_TRACE_ENTER;

	if(!emgd_crtc) {
		EMGD_ERROR("Invalid NULL parameters: emgd_crtc");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	port = PORT(emgd_crtc);

	if (!coords || !coords->visible || !fb) {
		return 0;
	}
	newbufobj = emgd_fb->bo;
	drm_fmt = emgd_fb->base.pixel_format;

	/* crtc_x and crtc_y are values relative to the framebuffer whereas
	 * x_offset/y_offset represent the actual screen dimensions.
	 */
	coords->crtc_x -= port->pt_info->x_offset;
	coords->crtc_y -= port->pt_info->y_offset;

	/*
	 * Clamp the width & height into the visible area.  Note we don't
	 * try to scale the source if part of the visible region is offscreen.
	 * The caller must handle that by adjusting source offset and size.
	 */
	if (coords->crtc_x < 0) {
		coords->crtc_w += coords->crtc_x;
		coords->crtc_x = 0;
	}
	if (coords->crtc_x + coords->crtc_w > primary_w) {
		coords->crtc_w = primary_w - coords->crtc_x;
	}
	if (coords->crtc_y < 0) {
		coords->crtc_h += coords->crtc_y;
		coords->crtc_y = 0;
	}
	if (coords->crtc_y + coords->crtc_h > primary_h) {
		coords->crtc_h = primary_h - coords->crtc_y;
	}

	mutex_lock(&(dev_priv->dev->struct_mutex));
	plane->oldbufobj = plane->curr_surf.bo;
	
	drm_gem_object_reference(&newbufobj->base);

	/* Update up the private device specific state */
	plane->emgd_crtc   = emgd_crtc;

	/* Prep from local validated params into igd hal structs */
	plane->curr_surf.base.width  = coords->src_w;
	plane->curr_surf.base.height = coords->src_h;
	plane->curr_surf.base.DRMFB_PITCH  = emgd_fb->base.pitches[0];
	plane->curr_surf.bo          = newbufobj;

	plane->src_rect.x1 = coords->src_x;
	plane->src_rect.x2 = coords->src_x + coords->src_w;
	plane->src_rect.y1 = coords->src_y;
	plane->src_rect.y2 = coords->src_y + coords->src_h;
	plane->dst_rect.x1 = coords->crtc_x;
	plane->dst_rect.x2 = coords->crtc_x + coords->crtc_w;
	plane->dst_rect.y1 = coords->crtc_y;
	plane->dst_rect.y2 = coords->crtc_y + coords->crtc_h;

	plane->curr_surf.base.pixel_format = drm_fmt;
	plane->curr_surf.igd_pixel_format = convert_pf_drm_to_emgd(drm_fmt);
	if (plane->curr_surf.igd_pixel_format == IGD_PF_UNKNOWN) {
		EMGD_ERROR("unsupported pxl format conversion - default ARGB8888");
		plane->curr_surf.igd_pixel_format = IGD_PF_ARGB32;
	}

	/* Processing of Tiling...
	 * To unset/reset flag first */
	plane->curr_surf.igd_flags &= ~(IGD_SURFACE_TILED);
	/* to check if tiling enabled or not */
	if(newbufobj->tiling_mode != I915_TILING_NONE) {
		plane->curr_surf.igd_flags |= IGD_SURFACE_TILED;
	}

	plane->coords = (void *)coords;

	emgd_crtc->sprite1->zorder_control_bits =
		get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite1->plane_id);
	emgd_crtc->sprite2->zorder_control_bits =
		get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite2->plane_id);

	ret = dev_priv->context->drv_dispatch.calc_spr_regs(
				(igd_display_h) emgd_crtc,
				plane->plane_id,
				&(plane->curr_surf),
				&(plane->src_rect),
				&(plane->dst_rect),
				&(plane->ovl_info), 0,
				(void *)plane->regs);

	mutex_unlock(&(dev_priv->dev->struct_mutex));
	EMGD_TRACE_EXIT;
	return 0;
}

int emgd_core_commit_sprite(struct drm_plane *_plane, const struct drm_crtc *crtc,
	void *regs)
{
	struct drm_device *dev = _plane->dev;
	drm_emgd_private_t *dev_priv = dev->dev_private;
	igd_ovl_plane_t *plane = container_of(_plane, igd_ovl_plane_t, base);
	igd_ovl_plane_t *other_sprplane = plane->other_sprplane;
	emgd_crtc_t *emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	spr_reg_cache_vlv_t *spr_flip_regs = (spr_reg_cache_vlv_t *)regs;
	spr_reg_cache_vlv_t *other_sprite_regs;
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* We are using a spin lock instead of a mutex (struct_mutex) because this code might
	 * run in an interrupt context (worker thread inside intel_atomic.c)
	 */
	spin_lock(&plane->commit_lock_workers);

	ret = dev_priv->context->drv_dispatch.commit_spr_regs(
				(igd_display_h) emgd_crtc,
				plane->plane_id,
				&(plane->ovl_info),
				regs);

	if(!ret){
		plane->enabled     = 1;
		if (other_sprplane) {
			other_sprite_regs = (spr_reg_cache_vlv_t *)other_sprplane->regs;
			if (other_sprite_regs &&
				other_sprite_regs->control != spr_flip_regs->other_sprite_control &&
				spr_flip_regs->other_sprite_control != 0) {
				other_sprite_regs->control = spr_flip_regs->other_sprite_control;
			}
		}
		/* Unpin old obj after new one is active to avoid ugliness */
		if (plane->oldbufobj) {
			drm_gem_object_unreference(&plane->oldbufobj->base);
			plane->oldbufobj = NULL;
		}
	} else {
		EMGD_ERROR("commit_spr_regs failed on plane id %d", plane->plane_id);
	}


	spin_unlock(&plane->commit_lock_workers);

	EMGD_TRACE_EXIT;

	return ret;
}

static int emgd_drm_update_ovlplane(struct drm_plane *plane, struct drm_crtc *crtc,
		   struct drm_framebuffer *fb, int crtc_x, int crtc_y,
		   unsigned int crtc_w, unsigned int crtc_h,
		   uint32_t src_x, uint32_t src_y,
		   uint32_t src_w, uint32_t src_h)
{
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	emgd_crtc_t *emgd_crtc      = NULL;
	igd_ovl_plane_t * igd_plane = NULL;
	emgd_framebuffer_t *emgd_fb = NULL;
	struct drm_i915_gem_object *bufobj = NULL;
	int ret;

	EMGD_TRACE_ENTER;

	if(!(crtc) || (!plane) || !(fb)) {
		EMGD_ERROR("Invalid parameters: crtc, plane or fb!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	dev = plane->dev;
	dev_priv = dev->dev_private;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	igd_plane = container_of(plane, igd_ovl_plane_t, base);
	emgd_fb = container_of(fb, emgd_framebuffer_t, base);

	if(!(emgd_crtc) || (!igd_plane) || !(emgd_fb)) {
		EMGD_ERROR("Cant get emgd crtc, plane or fb!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	if(igd_plane->enabled_by_s3d) {
		EMGD_ERROR("Plane is used by S3D!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	bufobj = emgd_fb->bo;

	if(src_w == 0 && src_h == 0 && crtc_w == 0 && crtc_h == 0)
	{
		EMGD_TRACE_EXIT;
		return 0;
	}

	ret = emgd_core_update_ovlplane(dev_priv, igd_plane,
				emgd_crtc, bufobj, 1,
				crtc_x, crtc_y, crtc_w, crtc_h,
				(src_x >>16),(src_y>>16),src_w,src_h,
				emgd_fb->base.pitches[0], emgd_fb->base.pixel_format);
	EMGD_TRACE_EXIT;

	return ret;
}

int emgd_drm_disable_ovlplane(struct drm_plane *plane)
{
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	igd_ovl_plane_t * igd_plane = NULL;
	int ret = 0;

	EMGD_TRACE_ENTER;

	if((!plane) ) {
		EMGD_ERROR("Invalid parameter: plane !");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	dev = plane->dev;
	dev_priv = dev->dev_private;

	if(dev_priv->spvideo_holdplane) {
		return 0;
	}
	igd_plane = container_of(plane, igd_ovl_plane_t, base);
	if(!igd_plane){
		EMGD_ERROR("Cant get emgd plane!");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	emgd_core_disable_ovlplane(dev_priv, igd_plane, 1);

	EMGD_TRACE_EXIT;

	return ret;

}

static void emgd_drm_destroy_ovlplane(struct drm_plane * plane)
{
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	igd_ovl_plane_t * igd_plane = NULL;

	EMGD_TRACE_ENTER;

	if((!plane) ) {
		EMGD_ERROR("Invalid parameter: plane !");
		EMGD_TRACE_EXIT;
		return;
	}
	dev = plane->dev;
	dev_priv = dev->dev_private;

	igd_plane = container_of(plane, igd_ovl_plane_t, base);
	if(!igd_plane){
		EMGD_ERROR("Cant get emgd plane!");
		EMGD_TRACE_EXIT;
		return;
	}

	emgd_core_disable_ovlplane(dev_priv, igd_plane, 1);

	if (igd_plane->regs) {
		kfree(igd_plane->regs);
		igd_plane->regs = NULL;
	}

	drm_plane_cleanup(plane);

	/* We used to clean up the whole ovl_planes array if this was the last plane,
	 * however we ran into cases where not all the planes allocated were being
	 * used so we never got the destroy call for the last plane. We free the
	 * planes in emgd_driver_unload after we call emgd_overlay_planes_cleanup.
	 */

	EMGD_TRACE_EXIT;
}

#ifdef SPLASH_VIDEO_SUPPORT /* *NOTE*: drm_mode_setplane() function symbol has to be exported in drm */
int emgd_ioctl_init_spvideo(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct spvideo_fb_list *spv_bufs = (struct spvideo_fb_list*)data;
	int counter;

	if((dev != NULL) && (dev->dev_private != NULL)) {
		drm_emgd_private_t *dev_priv = dev->dev_private;
		if(spv_bufs->fbs_allocated < SPVIDEO_MAX_FB_SUPPORTED) {
			for(counter = 0; counter < spv_bufs->fbs_allocated; counter++) {
				dev_priv->spvideo_fb_ids[counter] = spv_bufs->fb_ids[counter];
			}
			dev_priv->spvideo_fbs_allocated = spv_bufs->fbs_allocated;
			return 0;
		}
	}
	return -EINVAL;
}

int emgd_ioctl_display_spvideo(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_mode_set_plane *plane_write = (struct drm_mode_set_plane*)data;

	if((dev != NULL) && (dev->dev_private != NULL)) {
		drm_emgd_private_t *dev_priv = dev->dev_private;

		if(plane_write->crtc_w == 0 && plane_write->crtc_h == 0) {
			plane_write->fb_id = dev_priv->spvideo_fb_ids[0];
			dev_priv->spvideo_fbs_allocated = 0; /* reset the spvideo fb list */
			return drm_mode_setplane(dev, data, file_priv);
		}

		if(plane_write->fb_id < dev_priv->spvideo_fbs_allocated) {
			plane_write->fb_id = dev_priv->spvideo_fb_ids[plane_write->fb_id];
			return drm_mode_setplane(dev, data, file_priv);
		}
	}
	return -EINVAL;
}
#endif

/*
 * Note on refcounting:
 * When the user creates an fb for the GEM object to be used for the plane,
 * a ref is taken on the object.  However, if the application exits before
 * disabling the plane, the DRM close handling will free all the fbs and
 * unless we take a ref on the object, it will be destroyed before the
 * plane disable hook is called, causing obvious trouble with our efforts
 * to look up and unpin the object.  So we take a ref after we move the
 * object to the display plane so it won't be destroyed until our disable
 * hook is called and we drop our private reference.
 */

int emgd_ioctl_set_ovlplane_colorkey(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_colorkey *setckreq = data;
	drm_emgd_private_t * dev_priv = dev->dev_private;
	igd_ovl_plane_t *  igd_plane = NULL;
	int ret = 0;
	struct drm_mode_object *obj;
	struct drm_plane * drm_plane;

	EMGD_TRACE_ENTER;

	if (!dev_priv) {
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);


	obj = drm_mode_object_find(dev, setckreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", setckreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", setckreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}

	if(setckreq->flags & I915_SET_COLORKEY_DESTINATION) {
		igd_plane->ovl_info.color_key.dest   = setckreq->min_value;
		igd_plane->ovl_info.color_key.flags |= IGD_OVL_DST_COLOR_KEY_ENABLE;
	} else  if(setckreq->flags & I915_SET_COLORKEY_SOURCE) {
		igd_plane->ovl_info.color_key.src_hi = setckreq->max_value;
		igd_plane->ovl_info.color_key.src_lo = setckreq->min_value;
		igd_plane->ovl_info.color_key.flags |= IGD_OVL_SRC_COLOR_KEY_ENABLE;
	} else if(setckreq->flags & I915_SET_COLORKEY_NONE) {
		igd_plane->ovl_info.color_key.dest   = 0;
		igd_plane->ovl_info.color_key.src_hi = 0;
		igd_plane->ovl_info.color_key.src_lo = 0;
		igd_plane->ovl_info.color_key.flags &= ~(IGD_OVL_SRC_COLOR_KEY_ENABLE |
							IGD_OVL_DST_COLOR_KEY_ENABLE);
	}

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;
	return ret;
}

int emgd_ioctl_get_ovlplane_colorkey(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_colorkey *getckreq = data;
	drm_emgd_private_t * dev_priv = dev->dev_private;
	igd_ovl_plane_t *  igd_plane = NULL;
	int ret = 0;
	struct drm_mode_object *obj;
	struct drm_plane * drm_plane;

	EMGD_TRACE_ENTER;

	if (!dev_priv) {
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);

	obj = drm_mode_object_find(dev, getckreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", getckreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", getckreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}

	memset(getckreq, 0, sizeof(struct drm_intel_sprite_colorkey));

	if(igd_plane->ovl_info.color_key.flags & IGD_OVL_DST_COLOR_KEY_ENABLE){
		getckreq->flags    |= I915_SET_COLORKEY_DESTINATION;
		getckreq->min_value = igd_plane->ovl_info.color_key.dest;
	}

	if(igd_plane->ovl_info.color_key.flags & IGD_OVL_SRC_COLOR_KEY_ENABLE){
		getckreq->flags    |= I915_SET_COLORKEY_SOURCE;
		getckreq->max_value = igd_plane->ovl_info.color_key.src_hi;
		getckreq->min_value = igd_plane->ovl_info.color_key.src_lo;
	}

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;
	return ret;
}

int emgd_ioctl_get_pipe_from_crtc_id(struct drm_device *dev, void *arg,
                 struct drm_file *file_priv)
{
	struct drm_i915_get_pipe_from_crtc_id *pipe_from_crtc_id = arg;
	struct drm_mode_object *drmmode_obj;
	emgd_crtc_t *emgd_crtc   = NULL;
	EMGD_TRACE_ENTER;
	drmmode_obj = drm_mode_object_find(dev, pipe_from_crtc_id->crtc_id,
			DRM_MODE_OBJECT_CRTC);
	if (!drmmode_obj) {
		DRM_ERROR("no such CRTC id\n");
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}
	emgd_crtc = container_of(obj_to_crtc(drmmode_obj), emgd_crtc_t, base);
	pipe_from_crtc_id->pipe = emgd_crtc->pipe;
	EMGD_TRACE_EXIT;
	return 0;
}
/*
 * Comment emgd_ioctl_set_ovlplane_colorcorrection and
 * emgd_ioctl_get_ovlplane_colorcorrection functions
 * because this two function didn't get using for now and
 * to increase % kernel code coverage. In future, this two
 * function got use, then will be turn back it
 */

#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
int emgd_ioctl_set_ovlplane_colorcorrection(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_colorcorrect *setccreq = data;
	drm_emgd_private_t * dev_priv = dev->dev_private;
	igd_ovl_plane_t *  igd_plane = NULL;
	int ret = 0;
	struct drm_mode_object *obj;
	struct drm_plane * drm_plane;


	EMGD_TRACE_ENTER;

	if (!dev_priv) {
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);


	obj = drm_mode_object_find(dev, setccreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", setccreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", setccreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}

	igd_plane->ovl_info.color_correct.brightness = (unsigned short) ((setccreq->brightness + 0x00008000) >> 16);
	igd_plane->ovl_info.color_correct.contrast   = (unsigned short) ((setccreq->contrast + 0x00008000) >> 16);
	igd_plane->ovl_info.color_correct.saturation = (unsigned short) ((setccreq->saturation + 0x00008000) >> 16);
	igd_plane->ovl_info.color_correct.hue        = (unsigned short) ((setccreq->hue + 0x00008000) >> 16);


	dev_priv->context->drv_dispatch.alter_ovl_attr(
			igd_plane->emgd_crtc, igd_plane->plane_id,
			NULL, 0, 0,
			&igd_plane->ovl_info.color_correct, 1,
			NULL, 0);

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;
	return ret;
}
#endif /* UNBLOCK_OVERLAY_KERNELCOVERAGE */

#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
int emgd_ioctl_get_ovlplane_colorcorrection(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_colorcorrect *getccreq = data;
	drm_emgd_private_t * dev_priv = dev->dev_private;
	igd_ovl_plane_t *  igd_plane = NULL;
	int ret = 0;
	struct drm_mode_object *obj;
	struct drm_plane * drm_plane;


	EMGD_TRACE_ENTER;

	if (!dev_priv) {
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);

	obj = drm_mode_object_find(dev, getccreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", getccreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", getccreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}

	getccreq->brightness = igd_plane->ovl_info.color_correct.brightness;
	getccreq->contrast   = igd_plane->ovl_info.color_correct.contrast;
	getccreq->saturation = igd_plane->ovl_info.color_correct.saturation;
	getccreq->hue        = igd_plane->ovl_info.color_correct.hue;

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;
	return ret;
}
#endif /* UNBLOCK_OVERLAY_KERNELCOVERAGE */

int emgd_ioctl_set_ovlplane_pipeblending(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_pipeblend *setpbreq = data;
	drm_emgd_private_t *dev_priv = dev->dev_private;
	igd_ovl_plane_t *igd_plane 	 = NULL;
	emgd_crtc_t     *emgd_crtc   = NULL;
	struct drm_crtc *drm_crtc    = NULL;
	struct drm_mode_object *obj  = NULL;
	int ret = 0;
	//struct drm_mode_object *obj;
	struct drm_plane * drm_plane;

	EMGD_TRACE_ENTER;

	if (!dev_priv) {
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);


	obj = drm_mode_object_find(dev, setpbreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", setpbreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", setpbreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}

	/* This is value should not be NULL,
	 * but in case if it is happened ...*/
	if (igd_plane->emgd_crtc != NULL)
	{
		emgd_crtc = (emgd_crtc_t *)igd_plane->emgd_crtc;
	}
	else
	{
		/*to take pointer to emgd crtc*/
		obj = drm_mode_object_find(dev, setpbreq->crtc_id,
					   DRM_MODE_OBJECT_CRTC);
		if (!obj) {
			EMGD_ERROR("Unknown crtc ID %d", setpbreq->crtc_id);
			mutex_unlock(&dev->mode_config.mutex);
			EMGD_TRACE_EXIT;
			return -ENOENT;
		}
		drm_crtc  = obj_to_crtc(obj);
		emgd_crtc = container_of(drm_crtc, emgd_crtc_t, base);
		if(!emgd_crtc) {
			EMGD_ERROR("Cant convert from DRM crtc id %d to emgd",
					setpbreq->crtc_id);
			mutex_unlock(&dev->mode_config.mutex);
			EMGD_TRACE_EXIT;
			return -ENOENT;
		}
		igd_plane->emgd_crtc = emgd_crtc;
	}

	if(setpbreq->enable_flags & I915_SPRITEFLAG_PIPEBLEND_FBBLENDOVL) {
		igd_plane->ovl_info.pipeblend.enable_flags |= IGD_OVL_PIPEBLEND_SET_FBBLEND;
		emgd_crtc->fb_blend_ovl_on = setpbreq->fb_blend_ovl;
		igd_plane->ovl_info.pipeblend.fb_blend_ovl  = setpbreq->fb_blend_ovl;
	}

	if(setpbreq->enable_flags & I915_SPRITEFLAG_PIPEBLEND_CONSTALPHA) {
		igd_plane->ovl_info.pipeblend.enable_flags |= IGD_OVL_PIPEBLEND_SET_CONSTALPHA;
		igd_plane->ovl_info.pipeblend.has_const_alpha = setpbreq->has_const_alpha;
		igd_plane->ovl_info.pipeblend.const_alpha   = setpbreq->const_alpha;
	}

	if(setpbreq->enable_flags & I915_SPRITEFLAG_PIPEBLEND_ZORDER) {
		igd_plane->ovl_info.pipeblend.enable_flags |= IGD_OVL_PIPEBLEND_SET_ZORDER;
		igd_plane->ovl_info.pipeblend.zorder_combined = setpbreq->zorder_value;
		emgd_crtc->zorder = setpbreq->zorder_value;
		emgd_crtc->sprite1->zorder_control_bits = get_zorder_control_values(
															setpbreq->zorder_value,
															emgd_crtc->sprite1->plane_id);
		emgd_crtc->sprite2->zorder_control_bits = get_zorder_control_values(
															setpbreq->zorder_value,
															emgd_crtc->sprite2->plane_id);
		if(igd_plane->ovl_info.pipeblend.zorder_combined == IGD_OVL_ZORDER_INVALID) {
			/* this is basically a 0x0, we should safe back to "default" */
			igd_plane->ovl_info.pipeblend.zorder_combined = IGD_OVL_ZORDER_DEFAULT;
		}
	}

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;
	return ret;
}

int emgd_ioctl_get_ovlplane_pipeblending(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_intel_sprite_pipeblend *getpbreq = data;
	drm_emgd_private_t * dev_priv = dev->dev_private;
	igd_ovl_plane_t *  igd_plane = NULL;
	emgd_crtc_t     *emgd_crtc   = NULL;
	struct drm_crtc *drm_crtc    = NULL;
	struct drm_mode_object *obj  = NULL;
	int ret = 0;
	struct drm_plane * drm_plane;

	EMGD_TRACE_ENTER;

	if (!dev_priv)
		return -EINVAL;

	mutex_lock(&dev->mode_config.mutex);

	obj = drm_mode_object_find(dev, getpbreq->plane_id, DRM_MODE_OBJECT_PLANE);
	if (!obj) {
		EMGD_ERROR("Cant find plane drm object %d from user", getpbreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}
	drm_plane = obj_to_plane(obj);
	igd_plane = container_of(drm_plane, igd_ovl_plane_t, base);

	if (!igd_plane) {
		EMGD_ERROR("Cant find plane id %d from user request", getpbreq->plane_id);
		mutex_unlock(&dev->mode_config.mutex);
		EMGD_TRACE_EXIT;
		return -ENOENT;
	}
	/* This is value should not be NULL,
	 * but in case if it is happened ...*/
	if (igd_plane->emgd_crtc != NULL)
	{
		emgd_crtc = (emgd_crtc_t *)igd_plane->emgd_crtc;
	}
	else
	{
		/*to take pointer to emgd crtc*/
		obj = drm_mode_object_find(dev, getpbreq->crtc_id,
					   DRM_MODE_OBJECT_CRTC);
		if (!obj) {
			EMGD_ERROR("Unknown crtc ID %d", getpbreq->crtc_id);
			mutex_unlock(&dev->mode_config.mutex);
			EMGD_TRACE_EXIT;
			return -ENOENT;
		}
		drm_crtc  = obj_to_crtc(obj);
		emgd_crtc = container_of(drm_crtc, emgd_crtc_t, base);
		if(!emgd_crtc) {
			EMGD_ERROR("Cant convert from DRM crtc id %d to emgd",
					getpbreq->crtc_id);
			mutex_unlock(&dev->mode_config.mutex);
			EMGD_TRACE_EXIT;
			return -ENOENT;
		}
		igd_plane->emgd_crtc = emgd_crtc;
	}

	getpbreq->enable_flags    = 0; /* not required for query*/
	getpbreq->has_const_alpha = igd_plane->ovl_info.pipeblend.has_const_alpha;
	getpbreq->const_alpha     = igd_plane->ovl_info.pipeblend.const_alpha;
	getpbreq->fb_blend_ovl = emgd_crtc->fb_blend_ovl_on;
	getpbreq->zorder_value = emgd_crtc->zorder;

	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;

	return ret;
}


int emgd_overlay_planes_shutdown(struct drm_device *dev,
		emgd_crtc_t *emgd_crtc)
{
	drm_emgd_private_t * dev_priv = dev->dev_private;

	EMGD_TRACE_ENTER;

	if(dev_priv->ovl_drmformats){
		kfree(dev_priv->ovl_drmformats);
		dev_priv->ovl_drmformats = NULL;
	}
	if(dev_priv->ovl_planes) {
		kfree(dev_priv->ovl_planes);
		dev_priv->ovl_planes = NULL;
	}

	EMGD_TRACE_EXIT;

	return 0;
}


/* polyphase filter coefficients */
#define N_HORIZ_Y_TAPS          5
#define N_VERT_Y_TAPS           3
#define N_HORIZ_UV_TAPS         3
#define N_VERT_UV_TAPS          3
#define N_PHASES                17

/* memory bufferd overlay registers */
struct overlay_registers {
        u32 OBUF_0Y;
        u32 OBUF_1Y;
        u32 OBUF_0U;
        u32 OBUF_0V;
        u32 OBUF_1U;
        u32 OBUF_1V;
        u32 OSTRIDE;
        u32 YRGB_VPH;
        u32 UV_VPH;
        u32 HORZ_PH;
        u32 INIT_PHS;
        u32 DWINPOS;
        u32 DWINSZ;
        u32 SWIDTH;
        u32 SWIDTHSW;
        u32 SHEIGHT;
        u32 YRGBSCALE;
        u32 UVSCALE;
        u32 OCLRC0;
        u32 OCLRC1;
        u32 DCLRKV;
        u32 DCLRKM;
        u32 SCLRKVH;
        u32 SCLRKVL;
        u32 SCLRKEN;
        u32 OCONFIG;
        u32 OCMD;
        u32 RESERVED1; /* 0x6C */
        u32 OSTART_0Y;
        u32 OSTART_1Y;
        u32 OSTART_0U;
        u32 OSTART_0V;
        u32 OSTART_1U;
        u32 OSTART_1V;
        u32 OTILEOFF_0Y;
        u32 OTILEOFF_1Y;
        u32 OTILEOFF_0U;
        u32 OTILEOFF_0V;
        u32 OTILEOFF_1U;
        u32 OTILEOFF_1V;
        u32 FASTHSCALE; /* 0xA0 */
        u32 UVSCALEV; /* 0xA4 */
        u32 RESERVEDC[(0x200 - 0xA8) / 4]; /* 0xA8 - 0x1FC */
        u16 Y_VCOEFS[N_VERT_Y_TAPS * N_PHASES]; /* 0x200 */
        u16 RESERVEDD[0x100 / 2 - N_VERT_Y_TAPS * N_PHASES];
        u16 Y_HCOEFS[N_HORIZ_Y_TAPS * N_PHASES]; /* 0x300 */
        u16 RESERVEDE[0x200 / 2 - N_HORIZ_Y_TAPS * N_PHASES];
        u16 UV_VCOEFS[N_VERT_UV_TAPS * N_PHASES]; /* 0x500 */
        u16 RESERVEDF[0x100 / 2 - N_VERT_UV_TAPS * N_PHASES];
        u16 UV_HCOEFS[N_HORIZ_UV_TAPS * N_PHASES]; /* 0x600 */
        u16 RESERVEDG[0x100 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
};

struct intel_overlay {
        struct drm_device *dev;
        struct intel_crtc *crtc;
        struct drm_i915_gem_object *vid_bo;
        struct drm_i915_gem_object *old_vid_bo;
        int active;
        int pfit_active;
        u32 pfit_vscale_ratio; /* shifted-point number, (1<<12) == 1.0 */
        u32 color_key;
        s32 brightness;
        u32 contrast, saturation;
        u32 old_xscale, old_yscale;
        /* register access */
        u32 flip_addr;
        struct drm_i915_gem_object *reg_bo;
        /* flip handling */
        uint32_t last_flip_req;
        void (*flip_tail)(struct intel_overlay *);
};

struct intel_overlay_error_state {
	struct overlay_registers regs;
	unsigned long base;
	u32 dovsta;
	u32 isr;
};

static struct overlay_registers *
intel_overlay_map_regs_atomic(struct intel_overlay *overlay)
{
	drm_emgd_private_t *dev_priv = overlay->dev->dev_private;
	struct overlay_registers *regs;

	if (OVERLAY_NEEDS_PHYSICAL(overlay->dev))
		regs = overlay->reg_bo->phys_obj->handle->vaddr;
	else
		regs = io_mapping_map_atomic_wc(dev_priv->gtt.mappable,
						i915_gem_obj_ggtt_offset(overlay->reg_bo));

	return regs;
}

static void intel_overlay_unmap_regs_atomic(struct intel_overlay *overlay,
					    struct overlay_registers *regs)
{
	if (!OVERLAY_NEEDS_PHYSICAL(overlay->dev))
		io_mapping_unmap_atomic(regs);
}


struct intel_overlay_error_state *
intel_overlay_capture_error_state(struct drm_device *dev)
{
	drm_emgd_private_t *dev_priv = dev->dev_private;
	struct intel_overlay *overlay = dev_priv->overlay;
	struct intel_overlay_error_state *error;
	struct overlay_registers __iomem *regs;

	if (!overlay || !overlay->active)
		return NULL;

	error = kmalloc(sizeof(*error), GFP_ATOMIC);
	if (error == NULL)
		return NULL;

	error->dovsta = I915_READ(DOVSTA);
	error->isr = I915_READ(ISR);
	if (OVERLAY_NEEDS_PHYSICAL(overlay->dev))
		error->base = (long) overlay->reg_bo->phys_obj->handle->vaddr;
	else
		error->base = i915_gem_obj_ggtt_offset(overlay->reg_bo);

	regs = intel_overlay_map_regs_atomic(overlay);
	if (!regs)
		goto err;

	memcpy_fromio(&error->regs, regs, sizeof(struct overlay_registers));
	intel_overlay_unmap_regs_atomic(overlay, regs);

	return error;

err:
	kfree(error);
	return NULL;
}

void
intel_overlay_print_error_state(struct drm_i915_error_state_buf *m,
				struct intel_overlay_error_state *error)
{
	i915_error_printf(m, "Overlay, status: 0x%08x, interrupt: 0x%08x\n",
			  error->dovsta, error->isr);
	i915_error_printf(m, "  Register file at 0x%08lx:\n",
			  error->base);

#define P(x) i915_error_printf(m, "    " #x ":	0x%08x\n", error->regs.x)
	P(OBUF_0Y);
	P(OBUF_1Y);
	P(OBUF_0U);
	P(OBUF_0V);
	P(OBUF_1U);
	P(OBUF_1V);
	P(OSTRIDE);
	P(YRGB_VPH);
	P(UV_VPH);
	P(HORZ_PH);
	P(INIT_PHS);
	P(DWINPOS);
	P(DWINSZ);
	P(SWIDTH);
	P(SWIDTHSW);
	P(SHEIGHT);
	P(YRGBSCALE);
	P(UVSCALE);
	P(OCLRC0);
	P(OCLRC1);
	P(DCLRKV);
	P(DCLRKM);
	P(SCLRKVH);
	P(SCLRKVL);
	P(SCLRKEN);
	P(OCONFIG);
	P(OCMD);
	P(OSTART_0Y);
	P(OSTART_1Y);
	P(OSTART_0U);
	P(OSTART_0V);
	P(OSTART_1U);
	P(OSTART_1V);
	P(OTILEOFF_0Y);
	P(OTILEOFF_1Y);
	P(OTILEOFF_0U);
	P(OTILEOFF_0V);
	P(OTILEOFF_1U);
	P(OTILEOFF_1V);
	P(FASTHSCALE);
	P(UVSCALEV);
#undef P
}

static int emgd_drm_set_property(struct drm_plane *plane,
		struct drm_property *property,
		uint64_t value)
{
	struct drm_device *dev;
	drm_emgd_private_t *dev_priv;
	igd_ovl_plane_t *igd_plane = NULL;
	int update_gamma = 0;
	int update_ccorrect = 0;
	int update_pipeblend = 0;
	emgd_crtc_t *emgd_crtc = NULL;

	EMGD_TRACE_ENTER;
	BUG_ON(!plane);

	dev = plane->dev;
	dev_priv = dev->dev_private;
	igd_plane = container_of(plane, igd_ovl_plane_t, base);

	emgd_crtc = (emgd_crtc_t *)igd_plane->emgd_crtc;

	/*
	 * crtc required by gamma properties
	 */
	if(!emgd_crtc) {
		EMGD_ERROR("Trying to set property with NULL crtc");
		return -EINVAL;
	}

	mutex_lock(&dev_priv->dev->struct_mutex);

	if (property == dev_priv->ovl_properties.brightness) {
		igd_plane->ovl_info.color_correct.brightness =
			(short)((char) value); /* libdrm passes this value
									  as unsigned 64 bits, we want to
									  restore the sign */
		update_ccorrect = 1;
	} else if (property == dev_priv->ovl_properties.contrast) {
		igd_plane->ovl_info.color_correct.contrast =
			(unsigned short) value;
		update_ccorrect = 1;
	} else if (property == dev_priv->ovl_properties.saturation) {
		igd_plane->ovl_info.color_correct.saturation =
			(unsigned short) value;
		update_ccorrect = 1;
	} else if (property == dev_priv->ovl_properties.hue) {
		igd_plane->ovl_info.color_correct.hue =
			(unsigned short) value;
		update_ccorrect = 1;
	} else if (property == dev_priv->ovl_properties.gamma_enable) {
		if (value) {
			igd_plane->ovl_info.gamma.flags = IGD_OVL_GAMMA_ENABLE;
		} else {
			igd_plane->ovl_info.gamma.flags = IGD_OVL_GAMMA_DISABLE;
		}
		update_gamma = 1;
	} else if (property == dev_priv->ovl_properties.gamma_red) {
		igd_plane->ovl_info.gamma.red = (uint32_t) value;
		update_gamma = 1;
	} else if (property == dev_priv->ovl_properties.gamma_green) {
		igd_plane->ovl_info.gamma.green = (uint32_t) value;
		update_gamma = 1;
	} else if (property == dev_priv->ovl_properties.gamma_blue) {
		igd_plane->ovl_info.gamma.blue = (uint32_t) value;
		update_gamma = 1;
	} else if (property == dev_priv->ovl_properties.const_alpha) {
		if (value == 0xff) {
			igd_plane->ovl_info.pipeblend.has_const_alpha = 0;
			igd_plane->ovl_info.pipeblend.const_alpha     = 0;
		} else {
			igd_plane->ovl_info.pipeblend.has_const_alpha = 1;
			igd_plane->ovl_info.pipeblend.const_alpha     =
				(uint32_t) value;
		}
		update_pipeblend = 1;
	}

	mutex_unlock(&dev_priv->dev->struct_mutex);

	dev_priv->context->drv_dispatch.alter_ovl_attr(
		(igd_display_h)emgd_crtc, igd_plane->plane_id,
		&igd_plane->ovl_info.pipeblend, update_pipeblend, 0,
		&igd_plane->ovl_info.color_correct, update_ccorrect,
		&igd_plane->ovl_info.gamma, update_gamma);

	return 0;
}

