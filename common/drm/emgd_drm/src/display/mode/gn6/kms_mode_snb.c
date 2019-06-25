/*
 *-----------------------------------------------------------------------------
 * Filename: kms_mode_snb.c
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
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.mode


#include <math_fix.h>
#include <memory.h>
#include <ossched.h>

#include <igd_core_structs.h>
#include <vga.h>
#include <pi.h>
#include <gn6/regs.h>
#include <gn6/context.h>
#include <gn6/instr.h>
#include "regs_common.h"

#include <i915/intel_bios.h>
#include <i915/intel_ringbuffer.h>
#include "drm_emgd_private.h"
#include "emgd_drm.h"

#include "../cmn/match.h"
#include "mode_snb.h"


/*------------------------------------------------------------------------------
 * Function Prototypes : Exported Device Dependent Hooks for KMS
 *----------------------------------------------------------------------------*/
static void kms_program_pipe_snb(emgd_crtc_t *emgd_crtc);
static void kms_set_pipe_pwr_snb(emgd_crtc_t *emgd_crtc, unsigned long enable);
static int __must_check kms_program_plane_snb(emgd_crtc_t *emgd_crtc, bool preserve);
static void kms_set_plane_pwr_snb(emgd_crtc_t *emgd_crtc, unsigned long enable, bool wait_for_vblank);
static int  kms_program_port_snb(emgd_encoder_t *emgd_encoder,
				unsigned long status);
static int  kms_post_program_port_snb(emgd_encoder_t *emgd_encoder,
				unsigned long status);
static u32  kms_get_vblank_counter_snb(emgd_crtc_t *emgd_crtc);
static void kms_disable_vga_snb(igd_context_t * context);
static void kms_program_cursor_snb(emgd_crtc_t *emgd_crtc,
				unsigned long status);
static int  kms_alter_cursor_pos_snb(emgd_crtc_t *emgd_crtc);
static int  kms_queue_flip_snb(emgd_crtc_t *emgd_crtc,
				emgd_framebuffer_t *emgd_fb,
               	struct drm_i915_gem_object *bo,
               	int plane_select);
static int  kms_set_palette_entries_snb(emgd_crtc_t *emgd_crtc,
				unsigned int start_index, unsigned int count);
static void kms_set_display_base_snb(emgd_crtc_t *emgd_crtc,
				emgd_framebuffer_t *fb);
static int kms_set_color_correct_snb(emgd_crtc_t *emgd_crtc);


/*------------------------------------------------------------------------------
 * Function Prototypes : Mode-Exported Inter-Module helper functions
 *----------------------------------------------------------------------------*/
static void im_reset_plane_pipe_ports_snb(igd_context_t *context);
static void im_filter_modes_snb(igd_context_t *context, igd_display_port_t *port,
		pd_timing_t *in_list);


/*------------------------------------------------------------------------------
 * KMS Dispatch Table
 *----------------------------------------------------------------------------*/
igd_mode_dispatch_t mode_kms_dispatch_snb = {
	/* Device Independent Layer Interfaces - (Filled by mode_init?) */
		kms_match_mode,
	/* Device Dependent Layer Interfaces - Defined by this table */
		kms_program_pipe_snb,
		kms_set_pipe_pwr_snb,
		kms_program_plane_snb,
		NULL, NULL,
		kms_set_plane_pwr_snb,
		kms_program_port_snb,
		kms_post_program_port_snb,
		kms_get_vblank_counter_snb,
		kms_disable_vga_snb,
		kms_program_cursor_snb,
		kms_alter_cursor_pos_snb,
		kms_queue_flip_snb,
		kms_set_palette_entries_snb,
		kms_set_display_base_snb,
		kms_set_color_correct_snb,
		0,
		0,
	/* Device Dependant Layer - Inter-Module Interfaces - not called by KMS */
		im_reset_plane_pipe_ports_snb,
		im_filter_modes_snb,
		0,
		0,
		0,
		0,

};

/*------------------------------------------------------------------------------
 * Function Prototypes : Internal Device Dependent helper functions
 *----------------------------------------------------------------------------*/
extern int program_clock_snb(emgd_crtc_t *emgd_crtc, igd_clock_t *clock,
				unsigned long dclk, unsigned short operation);
static void program_pipe_vga_snb(emgd_crtc_t *emgd_crtc);
static int  program_port_digital_snb(emgd_encoder_t *emgd_encoder,
	unsigned long status);
static void wait_pipe(unsigned char *mmio, unsigned long pipe_reg,
				unsigned long check_on_off);
static void disable_pipe_snb(igd_context_t *context,
				unsigned int pipe_num);
void wait_for_vblank_snb(unsigned char *mmio, unsigned long pipe_reg);
extern int s3d_timings_fixup(igd_display_port_t *port,
				igd_timing_info_t *current_timings,
				igd_timing_info_t *new_timings);

/*------------------------------------------------------------------------------
 * Function Prototypes : Externs outside of HAL - Should we wrap this??
 *----------------------------------------------------------------------------*/
extern int intel_pin_and_fence_fb_obj(struct drm_device *dev,
		struct drm_i915_gem_object *obj,
		struct intel_ring_buffer *pipelined);

/*------------------------------------------------------------------------------
 * Internal Device Dependent Global Variables
 *----------------------------------------------------------------------------*/
extern unsigned long ports_snb[2];

mode_data_snb_t device_data_snb[1] = {
	{
		0x00000000, /* plane a preservation */
		0x00000000, /* plane b/c preservation */
		0x00000000, /* pipe preservation */
		0x1f8f0f0f, /* watermark plane a/b */
		0x00000f0f, /* watermark plane c */
		0x00001d9c, /* dsp arb */
	}
};

/*!
 * kms_set_color_correct_snb
 *
 * @param emgd_crtc
 *
 * @return 0 on success
 * @return -IGD_ERROR_INVAL if color attributes not found
 */
static int kms_set_color_correct_snb(emgd_crtc_t *emgd_crtc)
{
	const int        MID_PIXEL_VAL    = 125;
	const int        MAX_PIXEL_VAL    = 255;
	const int        NUM_PALETTE_ENTRIES = 256;

	igd_display_port_t *port         = NULL;
	igd_display_pipe_t *pipe         = NULL;
	igd_context_t      *context      = NULL;

	unsigned int     gamma_r_max_24i_8f, gamma_r_min_24i_8f;
	unsigned int     gamma_g_max_24i_8f, gamma_g_min_24i_8f;
	unsigned int     gamma_b_max_24i_8f, gamma_b_min_24i_8f;
	unsigned int     new_gamma_r_24i_8f, new_gamma_g_24i_8f;
	unsigned int     new_gamma_b_24i_8f;
	unsigned int     gamma_normal_r_24i_8f, gamma_normal_g_24i_8f;
	unsigned int     gamma_normal_b_24i_8f;
	int              brightness_factor_r, brightness_factor_g;
	int              brightness_factor_b;
	int              contrast_factor_r, contrast_factor_g;
	int              contrast_factor_b;

	unsigned int      *palette;
	unsigned int       i;

	igd_range_attr_t *gamma_attr      = NULL, *contrast_attr = NULL;
	igd_range_attr_t *brightness_attr = NULL;
	igd_attr_t       *hal_attr_list   = NULL;

	EMGD_TRACE_ENTER;

	pipe    = PIPE(emgd_crtc);
	port    = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	if (!port) {
		EMGD_ERROR_EXIT("No port being used.");
		return -IGD_ERROR_INVAL;
	}
	hal_attr_list  = port->attributes;

	/* Using OS_ALLOC to avoid using > 1024 on stack (frame size warning ) */
	palette = OS_ALLOC(sizeof (unsigned int) * NUM_PALETTE_ENTRIES);

	/* start with a fresh palette */
	for (i = 0; i < NUM_PALETTE_ENTRIES; i++) {
		palette[i] = (emgd_crtc->default_lut_r[i] << 16) |
			(emgd_crtc->default_lut_g[i] << 8) |
			emgd_crtc->default_lut_b[i];

	}

	/* get a pointer to gamma, contrast, and brightness attr */
	i = 0;

	while (PD_ATTR_LIST_END != hal_attr_list[i].id) {
		switch (hal_attr_list[i].id) {
		case PD_ATTR_ID_FB_GAMMA:
			gamma_attr      = (igd_range_attr_t *) &hal_attr_list[i];
			break;

		case PD_ATTR_ID_FB_BRIGHTNESS:
			brightness_attr = (igd_range_attr_t *) &hal_attr_list[i];
			break;

		case PD_ATTR_ID_FB_CONTRAST:
			contrast_attr   = (igd_range_attr_t *) &hal_attr_list[i];
			break;

		default:
			break;
		}

		i++;
	}

	if(!gamma_attr || !brightness_attr || !contrast_attr) {
		EMGD_ERROR_EXIT("Color Correction Atrributes not found!");
		OS_FREE(palette);
		return -IGD_ERROR_INVAL;
	}

	EMGD_DEBUG("Port=0x%x, Gamma=0x%x, Brightness=0x%x, Contrast=0x%x",
			port->port_number, gamma_attr->current_value,
			brightness_attr->current_value, contrast_attr->current_value);

	/* Get the max and min */
	gamma_r_max_24i_8f = ((gamma_attr->max >> 16) & 0xFF) << 3;
	gamma_g_max_24i_8f = ((gamma_attr->max >>  8) & 0xFF) << 3;
	gamma_b_max_24i_8f =  (gamma_attr->max        & 0xFF) << 3;

	gamma_r_min_24i_8f = ((gamma_attr->min >> 16) & 0xFF) << 3;
	gamma_g_min_24i_8f = ((gamma_attr->min >>  8) & 0xFF) << 3;
	gamma_b_min_24i_8f =  (gamma_attr->min        & 0xFF) << 3;

	/* The new gamma values are in 3i.5f format, but we must convert it
	 * to 24i.8f format before passing it to OS_POW_FIX
	 */
	new_gamma_r_24i_8f = ((gamma_attr->current_value >> 16) & 0xFF) << 3;
	new_gamma_g_24i_8f = ((gamma_attr->current_value >> 8) & 0xFF) << 3;
	new_gamma_b_24i_8f = (gamma_attr->current_value & 0xFF) << 3;

	/* make sure the new gamma is within range */
	new_gamma_r_24i_8f = OS_MIN(gamma_r_max_24i_8f, new_gamma_r_24i_8f);
	new_gamma_r_24i_8f = OS_MAX(gamma_r_min_24i_8f, new_gamma_r_24i_8f);
	new_gamma_g_24i_8f = OS_MIN(gamma_g_max_24i_8f, new_gamma_g_24i_8f);
	new_gamma_g_24i_8f = OS_MAX(gamma_g_min_24i_8f, new_gamma_g_24i_8f);
	new_gamma_b_24i_8f = OS_MIN(gamma_b_max_24i_8f, new_gamma_b_24i_8f);
	new_gamma_b_24i_8f = OS_MAX(gamma_b_min_24i_8f, new_gamma_b_24i_8f);


	gamma_normal_r_24i_8f =
		OS_POW_FIX(MAX_PIXEL_VAL, (1<<16)/new_gamma_r_24i_8f);

	gamma_normal_g_24i_8f =
		OS_POW_FIX(MAX_PIXEL_VAL, (1<<16)/new_gamma_g_24i_8f);

	gamma_normal_b_24i_8f =
		OS_POW_FIX(MAX_PIXEL_VAL, (1<<16)/new_gamma_b_24i_8f);

	for( i = 0; i < NUM_PALETTE_ENTRIES; i++ ) {
		unsigned int new_gamma;
		unsigned int cur_color;
		unsigned int cur_palette = palette[i];

		/* Note that we do not try to calculate the gamma if it
		 * is 1.0, e.g. 0x100.  This is to avoid round-off errors
		 */

		/* red: calculate and make sure the result is within range */
		if (0x100 != new_gamma_r_24i_8f) {
			cur_color  = (cur_palette >> 16) & 0xFF;
			new_gamma  = OS_POW_FIX(cur_color, (1<<16)/new_gamma_r_24i_8f);
			new_gamma  = (MAX_PIXEL_VAL * new_gamma)/gamma_normal_r_24i_8f;
			palette[i] &= 0x00FFFF;
			palette[i] |=
				(OS_MIN(new_gamma, (unsigned) MAX_PIXEL_VAL) & 0xFF) << 16;
		}

		/* green: calculate and make sure the result is within range */
		if (0x100 != new_gamma_g_24i_8f) {
			cur_color  = (cur_palette >> 8) & 0xFF;
			new_gamma  = OS_POW_FIX(cur_color, (1<<16)/new_gamma_g_24i_8f);
			new_gamma  = (MAX_PIXEL_VAL * new_gamma)/gamma_normal_g_24i_8f;
			palette[i] &= 0xFF00FF;
			palette[i] |=
				(OS_MIN(new_gamma, (unsigned) MAX_PIXEL_VAL) & 0xFF) << 8;
		}

		/* blue: calculate and make sure the result is within range */
		if (0x100 != new_gamma_b_24i_8f) {
			cur_color  = cur_palette & 0xFF;
			new_gamma  = OS_POW_FIX(cur_color, (1<<16)/new_gamma_b_24i_8f);
			new_gamma  = (MAX_PIXEL_VAL * new_gamma)/gamma_normal_b_24i_8f;
			palette[i] &= 0xFFFF00;
			palette[i] |=
				(OS_MIN(new_gamma, (unsigned) MAX_PIXEL_VAL) & 0xFF);
		}
	}


	/* Brightness correction */
	brightness_factor_r = (brightness_attr->current_value >> 16) & 0xFF;
	brightness_factor_g = (brightness_attr->current_value >> 8) & 0xFF;
	brightness_factor_b = brightness_attr->current_value & 0xFF;

	/* The factors are offset by 0x80 because 0x80 is 0 correction */
	brightness_factor_r -= 0x80;
	brightness_factor_g -= 0x80;
	brightness_factor_b -= 0x80;

	for( i = 0; i < NUM_PALETTE_ENTRIES; i++ ) {
		int          new_pixel_val;
		unsigned int cur_color;
		unsigned int cur_palette = palette[i];

		/* red: calculate and make sure the result is within range */
		cur_color     =  (cur_palette >> 16) & 0xFF;
		new_pixel_val =  cur_color + brightness_factor_r;
		new_pixel_val =  OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
		palette[i]    &= 0x00FFFF;
		palette[i]    |= (OS_MAX(new_pixel_val, 0) & 0xFF) << 16;

		/* green: calculate and make sure the result is within range */
		cur_color     =  (cur_palette >> 8) & 0xFF;
		new_pixel_val =  cur_color + brightness_factor_g;
		new_pixel_val =  OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
		palette[i]    &= 0xFF00FF;
		palette[i]    |= (OS_MAX(new_pixel_val, 0) & 0xFF) << 8;

		/* blue: calculate and make sure the result is within range */
		cur_color     =  cur_palette & 0xFF;
		new_pixel_val =  cur_color + brightness_factor_b;
		new_pixel_val =  OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
		palette[i]    &= 0xFFFF00;
		palette[i]    |= OS_MAX(new_pixel_val, 0) & 0xFF;
	}


	/* contrast correction */
	contrast_factor_r = (contrast_attr->current_value >> 16) & 0xFF;
	contrast_factor_g = (contrast_attr->current_value >> 8) & 0xFF;
	contrast_factor_b = contrast_attr->current_value & 0xFF;

	/* make sure values are within range */
	contrast_factor_r -= 0x80;
	contrast_factor_g -= 0x80;
	contrast_factor_b -= 0x80;


	/* We're doing integer division in this loop using 16i.16f
	 * integers.  The result will then be converted back into a
	 * regular, 32-bit integer
	 */
	for( i = 0; i < NUM_PALETTE_ENTRIES; i++ ) {
		int new_pixel_val;
		unsigned int cur_color;
		unsigned int cur_palette = palette[i];

		/* red: calculate and make sure the result is within range */
		if (0 != contrast_factor_r ) {
			cur_color     = (cur_palette >> 16) & 0xFF;
			new_pixel_val =
				(MAX_PIXEL_VAL << 16) / (MAX_PIXEL_VAL - contrast_factor_r);
			new_pixel_val =   new_pixel_val * (cur_color - MID_PIXEL_VAL);
			new_pixel_val >>= 16;  /* convert back to 32i format */
			new_pixel_val +=  MID_PIXEL_VAL;
			new_pixel_val =   OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
			palette[i]    &=  0x00FFFF;  /* clear out the R color */
			palette[i]    |=  (OS_MAX(new_pixel_val, 0) & 0xFF) << 16;
		}

		/* green: calculate and make sure the result is within range */
		if (0 != contrast_factor_g ) {
			cur_color     = (cur_palette >> 8) & 0xFF;
			new_pixel_val =
				(MAX_PIXEL_VAL << 16) / (MAX_PIXEL_VAL - contrast_factor_g);
			new_pixel_val =   new_pixel_val * (cur_color - MID_PIXEL_VAL);
			new_pixel_val >>= 16;  /* convert back to 32i format */
			new_pixel_val +=  MID_PIXEL_VAL;
			new_pixel_val =   OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
			palette[i]    &=  0xFF00FF;  /* clear out the G color */
			palette[i]    |=  (OS_MAX(new_pixel_val, 0) & 0xFF) << 8;
		}

		/* blue: calculate and make sure the result is within range */
		if (0 != contrast_factor_b) {
			cur_color     = cur_palette & 0xFF;
			new_pixel_val =
				(MAX_PIXEL_VAL << 16) / (MAX_PIXEL_VAL - contrast_factor_b);
			new_pixel_val =   new_pixel_val * (cur_color - MID_PIXEL_VAL);
			new_pixel_val >>= 16;  /* convert back to 32i format */
			new_pixel_val +=  MID_PIXEL_VAL;
			new_pixel_val =   OS_MIN(new_pixel_val, MAX_PIXEL_VAL);
			palette[i]    &=  0xFFFF00;  /* clear out the B color */
			palette[i]    |=   OS_MAX(new_pixel_val, 0) & 0xFF;
		}
	}


	/* write the new values in the palette */
	/*
	 * FIXME:  This is using the legacy palette and should be updated to
	 * use the new, higher resolution palette.
	 */
	for (i = 0; i < NUM_PALETTE_ENTRIES; i++) {
		/* SDVO palette register is not accesible */
		EMGD_WRITE32(palette[i],
			context->device_context.virt_mmadr +
			pipe->palette_reg + i*4);

		/* Update the crtc LUT with the newly calculated values */
		emgd_crtc->lut_r[i] = (palette[i] >> 16) & 0xFF;
		emgd_crtc->lut_g[i] = (palette[i] >> 8) & 0xFF;
		emgd_crtc->lut_b[i] = (palette[i]) & 0xFF;
	}

	OS_FREE(palette);

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * The assumption here is that palette_colors points to index 0 and
 * this function indexes into the palette_colors array by start_index
 *
 * @param display_handle
 * @param palette_colors
 * @param start_index
 * @param count
 *
 * @return 0
 */
static int kms_set_palette_entries_snb(
		emgd_crtc_t *emgd_crtc,
		unsigned int start_index,
		unsigned int count)
{
	unsigned int i;
	igd_context_t *context;
	unsigned char *mmio;
	unsigned long color;

	context = (igd_context_t *)emgd_crtc->igd_context;
	mmio = context->device_context.virt_mmadr;

	EMGD_TRACE_ENTER;

	for(i = start_index; i < start_index + count; i++) {
		color = emgd_crtc->lut_r[i] << 16;
		color |= emgd_crtc->lut_g[i] << 8;
		color |= emgd_crtc->lut_b[i];
		EMGD_WRITE32(color, mmio +
				emgd_crtc->igd_pipe->palette_reg + (i * 4));
	}

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 * program_pipe_vga_snb
 *
 * @param emgd_crtc Pointer to our private kms crtc structure
 *
 * @return void
 */
void program_pipe_vga_snb(
	emgd_crtc_t *emgd_crtc)
{
	igd_display_pipe_t *pipe = NULL;
	igd_context_t      *context = NULL;
	igd_display_port_t *port = NULL;

	igd_timing_info_t  *timing = NULL;
	unsigned long       vga_control;
	unsigned long       upscale = 0;
	int                 centering = 1;

	EMGD_TRACE_ENTER;

	pipe = PIPE(emgd_crtc);
	port = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	/*
	 * VGA Plane can attach to only one pipe at a time. LVDS can
	 * only attach to pipe B. We need to use the display passed to
	 * determine the pipe number to use. (Napa is same as Alm).
	 */

	/*
	 * We can come here with following cases:
	 *   1. magic->vga    CRT, DVI type displays
	 *   2. native->vga   int-lvds, and up-scaling lvds displays
	 *   3. pipe->vga     TV and other unscaled-lvds displays
	 */
	vga_control = EMGD_READ32(context->device_context.virt_mmadr + VGACNTRL);
	vga_control &= 0x5ae3ff00;
	vga_control |= 0x8e;

	timing = pipe->timing;
	if(!timing->private.extn_ptr) {
		EMGD_ERROR_EXIT("No Extension pointer in program_pipe_vga_snb");
		return;
	}

	if(port){
		/* Find UPSCALING attr value*/
		pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_PANEL_FIT,
			0,/*no PD_FLAG for UPSCALING */
			NULL, /* dont need the attr ptr*/
			&upscale);
		/* this PI func will not modify value
		 * of upscale if attr does not exist
		 */
	}

	/* magic->vga || native->vga cases, centering isn't required */
	if ((timing->width == 720 && timing->height == 400) || upscale) {
		EMGD_DEBUG("Centering = 0");
		centering = 0;
	}

	/* Enable border */
	if((timing->width >= 800) && !upscale) {
		EMGD_DEBUG("Enable VGA Border");
		vga_control |= (1L<<26);
	}

	if(timing->width == 640) {
		EMGD_DEBUG("Enable Nine Dot Disable");
		vga_control |= (1L<<18);
	}

/* FIXME(SNB): Bit 25 and 30 are reserved in SNB */
/*
	if(centering) {
		EMGD_DEBUG("Enable VGA Center Centering");
		vga_control |= 1L<<24;

		if(timing->height >= 960) {
			if(timing->width >= 1280) {
				EMGD_DEBUG("Enable VGA 2x (Nine Dot Disable)");
				vga_control |= (1L<<30) | (1L<<18);
			}
		}
	} else {
		pt = get_port_type(emgd_crtc->crtc_id);
		if(pt == IGD_PORT_LVDS) {
			EMGD_DEBUG("Enable VGA Upper-Left Centering & Nine Dot Disable");
			vga_control |= (1L<<25 | (1L<<18));
		} else if (upscale) {
			EMGD_DEBUG("Enable VGA Center Upper-left for upscale ports");
			vga_control |= 1L<<25;
		}
	}
*/
	if(pipe->pipe_num) {
		vga_control |= 1L<<29;
	}

	kms_program_pipe_vga(emgd_crtc, (igd_timing_info_t *)timing->private.extn_ptr);
	EMGD_WRITE32(vga_control, context->device_context.virt_mmadr + VGACNTRL);

	EMGD_TRACE_EXIT;
	return;
}


/*!
 * This function waits for any given register bits to meet
 * an expected test mask value
 *
 * @param mmio
 * @param pipe_reg
 * @param test_mask
 * @param test
 * @param msec
 *
 * @return void
 */
int wait_for(unsigned char *mmio, unsigned long reg,
	unsigned long test_mask, unsigned long test, unsigned long msec)
{
	unsigned long temp;
	os_alarm_t timeout;
	int ret = 0;

	EMGD_TRACE_ENTER;

	/* Wait for Pipe enable/disable, about 50 msec (20Hz). */
	timeout = OS_SET_ALARM(msec);
	do {
		temp = EMGD_READ32(mmio + reg) & test_mask;
		/* Check for timeout */
	} while ((temp != test) && (!OS_TEST_ALARM(timeout)));

	if (temp != test) {
		ret = -1;
	}

	EMGD_TRACE_EXIT;
	return ret;
}

/*!
 * This function disable the pipes, FDI and transcoder
 *
 * igd_context_t *context
 * @param pipe_num
 *
 * @return void
 */
void disable_pipe_snb(igd_context_t *context, unsigned int pipe_num)
{
	unsigned long reg, temp;
	unsigned char *mmio;


	/*******************************************************
	 * FIXME: technically, from a design / arch perspective
	 * the pipe is seperated from the clock and we should
	 * use the program_clock(status, PRE_PIPE) and
	 * program_clock(status, POST_PIPE) to follow the layout.
	 * Already done this for enablement of display pipeline.
	 * For disablement, we just dont have the time for this
	 *******************************************************
	 */


	mmio = context->device_context.virt_mmadr;

	/* disable cpu pipe, disable after all planes disabled */
	reg = PIPECONF(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	if (temp & PIPE_ENABLE) {
		EMGD_WRITE32(temp & ~PIPE_ENABLE, mmio + reg);
		/*POSTING_READ(reg);*/
		/* wait for cpu pipe off, pipe state */
		wait_pipe(mmio, reg, 0);
	}

	/* Disable PF */
	EMGD_WRITE32(0, mmio + (pipe_num ? PFB_CTL_1 : PFA_CTL_1));
	EMGD_WRITE32(0, mmio + (pipe_num ? PFB_WIN_SZ : PFA_WIN_SZ));

	/* Disable CPU FDI TX PLL */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~FDI_TX_ENABLE, mmio + reg);

	/*POSTING_READ(reg);*/
	temp = EMGD_READ32(mmio + reg);
	EMGD_DEBUG("FDI_TX_CTRL read = 0x%lx", temp);
	udelay(100);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~FDI_RX_ENABLE, mmio + reg);

	/* Wait for the clocks to turn off. */
	/*POSTING_READ(reg);*/
	temp = EMGD_READ32(mmio + reg);
	EMGD_DEBUG("FDI_RX_CTRL read = 0x%lx", temp);
	udelay(100);

	/* Ironlake workaround, disable clock pointer after downing FDI */
	EMGD_WRITE32(EMGD_READ32(mmio + FDI_RX_CHICKEN(pipe_num)) &
				~FDI_RX_PHASE_SYNC_POINTER_ENABLE,
				mmio + FDI_RX_CHICKEN(pipe_num));

	/* still set train pattern 1 */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_PATTERN_1;
	EMGD_WRITE32(temp, mmio + reg);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	if (1) {
		temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
		temp |= FDI_LINK_TRAIN_PATTERN_1_CPT;
	} else {
		temp &= ~FDI_LINK_TRAIN_NONE;
		temp |= FDI_LINK_TRAIN_PATTERN_1;
	}

	/* BPC in FDI rx is consistent with that in PIPECONF */
	temp &= ~(0x07 << 16);
	temp |= (EMGD_READ32(mmio + PIPECONF(pipe_num)) & PIPE_BPC_MASK) << 11;
	EMGD_WRITE32(temp, mmio + reg);

	/*POSTING_READ(reg);*/
	udelay(100);

	/* No LVDS on SNB
	if (intel_pipe_has_type(crtc, INTEL_OUTPUT_LVDS)) {
		temp = EMGD_READ32(mmio + PCH_LVDS);
		if (temp & LVDS_PORT_EN) {
			EMGD_WRITE32(temp & ~LVDS_PORT_EN, mmio + PCH_LVDS);
			POSTING_READ(PCH_LVDS);
			udelay(100);
		}
	}
	*/

	/* disable PCH transcoder */
	reg = TRANSCONF(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	if (temp & TRANS_ENABLE) {
		EMGD_WRITE32(temp & ~TRANS_ENABLE, mmio + reg);
		/* wait for PCH transcoder off, transcoder state */
		if (wait_for(mmio, reg, TRANS_STATE_ENABLE, 0, 100)) {
			EMGD_DEBUG("Timeout: Failed to disable transcoder");
		}
	}

	/* disable TRANS_DP_CTL */
	reg = TRANS_DP_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~(TRANS_DP_OUTPUT_ENABLE | TRANS_DP_PORT_SEL_MASK);
	EMGD_WRITE32(temp, mmio + reg);

	/* disable DPLL_SEL */
	temp = EMGD_READ32(mmio + PCH_DPLL_SEL);
	if (pipe_num == 0)
		temp &= ~(TRANSA_DPLL_ENABLE | TRANSA_DPLLA_SEL); //IT IS A!
	else if (pipe_num == 1)
		temp &= ~(TRANSB_DPLL_ENABLE | TRANSB_DPLLB_SEL);
	else
		temp &= ~(TRANSC_DPLL_ENABLE | TRANSB_DPLLB_SEL);
	EMGD_WRITE32(temp, mmio + PCH_DPLL_SEL);

	/* disable PCH DPLL */
	reg = PCH_DPLL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~DPLL_VCO_ENABLE, mmio + reg);

	/* Switch from PCDclk to Rawclk */
	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~FDI_PCDCLK, mmio + reg);

	/* Disable CPU FDI TX PLL */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~FDI_TX_PLL_ENABLE, mmio + reg);

	/*POSTING_READ(reg);*/
	udelay(100);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp & ~FDI_RX_PLL_ENABLE, mmio + reg);

	/* Wait for the clocks to turn off. */
	/*POSTING_READ(reg);*/
	udelay(100);
}



/**
 * kms_set_pipe_pwr_snb
 *
 * Turns the pipe ON or OFF depending on the input
 *
 * @param emgd_crtc (IN) specifies the pipe to change
 * @param enable    (IN) TRUE to enable pipe, FALSE to disable
 *
 * @return
 */
static void kms_set_pipe_pwr_snb(emgd_crtc_t *emgd_crtc, unsigned long enable)
{
	unsigned long       pipe_conf;
	igd_display_pipe_t *pipe;
	igd_context_t      *context;

	EMGD_TRACE_ENTER;

	pipe      = PIPE(emgd_crtc);
	context   = emgd_crtc->igd_context;
	pipe_conf = EMGD_READ32(context->device_context.virt_mmadr + pipe->pipe_reg);

	/* Do nothing if current power state is same as what we want to set */
	/* The PIPE_ENABLE bit is at bit-position 31 */
	if ( (enable << 31) == (pipe_conf & PIPE_ENABLE) ){
		EMGD_TRACE_EXIT;
		return;
	}

	if (!enable) {

		disable_pipe_snb(context, pipe->pipe_num);

		EMGD_DEBUG("Set Pipe Power: OFF");

	} else {
		/* Enable pipe */
		EMGD_WRITE32(pipe_conf | PIPE_ENABLE,
			context->device_context.virt_mmadr + pipe->pipe_reg);

		EMGD_DEBUG("Set Pipe Power: ON");
	}


	EMGD_TRACE_EXIT;
	return;
}

/*!
 * This function waits for the pipe to be enabled or disabled.
 * check_on_off = 0 to wait for the pipe to disable.
 * check_on_off = 0x40000000 to wait for the pipe to enable.
 *
 * @param mmio
 * @param pipe_reg
 * @param check_on_off
 *
 * @return void
 */
void wait_pipe(unsigned char *mmio, unsigned long pipe_reg,
	unsigned long check_on_off)
{
	EMGD_TRACE_ENTER;

	/* Wait for Pipe enable/disable, about 50 msec (20Hz). */
	if(wait_for(mmio, pipe_reg, PIPE_STATE, check_on_off, 50)) {
		EMGD_ERROR_EXIT("Timeout waiting for pipe %s",
				check_on_off ? "enable":"disable");
	}

	EMGD_TRACE_EXIT;
	return;
}

/**
 * kms_program_pipe_snb
 *
 * Called before a mode set, takes the input "mode", matches it to the closest
 * supported mode, then put the supported mode into "adjusted_mode" to let the
 * caller know.
 *
 * @param encoder (IN) Encoder being prepared
 *
 * @return true, false (details TBD)
 */
static void kms_program_pipe_snb(emgd_crtc_t *emgd_crtc)
{
	unsigned long       timing_reg;
	unsigned long       pipe_conf;
	unsigned long       hactive, vactive;
	igd_timing_info_t  *current_timings, s3d_timings;
	igd_display_port_t *port = NULL;
	igd_display_pipe_t *pipe = NULL;

	int                 i;
	unsigned int   pipe_num;
	unsigned long  temp;
	fdi_m_n *      new_mn_ctx;
	unsigned char *mmio;


	EMGD_TRACE_ENTER;

	pipe            = PIPE(emgd_crtc);
	pipe_num        = pipe->pipe_num;
	current_timings = pipe->timing;
	port            = PORT(emgd_crtc);
	mmio            = MMIO(emgd_crtc);

	if (NULL == port) {
		EMGD_ERROR_EXIT("Invalid CRTC selected - empty port.");
		return;
	}

	pipe_conf = EMGD_READ32(mmio + pipe->pipe_reg);

	/* S3D format like Framepacking requires timing fix */
	if (!s3d_timings_fixup(port, current_timings, &s3d_timings)) {
		current_timings = &s3d_timings;
	}

	/* Debug messages */
	EMGD_DEBUG("Current timings %ux%u mode_number = %u "
			"flags = 0x%lx, dclk = %lu",
			current_timings->width,
			current_timings->height,
			current_timings->mode_number,
			current_timings->flags,
			current_timings->dclk);

	if( (IGD_PORT_ANALOG != port->port_type) &&
		( (IGD_PORT_DIGITAL != port->port_type) &&
		(PD_DISPLAY_HDMI_INT != port->pd_driver->type)) ) {
		EMGD_ERROR_EXIT("Codes not yet supporting non HDMI/Analog ports!");
	}


	/*
	 * If the mode is VGA and the PD says it handles all VGA modes without
	 * reprogramming then just set the mode and leave centering off.
	 */
	if(current_timings->flags & IGD_MODE_VESA) {
		EMGD_DEBUG("IGD_MODE_VESA");

		if (current_timings->mode_number <= VGA_MODE_NUM_MAX) {
			EMGD_DEBUG("current_timings->mode_number <= VGA_MODE_NUM_MAX");
			program_pipe_vga_snb(emgd_crtc);

			EMGD_TRACE_EXIT;
			return;
		}
	}


	/* Program dot clock divisors. */
	program_clock_snb(emgd_crtc, pipe->clock_reg, current_timings->dclk,
			PROGRAM_PRE_PIPE);

	if (0 == pipe_num) {
		new_mn_ctx = fdi_mn_new_pipea;
	} else {
		new_mn_ctx = fdi_mn_new_pipeb;
	}


	/* Program timing registers for the pipe */
	timing_reg = pipe->timing_reg;
	if (current_timings->flags & IGD_PIXEL_DOUBLE) {
		hactive = (unsigned long)current_timings->width*2 - 1;
	} else {
		hactive = (unsigned long)current_timings->width - 1;
	}

	if (current_timings->flags & IGD_LINE_DOUBLE) {
		vactive = (unsigned long)current_timings->height*2 - 1;
	} else {
		/* Hardware will automatically divide by 2 to
		 * get the number of line in each field */
		vactive = (unsigned long)current_timings->height - 1;
	}


	/* Reset the Palette */
	for (i = 0; i < 256; i++) {
		/* Program each of the 256 4-byte palette entries */
		EMGD_WRITE32(((i<<16) | (i<<8) | i),
			mmio + pipe->palette_reg + (i << 2));
	}

	if (port) {
		/* apply color correction */
		for( i = 0; PD_ATTR_LIST_END != port->attributes[i].id; i++ ) {

			if ((PD_ATTR_ID_FB_GAMMA      == (port->attributes[i].id)) ||
				(PD_ATTR_ID_FB_BRIGHTNESS == (port->attributes[i].id)) ||
				(PD_ATTR_ID_FB_CONTRAST == (port->attributes[i].id)))  {

				kms_set_color_correct_snb(emgd_crtc);
			}
		}
	}


	/* Program Timings */
	temp = (unsigned long)(current_timings->htotal) << 16 | hactive;
	EMGD_WRITE32(temp, mmio + timing_reg);

	temp = ((unsigned long) current_timings->hblank_end << 16) |
		(unsigned long)(current_timings->hblank_start);
	EMGD_WRITE32(temp, mmio + timing_reg + 0x04);
	/* FIXME - for lvds dual channel, blanking period = multiple of 2 */

	temp = ((unsigned long)(current_timings->hsync_end) << 16) |
		(unsigned long)(current_timings->hsync_start);
	EMGD_WRITE32(temp, mmio + timing_reg + 0x08);

	temp = ((unsigned long)(current_timings->vtotal) << 16) | vactive;
	EMGD_WRITE32(temp, mmio + timing_reg + 0x0C);

	temp = ((unsigned long)(current_timings->vblank_end) << 16) |
		(unsigned long)(current_timings->vblank_start);
	EMGD_WRITE32(temp, mmio + timing_reg + 0x10);

	temp = ((unsigned long)(current_timings->vsync_end)<< 16) |
		(unsigned long)(current_timings->vsync_start);
	EMGD_WRITE32(temp, mmio + timing_reg + 0x14);


	/*
	 * If there is a linked mode it is either the VGA or a scaled
	 * mode. If it is scaled then we need to use it as the source size.
	 */
	if (current_timings->private.extn_ptr) {
		igd_timing_info_t *scaled_timings =
			(igd_timing_info_t *)current_timings->private.extn_ptr;

		if((scaled_timings->flags & IGD_MODE_VESA) &&
			(scaled_timings->mode_number <= VGA_MODE_NUM_MAX)) {
			temp = (hactive << 16) | vactive;
		} else {
			EMGD_DEBUG("scaled_timings->width [%d], "
					"scaled_timings->height [%d]",
					scaled_timings->width, scaled_timings->height);
			temp = (unsigned long)scaled_timings->width  - 1;
			temp = (temp << 16) |
				(unsigned long)(scaled_timings->height - 1);
		}
	} else {
		temp = (hactive << 16) | vactive;
	}
	EMGD_WRITE32(temp, mmio + timing_reg + 0x1C);

	EMGD_WRITE32(TU_SIZE(new_mn_ctx->tu) | new_mn_ctx->gmch_m,
			mmio + PIPE_DATA_M1(pipe_num));
	EMGD_WRITE32(new_mn_ctx->gmch_n, mmio + PIPE_DATA_N1(pipe_num));
	EMGD_WRITE32(new_mn_ctx->link_m, mmio + PIPE_LINK_M1(pipe_num));
	EMGD_WRITE32(new_mn_ctx->link_n, mmio + PIPE_LINK_N1(pipe_num));

	/* Put pipe in interlaced mode if requested:
	 *     should only happen for LVDS display if at all. */
	if (current_timings->flags & IGD_SCAN_INTERLACE) {
		EMGD_ERROR_EXIT("Codes not yet supporting any modes with interlace "
				"combination!");
		/*
		 * FIXME:
		 * Dont forget to consider both fields of bits 23:21 and bit 20 of
		 * pipe_conf  Also dont forget to check VSYNCSHIFT register for
		 * additional programming.
		 */
	} else {
		pipe_conf |= PROGRESSIVE_FETCH_PROGRESSIVE_DISPLAY;
	}

	/* assume default panel depth is 8BPC (for 24BPP).
	 * Actually, be getting this data from the current display FB pixel format
	 * which currently can either be 565 or 8888 - for now we dont care
	 * about 2101010 or 16161616. So we can simply assume bpp = 24.
	 */
	pipe_conf |= PIPE_8BPC;

	/* Depending on the PIPE_BPC value and the output port BPC,
	 * we may need to program the pipe dithering bits 4, and 3:2.
	 * For now, because we currently only support 24BPP and HDMI
	 * we can skip and leave this as default = NO DITHERING
	 */

	switch(current_timings->flags & IGD_MODE_INFO_ROTATE_MASK){
		default:
		case IGD_MODE_INFO_ROTATE_0:
			break;

		case IGD_MODE_INFO_ROTATE_90:
			pipe_conf |= (1 << 14);
			break;
		case IGD_MODE_INFO_ROTATE_180:
			pipe_conf |= (1 << 15);
			break;
		case IGD_MODE_INFO_ROTATE_270:
			pipe_conf |= (3 << 14);
			break;
	}

	/* Map the pipe to the blender and transcoder?
	 * Actually, we'll skip this leave the default of
	 * BIT10:9 as 00 (indicating use blender A?)
	 */

	/* We may not want to actually enable the pipe here as CRTC can get enabled
	 * via a different KMS interface */
	pipe_conf |= PIPE_ENABLE;
	EMGD_WRITE32(pipe_conf, mmio + pipe->pipe_reg);


	/* Gen6 can check when the pipe is enabled. */
	wait_pipe(mmio, pipe->pipe_reg, 0x40000000);

	/*
	 * Set the VGA address range to 0xa0000 so that a normal (not VGA)
	 * mode can be accessed through 0xa0000 in a 16bit world.
	 */
	WRITE_AR2(mmio, 0x10, 0xb);
	WRITE_VGA2(mmio, GR_PORT, 0x06, 0x5);
	WRITE_VGA2(mmio, GR_PORT, 0x10, 0x1);

	if(current_timings->private.extn_ptr) {
		/* This means either internal scaling (LVDS) or centered VGA */
		current_timings = current_timings->private.extn_ptr;
		if(current_timings->private.extn_ptr) {
			/* This is both the scaled and centered VGA */
			current_timings = current_timings->private.extn_ptr;
		}
		if (current_timings->flags & IGD_MODE_VESA) {
			if (current_timings->mode_number <= VGA_MODE_NUM_MAX) {
				program_pipe_vga_snb(emgd_crtc);
			}
		}
	}

	/* Program dot clock divisors. */
	program_clock_snb(emgd_crtc, pipe->clock_reg, current_timings->dclk,
			PROGRAM_POST_PIPE);

	EMGD_TRACE_EXIT;
	return;
}



/*
 * Turn a plane on or off.
 */
static void kms_set_plane_pwr_snb(emgd_crtc_t *emgd_crtc, unsigned long enable,
							bool wait_for_vblank)
{
	unsigned long           plane_control;
	unsigned long           plane_reg;
	igd_display_plane_t    *plane       = NULL;
	igd_cursor_t           *cursor      = NULL;
	igd_display_pipe_t     *pipe        = NULL;
	igd_context_t          *context     = NULL;
	unsigned char          *mmio        = NULL;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("Program Plane: %s", enable?"ENABLE":"DISABLE");

	pipe    = PIPE(emgd_crtc);
	plane   = PLANE(emgd_crtc);
	cursor  = CURSOR(emgd_crtc);
	context = emgd_crtc->igd_context;
	mmio    = MMIO(emgd_crtc);

	if (!plane) {
		EMGD_ERROR_EXIT("Trying to set power to a plane that is not tied "
			" to a crtc.");
		return;
	}

	if (((plane->plane_reg & BIT31) && (enable))	||
		(((plane->plane_reg & BIT31) == 0) && (enable == 0))){
		return;
	}

	/* In case a plane update is already in progress */
	wait_for_vblank_snb(mmio, pipe->pipe_reg);

	/* Get the current value of the plane control register */
	plane_reg     = plane->plane_reg;
	plane_control = EMGD_READ32(mmio +
						plane_reg);

	if((enable == FALSE) ||
		(context->device_context.power_state != IGD_POWERSTATE_D0)) {
		/*
		 * FIXME:  This is being done a lot! Review and fix.
		 * Note: The vga programming code does not have an "off". So
		 * when programming the plane to off we make sure VGA is off
		 * as well.
		 */
		kms_disable_vga_snb(context);

		/*
		 * To turn off plane A or B, the program have to trigger the plane A
		 * or B start register.  Or else, it will not work.
		 */
		plane_control &= ~BIT31;

		EMGD_WRITE32(plane_control,
						mmio + plane_reg);

		EMGD_WRITE32(EMGD_READ32(mmio +
			plane_reg + DSP_START_OFFSET),
			mmio + plane_reg + DSP_START_OFFSET);

		/* make sure the cursor plane is off too.  Bits 0,1,2 should be 0 */
		plane_control = EMGD_READ32(mmio +
				cursor->cursor_reg);
		EMGD_WRITE32((plane_control & 0xfffffff8),
				mmio + cursor->cursor_reg);
	} else {
		/* Enable plane */
		plane_control |= BIT31;

		EMGD_WRITE32(plane_control,
			mmio + plane_reg);

		EMGD_WRITE32(EMGD_READ32(mmio +
			plane_reg + DSP_START_OFFSET),
			mmio + plane_reg + DSP_START_OFFSET);
	}

	if (wait_for_vblank) {
		wait_for_vblank_snb(mmio, pipe->pipe_reg);
	}

	EMGD_TRACE_EXIT;
	return;
}


/*!
 * wait_for_vblank_snb()
 *
 * Waits for a vblank interrupt.  Note that this races with our main interrupt
 * handler so either this function or the vblank handler may miss a vblank
 * event.  As such, timeouts here are expected and non-fatal
 *
 * @param mmio
 * @param pipe_reg
 */
void wait_for_vblank_snb(unsigned char *mmio, unsigned long pipe_reg)
{
	unsigned long pipe_status_reg = pipe_reg + PIPE_STATUS_OFFSET;
	unsigned long tmp;
	os_alarm_t timeout;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("MMIO = %p, pipe_reg = %lx", mmio, pipe_reg);

	/* If pipe is off then just return */
	if(!((1L<<31) & EMGD_READ32(mmio + pipe_reg))) {
		EMGD_DEBUG("Pipe disabled/Off");
		EMGD_TRACE_EXIT;
		return;
	}

	/*
	 * When VGA plane is on the normal wait for vblank won't work
	 * so just skip it.
	 */
	if(!(EMGD_READ32(mmio + VGACNTRL) & 0x80000000)) {
		EMGD_DEBUG("VGA Plane On");
		EMGD_TRACE_EXIT;
		return;
	}

	/* 1. Clear VBlank interrupt status (by writing a 1) */
	tmp = EMGD_READ32(mmio + pipe_status_reg);
	EMGD_WRITE32 (tmp | VBLANK_STS, mmio + pipe_status_reg);

	/*
	 * 2. Wait for VBlank about 50 msec (20Hz). If we don't get the interrupt
	 * by that time, we probably lost the vblank events to the main handler
	 * that we're racing with.
	 */
	timeout = OS_SET_ALARM(50);
	do {
		tmp = EMGD_READ32(mmio + pipe_status_reg) & VBLANK_STS;
	} while ((tmp == 0x00) && (!OS_TEST_ALARM(timeout)));

	if (tmp == 0) {
		EMGD_DEBUG("VBlank timed out");
	}

	EMGD_TRACE_EXIT;
} /* wait_for_vblank_snb */


/*
 * Set plane base address
 *
 * This updates the plane to point to a new surface and/or
 * a new offset.
 */
static void kms_set_display_base_snb(emgd_crtc_t *emgd_crtc,
		emgd_framebuffer_t *fb)
{
	unsigned long base;
	unsigned long ctrl;

	EMGD_TRACE_ENTER;

	ctrl = READ_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg);

	if(fb->bo->tiling_mode != I915_TILING_NONE) {
		EMGD_DEBUG("Pan tiled");

		base = (emgd_crtc->igd_plane->xoffset & 0x00000fff) | ((emgd_crtc->igd_plane->yoffset & 0x00000fff) << 16);

		WRITE_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg
				+ DSP_TOFF_OFFSET, base);
		ctrl |= BIT10;

	} else {
		EMGD_DEBUG ("Pan linear");

		base = ((emgd_crtc->igd_plane->yoffset * fb->base.DRMFB_PITCH) +
				(emgd_crtc->igd_plane->xoffset * IGD_PF_BYPP(fb->igd_pixel_format)));

		WRITE_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg
				+ DSP_LINEAR_OFFSET, base);
		ctrl &= ~BIT10;
	}

	WRITE_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg, ctrl);
	WRITE_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg + DSP_STRIDE_OFFSET,
			fb->base.DRMFB_PITCH);

	WRITE_MMIO_REG(emgd_crtc, PLANE(emgd_crtc)->plane_reg + DSP_START_OFFSET,
			i915_gem_obj_ggtt_offset(fb->bo));

	PLANE(emgd_crtc)->fb_info = fb;

	//wait_for_vblank_snb(MMIO(emgd_crtc), PIPE(emgd_crtc)->pipe_reg);

	EMGD_TRACE_EXIT;
}


/*
 * Program the plane.
 *
 * This assumes the plane should be enabled as part of the programming.
 * The set_plane_pwr function above should be used to enable/disable the
 * plane independent of the programming.
 */
static int kms_program_plane_snb(emgd_crtc_t *emgd_crtc, bool preserve)
{
	unsigned long plane_control;
	int visible_offset;
	igd_timing_info_t      *timing      = NULL;
	emgd_framebuffer_t     *fb_info     = NULL;
	unsigned long           plane_reg;

	struct drm_device      *dev         = NULL;
	igd_display_plane_t    *plane       = NULL;
	igd_display_pipe_t     *pipe        = NULL;
	igd_context_t          *context     = NULL;
	unsigned char          *mmio        = NULL;

	EMGD_TRACE_ENTER;

	pipe    = PIPE(emgd_crtc);
	plane   = PLANE(emgd_crtc);
	dev     = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context = emgd_crtc->igd_context;
	mmio    = MMIO(emgd_crtc);

	if (!plane || !pipe->timing) {
		EMGD_ERROR("Trying to program a plane that is not tied to a crtc.");
		return -EINVAL;
	}
	fb_info   = plane->fb_info;
	plane_reg = plane->plane_reg;

	plane_control = EMGD_READ32(mmio + plane_reg);

	timing = pipe->timing;
	/* There is a special case code for legacy VGA modes */
	while (timing->private.extn_ptr) {
		timing = (igd_timing_info_t *)timing->private.extn_ptr;
	}
	if (MODE_IS_VGA(timing)) {
		kms_program_plane_vga(mmio, timing);
		EMGD_TRACE_EXIT;
		return 0;
	}

	/* set color depth */
	plane_control &= ~(BIT26 | BIT27 | BIT28);
	switch (IGD_PF_DEPTH(fb_info->igd_pixel_format)) {
	case PF_DEPTH_8:
		plane_control |= BIT27;
		break;
	case PF_DEPTH_16:
		plane_control |= BIT28 | BIT26;
		break;
	default:
	case PF_DEPTH_32:
		plane_control |= BIT28 | BIT27;
		break;
	}

	/* Gamma control */
	if(fb_info->igd_flags & IGD_ENABLE_DISPLAY_GAMMA) {
		plane_control |= (BIT30);
	} else {
		plane_control &= ~(BIT30);
	}

	if(fb_info->bo->tiling_mode != I915_TILING_NONE) {
		plane_control |= (BIT10);
	} else {
		plane_control &= ~(BIT10);
	}

	/* Bspec: For Sandybridge this Trickle Feed must always disable */
	if(context->device_context.did == PCI_DEVICE_ID_VGA_SNB) {
		plane_control |= BIT14;
	} else {
		plane_control &= ~BIT14;

	}
	/*
	 * FIXME: The B-Spec states that rendering will be slower if the fences
	 * are not a power of 2.  So for now, always use a power of 2.
	 */

	/* FIXME(SNB): Is this required for SNB */
	/*EMGD_WRITE32(0x04000400, MMIO(display) + 0x209c);*/

	EMGD_DEBUG(" Plane Ctrl :  0x%lx", plane_control);
	EMGD_DEBUG(" Plane Base :  0x%lx", i915_gem_obj_ggtt_offset(fb_info->bo));
	EMGD_DEBUG(" X,Y Offset :  %d,%d",
				(int)emgd_crtc->igd_plane->xoffset, (int)emgd_crtc->igd_plane->yoffset);
	EMGD_DEBUG(" Plane Pitch:  0x%lx", (unsigned long)fb_info->base.DRMFB_PITCH);

	EMGD_WRITE32(plane_control, mmio + plane_reg);
	EMGD_WRITE32(fb_info->base.DRMFB_PITCH, mmio + plane_reg + DSP_STRIDE_OFFSET);

	visible_offset = ((emgd_crtc->igd_plane->yoffset * fb_info->base.DRMFB_PITCH) +
			(emgd_crtc->igd_plane->xoffset * IGD_PF_BYPP(fb_info->igd_pixel_format)));
	EMGD_WRITE32(visible_offset, mmio + plane_reg + DSP_LINEAR_OFFSET);

	if(fb_info->bo->tiling_mode != I915_TILING_NONE) {
		visible_offset = (emgd_crtc->igd_plane->xoffset & 0x00000fff) |
							((emgd_crtc->igd_plane->yoffset & 0x00000fff) << 16);
		EMGD_WRITE32(visible_offset, mmio + plane_reg + DSP_TOFF_OFFSET);
	} else {
		EMGD_WRITE32(0, mmio + plane_reg + DSP_TOFF_OFFSET);
	}

	/* Writing the base offset should trigger the register updates */
	EMGD_WRITE32(i915_gem_obj_ggtt_offset(fb_info->bo), mmio + plane_reg + DSP_START_OFFSET);

	EMGD_TRACE_EXIT;
	return 0;
}



/*!
 * kms_program_port_snb
 *
 * @param emgd_encoder
 * @param status
 *
 * @return program_port_lvds_gen4()
 * @return program_port_sdvo_gen4()
 * @return -IGD_ERROR_INVAL on failure
 */
static int kms_program_port_snb(emgd_encoder_t *emgd_encoder,
		unsigned long status)
{
	EMGD_TRACE_ENTER;
	EMGD_TRACE_EXIT;

	/*
	 * FIXME: Look at encoder type and call the correct program
	 * port function.
	 */
	return program_port_digital_snb(emgd_encoder, status);
}



/*!
 * program_port_digital_snb
 *
 * @param emgd_encoder
 * @param status
 *
 * @return 0 on success
 * @return -IGD_ERROR_INVAL on failure
 */
static int program_port_digital_snb(emgd_encoder_t *emgd_encoder,
		unsigned long status)
{
	unsigned long pipe_number;
	unsigned long port_control;
	unsigned long pd_powerstate = PD_POWER_MODE_D3;
	unsigned long upscale = 0;
	igd_timing_info_t  local_timing;
	igd_timing_info_t  *timing    = NULL;
	igd_display_port_t *port      = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;
	igd_display_pipe_t *pipe      = NULL;
	igd_context_t      *context   = NULL;
	unsigned long temp;
	int ret;

	EMGD_TRACE_ENTER;

	port = emgd_encoder->igd_port;
	emgd_crtc = CRTC_FROM_ENCODER(emgd_encoder);
	if(emgd_crtc == NULL) {
		EMGD_ERROR("emgd_crtc is NULL");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	pipe = PIPE(emgd_crtc);
	context = emgd_crtc->igd_context;
	timing = pipe->timing;

	EMGD_DEBUG("Program Port: (%s)", status?"ENABLE":"DISABLE");
	EMGD_DEBUG("pd_flags: 0x%lx", port->pd_flags);

	port_control = EMGD_READ32(context->device_context.virt_mmadr +
			port->port_reg);

	if (status == TRUE) {
		if (!(port->pt_info->flags & IGD_DISPLAY_ENABLE)) {
			EMGD_TRACE_EXIT;
			return 0;
		}

		/* Both IGD_ powerstates and PD_ powermodes have
		 * same definitions */
		pd_powerstate =
				(context->device_context.power_state > port->power_state) ?
				context->device_context.power_state : port->power_state;

		if (pd_powerstate == IGD_POWERSTATE_D0) {
			EMGD_DEBUG("Power State: D0");
			/* Upscale */
			pi_pd_find_attr_and_value(port,
					PD_ATTR_ID_PANEL_FIT,
					0, /*no PD_FLAG for UPSCALING */
					NULL, /* dont need the attr ptr*/
					&upscale);

			/* Reach the end timing if upscaling is enabled */
			if (timing->private.extn_ptr && upscale) {
				timing = (pd_timing_t *)timing->private.extn_ptr;
			}

			local_timing = *timing;
			if (upscale) {
				/* For timings smaller than width 360 and height 200,
				 * double the size. This is because the active area of the mode
				 * is double the size of the resolution for these modes
				 *  - Very tricky huh */
				if (local_timing.width <= 360) {
					local_timing.width <<= 1;
				}
				if (local_timing.height <= 200) {
					local_timing.height <<= 1;
				}
			}

			/* In SNB, the pipe from North Display will become transcoder
			 * in the South Display (PCH), the transcoder are selected
			 * from bit[30:29] inthe port control register */
			pipe_number = pipe->pipe_num;

			port_control |= (pipe_number<<29);

			/* This is used to tell the port driver what pipe the port is connected to */
			port->callback->current_pipe = pipe->pipe_num;

			/* in Gen4 B-Specs, Gang Mode is not supported on SDVO */

			/* in Gen4 B-Specs, no clock rate multiplier */
			if (port->pd_driver->type == PD_DISPLAY_HDMI_INT) {

				/* Check if audio is enabled on the other port.
				 * Audio can only be enabled on 1 port at a time.
				 * The first hdmi port called will have audio. */
				if (port->port_reg == HDMIB) {
					temp =
						EMGD_READ32(context->device_context.virt_mmadr + HDMIB);
				} else if (port->port_reg == HDMIC) {
					temp =
						EMGD_READ32(context->device_context.virt_mmadr + HDMIC);
				} else {
					temp =
						EMGD_READ32(context->device_context.virt_mmadr + HDMID);
				}

				if(!(temp & BIT(6))) {
					/* Enable audio */
					port_control |= BIT(6);
					/* Enable NULL Infoframes */
					port_control |= BIT(9);
				}

				/* Set to TMDS encoding */
				port_control |= BIT(11);
				port_control &= (~BIT(10));

				/* enable the border */
				/* Centering would not work if this is disabled */
				port_control |= BIT7;

				/* disable the stall */
				port_control &= (~BIT25);

				/* FIXME(SNB): These are reserved bit on SNB */
				/*port_control &= (~BIT20);
				 * port_control &= (~BIT19);
				 * port_control &= (~BIT12);*/

			} else {  /* Not HDMI (DVI?) */
				/* enable the border */
				/* Do we need to disable the SDVO border *
				 * for native VGA timings(i.e., use DE)? */
				port_control |= BIT7;

				/* enable the stall */
				port_control |= BIT25;

				/* in Gen4 B-Specs, no clock phase */
			}
			/* enable the register */
			port_control |= (BIT31);

			/* Set sync polarities as active high by default. SDVO device will
			 * set the required sync polarities based on the timing selected */
			port_control |= (BIT4|BIT3);
		}
	}


	if (pd_powerstate == PD_POWER_MODE_D0) {
		ret = port->pd_driver->set_mode(port->pd_context, &local_timing, 0);
	} else {
		ret = port->pd_driver->set_power(port->pd_context, pd_powerstate);
	}

	if (ret) {
		EMGD_ERROR_EXIT("PD set_%s returned: 0x%x",
			(pd_powerstate == PD_POWER_MODE_D0)?"mode":"power", ret);
		return -IGD_ERROR_INVAL;
	}

	EMGD_DEBUG("Port_control: 0x%lx", port_control);

	EMGD_WRITE32(port_control,
			context->device_context.virt_mmadr + port->port_reg);

	EMGD_TRACE_EXIT;
	return 0;
}



/*!
 * kms_post_program_port_snb
 *
 * @param emgd_encoder
 * @param status
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int kms_post_program_port_snb(emgd_encoder_t * emgd_encoder,
	unsigned long status)
{
	int ret;
	igd_display_port_t *port      = NULL;
	igd_display_pipe_t *pipe      = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;
	igd_timing_info_t  *timings   = NULL;
	unsigned char      *mmio      = NULL;
	unsigned long       portreg;
	unsigned long       portregchk;
	os_alarm_t          timeout;

	EMGD_TRACE_ENTER;

	port = emgd_encoder->igd_port;
	emgd_crtc = CRTC_FROM_ENCODER(emgd_encoder);
	if(emgd_crtc == NULL) {
		/* if we get post_program_port for port that
		 * is not part of any active display pipeline
		 * then bail (this can happen because default
		 * kms helper in upper DRM layer calls all encoders
		 * for dpms irrespective of hookup to crtc?
		 */
		EMGD_TRACE_EXIT;
		return 0;
	}
	
	pipe = PIPE(emgd_crtc);
	mmio = MMIO(emgd_crtc);
	timings = pipe->timing;

	/*
	 * The programming found in the common code for all chipsets
	 * has the device programming sequence as follows:
	 *  Port
	 *  Pipe
	 *  Post Port
	 *  Plane
	 * On Gen4, if the port is enabled before the pipe, there is a 10%
	 * chance that the port will not turn on properly.
	 * Due to compatability requires with other chipsets, this workaround
	 * fixes this issue
	 */
	portreg = EMGD_READ32(mmio + port->port_reg);
	portregchk = portreg;

	timeout = OS_SET_ALARM(20);
	EMGD_WRITE32(portreg & ~BIT31,
		mmio + port->port_reg);

	/* Make sure the port B enable bit is actually updated
	 */
	do {
		portregchk = EMGD_READ32(mmio + port->port_reg);
	}while( (portregchk & BIT31) && !(OS_TEST_ALARM(timeout)) );

	if (status) {
		portreg |= BIT31;
	} else {
		portreg &= ~BIT31;
	}

	EMGD_WRITE32(portreg, mmio + port->port_reg);

	ret = 0;
	/* call post_set_mode() if exists - and if we're enabling to D0 
	 * see PI definition of this func to understand port driver expectation */
	if (port->pd_driver->post_set_mode && status) {
		ret = port->pd_driver->post_set_mode(port->pd_context, timings,
				status);
		if (ret) {
			EMGD_ERROR_EXIT("PD post_set_mode returned: 0x%x", ret);
		}
	}

	EMGD_TRACE_EXIT;
	return ret;
}

/*!
 * kms_program_cursor_snb
 *
 * This function programs the cursor registers
 *
 * @param display
 * @param status
 *
 * @return void
 */
static void kms_program_cursor_snb(emgd_crtc_t *emgd_crtc,
	unsigned long status)
{
	unsigned long cursor_reg;
	unsigned long cursor_control = 0x00000000;
	unsigned long cursor_pos;
	unsigned long cursor_base;
	igd_cursor_info_t *cursor_info;
	int i;
	struct drm_device *dev;
	igd_context_t *context;
	unsigned char *mmio;

	dev     = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;
	mmio = context->device_context.virt_mmadr;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("Program Cursor: %s", status?"ENABLE":"DISABLE");

	cursor_reg = emgd_crtc->igd_cursor->cursor_reg;
	cursor_info = emgd_crtc->igd_cursor->cursor_info;

	/* Turn off cursor before changing anything */
	cursor_base = EMGD_READ32(mmio + cursor_reg + CUR_BASE_OFFSET);

	EMGD_WRITE32(cursor_control, mmio + cursor_reg);
	EMGD_WRITE32(cursor_base, mmio + cursor_reg + CUR_BASE_OFFSET);

	if(cursor_info->flags & IGD_CURSOR_GAMMA) {
		cursor_control |= BIT26;
	}

	cursor_info->argb_pitch = 64*4;
	cursor_info->xor_pitch = 16;

	/* Setting the cursor format/pitch */
	switch(cursor_info->pixel_format) {
	case IGD_PF_ARGB32:
		cursor_control |= BIT5 | 0x7;
		break;
	case IGD_PF_RGB_XOR_2:
		cursor_control |= 0x5;
		break;
	case IGD_PF_RGB_T_2:
		cursor_control |= 0x4;
		break;
	case IGD_PF_RGB_2:
		cursor_control |= 0x6;
		break;
	default:
		EMGD_ERROR_EXIT("Invalid Pixel Format");
		return;
	}

	switch(cursor_info->pixel_format) {
	case IGD_PF_ARGB32:
		/* The cursor on Gen4 is in graphics memory */
		cursor_base = cursor_info->argb_offset;
		break;
	default:
		/* The cursor on Gen4 is in graphics memory */
		cursor_base = cursor_info->xor_offset;
		break;
	}

	/* If status is FALSE return with the cursor off */
	if(!status) {
		EMGD_TRACE_EXIT;
		return;
	}

	if(cursor_info->y_offset >= 0) {
		cursor_pos = cursor_info->y_offset << 16;
	} else {
		cursor_pos = ((-(cursor_info->y_offset)) << 16) | 0x80000000;
	}
	if(cursor_info->x_offset >= 0) {
		cursor_pos |= cursor_info->x_offset;
	} else {
		cursor_pos |= (-(cursor_info->x_offset)) | 0x00008000;
	}


	EMGD_WRITE32(cursor_pos, mmio + cursor_reg + CUR_POS_OFFSET);

	for(i=0; i<4; i++) {
		EMGD_WRITE32(cursor_info->palette[i], mmio + cursor_reg +
				CUR_PAL0_OFFSET + i * 4);
	}

	cursor_control = cursor_control | emgd_crtc->igd_pipe->pipe_num<<28;

	EMGD_WRITE32(cursor_control, mmio + cursor_reg);
	EMGD_WRITE32(cursor_base, mmio + cursor_reg + CUR_BASE_OFFSET);

	EMGD_TRACE_EXIT;
}


/*!
 * kms_alter_cursor_pos_snb
 *
 * This function alters the position parameters associated with a cursor.
 *
 * @param display_handle
 * @param cursor_info
 *
 * @return 0
 */
static int kms_alter_cursor_pos_snb(emgd_crtc_t *emgd_crtc)
{
	unsigned long cursor_reg;
	unsigned long new_pos;
	unsigned long cursor_base;
	unsigned char * mmio;

	cursor_reg = CURSOR(emgd_crtc)->cursor_reg;
	mmio = MMIO(emgd_crtc);

	/* To Fast For Tracing */

	if (0x27 & EMGD_READ32(mmio + cursor_reg)) {
		/*
		 * The base offset must be updated to trigger the position
		 * update. However, this also means an invalid cursor surface
		 * could be enabled if the cursor was not already enabled.
		 * Make sure this isn't the case.
		 */

		if(emgd_crtc->igd_cursor->cursor_info->y_offset >= 0) {
			new_pos = (emgd_crtc->igd_cursor->cursor_info->y_offset << 16);
		} else {
			new_pos =
				((-(emgd_crtc->igd_cursor->cursor_info->y_offset)) << 16) |
				0x80000000;
		}
		if(emgd_crtc->igd_cursor->cursor_info->x_offset >= 0) {
			new_pos |= (emgd_crtc->igd_cursor->cursor_info->x_offset);
		} else {
			new_pos |= (-(emgd_crtc->igd_cursor->cursor_info->x_offset)) |
				0x00008000;
		}

		cursor_base =
			EMGD_READ32(mmio + cursor_reg + CUR_BASE_OFFSET);

		EMGD_WRITE32(new_pos, mmio + cursor_reg + CUR_POS_OFFSET);
		EMGD_WRITE32(cursor_base, mmio + cursor_reg + CUR_BASE_OFFSET);
	}


	return 0;
}



/*!
 * kms_get_vblank_counter_snb
 *
 * This function returns the vblank counter number back to the caller.
 *
 * @param emgd_crtc [IN] The pipe to get frame value from
 *
 * @return 0 frame number, which can also be used as a vblank counter number
 */
static u32 kms_get_vblank_counter_snb(emgd_crtc_t *emgd_crtc)
{
	unsigned long      frame_counter_reg;
	unsigned char      *mmio = NULL;


	mmio = MMIO(emgd_crtc);

	switch (emgd_crtc->igd_pipe->pipe_features & IGD_PORT_MASK) {
		case IGD_PORT_SHARE_DIGITAL:
			frame_counter_reg = PIPEB_FRAME_HIGH;
			break;

		case IGD_PORT_SHARE_LVDS:
		default:
			frame_counter_reg = PIPEA_FRAME_HIGH;
			break;
	}

	return EMGD_READ32(mmio + frame_counter_reg);
}


/*!
 * kms_disable_vga_snb
 *
 * Disable the VGA plane
 */
static void kms_disable_vga_snb(igd_context_t * context)
{
	unsigned char *mmio;
	unsigned long temp;
	unsigned char sr01;

	EMGD_TRACE_ENTER;

	mmio = context->device_context.virt_mmadr;

	/*
	 * Disable VGA plane if it is enabled.
	 *
	 * Before modifying the VGA control register, the SR01 bit 5 must
	 * be turned on.
	 *
	 * The VGA plane is enabled when bit31 is cleared and disabled when
	 * bit31 is set.  This is backwards from the other plane controls.
	 */
	temp = EMGD_READ32(mmio + VGACNTRL);
	if ((temp & BIT31) == 0) {
		READ_VGA2(mmio, SR_PORT, SR01, sr01); /* Read SR01 */
		WRITE_VGA2(mmio, SR_PORT, SR01, sr01|BIT(5)); /* Turn on SR01 bit 5 */

		udelay(100); /* Wait for 100us */

		temp |= BIT31;     /* set bit 31 to disable */

		/* Clear out the lower word */
		temp &= ~0x0000ffff;
		temp |= 0x00002900;  /* Set these to the default values */

		EMGD_WRITE32(temp, mmio + VGACNTRL);
	}

	EMGD_TRACE_EXIT;
}



/*!
 * kms_queue_flip_snb
 *
 * Queues a flip instruction in the ring buffer.  This implementation
 * only works for full-screen flips.
 *
 * @param emgd_crtc [IN]  CRTC containing the buffer to flip
 * @param emgd_fb
 */
static int kms_queue_flip_snb(emgd_crtc_t *emgd_crtc,
           	emgd_framebuffer_t *emgd_fb,
           	struct drm_i915_gem_object *bo,
           	int plane_select)
{
	struct drm_device  *dev;
	drm_emgd_private_t *dev_priv;
	unsigned char      *mmio;
	uint32_t            pf, pipesrc;
	int                 ret;
	uint32_t            pitch, offset;


	EMGD_TRACE_ENTER;

	dev      = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	dev_priv = (drm_emgd_private_t *) dev->dev_private;

	ret = intel_pin_and_fence_fb_obj(dev_priv->dev,
			bo, LP_RING(dev_priv));
	if (ret) {
		EMGD_ERROR_EXIT("Failed to pin buffer object");
		return ret;
	}

	/*
	 * If the plane is locked, then save off the info and
	 * flip to the temporary buffer the plane is locked to.
	 * This keeps the normal flip behavoir without actually
	 * changing the display.
	 */
	mmio    = dev_priv->context->device_context.virt_mmadr;
	if (emgd_crtc->freeze_state) {
		emgd_crtc->freeze_crtc = emgd_crtc;
		emgd_crtc->freeze_fb = emgd_fb;
		emgd_crtc->freeze_pitch = emgd_crtc->base.fb->DRMFB_PITCH;

		/* Sustitue our temporary frame buffer */
		pitch = emgd_crtc->freezed_pitch | emgd_crtc->freeze_bo->tiling_mode;
		offset = i915_gem_obj_ggtt_offset(emgd_crtc->freeze_bo);

		/*
		 * If the surface that was copied into the temporary
		 * buffer was tiled, then our temporary buffer looks like
		 * it is tiled too.  The freezed_tiled flag is the indication
		 * that the instruction should set the tiled bit.
		 */
		pitch = emgd_crtc->freezed_pitch | emgd_crtc->freezed_tiled;
	} else {
		pitch = emgd_crtc->base.fb->DRMFB_PITCH | emgd_fb->bo->tiling_mode;
		offset = i915_gem_obj_ggtt_offset(emgd_fb->bo);
	}

	ret = BEGIN_LP_RING(4);
	if (ret) {
		EMGD_ERROR_EXIT("Failed to allocate space on the ring");
		return ret;
	}


	mmio    = dev_priv->context->device_context.virt_mmadr;

	OUT_RING(MI_DISPLAY_FLIP |
			MI_DISPLAY_FLIP_PLANE(plane_select));
	OUT_RING(pitch);
	OUT_RING(offset);

	/*
	 * Using the panel fit and pipe size values in the 4th DW seems
	 * to be a hardware errata for GEN4-6. Also note that the
	 * stereo 3D bit is set in the MI_DISPLAY_FLIP instruction. This
	 * probably means that stereo 3D doesn't work.
	 */
	if (IGD_KMS_PIPEA == emgd_crtc->crtc_id) {
		pf      = EMGD_READ32(mmio + PFA_CTL_1) & PF_ENABLE;
		pipesrc = EMGD_READ32(mmio + PIPEASRC) & 0x0fff0fff;
	} else {
		pf      = EMGD_READ32(mmio + PFB_CTL_1) & PF_ENABLE;
		pipesrc = EMGD_READ32(mmio + PIPEBSRC) & 0x0fff0fff;
	}
	OUT_RING(pf | pipesrc);

	ADVANCE_LP_RING();

	EMGD_TRACE_EXIT;

	return ret;
}



/*!
 *
 * im_reset_plane_pipe_ports_snb
 *
 * Resets all the planes, pipes and ports
 * for this chips display pipelines
 *
 * @param context
 *
 * @return void
 */
static void im_reset_plane_pipe_ports_snb(igd_context_t *context)
{
	igd_display_plane_t *plane;
	igd_display_pipe_t *pipe;
	igd_display_port_t *port,*tv_port=NULL;

	unsigned long temp = 0;
	unsigned char *mmio;
	unsigned long reg;
	unsigned int pipe_num = 0, plane_num = 0;
	igd_module_dispatch_t *module_dispatch;

	EMGD_TRACE_ENTER;

	/*
	 * Disable all plane, pipe and port registers because the
	 * bios may have been using a different set. Only unset the
	 * enable bit.
	 */
	mmio = context->device_context.virt_mmadr;
	module_dispatch = &context->module_dispatch;

	/* Turn off ports */
	port = NULL;
	while ((port = module_dispatch->dsp_get_next_port(context, port, 0))
			!= NULL) {
		/* if the port is TV, then don't set the power to S3 as this causes
		 * blank screen on analog port after killx or cosole mode,
		 * probably because the external clock needs to be on till the pipes and
		 * DPLLs are off
		 */
		if(port->pd_type == PD_DISPLAY_TVOUT_EXT) {
			tv_port = port;
		}else {
			temp = EMGD_READ32(mmio + port->port_reg);
			EMGD_WRITE32(temp & ~BIT31, mmio + port->port_reg);
			if (port->pd_driver) {
				port->pd_driver->set_power(port->pd_context, IGD_POWERSTATE_D3);
			}
		}

	}

	/* Disable VGA plane */
	kms_disable_vga_snb(context);

	/* Disable display plane */
	plane = NULL;
	plane_num = 0;
	while ((plane = module_dispatch->dsp_get_next_plane(context, plane, 0))
			!= NULL) {
		/*  This section only deals with display planes.
		 *  Leave cursor, VGA, overlay, sprite planes alone since they will
		 *  need a different disable bit/sequence.
		 */
		if ((plane->plane_features & IGD_PLANE_DISPLAY)) {
			/* Disable display plane
			 * SNB has Display Plane A and B */
			plane_num = (plane->plane_reg == DSPACNTR) ? 0 :1 ;
			reg = DSPCNTR(plane_num);
			temp = EMGD_READ32(mmio + reg);
			if (temp & PLANE_ENABLE) {
				EMGD_WRITE32(temp & ~PLANE_ENABLE, mmio + reg);

				/* Flush the display plane */
				reg = DSPADDR(plane_num);
				EMGD_WRITE32(EMGD_READ32(mmio + reg), mmio + reg);

				EMGD_DEBUG("Display Plane %d Disable", plane_num);
			}
		} else if ((plane->plane_features & IGD_PLANE_CURSOR)) {
			 /* SNB has Cursor Plane A and B */
			EMGD_WRITE32((temp & 0xffffffd8),
				mmio + plane->plane_reg);
			EMGD_WRITE32(0, mmio + plane->plane_reg+4);
			EMGD_DEBUG("Cursor Plane Disable");
		}
	}

	/* Turn off pipes and the rest of components linked to pipe */
	pipe = NULL;
	pipe_num = 0;
	while ((pipe = module_dispatch->dsp_get_next_pipe(context, pipe, 0))
			!= NULL) {

		/* FIXME: There is no PIPESTAT register in SNB,
		 * wait for vblank won't work here
		 */
		wait_for_vblank_snb(mmio, pipe->pipe_reg);

		disable_pipe_snb(context, pipe->pipe_num);

		EMGD_DEBUG("Pipe %lu Disable", pipe->pipe_num);
	}

	/* pipes and DPLLs are off, now set the power for TV */
	if(tv_port && tv_port->pd_driver) {
		tv_port->pd_driver->set_power(tv_port->pd_context, IGD_POWERSTATE_D3);
	}

	EMGD_TRACE_EXIT;

} /* end im_reset_plane_pipe_ports_snb */

/*!
 *
 * im_filter_modes_snb
 *
 * filters the mode list that the caller (PI at init) will provide.
 * the filtering here is purely HW display pipeline side limited
 * (advanced edid filtering done by PI)
 *
 * @param context
 * @param in_list
 *
 * @return void - To small to trace
 */
static void im_filter_modes_snb(igd_context_t *context,
		igd_display_port_t *port,
		pd_timing_t *in_list)
{
	while (in_list->width != IGD_TIMING_TABLE_END) {
		/*
		 * Dotclock limits
		 * HDMI  25 - 225MHz
		 * CRT   25 - 350MHz
		 * LVDS  25 - 112MHz (single channel)
		 * LVDS  80 - 224MHz (dual channel)
		 * sDVO 100 - 225MHz
		 */
		switch(port->port_type) {
			case IGD_PORT_DIGITAL:
				if ((in_list->dclk < 25000) || (in_list->dclk > 225000)) {
					in_list->flags &= ~IGD_MODE_SUPPORTED;
				}
				break;
			case IGD_PORT_LVDS:
				/* LVDS port driver should be limiting modes already. */
				break;
			case IGD_PORT_ANALOG:
				if ((in_list->dclk < 25000) || (in_list->dclk > 350000)) {
					in_list->flags &= ~IGD_MODE_SUPPORTED;
				}
				break;
			case IGD_PORT_TV:
			case IGD_PORT_MIPI_PORT:
				break;
		}
		in_list++;
		if (in_list->width == IGD_TIMING_TABLE_END && in_list->private.extn_ptr) {
			in_list = in_list->private.extn_ptr;
		}
	}
	return;
} /* end im_filter_modes_snb */


