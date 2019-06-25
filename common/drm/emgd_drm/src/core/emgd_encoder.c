/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_encoder.c
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
 *  Encoder / kernel mode setting functions.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.kms

#include <linux/version.h>
#include <drmP.h>
#include <drm_crtc_helper.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "drm_emgd_private.h"
#include "emgd_drm.h"
#include <igd_core_structs.h>



/*------------------------------------------------------------------------------
 * External Functions
 *------------------------------------------------------------------------------
 */
extern int calculate_eld(igd_display_port_t *port,
				igd_timing_info_t *timing_info);

/*Panel Backlight Related Functions*/
extern int blc_init(struct drm_device *dev);
extern int blc_exit(struct drm_device *dev);

static void emgd_encoder_destroy(struct drm_encoder *encoder);
static void emgd_encoder_dpms(struct drm_encoder *encoder, int mode);
static bool emgd_encoder_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
static void emgd_encoder_prepare(struct drm_encoder *encoder);
static void emgd_encoder_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
static void emgd_encoder_commit(struct drm_encoder *encoder);


const struct drm_encoder_funcs emgd_encoder_funcs = {
	.destroy = emgd_encoder_destroy,
};

const struct drm_encoder_helper_funcs emgd_encoder_helper_funcs = {
	.dpms       = emgd_encoder_dpms,
	.mode_fixup = emgd_encoder_mode_fixup,
	.prepare    = emgd_encoder_prepare,
	.mode_set   = emgd_encoder_mode_set,
	.commit     = emgd_encoder_commit,
};


/**
 * emgd_encoder_dpms
 *
 * This function will put the encoder to either an ON or OFF state.  Anything
 * that is not DRM_MODE_DPMS_ON is treated as an off-state.
 *
 * @param encoder (IN) Encoder
 * @param mode    (IN) power mode
 *
 * @return None
 */
static void emgd_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	emgd_encoder_t *emgd_encoder = container_of(encoder, emgd_encoder_t, base);
	igd_display_port_t *igd_port = emgd_encoder->igd_port;
	pd_driver_t           *pd   = igd_port->pd_driver;
	unsigned long new_port_state;
	struct drm_device *dev = NULL;
	igd_context_t * context;
	emgd_crtc_t  *emgd_crtc = NULL;
	igd_display_pipe_t *pipe;
	int ret = 0;

	EMGD_TRACE_ENTER;

	dev = encoder->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;

	switch(mode) {
		case DRM_MODE_DPMS_ON:
			new_port_state = IGD_POWERSTATE_D0;
			break;

		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			new_port_state = IGD_POWERSTATE_D3;
			break;

		default:
			EMGD_ERROR_EXIT("Unsupported DPMS mode");
			return;
	}


	EMGD_DEBUG("Setting port %x power to %lx",
			igd_port->port_number, new_port_state);

	emgd_crtc = container_of(encoder->crtc, emgd_crtc_t, base);

	/*
	 * When the plane is locked for seamless handover,
	 * set power & port programming are ignored.
	*/
	if((emgd_crtc == NULL) && (new_port_state == IGD_POWERSTATE_D0)){
			new_port_state = IGD_POWERSTATE_D3;
	}
	
	if (emgd_crtc != NULL) {
		pipe = emgd_crtc->igd_pipe;
			/* Here we call DP read DPCD. */
			if (igd_port->pd_driver->pipe_info){
				ret = igd_port->pd_driver->pipe_info(
					igd_port->pd_context,
					pipe->timing, 
					1<<pipe->pipe_num);

				if (ret != 0){
					EMGD_ERROR("DP/eDP DPCD Read Error");
				}
			}
	}
	if((emgd_crtc != NULL) && !emgd_crtc->freeze_state) {
		igd_port->pd_driver->set_power(igd_port->pd_context,
			new_port_state);
		if(new_port_state == IGD_POWERSTATE_D0) {
			context->mode_dispatch->kms_post_program_port(emgd_encoder,
					TRUE);
		} else if(new_port_state == IGD_POWERSTATE_D3) {
			context->mode_dispatch->kms_post_program_port(emgd_encoder,
					FALSE);
		}
		if ((pd != NULL) &&  (pd->type == PD_DISPLAY_DP_INT)){
			if (mode == DRM_MODE_DPMS_ON) {
				blc_init(dev);
			} else if ( mode == DRM_MODE_DPMS_OFF ){
				blc_exit(dev);
			}
		}
	}

	EMGD_TRACE_EXIT;
}



/**
 * emgd_encoder_mode_fixup
 *
 * Called before a mode set, takes the input "mode", matches it to the closest
 * supported mode, then put the supported mode into "adjusted_mode" to let the
 * caller know.
 *
 * Note: We cannot handle centered and scaled mode with this.  To handle this
 *       we need to program the pipe and the port to different sets of timings.
 *       The CRTC Helper does not allow this.  It wants to send adjusted_mode
 *       to both the CRTC and the Encoder.  We can maybe get around this by
 *       modifying the "mode" parameter, but that is not the right approach.
 *
 * @param encoder (IN) Encoder being prepared
 * @param mode    (IN) Requested mode
 * @param adjusted_mode (IN) Encoder supported mode
 *
 * @return true, false (details TBD)
 */
static bool emgd_encoder_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_device      *dev          = NULL;
	igd_context_t          *context      = NULL;
	igd_display_port_t     *port         = NULL;
	emgd_framebuffer_t     *fb_info      = NULL;
	emgd_encoder_t         *emgd_encoder = NULL;
	igd_timing_info_t      *timing       = NULL;
	igd_display_info_t     *pt_info      = NULL;
	unsigned long           existing_height  = 0;
	unsigned long           existing_width   = 0;
	unsigned long           existing_refresh = 0;
	int                     ret;

	EMGD_TRACE_ENTER;

	/* Check ajusted mode to see if it's valid.  If not, populate it */
	if (adjusted_mode->crtc_htotal == 0) {
		EMGD_DEBUG("No valid mode in adjusted mode, setting valid mode");
		drm_mode_set_crtcinfo(adjusted_mode, 0);
	}

	dev = encoder->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;
	emgd_encoder = container_of(encoder, emgd_encoder_t, base);
	port = emgd_encoder->igd_port;
	if (!port->pt_info) {
		port->pt_info = kzalloc(sizeof(igd_display_info_t), GFP_KERNEL);
		if (!port->pt_info) {
			EMGD_DEBUG("Cannot allocate igd_display_into_t");
			return false;
		}
	}
	existing_height  = port->pt_info->height;
	existing_width   = port->pt_info->width;
	existing_refresh = port->pt_info->refresh;
	pt_info          = port->pt_info;

	if (!encoder->crtc) {
		EMGD_ERROR("Encoder is missing CRTC.");
		return false;
	}

	fb_info = container_of(encoder->crtc->fb, emgd_framebuffer_t, base);
	if (!fb_info) {
		EMGD_DEBUG("Invalid fb_info.");
		return false;
	}


	if (!mode) {
		EMGD_ERROR("Incoming mode is NULL.");
		return false;
	}

	if (!pt_info) {
		EMGD_ERROR("PT Info pointer is NULL.");
		return false;
	}
	EMGD_DEBUG("Setting fb_info to: %dx%d", fb_info->base.width, fb_info->base.height);

	pt_info->width        = mode->crtc_hdisplay;
	pt_info->height       = mode->crtc_vdisplay;
	/*
	 * mode->refresh may be 0. it can be calculated by dot clk, htotal/vtotal
	 * there are 1080p@60 and 1080p@50 with same dot clk 148500
	 * need refresh for matching
	 */
	pt_info->refresh      = drm_mode_vrefresh(mode);
	pt_info->dclk         = mode->clock;
	pt_info->htotal       = mode->crtc_htotal;
	pt_info->hblank_start = mode->crtc_hblank_start;
	pt_info->hblank_end   = mode->crtc_hblank_end;
	pt_info->hsync_start  = mode->crtc_hsync_start;
	pt_info->hsync_end    = mode->crtc_hsync_end;
	pt_info->vtotal       = mode->crtc_vtotal;
	pt_info->vblank_start = mode->crtc_vblank_start;
	pt_info->vblank_end   = mode->crtc_vblank_end;
	pt_info->vsync_start  = mode->crtc_vsync_start;
	pt_info->vsync_end    = mode->crtc_vsync_end;
	pt_info->flags        = 0;
	EMGD_DEBUG("Setting pt_info to: %dx%d@%d with dclk = %ld", 
		pt_info->width, pt_info->height, pt_info->refresh, pt_info->dclk);

	ret = context->mode_dispatch->kms_match_mode((void *)emgd_encoder,
		(void *)fb_info, &timing);

	if (!ret) {
		adjusted_mode->crtc_hdisplay     = timing->width;
		adjusted_mode->crtc_vdisplay     = timing->height;
		adjusted_mode->vrefresh          = timing->refresh;
		adjusted_mode->clock             = timing->dclk;
		adjusted_mode->crtc_htotal       = timing->htotal;
		adjusted_mode->crtc_hblank_start = timing->hblank_start;
		adjusted_mode->crtc_hblank_end   = timing->hblank_end;
		adjusted_mode->crtc_hsync_start  = timing->hsync_start;
		adjusted_mode->crtc_hsync_end    = timing->hsync_end;
		adjusted_mode->crtc_vtotal       = timing->vtotal;
		adjusted_mode->crtc_vblank_start = timing->vblank_start;
		adjusted_mode->crtc_vblank_end   = timing->vblank_end;
		adjusted_mode->crtc_vsync_start  = timing->vsync_start;
		adjusted_mode->crtc_vsync_end    = timing->vsync_end;
		adjusted_mode->private_flags     = timing->flags;

		timing->private.mode_number = timing->mode_number;
		adjusted_mode->private			 = (int *)&timing->private;

		sprintf(adjusted_mode->name, "%dx%d",
				mode->crtc_hdisplay, mode->crtc_vdisplay);

		EMGD_DEBUG("(%dx%d@%d)->(%dx%d@%d)",
			mode->crtc_hdisplay, mode->crtc_vdisplay, mode->vrefresh,
			adjusted_mode->crtc_hdisplay, adjusted_mode->crtc_vdisplay,
			adjusted_mode->vrefresh);

	}

	EMGD_TRACE_EXIT;
	return (!ret);
}



/**
 * emgd_encoder_prepare
 *
 * Based on the available documentation at the moment, this function gets
 * called right before a mode change.  Its job is to turn off the display.
 *
 * @param encoder (IN) Encoder being prepared
 *
 * @return None
 */
static void emgd_encoder_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs;
	emgd_encoder_t *emgd_encoder;

	EMGD_TRACE_ENTER;
	emgd_encoder = container_of(encoder, emgd_encoder_t, base);

	encoder_funcs = encoder->helper_private;
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);

	EMGD_TRACE_EXIT;
}



/**
 * emgd_encoder_commit
 *
 * This function commits the mode change sequence by actually programming
 * the registers.
 *
 * @param encoder (IN) Encoder being prepared
 *
 * @return None
 */
static void emgd_encoder_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs;
	emgd_encoder_t     *emgd_encoder = NULL;
	emgd_crtc_t        *emgd_crtc;
	igd_display_port_t *port;
	igd_display_pipe_t *pipe;

	EMGD_TRACE_ENTER;

	emgd_encoder = container_of(encoder, emgd_encoder_t, base);

	port      = emgd_encoder->igd_port;
	emgd_crtc = container_of(encoder->crtc, emgd_crtc_t, base);
	pipe      = emgd_crtc->igd_pipe;

	encoder_funcs = encoder->helper_private;

	/*
	 * This code is not called for seamless handover,
	*/
	port->pd_driver->set_mode(port->pd_context, pipe->timing,
							1<<pipe->pipe_num);
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);

	emgd_encoder->new_crtc = emgd_crtc;
	EMGD_TRACE_EXIT;
}



/**
 * emgd_encoder_mode_set
 *
 * This function saves the requested timings into the Port Timing Info
 * structure.  At emgd_encoder_commit() time we should be using these
 * timings to program the port, but currently we are using timings from
 * the pipe.  This is fine for now, but at one point we should investigate
 * the centering case in which the port timings may not match the pipe timings.
 *
 * @param encoder (IN) Encoder being prepared
 * @param mode    (IN)
 * @param adjusted_mode (IN)
 *
 * @return None
 */
static void emgd_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_device  *dev           = NULL;
	igd_context_t      *context       = NULL;
	emgd_crtc_t        *emgd_crtc    = NULL;
	emgd_encoder_t     *emgd_encoder = NULL;
	igd_display_pipe_t *pipe         = NULL;
	igd_display_port_t *port         = NULL;
	pd_timing_t        *timing       = NULL;
	igd_display_info_ext   *temp;

	EMGD_TRACE_ENTER;

	dev = encoder->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;
	emgd_encoder = container_of(encoder, emgd_encoder_t, base);

	port = emgd_encoder->igd_port;
	emgd_crtc = container_of(encoder->crtc, emgd_crtc_t, base);
	pipe = emgd_crtc->igd_pipe;

	if (pipe) {
		timing = (pd_timing_t *)pipe->timing;

		if (NULL == port->pt_info) {
			port->pt_info = kzalloc(sizeof(igd_display_info_t), GFP_KERNEL);

			if (!port->pt_info) {
				EMGD_ERROR_EXIT("Unable to allocate pt_info.");
				return;
			}
		}

		port->pt_info->width        = adjusted_mode->crtc_hdisplay;
		port->pt_info->height       = adjusted_mode->crtc_vdisplay;
		port->pt_info->refresh      = adjusted_mode->vrefresh;
		port->pt_info->dclk         = adjusted_mode->clock;
		port->pt_info->htotal       = adjusted_mode->crtc_htotal;
		port->pt_info->hblank_start = adjusted_mode->crtc_hblank_start;
		port->pt_info->hblank_end   = adjusted_mode->crtc_hblank_end;
		port->pt_info->hsync_start  = adjusted_mode->crtc_hsync_start;
		port->pt_info->hsync_end    = adjusted_mode->crtc_hsync_end;
		port->pt_info->vtotal       = adjusted_mode->crtc_vtotal;
		port->pt_info->vblank_start = adjusted_mode->crtc_vblank_start;
		port->pt_info->vblank_end   = adjusted_mode->crtc_vblank_end;
		port->pt_info->vsync_start  = adjusted_mode->crtc_vsync_start;
		port->pt_info->vsync_end    = adjusted_mode->crtc_vsync_end;
		port->pt_info->flags        = adjusted_mode->private_flags;

		port->pt_info->x_offset     = timing->x_offset;
		port->pt_info->y_offset     = timing->y_offset;
		port->pt_info->flags       |= IGD_DISPLAY_ENABLE;

		temp = (igd_display_info_ext *)adjusted_mode->private;
		port->pt_info->mode_number = temp->mode_number;


		EMGD_DEBUG("Calculate ELD");
		if (calculate_eld(port, timing)) {
			EMGD_DEBUG("Fail to calculate ELD");
		}

	} else {
		EMGD_ERROR("Trying to set the mode without a pipe attached.");
	}

	/*
	 * When the plane is locked for seamless handover,
	 * port programming is ignored.
	*/
	if(!emgd_crtc->freeze_state) {
		context->mode_dispatch->kms_program_port(emgd_encoder,
			IGD_DISPLAY_ENABLE);
	}

	EMGD_TRACE_EXIT;
}



/**
 * emgd_encoder_destroy
 *
 * Frees the resources allocated for this encoder during "create_encoder()"
 *
 * @param encoder (IN) Encoder to be freed
 *
 * @return None
 */
static void emgd_encoder_destroy(struct drm_encoder *encoder)
{
	emgd_encoder_t *emgd_encoder;

	EMGD_TRACE_ENTER;
	emgd_encoder = container_of(encoder, emgd_encoder_t, base);

	drm_encoder_cleanup(encoder);

	kfree(emgd_encoder);

	EMGD_TRACE_EXIT;
}
