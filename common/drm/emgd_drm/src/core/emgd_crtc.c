/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_crtc.c
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
 *  CRTC / kernel mode setting functions.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.kms


#include <drmP.h>
#include <drm_crtc_helper.h>
#include <linux/version.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "i915/intel_drv.h"
#include "drm_emgd_private.h"
#include "emgd_drm.h"
#include "emgd_drv.h"
#include <igd_core_structs.h>
#include <memory.h>

/* Maximum cursor size supported by our HAL: 64x64 in ARGB */
#define MAX_CURSOR_SIZE (64*64*4)

static void emgd_crtc_dpms(struct drm_crtc *crtc, int mode);
static bool emgd_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode);
int emgd_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode,
		int x, int y, struct drm_framebuffer *old_fb);
static int emgd_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
		struct drm_framebuffer *old_fb);
static void emgd_crtc_prepare(struct drm_crtc *crtc);
void emgd_crtc_commit(struct drm_crtc *crtc);

static void emgd_crtc_save(struct drm_crtc *crtc);
static void emgd_crtc_restore(struct drm_crtc *crtc);
static int emgd_crtc_cursor_set(struct drm_crtc *crtc,
		struct drm_file *file_priv, uint32_t handle,
		uint32_t width, uint32_t height);
static int emgd_crtc_cursor_move(struct drm_crtc *crtc, int x, int y);
static void emgd_crtc_gamma_set(struct drm_crtc *crtc,
		unsigned short *red, unsigned short *green, unsigned short *blue,
		uint32_t start,
		uint32_t size);
static void emgd_crtc_destroy(struct drm_crtc *crtc);
static void emgd_crtc_load_lut(struct drm_crtc *crtc);
static int emgd_crtc_page_flip(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
				struct drm_pending_vblank_event *event,
				uint32_t page_flip_flags);
#else
				struct drm_pending_vblank_event *event);
#endif
static int emgd_crtc_set_property(struct drm_crtc *crtc,
		struct drm_property *property, uint64_t val);
static int emgd_stereo_crtc_page_flip(struct drm_crtc *crtc,
                                struct drm_framebuffer *fb,
                                struct drm_pending_vblank_event *event);
 static int emgd_crtc_set_config(struct drm_mode_set *set);
int emgd_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
		struct drm_framebuffer *fb, int x, int y,
		enum mode_set_atomic state);

void emgd_sysfs_switch_crtc_fb(emgd_crtc_t *emgdcrtc);

void intel_update_watermarks(struct drm_device *dev);
extern int s3d_mode_set(emgd_crtc_t *emgd_crtc,
				struct drm_display_mode *mode);
extern int s3d_timings_fixup(igd_display_port_t *port,
		igd_timing_info_t *current_timings,
		igd_timing_info_t *new_timings);

unsigned int get_zorder_control_values(unsigned long zorder_combined,
		int plane_id);
bool isvalid_zorder_value(unsigned long zorder);

const struct drm_crtc_helper_funcs emgd_crtc_helper_funcs = {
	.disable              = emgd_crtc_disable,
	.dpms                 = emgd_crtc_dpms,
	.mode_fixup           = emgd_crtc_mode_fixup,
	.mode_set             = emgd_crtc_mode_set,
	.mode_set_base        = emgd_crtc_mode_set_base,
	.mode_set_base_atomic = emgd_crtc_mode_set_base_atomic,
	.prepare              = emgd_crtc_prepare,
	.commit               = emgd_crtc_commit,
	.load_lut             = emgd_crtc_load_lut,
};

const struct drm_crtc_funcs emgd_crtc_funcs = {
	.save        = emgd_crtc_save,
	.restore     = emgd_crtc_restore,
	.cursor_set  = emgd_crtc_cursor_set,
	.cursor_move = emgd_crtc_cursor_move,
	.gamma_set   = emgd_crtc_gamma_set,
	.set_config  = emgd_crtc_set_config,
	.destroy     = emgd_crtc_destroy,
	.page_flip   = emgd_crtc_page_flip,
	.set_property= emgd_crtc_set_property,
};



static int emgd_crtc_set_config(struct drm_mode_set *set)
{
	int ret;

	EMGD_TRACE_ENTER;

	ret = drm_crtc_helper_set_config(set);

	if (ret) {
		EMGD_ERROR_EXIT("Failed to set config");
		return ret;
	}

	EMGD_TRACE_EXIT;

	return ret;
}

/*
 * Sets the power management mode of the pipe.
 */
static void emgd_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	emgd_crtc_t           *emgd_crtc = NULL;
	igd_display_pipe_t    *pipe = NULL;
	struct drm_device      *dev = crtc->dev;
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;

	EMGD_TRACE_ENTER;


	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	EMGD_DEBUG("pipe=%d, mode=%d", emgd_crtc->crtc_id, mode);
	pipe = emgd_crtc->igd_pipe;


	switch(mode) {
	case DRM_MODE_DPMS_ON:
		EMGD_DEBUG("Checking if we have pipe timings");
		if (!pipe->timing) {
			/* If there is no pipe timing, we cannot enable */
			EMGD_ERROR("No pipe timing, can't enable pipe");
		} else {
			EMGD_DEBUG("Turn on pipe");
			/*
			 * When the plane is locked for seamless handover,
			 * dpms is ignored.
			*/
			if(!emgd_crtc->freeze_state) {
				intel_update_watermarks(dev);
				EMGD_DEBUG("Turn on pipe and plane");
				context->mode_dispatch->kms_program_pipe(emgd_crtc);
				context->mode_dispatch->kms_set_plane_pwr(emgd_crtc, TRUE, TRUE);
			}
		}
		break;

	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		/*
		 * When the plane is locked for seamless handover,
		 * dpms is ignored.
		*/
		if(!emgd_crtc->freeze_state) {
			EMGD_DEBUG("Turn off plane and pipe");
			context->mode_dispatch->kms_set_plane_pwr(emgd_crtc, FALSE, TRUE);
			context->mode_dispatch->kms_set_pipe_pwr(emgd_crtc, FALSE);
			intel_update_watermarks(dev);
		}
		break;
	default:
		break;
	}

	EMGD_TRACE_EXIT;
}



static bool emgd_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	EMGD_TRACE_ENTER;

	/* Check ajusted mode to see if it's valid.  If not, populate it */
	if (adjusted_mode->crtc_htotal == 0) {
		drm_mode_set_crtcinfo(adjusted_mode, 0);
	}

	EMGD_TRACE_EXIT;
	return 1;
}



/**
 * emgd_crtc_mode_set
 *
 * Sets mode for the selected CRTC.  This function only sets the timings
 * into the CRTC, but doesn't actually program the timing values into the
 * registers.  The actual programming is done in emgd_crtc_commit.
 *
 * @param crtc   (IN) CRTC to configure
 * @param x      (IN) starting X position in the frame buffer
 * @param y      (IN) starting Y position in the frame buffer
 * @param old_fb (IN) Not used
 *
 * @return 0
 */
int emgd_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode,
		int x, int y, struct drm_framebuffer *old_fb)
{
	emgd_crtc_t            *emgd_crtc = NULL;
	struct drm_device      *dev = NULL;
	igd_context_t          *context = NULL;
	igd_display_pipe_t     *pipe = NULL;
	igd_display_port_t     *port = NULL;
	igd_timing_info_t      *timing = NULL;
	igd_timing_info_t      s3d_timings;
	igd_display_info_ext   *temp;

	EMGD_TRACE_ENTER;


	dev = crtc->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;

	/* Which pipe are we using */
	EMGD_DEBUG("Getting PIPE");
	emgd_crtc = container_of(crtc, emgd_crtc_t, base);

	EMGD_DEBUG("\t\tpipe=%d, ->(%dx%d@%d)", emgd_crtc->crtc_id,
		adjusted_mode->crtc_hdisplay, adjusted_mode->crtc_vdisplay,
		adjusted_mode->vrefresh);

	pipe = emgd_crtc->igd_pipe;
	port = PORT(emgd_crtc);


	if (old_fb) {
		EMGD_DEBUG("Handling old framebuffer?");
		/* What do we do with the old framebuffer? */
	}

	timing = kzalloc(sizeof(*timing), GFP_KERNEL);
	if (!timing) {
		EMGD_ERROR_EXIT("unable to allocate a igd_timing_info struct.");
		return 1;
	}

	timing->width = adjusted_mode->crtc_hdisplay;
	timing->height = adjusted_mode->crtc_vdisplay;
	timing->refresh = adjusted_mode->vrefresh;
	timing->dclk = adjusted_mode->clock; /* Is this the right variable? */
	timing->htotal = adjusted_mode->crtc_htotal;
	timing->hblank_start = adjusted_mode->crtc_hblank_start;
	timing->hblank_end = adjusted_mode->crtc_hblank_end;
	timing->hsync_start = adjusted_mode->crtc_hsync_start;
	timing->hsync_end = adjusted_mode->crtc_hsync_end;
	timing->vtotal = adjusted_mode->crtc_vtotal;
	timing->vblank_start = adjusted_mode->crtc_vblank_start;
	timing->vblank_end = adjusted_mode->crtc_vblank_end;
	timing->vsync_start = adjusted_mode->crtc_vsync_start;
	timing->vsync_end = adjusted_mode->crtc_vsync_end;
	timing->flags = adjusted_mode->private_flags;
	timing->x_offset = x;
	timing->y_offset = y;
	timing->flags |= IGD_DISPLAY_ENABLE;

	temp = (igd_display_info_ext *)adjusted_mode->private;
	timing->mode_number = temp->mode_number;
	timing->private.extn_ptr = temp->extn_ptr;

	/* Two methods are exposed to set S3D mode:
	 * 1) Set the "S3D set" connector property and followed by
	 *    setting a mode
	 * 2) Set "expose 3D modes" connector property to enable and use
	 *    DRM_MODE_FLAG_3D_XXXX in the mode flag to set a 3D mode.
	 *    This method is ported from OTC implementation.
	 *
	 * s3d_mode_set will set the S3D port attribute ("S3D set" connector property)
	 * based on the mode flag. This is used by Method 2 when "expose 3D modes"
	 * is enable. Method 2 will override the value in "S3D set" connector property
	 * set by Method 1. Only one method can be used at one time.
	 */
	s3d_mode_set(emgd_crtc, mode);

	/* If 3D mode is requested, need to fix the timing accordingly based on
	 * the S3D format. S3D format like Framepacking requires timing fix.
	 */
	if (!s3d_timings_fixup(port, timing, &s3d_timings)) {
		OS_MEMCPY(timing, &s3d_timings, sizeof(igd_timing_info_t));
	}

	if (pipe->timing) {
		OS_MEMCPY(pipe->timing, timing, sizeof(igd_timing_info_t));
		kfree(timing);
	} else {
		pipe->timing = timing;
	}

	drm_vblank_pre_modeset(dev, emgd_crtc->pipe);

	/* The code above only sets the CRTC timing, not the plane */
	emgd_crtc_mode_set_base(crtc, x, y, old_fb);

	drm_vblank_post_modeset(dev, emgd_crtc->pipe);

	intel_update_watermarks(dev);

	EMGD_TRACE_EXIT;
	return 0;
}


/*
 * emgd_crtc_mode_set_base_atomic
 *
 * Just update the base pointers.  Rest of the plane programming
 * should remain untouched.
 *
 * @param crtc   (IN) CRTC to configure
 * @param fb     (IN) framebuffer to program plane with.
 * @param x      (IN) starting X position in the frame buffer
 * @param y      (IN) starting Y position in the frame buffer
 * @param state  (IN) atomic state.
 */
int emgd_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
		struct drm_framebuffer *fb, int x, int y,
		enum mode_set_atomic state)
{
	struct drm_device *dev = crtc->dev;
	drm_emgd_private_t *devpriv = dev->dev_private;
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = devpriv->context;
	//emgd_framebuffer_t *plane_fb_info;
	emgd_framebuffer_t *emgd_fb;
	int ret;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);

	emgd_fb = container_of(fb, emgd_framebuffer_t, base);

	if (emgd_crtc->igd_plane == NULL) {
		EMGD_ERROR("Plane is null.");
		return -1;
	}

	emgd_crtc->igd_plane->xoffset = x;
	emgd_crtc->igd_plane->yoffset = y;

	if (state == LEAVE_ATOMIC_MODE_SET) {
		emgd_crtc->igd_plane->fb_info = emgd_fb;
		emgd_crtc->igd_plane->ref_cnt++;
		/* Call program plane */
		ret = context->mode_dispatch->kms_program_plane(emgd_crtc, TRUE);
		if (ret < 0) {
			return ret;
		}

		context->mode_dispatch->kms_set_plane_pwr(emgd_crtc, TRUE, TRUE);

	} else {
		emgd_crtc->freeze_crtc = emgd_crtc;
		emgd_crtc->freeze_fb = emgd_fb;
		emgd_crtc->unfreeze_xoffset = x;
		emgd_crtc->unfreeze_yoffset = y;
		emgd_crtc->freeze_pitch = fb->DRMFB_PITCH;
	}

	return 0;
}




/**
 * emgd_crtc_mode_set_base
 *
 * Sets the starting position in the framebuffer for the given CRTC.
 *
 * @param crtc   (IN) CRTC to configure
 * @param x      (IN) starting X position in the frame buffer
 * @param y      (IN) starting Y position in the frame buffer
 * @param old_fb (IN) Not used
 *
 * @return 0
 */
static int emgd_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
		struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	drm_emgd_private_t *devpriv = dev->dev_private;
	struct drm_i915_gem_object *bo;
	emgd_crtc_t *emgd_crtc = NULL;
	emgd_framebuffer_t *emgd_fb, *emgd_oldfb = NULL;
	struct drm_framebuffer *fb = NULL;
	unsigned int alignment;
	int ret = 0;
	bool was_interruptible = devpriv->mm.interruptible;

	EMGD_TRACE_ENTER;
	/*
	 * Modesetting should be non-interruptible.  During a modeset, the GPU
	 * should be idle and not waiting for pageflips on other CRTC's.
	 */
	mutex_lock(&dev->struct_mutex);

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	fb        = crtc->fb;

	/* Make sure we have a framebuffer to switch to */
	if (!fb) {
		EMGD_ERROR("No framebuffer present for mode switch");
		mutex_unlock(&dev->struct_mutex);
		return 0;
	} else {
		emgd_fb = container_of(fb, emgd_framebuffer_t, base);
		bo = emgd_fb->bo;
	}

	if (bo->tiling_mode == I915_TILING_NONE) {
		/*
		 * Older chipsets had different aligment requirements. To properly
		 * supports those, this would need a device-dependent implementation.
		 */
		alignment = 4096;
	} else {
		/* GEM figures out the alignment requirements for tiled framebuffers. */
		alignment = 0;
	}

	/* Pin the framebuffer into the GTT. */
	devpriv->mm.interruptible = false;
	ret = intel_pin_and_fence_fb_obj(dev, bo, NULL);
	if (ret != 0) {
		EMGD_ERROR("Failed to pin framebuffer to GTT");
		devpriv->mm.interruptible = true;
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}
	devpriv->mm.interruptible = true;

	/*
	 * If other crtc's are in the process of flipping away from this
	 * framebuffer, or if batchbuffers in flight make references to it,
	 * wait for those to complete before proceeding.
	 */
	if (old_fb) {
		emgd_oldfb = container_of(old_fb, emgd_framebuffer_t, base);

		/*
		 * Wait until vblanks have happened on any other CRTC's that are
		 * flipping away from this FB, or until we decide that the GPU is hung.
		 */
		wait_event(devpriv->pending_flip_queue,
				i915_reset_in_progress(&devpriv->gpu_error) ||
				atomic_read(&emgd_oldfb->bo->pending_flip) == 0);

		/*
		 * There may still be references to the framebuffer's GTT mapping
		 * in batch buffers.  Wait until we're truly done with the fb.
		 * We can ignore the return value here since the only time this
		 * will fail is if the GPU hangs (in which case we'd be free to
		 * proceed).
		 */
		devpriv->mm.interruptible = false;
		ret = i915_gem_object_finish_gpu(emgd_oldfb->bo);
		devpriv->mm.interruptible = was_interruptible;
	}

	/*
	 * Okay, existing usage of the old framebuffer has completed.  Let's go
	 * ahead and program the framebuffer switch.
	 */
	if (emgd_crtc->freeze_state) {
		ret = emgd_crtc_mode_set_base_atomic(crtc, fb, x, y,
				ENTER_ATOMIC_MODE_SET);
	} else {
		ret = emgd_crtc_mode_set_base_atomic(crtc, fb, x, y,
				LEAVE_ATOMIC_MODE_SET);
	}

	/* Update sysfs view of the CRTC */
	emgd_sysfs_switch_crtc_fb(emgd_crtc);

	/*
	 * Now that we've programmed the hardware unpin the old framebuffer
	 */
	if (old_fb && !emgd_crtc->freeze_state) {
		intel_unpin_fb_obj(emgd_oldfb->bo);
	}

	mutex_unlock(&dev->struct_mutex);

	EMGD_TRACE_EXIT;
	return ret;
}

/* This function updates the fb associated with the crtc and calls kms_calc_plane
 * to calculate the register values.
 */
int emgd_crtc_update_fb_and_calc(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	drm_emgd_private_t *devpriv = dev->dev_private;
	igd_context_t *context = devpriv->context;
	struct drm_i915_gem_object *bo;
	emgd_crtc_t *emgd_crtc = NULL;
	emgd_framebuffer_t *emgd_fb;
	struct drm_framebuffer *fb = NULL;
	int ret = 0;

	EMGD_TRACE_ENTER;
	mutex_lock(&dev->struct_mutex);

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	fb        = crtc->fb;

	/* Make sure we have a framebuffer to switch to */
	if (!fb) {
		EMGD_ERROR("No framebuffer present");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	} else {
		emgd_fb = container_of(fb, emgd_framebuffer_t, base);
		bo = emgd_fb->bo;
	}

	if (emgd_crtc->igd_plane == NULL) {
		EMGD_ERROR("emgd_crtc->igd_plane is NULL.");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	emgd_crtc->igd_plane->xoffset = x;
	emgd_crtc->igd_plane->yoffset = y;

	emgd_crtc->igd_plane->fb_info = emgd_fb;
	emgd_crtc->igd_plane->ref_cnt++;
	/* Call program plane */
	ret = context->mode_dispatch->kms_calc_plane(emgd_crtc, false);

	mutex_unlock(&dev->struct_mutex);

	EMGD_TRACE_EXIT;
	return ret;
}

static void emgd_crtc_prepare(struct drm_crtc *crtc)
{
	EMGD_TRACE_ENTER;
	emgd_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
	EMGD_TRACE_EXIT;
}

void emgd_crtc_commit(struct drm_crtc *crtc)
{
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((igd_context_t *)emgd_crtc->igd_context);

	/*
	 * When the plane is locked for seamless handover,
	 * dpms & plane programming are ignored.
	*/
	if(!emgd_crtc->freeze_state) {
		/* context->mode_dispatch->kms_program_pipe(emgd_crtc); */
		/* Turn on pipe and plane */
		emgd_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
	}

	EMGD_TRACE_EXIT;
}


/*
 * Currently nothing in the drm code calls the crtc save/resume functions
 * The API docs are unclear what these would be used for.
 *
 * For now they are stubs just incase something eventually calls them.
 */
static void emgd_crtc_save(struct drm_crtc *crtc)
{
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((igd_context_t *)emgd_crtc->igd_context);

	/*
	 * This could call the DD layer's register save function. However
	 * that code would need to be re-worked so that it only saved a
	 * subset of the registers and there would need to be some provided
	 * locaton to save them.
	 */

	EMGD_TRACE_EXIT;
}

/*
 * Currently nothing in the drm code calls the crtc save/resume functions
 * The API docs are unclear what these would be used for.
 *
 * For now they are stubs just incase something eventually calls them.
 */
static void emgd_crtc_restore(struct drm_crtc *crtc)
{
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((igd_context_t *)emgd_crtc->igd_context);

	/*
	 * This could call the DD layer's register restore function. However
	 * that code would need to be re-worked so that it only restored a
	 * subset of the registers and there would need to be some provided
	 * locaton to restore them from.
	 */

	EMGD_TRACE_EXIT;
}

int emgd_crtc_cursor_prepare(struct drm_crtc *crtc, struct drm_file *file_priv, 
		uint32_t handle, uint32_t width, uint32_t height, 
		struct drm_i915_gem_object **obj_ret,uint32_t *addr_ret)
{
	struct drm_device *dev = crtc->dev;
	emgd_crtc_t *emgd_crtc = NULL;
	uint32_t addr;
	struct drm_i915_gem_object *obj;
	struct drm_i915_private *dev_priv = dev->dev_private;
	igd_context_t *context = NULL;
	int ret;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((drm_emgd_private_t *)crtc->dev->dev_private)->context;
	emgd_crtc->cursor_handle = handle;

	/*
	 * This code here is figuring out the memory that is backing the cursor
	 * image.  Once that is known, then the cursor programming can take
	 * place.
	 */
	if (!handle) {
		/* turn off the cursor? */
		addr = 0;
		obj = NULL;

		/* Mutex is assumed to be held during cleanup */
		mutex_lock(&dev->struct_mutex);
	} else {
		/* only support 64x64 cursors */
		if ((width != 64) || (height != 64)) {
			return -EINVAL;
		}

		obj = to_intel_bo(drm_gem_object_lookup(dev, file_priv, handle));
		if (&obj->base == NULL) {
			return -ENOENT;
		}

		/* Check the size of the buffer object */
		if (obj->base.size < (width * height * 4)) {
			drm_gem_object_unreference_unlocked(&obj->base);
			return -ENOMEM;
		}

		mutex_lock(&dev->struct_mutex);

		if (!dev_priv->info->cursor_needs_physical) {
			if (obj->tiling_mode) {
				EMGD_ERROR("Cursor cannot use tiled memory.");
				drm_gem_object_unreference(&obj->base);
				mutex_unlock(&dev->struct_mutex);
				return -EINVAL;
			}

			ret = i915_gem_object_pin_to_display_plane(obj, 0, NULL);
			if (ret) {
				EMGD_ERROR("Failed to move cursor bo into the GTT");
				drm_gem_object_unreference(&obj->base);
				mutex_unlock(&dev->struct_mutex);
				return -EINVAL;
			}

			ret = i915_gem_object_put_fence(obj);
			if (ret) {
				EMGD_ERROR("Failed to release fence for cursor");
				i915_gem_object_unpin(obj);
				drm_gem_object_unreference(&obj->base);
				mutex_unlock(&dev->struct_mutex);
				return -EINVAL;
			}

			addr = i915_gem_obj_ggtt_offset(obj);
		} else {
			int align = IS_I830(dev) ? 16 * 1024 : 256;

			ret = i915_gem_attach_phys_object(dev, obj,
					(emgd_crtc->pipe == 0) ?
					I915_GEM_PHYS_CURSOR_0 : I915_GEM_PHYS_CURSOR_1,
					align);
			if (ret) {
				EMGD_ERROR("Failed to attach phys object");
				drm_gem_object_unreference(&obj->base);
				mutex_unlock(&dev->struct_mutex);
				return -EINVAL;
			}
			addr = obj->phys_obj->handle->busaddr;
		}
	}

	mutex_unlock(&dev->struct_mutex);

	*obj_ret  = obj;
	*addr_ret = addr;

	EMGD_TRACE_EXIT;

	return 0;
}

void emgd_crtc_cursor_bo_unref(struct drm_crtc *crtc, struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	emgd_crtc_t *emgd_crtc = container_of(crtc, emgd_crtc_t, base);

	EMGD_TRACE_ENTER;

	/*
	 * If there's a reference to the cursor buffer object in the crtc,
	 * and it's not the same as the buffer object that's being used now,
	 * mark the old buffer object as unreferenced. The new buffer object
	 * would be attached to the crtc in emgd_crtc_cursor_commit.
	 */
	mutex_lock(&dev->struct_mutex);

	if (dev_priv->info->cursor_needs_physical) {
		if (emgd_crtc->cursor_bo != obj) {
			i915_gem_detach_phys_object(dev, obj);
		}
	} else {
		i915_gem_object_unpin(obj);
	}

	drm_gem_object_unreference(&obj->base);
	mutex_unlock(&dev->struct_mutex);

	EMGD_TRACE_EXIT;
}

void emgd_crtc_cursor_commit(struct drm_crtc *crtc, uint32_t handle, uint32_t width, 
		uint32_t height, struct drm_i915_gem_object *obj, uint32_t addr)
{
	emgd_crtc_t *emgd_crtc;
	igd_context_t *context;
	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((drm_emgd_private_t *)crtc->dev->dev_private)->context;
	emgd_crtc->igd_cursor->cursor_info->cursor_bo = obj;
	emgd_crtc->cursor_bo = obj;

	/* Update our cursor_info structure */
	emgd_crtc->igd_cursor->cursor_info->pixel_format = IGD_PF_ARGB32;
	emgd_crtc->igd_cursor->cursor_info->width = width;
	emgd_crtc->igd_cursor->cursor_info->height = height;
	emgd_crtc->igd_cursor->cursor_info->argb_offset = (unsigned long)addr;
	emgd_crtc->igd_cursor->cursor_info->argb_pitch = 64 * 4;

	/*
	 * The cursor palette is only used for 2 bit cursors.  Since this
	 * only supports ARGB cursors, the palette is set to 0.
	 */
	emgd_crtc->igd_cursor->cursor_info->palette[0] = 0;
	emgd_crtc->igd_cursor->cursor_info->palette[1] = 0;
	emgd_crtc->igd_cursor->cursor_info->palette[2] = 0;
	emgd_crtc->igd_cursor->cursor_info->palette[3] = 0;

	if (obj) {
		/* emgd_crtc->cursor_x and cursor_y would be used by Atomic page flip code through
		 * the properties interface. If the cursor coordinates (x,y) < 0, the appropriate
		 * adjustments would be made inside kms_program_cursor().
		 */
		emgd_crtc->igd_cursor->cursor_info->x_offset = emgd_crtc->cursor_x;
		emgd_crtc->igd_cursor->cursor_info->y_offset = emgd_crtc->cursor_y;

		context->mode_dispatch->kms_program_cursor(emgd_crtc, TRUE);
	} else {
		context->mode_dispatch->kms_program_cursor(emgd_crtc, FALSE);
	}

	EMGD_TRACE_EXIT;
}


/**
 * Handles attaching a crtc object to drm object properties and setting the
 * initial property value
 * @param crtc drm_crtc object
 */
void emgd_crtc_attach_properties(struct drm_crtc *crtc)
{
	struct drm_mode_object *obj = &crtc->base;
	emgd_crtc_t *emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	drm_i915_private_t* priv = (drm_i915_private_t *)crtc->dev->dev_private;

	/* Attach properties to crtc*/
	drm_object_attach_property(obj,	priv->crtc_properties.z_order_prop,
			emgd_crtc->zorder);
	drm_object_attach_property(obj, priv->crtc_properties.fb_blend_ovl_prop,
			emgd_crtc->fb_blend_ovl_on);
}


/**
 * Updates drm crtc properties with values from the driver
 * @param crtc drm crtc pointer
 * @param property_flag Flag for which property to update
 */
void emgd_crtc_update_properties(struct drm_crtc *crtc, uint32_t property_flag)
{
	struct drm_mode_object *obj = &crtc->base;
	emgd_crtc_t *emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	drm_i915_private_t* priv = (drm_i915_private_t *)crtc->dev->dev_private;

	if (property_flag & EMGD_CRTC_UPDATE_ZORDER_PROPERTY)
		drm_object_property_set_value(obj, priv->crtc_properties.z_order_prop,
				emgd_crtc->zorder);
	if (property_flag & EMGD_CRTC_UPDATE_FB_BLEND_OVL_PROPERTY)
		drm_object_property_set_value(obj,
				priv->crtc_properties.fb_blend_ovl_prop,
				emgd_crtc->fb_blend_ovl_on);
}


#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
void intel_crtc_update_properties(struct drm_crtc *crtc)
{
	struct drm_mode_object *obj = &crtc->base;
	struct drm_mode_config *config = &crtc->dev->mode_config;
	intel_crtc *intel_crtc = to_emgd_crtc(crtc);

	drm_crtc_update_properties(crtc);

	drm_object_property_set_value(obj, config->cursor_id_prop, intel_crtc->cursor_handle);
	drm_object_property_set_value(obj, config->cursor_x_prop, intel_crtc->cursor_x);
	drm_object_property_set_value(obj, config->cursor_y_prop, intel_crtc->cursor_y);
	drm_object_property_set_value(obj, config->cursor_w_prop, intel_crtc->cursor_width);
	drm_object_property_set_value(obj, config->cursor_h_prop, intel_crtc->cursor_height);
}

void intel_crtc_attach_properties(struct drm_crtc *crtc)
{
	struct drm_mode_object *obj = &crtc->base;
	struct drm_mode_config *config = &crtc->dev->mode_config;

	drm_crtc_attach_properties(crtc);

	drm_object_attach_property(obj, config->cursor_id_prop, 0);
	drm_object_attach_property(obj, config->cursor_x_prop, 0);
	drm_object_attach_property(obj, config->cursor_y_prop, 0);
	drm_object_attach_property(obj, config->cursor_w_prop, 0);
	drm_object_attach_property(obj, config->cursor_h_prop, 0);
}
#endif

static int emgd_crtc_cursor_set(struct drm_crtc *crtc,
		struct drm_file *file_priv, uint32_t handle,
		uint32_t width, uint32_t height)
{
	emgd_crtc_t *emgd_crtc = NULL;
	uint32_t addr;
	struct drm_i915_gem_object *obj, *old_obj;
	int ret;
	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	/* Update our cursor_info structure */
	EMGD_TRACE_ENTER;

	ret = emgd_crtc_cursor_prepare(crtc, file_priv, handle, width, height, &obj, &addr);
	if (ret) {
		return ret;
	}

	old_obj = emgd_crtc->cursor_bo;
	emgd_crtc_cursor_commit(crtc, handle, width, height, obj, addr);

	if (old_obj) {
		emgd_crtc_cursor_bo_unref(crtc, old_obj);
	}

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_crtc_update_properties(crtc);
#endif

	EMGD_TRACE_EXIT;
	return 0;
}


static int emgd_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	if (!emgd_crtc->igd_pipe->inuse) {
		EMGD_ERROR("\t\tpipe %d is not available", emgd_crtc->crtc_id);
		return 1;
	}
	context = ((drm_emgd_private_t *)crtc->dev->dev_private)->context;

	emgd_crtc->igd_cursor->cursor_info->x_offset = x;
	emgd_crtc->igd_cursor->cursor_info->y_offset = y;
	/* save latest cursor positon for later use	*/
	emgd_crtc->cursor_x = x;
	emgd_crtc->cursor_y = y;

	context->mode_dispatch->kms_alter_cursor_pos(emgd_crtc);

	return 0;
}

static void emgd_crtc_gamma_set(struct drm_crtc *crtc,
		unsigned short *red, unsigned short *green, unsigned short *blue,
		uint32_t start,
		uint32_t size)
{
	int end, i;
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	EMGD_TRACE_ENTER;

	end = (start + size > 256) ? 256 : start + size;
	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	context = ((igd_context_t *)emgd_crtc->igd_context);

	/*
	 * This function is called  by user space to set the gamma values.
	 *
	 * This sets the default palette.  The port attribute color correction
	 * values are then be applied on top of this.
	 */
	if (!emgd_crtc->palette_persistence) {
		for (i=start; i < end; i++) {
			emgd_crtc->default_lut_r[i] = red[i] >> 8;
			emgd_crtc->default_lut_g[i] = green[i] >> 8;
			emgd_crtc->default_lut_b[i] = blue[i] >> 8;
		}

		/* Apply port attribute settings */
		context->mode_dispatch->kms_set_color_correct(emgd_crtc);
	} else {
		EMGD_DEBUG("Ignore color palette from caller");
	}

	EMGD_TRACE_EXIT;
}

static void emgd_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;
	igd_display_pipe_t *igd_pipe = NULL;
	emgd_page_flip_work_t * work = NULL;
	unsigned long flags;
	int i;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	spin_lock_irqsave(&dev->event_lock, flags);
	work = emgd_crtc->flip_pending;
	emgd_crtc->flip_pending = NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (work) {
		cancel_work_sync(&work->work);
		kfree(work);
	}

	igd_pipe = emgd_crtc->igd_pipe;
	if (!igd_pipe) {
		EMGD_ERROR("\t\tpipe %d is not available", emgd_crtc->crtc_id);
		return;
	}
	context = ((igd_context_t *)emgd_crtc->igd_context);

	EMGD_DEBUG("\t\tpipe=%d", emgd_crtc->crtc_id);

	if (emgd_crtc->primary_regs) {
		kfree(emgd_crtc->primary_regs);
		emgd_crtc->primary_regs = NULL;
	}

	drm_crtc_cleanup(crtc);

	/* Remove ports from the CRTC. */
	for (i = 0; i < IGD_MAX_PORTS; i++) {
		if (emgd_crtc->ports[i] > 0) {
			context->module_dispatch.dsp_free_port(context,emgd_crtc->ports[i]);
			emgd_crtc->ports[i] = 0;
		}
	}

	/*
	 * This partially duplicates what's being done in dsp_free_pipe_planes
	 *
	 * It should all be done in one place.  Either mark all the pipes and
	 * planes as not in use here or in the dsp function (doesn't really
	 * matter much probably.
	 *
	 * The DRM code should have freed the framebuffers associated with
	 * the planes before calling this.  Don't worry about freeing any
	 * memory allocations for planes here.  (What about cursor planes?)
	 */
	/* context->module_dispatch.dsp_free_pipe_planes(context, emgd_crtc); */
	if (emgd_crtc->igd_pipe) {
		emgd_crtc->igd_pipe->inuse = 0;
		if (emgd_crtc->igd_pipe->timing) {
			OS_FREE(emgd_crtc->igd_pipe->timing);
			emgd_crtc->igd_pipe->timing = NULL;
		}
		emgd_crtc->igd_pipe = NULL;
	}
	if (emgd_crtc->igd_plane) {
		emgd_crtc->igd_plane->inuse = 0;
		emgd_crtc->igd_plane->fb_info = NULL;
		emgd_crtc->igd_plane = NULL;
	}
	if (emgd_crtc->igd_cursor) {
		if (emgd_crtc->igd_cursor->cursor_info) {
			OS_FREE(emgd_crtc->igd_cursor->cursor_info);
			emgd_crtc->igd_cursor->cursor_info = NULL;
		}
		emgd_crtc->igd_cursor->inuse = 0;
		emgd_crtc->igd_cursor = NULL;
	}

	EMGD_TRACE_EXIT;
}

static void emgd_crtc_load_lut(struct drm_crtc *crtc)
{
	emgd_crtc_t *emgd_crtc = NULL;
	igd_context_t *context = NULL;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	if (!emgd_crtc->igd_pipe) {
		EMGD_ERROR("\t\tpipe %d is not available", emgd_crtc->crtc_id);
		return;
	}
	context = ((igd_context_t *)emgd_crtc->igd_context);
	EMGD_DEBUG("\t\tpipe=%d", emgd_crtc->crtc_id);

	context->mode_dispatch->kms_set_palette_entries(emgd_crtc, 0, 256);

	EMGD_TRACE_EXIT;
}



void intel_prepare_page_flip(struct drm_device *dev, int crtcnum)
{
	drm_emgd_private_t	*priv = dev->dev_private;
	emgd_crtc_t		*emgd_crtc = priv->crtcs[crtcnum];
	unsigned long		flags;

	/* NB: An MMIO update of the plane base pointer will also
	 * generate a page-flip completion irq, i.e. every modeset
	 * is also accompanied by a spurious intel_prepare_page_flip().
	 */
	spin_lock_irqsave(&dev->event_lock, flags);
	if (emgd_crtc->flip_pending)
		atomic_inc_not_zero(&emgd_crtc->flip_pending->pending);
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static inline void intel_mark_page_flip_active(emgd_crtc_t *emgd_crtc)
{
	/* Ensure that the work item is consistent when activating it ... */
	smp_wmb();
	atomic_set(&emgd_crtc->flip_pending->pending, INTEL_FLIP_PENDING);
	/* and that it is marked active as soon as the irq could fire. */
	smp_wmb();
}

/*
 * crtc_pageflip_handler()
 *
 * VBlank handler to be called when a pageflip is complete.  This will send
 * the vblank event to userspace.
 *
 * State upon entry:
 *
 * State upon exit (assuming entered with vblank_expected):
 *
 * @param dev     [IN]  DRM device
 * @param crtcnum [IN]  CRTC Number, 0 for pipe A, 1 for pipe B
 */
int crtc_pageflip_handler(struct drm_device *dev, int crtcnum)
{
	drm_emgd_private_t    *priv = dev->dev_private;
	emgd_crtc_t           *emgd_crtc;
	struct timeval         now;
	igd_context_t         *context = NULL;
	unsigned long          flags, plane_select = 0;
	emgd_page_flip_work_t *page_flip_work;

	struct drm_pending_vblank_event *e;


	EMGD_TRACE_ENTER;

	context = priv->context;

	if (NULL == (emgd_crtc = priv->crtcs[crtcnum])) {
		EMGD_ERROR("Invalid crtc number in page flip");
		return 1;
	}

	do_gettimeofday(&now);

	spin_lock_irqsave(&dev->event_lock, flags);
	page_flip_work = emgd_crtc->flip_pending;

	/* 
	 * Daniel: Review the barriers a bit, we need a write barrier both
	 * before and after updating ->flip_pending. Similarly we need a read
	 * barrier in the interrupt handler both before and after reading
	 * ->flip_pending. With well-ordered irqs only one barrier in each place
	 * should be required, but since this patch explicitly sets out to combat
	 * spurious interrupts with is staged activation of the unpin work we need
	 * to go full-bore on the barriers, too.
	 */

	/* Ensure we don't miss a page_flip_work->pending update ... */
	smp_rmb();

	/* If there is nothing to do, then do nothing */
	if (NULL == page_flip_work || atomic_read(&page_flip_work->pending) <
			INTEL_FLIP_COMPLETE) {
		EMGD_DEBUG("Ignoring spurious flip completion triggered by MMIO plane update.");
		spin_unlock_irqrestore(&dev->event_lock, flags);
		return 0;
	}

	/* and that the unpin work is consistent wrt ->flip_pending. */
	smp_rmb();

	emgd_crtc->flip_pending = NULL;

	/* Flip is now complete; send userspace event, if requested */
	if (page_flip_work->event) {
		e = page_flip_work->event;
		e->event.sequence = 0;
		e->event.tv_sec   = now.tv_sec;
		e->event.tv_usec  = now.tv_usec;
		list_add_tail(&e->base.link, &e->base.file_priv->event_list);
		wake_up_interruptible(&e->base.file_priv->event_wait);
	}

	/* Release vblank refcount */
	drm_vblank_put(dev, crtcnum);

	if (emgd_crtc->igd_plane->plane_features & IGD_PLANE_IS_PLANEB) {
		plane_select = 1;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);

	atomic_clear_mask(1 << plane_select,
			  &page_flip_work->old_fb_obj->pending_flip.counter);

	wake_up_all(&priv->pending_flip_queue);

	queue_work(priv->wq, &page_flip_work->work);

	EMGD_TRACE_EXIT;
	return 1;
}


int sprite_pageflip_handler(struct drm_device *dev, int crtcnum)
{
   	drm_emgd_private_t    *priv = dev->dev_private;
    emgd_crtc_t           *emgd_crtc;
    struct timeval         now;
    igd_context_t         *context = NULL;
    unsigned long          flags, plane_select = 0;
    emgd_page_flip_work_t *page_flip_work;
 
    struct drm_pending_vblank_event *e;
    int i;
 

    EMGD_TRACE_ENTER;
 
    context = priv->context;
 
	if (NULL == (emgd_crtc = priv->crtcs[crtcnum])) {
    	EMGD_ERROR("Invalid crtc number in page flip");
        return 1;
  	}
 
    do_gettimeofday(&now);
 
    /* Protect access to the CRTC structure */
	spin_lock_irqsave(&dev->event_lock, flags);
 
    /* If there is nothing to do, then do nothing */
    if (NULL == emgd_crtc->sprite_flip_pending) {
    	EMGD_DEBUG("No pending page flip");
	spin_unlock_irqrestore(&dev->event_lock, flags);
        return 0;
  	}
 
   	page_flip_work          = emgd_crtc->sprite_flip_pending;
   	emgd_crtc->sprite_flip_pending = NULL;

   	/* Flip is now complete; send userspace event, if requested */
   	if (page_flip_work->event) {
    	e = page_flip_work->event;
        e->event.sequence = 0;
        e->event.tv_sec   = now.tv_sec;
        e->event.tv_usec  = now.tv_usec;
       	list_add_tail(&e->base.link, &e->base.file_priv->event_list);
        wake_up_interruptible(&e->base.file_priv->event_wait);
    }
 
  	/* Release vblank refcount */
    drm_vblank_put(dev, crtcnum);

    /* for  different CRTC, plane_select is diff */
	spin_unlock_irqrestore(&dev->event_lock, flags);
 
  	for (i = 0; i < 2; i++){
    	plane_select = 2 + i;
 
        atomic_clear_mask(1 << plane_select,
        &page_flip_work->old_stereo_obj[i]->pending_flip.counter);
 	}
  	/* Use queue_work() as in crtc_pageflip_handler */
  	queue_work(priv->wq, &page_flip_work->work);
 
  	EMGD_TRACE_EXIT;
   	return 1;
}

static void emgd_unpin_work(struct work_struct *__work)
{
	emgd_page_flip_work_t *page_flip_work =
			container_of(__work, emgd_page_flip_work_t, work);
	emgd_crtc_t * emgd_crtc = (emgd_crtc_t *)page_flip_work->emgd_crtc;

	/* Update sysfs' view of the CRTC */
	emgd_sysfs_switch_crtc_fb(page_flip_work->emgd_crtc);

	mutex_lock(&page_flip_work->dev->struct_mutex);
	intel_unpin_fb_obj(page_flip_work->old_fb_obj);
	drm_gem_object_unreference(&page_flip_work->new_fb_obj->base);
	drm_gem_object_unreference(&page_flip_work->old_fb_obj->base);
	mutex_unlock(&page_flip_work->dev->struct_mutex);

	BUG_ON(atomic_read(&emgd_crtc->unpin_work_count) == 0);
	atomic_dec(&emgd_crtc->unpin_work_count);

	kfree(page_flip_work);
}

static void emgd_unpin_stereo_work(struct work_struct *__work)
{
	int i;
 
    emgd_page_flip_work_t *page_flip_work =
    	container_of(__work, emgd_page_flip_work_t, work);
 
  	/* Update sysfs' view of the CRTC */
  	emgd_sysfs_switch_crtc_fb(page_flip_work->emgd_crtc);
 
  	mutex_lock(&page_flip_work->dev->struct_mutex);
 
  	for (i = 0; i < 2; i++) {
    	intel_unpin_fb_obj(page_flip_work->old_stereo_obj[i]);
      	drm_gem_object_unreference(&page_flip_work->old_stereo_obj[i]->base);
      	drm_gem_object_unreference(&page_flip_work->new_stereo_obj[i]->base);
   	}
 
  	mutex_unlock(&page_flip_work->dev->struct_mutex);
  	kfree(page_flip_work);
}


/**
 * emgd_crtc_page_flip
 *
 * This function will queue a page flip instruction into the ring buffer
 * and returns.  When the flip eventually happens, it generates an
 * interrupt which will then call crtc_pageflip_handler() to complete the
 * flip request, e.g. doing things like signaling user-application event
 * and unpinning the old framebuffer.
 *
 * @param crtc  (INOUT) The pipe to put the new framebuffer on
 * @param fb    (IN)    Framebuffer to flip to
 * @param event (IN)    Event to signal when flip has been completed
 *
 * @return 0 on success, an error code otherwise
 */
static int emgd_crtc_page_flip(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
				struct drm_pending_vblank_event *event,
				uint32_t page_flip_flags)
#else
				struct drm_pending_vblank_event *event)
#endif
{
	emgd_page_flip_work_t *page_flip_work;
	emgd_crtc_t           *emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	unsigned long          flags;
	emgd_framebuffer_t    *emgd_old_fb, *emgd_new_fb;
	int                    ret, plane_select = 0;
	struct drm_device      *dev = crtc->dev;
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;
	drm_emgd_private_t    *priv = dev->dev_private;


	EMGD_TRACE_ENTER;

   	emgd_old_fb = container_of(crtc->fb, emgd_framebuffer_t, base);
   	emgd_new_fb = container_of(fb, emgd_framebuffer_t, base);

   	if (emgd_crtc->current_s3d_mode != PD_S3D_MODE_OFF &&
   		emgd_new_fb->other_bo[0] && emgd_new_fb->other_bo[1])
   	{
   		emgd_new_fb->igd_flags |= IGD_HDMI_STEREO_3D_MODE;
		ret=emgd_stereo_crtc_page_flip(crtc, fb, event);
		if (ret) {
			EMGD_ERROR("emgd_stereo_crtc_page_flip failed![%d]", ret);
			return ret;
		}
   	}

	page_flip_work = kzalloc(sizeof(*page_flip_work), GFP_KERNEL);
	if (NULL == page_flip_work) {
		EMGD_ERROR("Cannot allocate memory for page_flip_work");
		return -ENOMEM;
	}

	page_flip_work->event      = event;
	page_flip_work->dev        = crtc->dev;
	page_flip_work->old_fb_obj = emgd_old_fb->bo;
	page_flip_work->emgd_crtc = emgd_crtc;
	INIT_WORK(&page_flip_work->work, emgd_unpin_work);

	ret = drm_vblank_get(crtc->dev, emgd_crtc->pipe);
	if (ret) {
		kfree(page_flip_work);
		return ret;
	}

	spin_lock_irqsave(&dev->event_lock, flags);

	if (emgd_crtc->flip_pending) {
		spin_unlock_irqrestore(&dev->event_lock, flags);
		drm_vblank_put(dev, emgd_crtc->pipe);
		kfree(page_flip_work);

		EMGD_ERROR("flip queue: crtc already busy");
		return -EBUSY;
	}

	emgd_crtc->flip_pending = page_flip_work;

	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (atomic_read(&emgd_crtc->unpin_work_count) >= 2) {
		flush_workqueue(priv->wq);
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret) {
		spin_lock_irqsave(&dev->event_lock, flags);
		emgd_crtc->flip_pending = NULL;
		spin_unlock_irqrestore(&dev->event_lock, flags);

		drm_vblank_put(crtc->dev, emgd_crtc->pipe);

		kfree(page_flip_work);

		return ret;
	}

	page_flip_work->new_fb_obj = emgd_new_fb->bo;


	/* Reference the objects for the scheduled work. */
	drm_gem_object_reference(&page_flip_work->old_fb_obj->base);
	drm_gem_object_reference(&page_flip_work->new_fb_obj->base);

	crtc->fb = fb;

	/* Block clients from rendering to the new back buffer until
	 * the flip occurs and the object is no longer visible.
	 */
	if (emgd_crtc->igd_plane->plane_features & IGD_PLANE_IS_PLANEB) {
		plane_select = 1;
	}

	atomic_inc(&emgd_crtc->unpin_work_count);
	atomic_add(1 << plane_select, &page_flip_work->old_fb_obj->pending_flip);

	ret = context->mode_dispatch->kms_queue_flip(emgd_crtc, emgd_new_fb,
			emgd_new_fb->bo, plane_select);

	intel_mark_page_flip_active(emgd_crtc);

	if (ret) {
		atomic_dec(&emgd_crtc->unpin_work_count);
		atomic_sub(1 << plane_select,
			&page_flip_work->old_fb_obj->pending_flip);
		drm_gem_object_unreference(&page_flip_work->old_fb_obj->base);
		drm_gem_object_unreference(&page_flip_work->new_fb_obj->base);
		mutex_unlock(&crtc->dev->struct_mutex);

		spin_lock_irqsave(&dev->event_lock, flags);
		emgd_crtc->flip_pending = NULL;
		spin_unlock_irqrestore(&dev->event_lock, flags);

		kfree(page_flip_work);
		drm_vblank_put(crtc->dev, emgd_crtc->pipe);

		return ret;
	}
	intel_mark_fb_busy(page_flip_work->new_fb_obj, NULL);
	mutex_unlock(&crtc->dev->struct_mutex);

	EMGD_TRACE_EXIT;

	return 0;
}

/**
 * emgd_crtc_set_property
 * drm_crtc_funcs.set_property function pointer implementation.
 * Called to set the driver value of a crtc property
 * @param crtc Pointer to drm_crtc
 * @param property	The drm crtc property to update
 * @param val The value to set
 * @return Returns 0 if successful
 */
static int emgd_crtc_set_property(struct drm_crtc *crtc,
		struct drm_property *property, uint64_t val)
{
	emgd_crtc_t *emgd_crtc = NULL;
	int ret = -EINVAL;
	drm_i915_private_t* priv = (drm_i915_private_t *)crtc->dev->dev_private;

	EMGD_TRACE_ENTER;

	emgd_crtc = container_of(crtc, emgd_crtc_t, base);
	if (property == priv->crtc_properties.z_order_prop &&
			isvalid_zorder_value(val)) {
		mutex_lock(&crtc->dev->struct_mutex);

		emgd_crtc->zorder = val;
		emgd_crtc->sprite1->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder,
						emgd_crtc->sprite1->plane_id);
		emgd_crtc->sprite2->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder,
						emgd_crtc->sprite2->plane_id);
		mutex_unlock(&crtc->dev->struct_mutex);
		ret = 0;
	} else if (property == priv->crtc_properties.fb_blend_ovl_prop) {
		emgd_crtc->fb_blend_ovl_on = val;
		ret = 0;
	}

	EMGD_TRACE_EXIT;
	return ret;
}

static int emgd_stereo_crtc_page_flip(struct drm_crtc *crtc,
               struct drm_framebuffer *fb,
               struct drm_pending_vblank_event *event)
{
  	emgd_page_flip_work_t *page_flip_work;
    emgd_crtc_t           *emgd_crtc = container_of(crtc, emgd_crtc_t, base);
    unsigned long          flags;
    emgd_framebuffer_t    *emgd_old_fb, *emgd_new_fb;
    int                    ret, plane_select = 0;
    struct drm_device      *dev = crtc->dev;
    int                    i;
    igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;

  	EMGD_TRACE_ENTER;

  	if(!emgd_crtc->sprite1 || !emgd_crtc->sprite2) {
  		EMGD_ERROR("Unable to flip, sprite plane in current CRTC is NULL!");
		return -ENXIO;
  	}

   	emgd_old_fb = container_of(crtc->fb, emgd_framebuffer_t, base);

    page_flip_work = kzalloc(sizeof(*page_flip_work), GFP_KERNEL);
    if (NULL == page_flip_work) {
    	EMGD_ERROR("Cannot allocate memory for page_flip_work");
        return -ENOMEM;
   	}

  	mutex_lock(&crtc->dev->struct_mutex);

  	/* The event will be handled by emgd_crtc_page_flip */
   	page_flip_work->event      = NULL;
   	page_flip_work->dev        = crtc->dev;
   	page_flip_work->emgd_crtc = emgd_crtc;
   	INIT_WORK(&page_flip_work->work, emgd_unpin_stereo_work);

   	ret = drm_vblank_get(crtc->dev, emgd_crtc->pipe);
   	if (ret) {
    	kfree(page_flip_work);
        mutex_unlock(&crtc->dev->struct_mutex);
        return ret;
   	}

	spin_lock_irqsave(&dev->event_lock, flags);

   	if (emgd_crtc->sprite_flip_pending) {
    	EMGD_ERROR("flip queue: crtc already busy with %p", emgd_crtc->sprite_flip_pending);
	spin_unlock_irqrestore(&dev->event_lock, flags);
       	drm_vblank_put(dev, emgd_crtc->pipe);
       	kfree(page_flip_work);

       	mutex_unlock(&crtc->dev->struct_mutex);
        return -EBUSY;
  	}

   	emgd_crtc->sprite_flip_pending = page_flip_work;

	spin_unlock_irqrestore(&dev->event_lock, flags);

   	emgd_new_fb                = container_of(fb, emgd_framebuffer_t, base);
   	crtc->fb = fb;

   	for (i = 0; i < 2; i++) {
    	if (!emgd_crtc->pipe) {
        	/* Pipe A */
    		/* Sprite A = 0x2
    		 * Sprite B = 0x3
    		 */
          	plane_select = 2 + i;
      	} else {
        	/* Pipe B */
      		/* The BSpec doesn't clearly specify the setting for Sprite C and D,
      		 * but from experiment, this is the setting found
    		 * Sprite C = 0x5
    		 * Sprite D = 0x4
             */
           	plane_select = 5 - i;
     	}

      	page_flip_work->old_stereo_obj[i] = emgd_old_fb->other_bo[i];
      	page_flip_work->new_stereo_obj[i] = emgd_new_fb->other_bo[i];

      	if (i == 0) {
      		emgd_crtc->sprite1->curr_surf.bo = page_flip_work->new_stereo_obj[i];
      	} else {
        	emgd_crtc->sprite2->curr_surf.bo = page_flip_work->new_stereo_obj[i];
      	}

      	/* Reference the objects for the scheduled work. */
      	drm_gem_object_reference(&page_flip_work->old_stereo_obj[i]->base);
      	drm_gem_object_reference(&page_flip_work->new_stereo_obj[i]->base);

      	atomic_add(1 << plane_select, &page_flip_work->old_stereo_obj[i]->pending_flip);

      	ret = context->mode_dispatch->kms_queue_flip(emgd_crtc, emgd_new_fb,
        		emgd_new_fb->other_bo[i], plane_select);

     	if (ret) {
        	atomic_sub(1 << plane_select,
            	&page_flip_work->old_stereo_obj[i]->pending_flip);
          	drm_gem_object_unreference(&page_flip_work->old_stereo_obj[i]->base);
            drm_gem_object_unreference(&page_flip_work->new_stereo_obj[i]->base);
            break;

      	}
 	}

  	if (ret) {
    	mutex_unlock(&crtc->dev->struct_mutex);

	spin_lock_irqsave(&dev->event_lock, flags);
        emgd_crtc->sprite_flip_pending = NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);

        kfree(page_flip_work);
        drm_vblank_put(crtc->dev, emgd_crtc->pipe);

        return ret;
 	}

  	mutex_unlock(&crtc->dev->struct_mutex);

    EMGD_TRACE_EXIT;

   	return 0;
}


/**
 * intel_update_watermarks - update FIFO watermark values based on current modes
 *
 * Calculate watermark values for the various WM regs based on current mode
 * and plane configuration.
 *
 * There are several cases to deal with here:
 *   - normal (i.e. non-self-refresh)
 *   - self-refresh (SR) mode
 *   - lines are large relative to FIFO size (buffer can hold up to 2)
 *   - lines are small relative to FIFO size (buffer can hold more than 2
 *     lines), so need to account for TLB latency
 *
 *   The normal calculation is:
 *     watermark = dotclock * bytes per pixel * latency
 *   where latency is platform & configuration dependent (we assume pessimal
 *   values here).
 *
 *   The SR calculation is:
 *     watermark = (trunc(latency/line time)+1) * surface width *
 *       bytes per pixel
 *   where
 *     line time = htotal / dotclock
 *     surface width = hdisplay for normal plane and 64 for cursor
 *   and latency is assumed to be high, as above.
 *
 * The final value programmed to the register should always be rounded up,
 * and include an extra 2 entries to account for clock crossings.
 *
 * We don't use the sprite, so we can ignore that.  And on Crestline we have
 * to set the non-SR watermarks to 8.
 */
void intel_update_watermarks(struct drm_device *dev)
{
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;

	if (context->mode_dispatch->kms_update_wm) {
		mutex_lock(&dev->struct_mutex);
		context->mode_dispatch->kms_update_wm(dev);
		mutex_unlock(&dev->struct_mutex);
	}
}

bool intel_encoder_crtc_ok(struct drm_encoder *encoder,
			   struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct drm_crtc *tmp;
	int crtc_mask = 1;

	WARN(!crtc, "checking null crtc?\n");
	if (!crtc) {
		return false;
	}

	dev = crtc->dev;

	list_for_each_entry(tmp, &dev->mode_config.crtc_list, head) {
		if (tmp == crtc)
			break;
		crtc_mask <<= 1;
	}

	if (encoder->possible_crtcs & crtc_mask)
		return true;
	return false;
}

int emgd_finish_fb(struct drm_framebuffer *old_fb)
{
	struct drm_i915_gem_object *obj = (container_of(old_fb, emgd_framebuffer_t, base))->bo;
	drm_emgd_private_t *dev_priv = obj->base.dev->dev_private;
	bool was_interruptible = dev_priv->mm.interruptible;
	int ret;

	wait_event(dev_priv->pending_flip_queue,
			i915_reset_in_progress(&dev_priv->gpu_error) ||
			atomic_read(&obj->pending_flip) == 0);

	/* Big Hammer, we also need to ensure that any pending
	 * MI_WAIT_FOR_EVENT inside a user batch buffer on the
	 * current scanout is retired before unpinning the old
	 * framebuffer.
	 *
	 * This should only fail upon a hung GPU, in which case we
	 * can safely continue.
	 */
	dev_priv->mm.interruptible = false;
	ret = i915_gem_object_finish_gpu(obj);
	dev_priv->mm.interruptible = was_interruptible;

	return ret;
}

/**
 * intel_modeset_commit_output_state
 *
 * This function copies the stage display pipe configuration to the real one.
 */
void intel_modeset_commit_output_state(struct drm_device *dev)
{
	emgd_encoder_t *encoder;
	emgd_connector_t *connector;
	emgd_crtc_t *new_crtc;

	list_for_each_entry(connector, &dev->mode_config.connector_list,
			    base.head) {
		connector->base.encoder = &connector->new_encoder->base;
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list,
			    base.head) {
		new_crtc = (emgd_crtc_t *)encoder->new_crtc;
		encoder->base.crtc = &new_crtc->base;
	}
}

/**
 * intel_modeset_update_staged_output_state
 *
 * Updates the staged output configuration state, e.g. after we've read out the
 * current hw state.
 */
void intel_modeset_update_staged_output_state(struct drm_device *dev)
{
	emgd_encoder_t *encoder;
	emgd_connector_t *connector;

	list_for_each_entry(connector, &dev->mode_config.connector_list,
			    base.head) {
		connector->new_encoder = to_emgd_encoder(connector->base.encoder);
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list,
			    base.head) {
		encoder->new_crtc = to_emgd_crtc(encoder->base.crtc);
	}
}

static bool intel_crtc_has_pending_flip(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	drm_emgd_private_t *dev_priv = dev->dev_private;
	emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);
	unsigned long flags;
	bool pending;

	if (i915_reset_in_progress(&dev_priv->gpu_error))
		return false;

	spin_lock_irqsave(&dev->event_lock, flags);
	pending = emgd_crtc->flip_pending != NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return pending;
}

static void intel_crtc_wait_for_pending_flips(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	drm_emgd_private_t *dev_priv = dev->dev_private;

	if (crtc->fb == NULL)
		return;

	wait_event(dev_priv->pending_flip_queue,
		   !intel_crtc_has_pending_flip(crtc));

	mutex_lock(&dev->struct_mutex);
	emgd_finish_fb(crtc->fb);
	mutex_unlock(&dev->struct_mutex);
}

void emgd_crtc_disable(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;
	emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);
	struct drm_encoder_helper_funcs *encoder_funcs;
	int pipe = emgd_crtc->pipe;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder_funcs = encoder->helper_private;
		if(!drm_helper_encoder_in_use(encoder)){
			encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
		}
	}

	intel_crtc_wait_for_pending_flips(crtc);

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_atomic_clear_flips(crtc);
#endif

	if (crtc->fb) {
		mutex_lock(&dev->struct_mutex);
		intel_unpin_fb_obj(to_intel_framebuffer(crtc->fb)->bo);
		mutex_unlock(&dev->struct_mutex);
		crtc->fb = NULL;
	}

	drm_vblank_off(dev, pipe);

	emgd_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
	emgd_crtc->active = false;
	intel_update_watermarks(dev);
}

void emgd_crtc_enable(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);

	if (emgd_crtc->active)
		return;

	emgd_crtc->active = true;
	intel_update_watermarks(dev);

	emgd_crtc_load_lut(crtc);
}

