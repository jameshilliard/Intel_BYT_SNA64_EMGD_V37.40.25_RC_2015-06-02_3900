/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: splash_video.c
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
 *  This is the EMGD Kernel driver splash video implementation file.
 *  This will either generate a static color map or take in video data from
 *  an external camera via a user configured physical DRAM address
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.splash

#include <linux/version.h>
#include <linux/sched.h>
#include <drm/drmP.h>
#include "i915/i915_reg.h"
#include "i915/intel_drv.h"
#include "drm_emgd_private.h"
#include "emgd_drm.h"
#include <igd_core_structs.h>
#include <igd_ovl.h>
#include "default_config.h"
#include "splash_video.h"
#include "io.h"

/*
 *
#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "drm_emgd_private.h"
#include "emgd_drm.h"
#include <igd_core_structs.h>
#include <igd_ovl.h>
#include <memory.h>
*/

extern emgd_drm_config_t *config_drm;

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

/**
 * create_rgb_picture
 *
 * This function will allocate a fixed buffer that represents and XRGB32 surface
 * (thats not padded or aligned), based on incoming width and height and will
 * populate that surface with a color map of some sort. The caller of this needs to
 * free the allocated buffer after use
 *
 * @param width  (IN) the width of the surface
 * @param height (IN) the width of the surface
 *
 * @return valid ptr on Success - needs to be freed by caller
 * @return NULL on Error
 */
unsigned char * create_rgb_picture(int width, int height)
{
	int loop1, loop2;
	unsigned char r, g, b;
	unsigned char * pixel;

	pixel= kmalloc((height * width * 4), GFP_KERNEL);
	if(!pixel){
		EMGD_ERROR("Splash video cant allocate rgb picture!");
		return NULL;
	}

	EMGD_ERROR("allocate rgb picture W X H = %d X %d!", width, height);

	r = b = g = 0;

	for(loop1=0; loop1< height; ++loop1) {

		for(loop2=0; loop2< width; ++loop2) {

			r = (unsigned char)(((loop2+1)*0xf0)/width);
			g = (unsigned char)(((loop1+1)*0xf0)/height);
			b = (0x3f - (r/8) - (g/8));

			pixel[(loop1*width*4)+(loop2*4)+0] = b;
			pixel[(loop1*width*4)+(loop2*4)+1] = g;
			pixel[(loop1*width*4)+(loop2*4)+2] = r;
			pixel[(loop1*width*4)+(loop2*4)+3] = 0;
		}
	}

	return pixel;
}


/**
 * convert_rgb_to_yuy2
 *
 * This function will convert from a source surface buffer (XRGB32) to a dest surface
 * buffer (YUY2), given the width, height,their pointers and strides
 *
 * @param src (IN) the pointer to the source surface
 * @param dst (IN) the pointer to the destination surface
 * @param s_pitch (IN) the stride of the source surface
 * @param d_pitch (IN) the stride of the destination surface
 * @param width  (IN) the width of the surfaces
 * @param height (IN) the width of the surfaces
 *
 */
void convert_rgb_to_yuy2(unsigned char * src, unsigned char * dest,
		int s_pitch, int d_pitch, int width, int height)
{
	int loop1, loop2;

	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	unsigned char x = 0;

	unsigned char y = 0;
	unsigned char u = 0;
	unsigned char v = 0;

	long inty = 0;
	long intu = 0;
	long intv = 0;
	long floatx;
	long floatr;
	long floatg;
	long floatb;

	for(loop1=0; loop1< height; ++loop1)
	{
		for(loop2=0; loop2< width/2; ++loop2)
		{
			/* Read RGB for the first set of pixel aligned data - YU */
			b = src[(loop1*s_pitch)+(loop2*2*4)+0];
			g = src[(loop1*s_pitch)+(loop2*2*4)+1];
			r = src[(loop1*s_pitch)+(loop2*2*4)+2];
			x = src[(loop1*s_pitch)+(loop2*2*4)+3];

			floatr = r;
			floatg = g;
			floatb = b;
			floatx = x;

			/* calculate the conversion to YUV */
			inty =  ((263 * floatr) + (516 * floatg) + (100 * floatb) + 1600);
			intv =  ((450 * floatr) - (377 * floatg) - (73 * floatb) + 12800);
			intu =  (-(151 * floatr) - (298 * floatg) + (450 * floatb) + 12800);
			inty = inty / 100;
			intu = intu / 100;
			intv = intv / 100;
			if(inty>255)
				inty = 255;
			if(inty<0)
				inty = 0;
			if(intu>255)
				intu = 255;
			if(intu<0)
				intu = 0;
			if(intv>255)
				intv = 255;
			if(intv<0)
				intv = 0;
			y = (unsigned char)inty;
			u = (unsigned char)intu;

			/* put the YU into the dest buffer */
			dest[(loop1*(d_pitch))+(loop2*2*2)+0] = y;
			dest[(loop1*(d_pitch))+(loop2*2*2)+1] = u;

			/* for the second set of pixel aligned data - YV */
			b = src[(loop1*s_pitch)+(loop2*2*4)+4];
			g = src[(loop1*s_pitch)+(loop2*2*4)+5];
			r = src[(loop1*s_pitch)+(loop2*2*4)+6];
			x = src[(loop1*s_pitch)+(loop2*2*4)+7];

			floatr = r;
			floatg = g;
			floatb = b;
			floatx = x;

			inty =  ((263 * floatr) + (516 * floatg) + (100 * floatb) + 1600);
			intv =  ((450 * floatr) - (377 * floatg) - (73 * floatb) + 12800);
			intu =  (-(151 * floatr) - (298 * floatg) + (450 * floatb) + 12800);
			inty = inty / 100;
			intu = intu / 100;
			intv = intv / 100;
			if(inty>255)
				inty = 255;
			if(inty<0)
				inty = 0;
			if(intu>255)
				intu = 255;
			if(intu<0)
				intu = 0;
			if(intv>255)
				intv = 255;
			if(intv<0)
				intv = 0;
			y = (unsigned char)inty;
			v = (unsigned char)intv;

			dest[(loop1*(d_pitch))+(loop2*2*2)+2] = y;
			dest[(loop1*(d_pitch))+(loop2*2*2)+3] = v;
		}
	}

	return;

}

/**
 * destroy_splash_video
 *
 * Function to close all hardware state related to splash video and free
 * all corresponding contextual info used for splash video.
 *
 * @param data (IN) a non null pointer to drm_emgd_private_t
 *
 * @return none
 */
void destroy_splash_video(drm_emgd_private_t * priv)
{
	struct drm_device *dev;
	int i;

	if (!priv) {
		return;
	}

	dev = priv->dev;

	if(priv->spvideo_fb) {

		if(priv->spvideo_fb->bo){

			while(priv->spvideo_num_enabled){

				for(i=0; i< priv->num_ovlplanes; ++i){
					if(priv->spvideo_planeid[priv->spvideo_num_enabled - 1] ==
							priv->ovl_planes[i].plane_id){

						emgd_core_disable_ovlplane(priv,
								&priv->ovl_planes[i], 0);
						/* After stagger the sprite, also need to destroy those sprite */
						//break;
					}
				}
				--priv->spvideo_num_enabled;
			}

			mutex_lock(&dev->struct_mutex);
			intel_unpin_fb_obj(priv->spvideo_fb->bo);
			drm_gem_object_unreference_unlocked(&priv->spvideo_fb->bo->base);
			mutex_unlock(&dev->struct_mutex);
		}
		kfree(priv->spvideo_fb);
		priv->spvideo_fb = NULL;
	}

	priv->spvideo_test_enabled = 0;

	return;
}

/**
 * display_splash_video
 *
 * Function to display a splash video to the user. The splash video data must be
 * provided to the kernel mode driver by another entity (like a driver or FPGA
 * HW). Splash video will be display after setting the mode (if requested by
 * the user through config options).
 *
 * @param data (IN) a non null pointer to drm_emgd_private_t
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int display_splash_video(void *data)
{
	struct drm_device *dev;
	drm_emgd_private_t *priv = (drm_emgd_private_t *)data;
	emgd_drm_splash_video_t *sv_data;
	//emgd_crtc_t * primary, emgd_crtc_t * secondary;
	emgd_framebuffer_t * surface;
	unsigned char * rgb_ptr;
	unsigned char * surf_ptr;
	igd_rect_t ovl_rect, surf_rect;
	unsigned int tmp;
	uint32_t drm_fmt;
	struct drm_mode_set_plane plane_req;
	emgd_crtc_t *emgd_crtc;
	struct drm_crtc *crtc = NULL;
	int i, ret, size = 0;

	if (!priv) {
		return 0;
	}
	if(!priv->emgd_fbdev){
		EMGD_ERROR("Splash video requested without FB console!");
		return 0;
	}

	dev = priv->dev;
	sv_data = config_drm->sv_data;

	if(!config_drm->sv_data->igd_pixel_format ||
			!config_drm->sv_data->src_width ||
			!config_drm->sv_data->src_height ||
			!config_drm->sv_data->src_pitch ||
			!config_drm->sv_data->dst_x ||
			!config_drm->sv_data->dst_y) {
		EMGD_ERROR("Splash video requested without any user config!");
		return 0;
	}

	if(!priv->spvideo_fb){
		priv->spvideo_fb = kzalloc(sizeof(emgd_framebuffer_t), GFP_KERNEL);
		if (!priv->spvideo_fb) {
			return -ENOMEM;
		} else {
			if(sv_data->built_in_test) {

				if(sv_data->src_pitch < (sv_data->src_width *
						((sv_data->igd_pixel_format & IGD_PF_DEPTH_MASK)/8)) ) {
					EMGD_ERROR("Src pitch insufficient for src_width and pixel format!");
					sv_data->src_pitch =  sv_data->src_width *
											((sv_data->igd_pixel_format & IGD_PF_DEPTH_MASK)/8);
				}
				if(sv_data->src_pitch & 0x0000001F) {
					EMGD_ERROR("Src pitch forced into 64 byte alignment for HW!");
					sv_data->src_pitch += 0x1F;
					sv_data->src_pitch &= ~0x1F;
				}
				size =  sv_data->src_pitch * sv_data->src_height;
				size = ALIGN(size, PAGE_SIZE); /* GEM demands all be page aligned */

				mutex_lock(&dev->struct_mutex);

				/* "Design for Test" - allocate our own internal GEM bo for
				 * splash video "design for test" where we populate
				 * our own static video image just to test the sprites.
				 */
				priv->spvideo_fb->bo = i915_gem_alloc_object(dev, size);
				if (priv->spvideo_fb->bo == NULL) {
					/* Failed to create bo */
					EMGD_ERROR("Failed to allocate builtin GEM bo for splash video!");
					mutex_unlock(&dev->struct_mutex);
					destroy_splash_video(priv);
					return -ENOMEM;
				}

				priv->spvideo_fb->bo->tiling_mode = I915_TILING_NONE;

				ret = i915_gem_object_pin_to_display_plane(priv->spvideo_fb->bo, 4096, false);
				if(ret){
					/* Failed to create bo */
					EMGD_ERROR("Failed to pin GEM bo for splash video!");
					mutex_unlock(&dev->struct_mutex);
					destroy_splash_video(priv);
					return -ENOMEM;
				}

				mutex_unlock(&dev->struct_mutex);

				rgb_ptr = create_rgb_picture((int)sv_data->src_width, (int)sv_data->src_height);
				if(!rgb_ptr){
					EMGD_ERROR("Cant alloc rgb picture for splash video!");
					destroy_splash_video(priv);
					return -ENOMEM;
				}

				/*surf_ptr = ioremap_wc(priv->context->device_context.fb_adr +
						priv->spvideo_fb->bo->gtt_offset, size); */
				surf_ptr = ioremap_wc(priv->gtt.mappable_base +
						i915_gem_obj_ggtt_offset(priv->spvideo_fb->bo), size);

				if(!surf_ptr){
					EMGD_ERROR("Cant memory map splash video!");
					kfree(rgb_ptr);
					destroy_splash_video(priv);
					return -ENOMEM;
				}

				if(sv_data->igd_pixel_format != IGD_PF_xRGB32 &&
						sv_data->igd_pixel_format != IGD_PF_YUV422_PACKED_YUY2){
					sv_data->igd_pixel_format = IGD_PF_YUV422_PACKED_YUY2;
				}
				/* If NOT XRGB32, then convert the rgb picture we generated */
				if(sv_data->igd_pixel_format == IGD_PF_YUV422_PACKED_YUY2) {
					convert_rgb_to_yuy2(rgb_ptr, surf_ptr,
							(sv_data->src_width*4), sv_data->src_pitch,
							sv_data->src_width, sv_data->src_height);
				} else {
					for(i=0; i < sv_data->src_height; ++i) {
						memcpy( (surf_ptr + (i *  sv_data->src_pitch)),
								(rgb_ptr  + (i * (sv_data->src_width * 4))),
								sv_data->src_width*4);
					}
				}
				kfree(rgb_ptr);
				iounmap(surf_ptr);

			} else {
				/* need to use the OEM specified sv_data->phys_contig_dram_start
				 * and pump that into GEM to get back a GTT offset after its been
				 * mapped into into video memory and use that offset.
				 */
				/*
				 * FIXME: Splash Video - not yet implemented feedin of
				 * external contig DMA pages
				 */
				/*
				priv->spvideo_fb->bo =
					kzalloc(sizeof(struct drm_i915_gem_object), GFP_KERNEL);
				if (!priv->spvideo_fb->bo) {
					kfree(priv->spvideo_fb);
					priv->spvideo_fb = NULL;
					return -ENOMEM;
				}
				*/
				EMGD_ERROR("Splash video with dram phys address - not supported!");
				destroy_splash_video(priv);
				return -EINVAL;
			}
		}
	}

	surface                    = priv->spvideo_fb;
	surface->base.DRMFB_PITCH  = sv_data->src_pitch;
	surface->base.width        = sv_data->src_width;
	surface->base.height       = sv_data->src_height;
	surface->igd_pixel_format  = sv_data->igd_pixel_format;
	surface->igd_flags         = IGD_SURFACE_OVERLAY;

	if(surface->igd_pixel_format == IGD_PF_YUV422_PACKED_YUY2) {
		drm_fmt = DRM_FORMAT_YUYV;
	} else {
		drm_fmt = DRM_FORMAT_XRGB8888;
	}

	/* Set the surface rect as big as the video frame size */
	surf_rect.x1 = 0;
	surf_rect.y1 = 0;
	surf_rect.x2 = sv_data->src_width;
	surf_rect.y2 = sv_data->src_height;


	/* The x,y postion of the sprite c */
	ovl_rect.x1 = sv_data->dst_x;
	ovl_rect.y1 = sv_data->dst_y;

	/*
	 *  NOTE: This for scaling if the hardware supports it.
	 *  If no dest w x h values are set,set it the the
	 *  src w x h
	 */
	tmp = sv_data->dst_width ? sv_data->dst_width : sv_data->src_width;
	ovl_rect.x2 = ovl_rect.x1 + tmp;
	tmp = sv_data->dst_height? sv_data->dst_height: sv_data->src_height;
	ovl_rect.y2 = ovl_rect.y1 + tmp;

	memset(&plane_req, 0, sizeof(struct drm_mode_set_plane));

	/* Attach the frame buffer to the CRTC(s)  */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if(crtc != NULL) {
			emgd_crtc = container_of(crtc, emgd_crtc_t, base);

			if(emgd_crtc) {

				if(crtc->fb == &priv->emgd_fbdev->emgd_fb->base){
					/* This means "this" CRTC is attached to the fb device
					 * Lets enable the splash video sprite for this crtc*/

					/* Loop thru overlay planes looking for matching "possible_crtcs"*/
					for(i=0; i< priv->num_ovlplanes; ++i){
						if(priv->ovl_planes[i].target_igd_pipe_ptr ==
								emgd_crtc->igd_pipe){

							/* OKAY! Found a plane and an active crtc for this plane!
							 * lets enable this overlay plane with content */

							/* We wont explicitly setup any of the overlay plane's
							 * configurations like Zorder, FbBlendOvl, color correction
							 * etc because the default_config defaults are already set
							 */
							ret = emgd_core_update_ovlplane(priv,
									&priv->ovl_planes[i], emgd_crtc,
									priv->spvideo_fb->bo, 0,
									ovl_rect.x1 + (i*100), ovl_rect.y1 + (i*100),
									(ovl_rect.x2 - ovl_rect.x1),
									(ovl_rect.y2 - ovl_rect.y1),
									0, 0,
									(sv_data->src_width << 16),
									(sv_data->src_height << 16),
									sv_data->src_pitch, drm_fmt);
							if(ret) {
								EMGD_ERROR("Cant set ovlplane for splash video!");
								if(!priv->spvideo_num_enabled) {
									destroy_splash_video(priv);
									return -EINVAL;
								}
							} else {
								priv->spvideo_planeid[priv->spvideo_num_enabled] =
									priv->ovl_planes[i].plane_id;
								++priv->spvideo_num_enabled;
							}
							/* To stagger the sprite */
							//break;
						}
					}
				}
			}
		}
	}

	priv->spvideo_test_enabled = 1;

	return 0;
}

