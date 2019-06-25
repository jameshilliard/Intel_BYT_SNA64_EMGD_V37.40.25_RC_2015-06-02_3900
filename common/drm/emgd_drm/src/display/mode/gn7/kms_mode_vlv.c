/*
 *-----------------------------------------------------------------------------
 * Filename: kms_mode_vlv.c
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
 *
 *-----------------------------------------------------------------------------
 * Description:
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.mode

#include <math_fix.h>
#include <memory.h>

#include <igd_core_structs.h>
#include <pi.h>
#include <vga.h>
#include <gn7/regs.h>
#include <gn7/context.h>
#include <gn7/instr.h>
#include "instr_common.h"
#include "regs_common.h"

#include <i915/intel_bios.h>
#include <i915/intel_ringbuffer.h>
#include "drm_emgd_private.h"
#include "emgd_drv.h"
#include "emgd_drm.h"
#include "i915/intel_drv.h"

#include "../cmn/match.h"
#include "mode_vlv.h"

#include "default_config.h"

extern int drm_emgd_configid;
extern emgd_drm_config_t *config_drm;

#define CHECK_VGA(a) 1
extern os_pci_dev_t bridge_dev; 

#define SPRITE_A 0x2
#define SPRITE_B 0x3
#define SPRITE_C 0x5
#define SPRITE_D 0x4

/*-----------------------------------------------------------------------------
 * Function Prototypes : Exported Device Dependent Hooks for KMS
 *---------------------------------------------------------------------------*/
static void kms_program_pipe_vlv(emgd_crtc_t *emgd_crtc);
static void kms_set_pipe_pwr_vlv(emgd_crtc_t *emgd_crtc, unsigned long enable);
static int __must_check kms_program_plane_vlv(emgd_crtc_t *emgd_crtc, bool preserve );
static int kms_calc_plane_vlv(emgd_crtc_t *emgd_crtc, bool preserve);
static void kms_commit_plane_vlv(emgd_crtc_t *emgd_crtc, void *regs);
static void kms_set_plane_pwr_vlv(emgd_crtc_t *emgd_crtc, unsigned long enable,
			bool wait_for_vblank);
static int  kms_program_port_vlv(emgd_encoder_t *emgd_encoder,
				unsigned long status);
static int  kms_post_program_port_vlv(emgd_encoder_t *emgd_encoder,
				unsigned long status);
static u32  kms_get_vblank_counter_vlv(emgd_crtc_t *emgd_crtc);
static void kms_disable_vga_vlv(igd_context_t * context);
static void kms_program_cursor_vlv(emgd_crtc_t *emgd_crtc,
				unsigned long status);
static int kms_alter_cursor_pos_vlv(emgd_crtc_t *emgd_crtc);
static int kms_queue_flip_vlv(emgd_crtc_t *emgd_crtc,
           		emgd_framebuffer_t *emgd_fb,
           		struct drm_i915_gem_object *bo,
           		int plane_select);
static int kms_set_palette_entries_vlv( emgd_crtc_t *emgd_crtc,
				unsigned int start_index, unsigned int count);
static void kms_set_display_base_vlv(emgd_crtc_t *emgd_crtc,
				emgd_framebuffer_t *fb);
static int kms_set_color_correct_vlv(emgd_crtc_t *emgd_crtc);
static void kms_update_wm_vlv(struct drm_device *dev);

int kms_change_plane_color_format (emgd_crtc_t *emgd_crtc, unsigned int fb_blend_flag);

/*-----------------------------------------------------------------------------
 * Function Prototypes : Mode-Exported Inter-Module helper functions
 *---------------------------------------------------------------------------*/
static void im_reset_plane_pipe_ports_vlv(igd_context_t *context);
static void im_filter_modes_vlv(igd_context_t *context, igd_display_port_t *port,
	pd_timing_t *in_list);

/*-----------------------------------------------------------------------------
 * Function Prototypes : 
 *---------------------------------------------------------------------------*/
static int kms_get_brightness (emgd_crtc_t *emgd_crtc);
static int kms_get_max_brightness (emgd_crtc_t *emgd_crtc);
static int kms_set_brightness (emgd_crtc_t *emgd_crtc, int level );
static int kms_blc_support (emgd_crtc_t *emgd_crtc, unsigned long *port_number);


/*-----------------------------------------------------------------------------
 * KMS Dispatch Table
 *---------------------------------------------------------------------------*/
igd_mode_dispatch_t mode_kms_dispatch_vlv = {
	/* Device Independent Layer Interfaces - (FIXME:Filled by mode_init?) */
		kms_match_mode,
	/* Device Dependent Layer Interfaces - Defined by this table */
		kms_program_pipe_vlv,
		kms_set_pipe_pwr_vlv,
		kms_program_plane_vlv,
		kms_calc_plane_vlv,
		kms_commit_plane_vlv,
		kms_set_plane_pwr_vlv,
		kms_program_port_vlv,
		kms_post_program_port_vlv,
		kms_get_vblank_counter_vlv,
		kms_disable_vga_vlv,
		kms_program_cursor_vlv,
		kms_alter_cursor_pos_vlv,
		kms_queue_flip_vlv,
		kms_set_palette_entries_vlv,
		kms_set_display_base_vlv,
		kms_set_color_correct_vlv,
		kms_update_wm_vlv,
		kms_change_plane_color_format,
	/* Device Dependant Layer - Inter-Module Interfaces - not called by KMS */
		im_reset_plane_pipe_ports_vlv,
		im_filter_modes_vlv,
	/* Pipe programming for backlight support. */
		kms_get_brightness,
		kms_get_max_brightness,
		kms_set_brightness,
		kms_blc_support
};



/*------------------------------------------------------------------------------
 * Function Prototypes : Mode-Internal Device-Dependent helper functions
 *----------------------------------------------------------------------------*/
extern int program_clock_vlv(emgd_crtc_t *emgd_crtc, igd_clock_t *clock,
		unsigned long dclk, unsigned short operation);
static int program_port_analog_vlv(emgd_encoder_t * emgd_encoder,
		unsigned long status);
static int  program_port_digital_vlv(emgd_encoder_t *emgd_encoder,
		unsigned long status);
static void wait_for_vblank_timeout_vlv(unsigned char *mmio,
		unsigned long pipe_reg,
		unsigned long time_interval);
static void wait_for_vblank_vlv(unsigned char *mmio, unsigned long pipe_reg);
static void program_pipe_vga_vlv(emgd_crtc_t *emgd_crtc);
static void disable_hdmi_for_dp_vlv(unsigned char *mmio,
		unsigned long reg, unsigned long pipe_reg);

/*------------------------------------------------------------------------------
 * Function Prototypes : Externs outside of HAL - Should we wrap this??
 *----------------------------------------------------------------------------*/
extern int intel_pin_and_fence_fb_obj(struct drm_device *dev,
		struct drm_i915_gem_object *obj,
		struct intel_ring_buffer *pipelined);


/*------------------------------------------------------------------------------
 * Internal Device Dependent Global Variables
 *----------------------------------------------------------------------------*/

extern unsigned long ports_vlv[2];
mode_data_vlv_t device_data[1] = {
	{
		0x000b0000, /* plane a preservation */
		0x00000000, /* plane b c preservation */
		0x60000000, /* pipe preservation */
		0xC080C080, /* FIFO Size PA=128, SA=64, SB=64; PB=128, SC=64, SD=64 */
		0x3f8f0f0f, /* FIFO watermark control1 */
		0x0b0f0f0f, /* FIFO watermark control2 */
		0x00000000, /* FIFO watermark control3 */
		0x00040404, /* FIFO watermark1 control4 */
		0x04040404, /* FIFO watermark1 control5 */
		0x00000078, /* FIFO watermark1 control6 */
		0x040f040f, /* FIFO watermark1 control7 */
	}
};

static const struct intel_watermark_params valleyview_wm_info = {
    VALLEYVIEW_FIFO_SIZE,
    VALLEYVIEW_MAX_WM,
    VALLEYVIEW_MAX_WM,
    2,
    G4X_FIFO_LINE_SIZE,
};
static const struct intel_watermark_params valleyview_cursor_wm_info = {
    I965_CURSOR_FIFO,
    VALLEYVIEW_CURSOR_MAX_WM,
    I965_CURSOR_DFT_WM,
    2,
    G4X_FIFO_LINE_SIZE,
};

/*
 * Latency for FIFO fetches is dependent on several factors:
 *   - memory configuration (speed, channels)
 *   - chipset
 *   - current MCH state
 * It can be fairly high in some situations, so here we assume a fairly
 * pessimal value.  It's a tradeoff between extra memory fetches (if we
 * set this value too high, the FIFO will fetch frequently to stay full)
 * and power consumption (set it too low to save power and we might see
 * FIFO underruns and display "flicker").
 *
 * A value of 5us seems to be a good balance; safe for very low end
 * platforms but not overly aggressive on lower latency configs.
 */
static const int latency_ns = 5000;

/*------------------------------------------------------------------------------
 * Function Prototypes : Watermark
 *----------------------------------------------------------------------------*/
static void valleyview_update_dl(struct drm_device *dev);
static bool g4x_compute_wm0(struct drm_device *dev,
                int plane,
                const struct intel_watermark_params *display,
                int display_latency_ns,
                const struct intel_watermark_params *cursor,
                int cursor_latency_ns,
                int *plane_wm,
                int *cursor_wm);
static int valleyview_compute_dl(struct drm_device *dev,
                    int plane,
                    int *plane_prec_mult,
                    int *plane_dl,
                    int *cursor_prec_mult,
                    int *cursor_dl);
static bool g4x_compute_srwm(struct drm_device *dev,
                 int plane,
                 int latency_ns,
                 const struct intel_watermark_params *display,
                 const struct intel_watermark_params *cursor,
                 int *display_wm, int *cursor_wm);
static bool g4x_check_srwm(struct drm_device *dev,
               int display_wm, int cursor_wm,
               const struct intel_watermark_params *display,
               const struct intel_watermark_params *cursor);



/*-----------------------------------------------------------------------------
 * Backlight support has 4 funtions in KMS layer:
 * 	-kms_get_brightness
 * 	-kms_get_max_brightness
 * 	-kms_set_brightness
 * 	-kms_blc_support
 *---------------------------------------------------------------------------*/

/*!
 * kms_get_brightness
 *
 * @param emgd_crtc
 *
 * @return current brigthness on success, -IGD_ERROR_INVAL on failure,
 * 0 for not applicable port.
 */
static int kms_get_brightness (emgd_crtc_t * emgd_crtc){
	igd_display_pipe_t *pipe = NULL;
	igd_context_t      *context = NULL;
	igd_display_port_t *port = NULL;
	int brightness_percent = 0;

	EMGD_TRACE_ENTER;

	if (emgd_crtc == NULL){
		EMGD_ERROR ("CRTC is not available");
		return -IGD_ERROR_INVAL;
	}

	pipe = PIPE(emgd_crtc);
	port = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	if ((port == NULL) || (pipe == NULL) || (context == NULL)){
		EMGD_ERROR ("Display is not properly set.");
		return -IGD_ERROR_INVAL;
	}

	if (port->pd_driver == NULL){
		EMGD_ERROR ("No port driver.");
		return -IGD_ERROR_INVAL;
	}

	if ((port->port_number != IGD_PORT_TYPE_DPB) && 
			(port->port_number != IGD_PORT_TYPE_DPC)) {
		/*No backlight*/
		EMGD_TRACE_EXIT;
		return 0;
	} 


	if (port->pd_driver->get_brightness) {
		brightness_percent = port->pd_driver->get_brightness(port->pd_context);
		return brightness_percent;
	}

	return 0;

	EMGD_TRACE_EXIT;
}

/*!
 * kms_get_max_brightness
 *
 * @param emgd_crtc
 *
 * @return max brigthness on success, -IGD_ERROR_INVAL on failure,
 * 0 for not applicable port.
 */
static int kms_get_max_brightness (emgd_crtc_t * emgd_crtc){
	igd_display_pipe_t *pipe = NULL;
	igd_context_t      *context = NULL;
	igd_display_port_t *port = NULL;

	EMGD_TRACE_ENTER;

	if (emgd_crtc == NULL){
		EMGD_ERROR ("CRTC is NULL");
		return -IGD_ERROR_INVAL;
	}
	pipe = PIPE(emgd_crtc);
	port = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	
	if ((port == NULL) || (pipe == NULL) || (context == NULL)){
		EMGD_ERROR ("Display is not properly set.");
		return -IGD_ERROR_INVAL;
	}

	if (port->pd_driver == NULL){
		EMGD_ERROR ("No port driver.");
		return -IGD_ERROR_INVAL;
	}

	if ((port->port_number != IGD_PORT_TYPE_DPB) && 
				(port->port_number != IGD_PORT_TYPE_DPC)) {
		/*No backlight*/
		EMGD_TRACE_EXIT;
		return 0;
	} 
	
	/* We fixed the max to 100. */
	return MAX_BRIGHTNESS; 	

	EMGD_TRACE_EXIT;
}

/*!
 * kms_set_brightness
 *
 * @param emgd_crtc, level
 *
 * @return 0 on succes, -IGD_ERROR_INVAL on failure,
 * 0 for not applicable port.
 */
static int kms_set_brightness (emgd_crtc_t * emgd_crtc, int level){
	igd_display_pipe_t *pipe = NULL;
	igd_context_t      *context = NULL;
	igd_display_port_t *port = NULL;

	EMGD_TRACE_ENTER;

	if (emgd_crtc == NULL){
		EMGD_ERROR ("CRTC is NULL");
		return -IGD_ERROR_INVAL;
	}
	pipe = PIPE(emgd_crtc);
	port = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	
	if ((port == NULL) || (pipe == NULL) || (context == NULL)){
		EMGD_ERROR ("Display is not properly set.");
		return -IGD_ERROR_INVAL;
	}

	if (port->pd_driver == NULL){
		EMGD_ERROR ("No port driver.");
		return -IGD_ERROR_INVAL;
	}

	if (port->pd_driver->set_brightness) {
		return port->pd_driver->set_brightness(port->pd_context, level); 
	}

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 * kms_blc_support
 *
 * @param emgd_crtc
 *
 * @return 1 for supported device, 0 otherwise. 
 * Return -IGD_ERROR_INVAL on failure,
 * 
 */
static int kms_blc_support (emgd_crtc_t * emgd_crtc, unsigned long *port_number){
	igd_display_pipe_t *pipe = NULL;
	igd_context_t      *context = NULL;
	igd_display_port_t *port = NULL;

	EMGD_TRACE_ENTER;

	if (emgd_crtc == NULL){
		EMGD_ERROR ("CRTC is NULL");
		return -IGD_ERROR_INVAL;
	}
	pipe = PIPE(emgd_crtc);
	port = PORT(emgd_crtc);
	context = emgd_crtc->igd_context;

	
	if ((port == NULL) || (pipe == NULL) || (context == NULL)){
		EMGD_ERROR ("Display is not properly set.");
		return -IGD_ERROR_INVAL;
	}

	if (port->pd_driver == NULL){
		EMGD_ERROR ("No port driver.");
		return -IGD_ERROR_INVAL;
	}

	if ((port->port_number != IGD_PORT_TYPE_DPB) && 
			(port->port_number != IGD_PORT_TYPE_DPC)) {
		/* No backlight support*/
		EMGD_DEBUG ("No backlight support, non DP.");
		EMGD_TRACE_EXIT;
		return 0;
	} 

	if (port->fp_info == NULL){
		/* Not a flat panel. No backlight support. */
		EMGD_DEBUG ("No backlight support, not flat panel.");
		EMGD_TRACE_EXIT;
		return 0;
	}

	if (port->fp_info->fp_pwr_method == IGD_PARAM_FP_PWR_METHOD_PD) {
		*port_number = port->port_number;
		EMGD_TRACE_EXIT;
		return 1;
	}

	EMGD_TRACE_EXIT;
	return 0;
}
/* End backlight functions */

/*!
 * kms_set_color_correct_vlv
 *
 * @param emgd_crtc
 *
 * @return 0 on success
 * @return -IGD_ERROR_INVAL if color attributes not found
 */
static int kms_set_color_correct_vlv(emgd_crtc_t *emgd_crtc)
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
	for (i = 0; i < NUM_PALETTE_ENTRIES; i++) {
		/* SDVO palette register is not accesible */
		WRITE_MMIO_REG_VLV(palette[i],
			context->device_context.virt_mmadr,
			pipe->palette_reg + i*4);

		/* Update the crtc's LUT with the newly calculated values */
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
static int kms_set_palette_entries_vlv(
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
		WRITE_MMIO_REG_VLV(color, mmio,
				emgd_crtc->igd_pipe->palette_reg + (i * 4));
	}

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 * program_pipe_vga_vlv
 *
 * @param display Pointer to hardware device instance data
 *
 * @return void
 */
static void program_pipe_vga_vlv(
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
	vga_control = READ_MMIO_REG_VLV(context->device_context.virt_mmadr, VGACNTRL);
	vga_control &= 0x18e3ff00;
	vga_control |= 0x8e;

	timing = pipe->timing;
	if(!timing->private.extn_ptr) {
		EMGD_ERROR_EXIT("No Extension pointer in program_pipe_vga_vlv");
		return;
	}

	if(port) {
		/* Find UPSCALING attr value*/
		pi_pd_find_attr_and_value(port,
				PD_ATTR_ID_PANEL_FIT,
				0,/*no PD_FLAG for UPSCALING */
				NULL, /* dont need the attr ptr*/
				&upscale);
	}

	/* magic->vga || native->vga cases, centering isn't required */
	if ((timing->width == 720 && timing->height == 400) || upscale) {
		EMGD_DEBUG("Centering = 0");
		centering = 0;
	}

	/* Enable border */
	if ((timing->width >= 800) && (!upscale)) {
		EMGD_DEBUG("Enable VGA Border");
		vga_control |= (1L<<26);
	}

	if(timing->width == 640) {
		EMGD_DEBUG("Enable Nine Dot Disable");
		vga_control |= (1L<<18);
	}

	if(centering) {
		EMGD_DEBUG("Enable VGA Center Centering");
		vga_control |= 1L<<24;

		if(timing->height >= 960 && timing->width >= 1280) {
			EMGD_DEBUG("Enable VGA 2x (Nine Dot Disable)");
			vga_control |= (1L<<30) | (1L<<18);
		}
	} else {
		if (upscale) {
			EMGD_DEBUG("Enable VGA Center Upper-left for upscale ports");
			vga_control |= 1L<<25;
		}
	}

	if(pipe->pipe_num) {
		vga_control |= 1L<<29;
	}

	kms_program_pipe_vga(emgd_crtc, (igd_timing_info_t *)timing->private.extn_ptr);
	WRITE_MMIO_REG_VLV(vga_control, context->device_context.virt_mmadr, VGACNTRL);

	EMGD_TRACE_EXIT;
	return;
}

/*!
 * wait_for_vblank_timeout_vlv()
 *
 * Waits for a vblank interrupt.  Note that this races with our main interrupt
 * handler so either this function or the vblank handler may miss a vblank
 * event.  As such, timeouts here are expected and non-fatal
 *
 * @param mmio
 * @param pipe_reg
 *
 * @return 0 on success
 * @return 1 on failure
 */
void wait_for_vblank_timeout_vlv(
	unsigned char *mmio,
	unsigned long pipe_reg,
	unsigned long time_interval)
{
	unsigned long pipe_status_reg = pipe_reg + PIPE_STATUS_OFFSET;
	unsigned long tmp;
	os_alarm_t timeout;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("MMIO = %p, pipe_reg = %lx", mmio, pipe_reg);

	/* If pipe is off then just return */
	if(!((1L<<31) & READ_MMIO_REG_VLV(mmio, pipe_reg))) {
		EMGD_DEBUG("Pipe disabled/Off");
		EMGD_TRACE_EXIT;
		return;
	}

	/*
	 * When VGA plane is on the normal wait for vblank won't work
	 * so just skip it.
	 */
	if(!(READ_MMIO_REG_VLV(mmio, 0x71400) & 0x80000000)) {
		EMGD_DEBUG("VGA Plane On");
		EMGD_TRACE_EXIT;
		return;
	}

	/* 1. Clear VBlank interrupt status (by writing a 1) */
	tmp = READ_MMIO_REG_VLV(mmio, pipe_status_reg);
	WRITE_MMIO_REG_VLV (tmp | VBLANK_STS, mmio, pipe_status_reg);

	/*
	 * 2. Wait for VBlank with specified timeout.  If we don't get the
	 * interrupt by that time, we probably lost the vblank events to the
	 * main handler that we're racing with.
	 */
	timeout = OS_SET_ALARM(time_interval);
	do {
		tmp = READ_MMIO_REG_VLV(mmio, pipe_status_reg) & VBLANK_STS;
	} while ((tmp == 0x00) && (!OS_TEST_ALARM(timeout)));
	if (tmp == 0) {
		EMGD_DEBUG("VBlank timed out");
	}

	EMGD_TRACE_EXIT;
} /* wait_for_vblank_timeout_vlv */


/*!
 * This procedure waits for the next vertical blanking (vertical retrace)
 * period. If the display is already in a vertical blanking period, this
 * procedure exits.
 *
 * @param mmio
 * @param pipe_reg
 *
 * @return 0 on success
 * @return 1 on failure
 */
void wait_for_vblank_vlv(unsigned char *mmio, unsigned long pipe_reg)
{
	wait_for_vblank_timeout_vlv(mmio, pipe_reg, 100);
} /* wait_for_vblank_plb */



/*!
 * Get the stride and stereo values based on the display.  This is also used
 * by the MI instructions.
 *
 * @param display Pointer to hardware device instance data
 * @param flags Should the stereo be for the frontbuffer or backbuffer?
 *
 * @return stride - Stride of the display
 * @return stereo - Stereo address of the display
 */
int kms_mode_get_stride_stereo_vlv(emgd_crtc_t *emgd_crtc,
	unsigned long *stride,
	unsigned long *stereo,
	unsigned long flags)
{


	unsigned long pitch = PLANE(emgd_crtc)->fb_info->base.DRMFB_PITCH;
	igd_timing_info_t *timing = PIPE(emgd_crtc)->timing;
	unsigned long base_offset;

	EMGD_TRACE_ENTER;

	base_offset = i915_gem_obj_ggtt_offset(PLANE(emgd_crtc)->fb_info->bo);
	*stride = pitch;
	*stereo = 0;

	/* For field replication, valid for interlaced modes only
 	 *     set stereo = fb_base
  	 *         stride = pitch
	 */
	if (timing->flags & IGD_SCAN_INTERLACE) {

		if(timing->flags & IGD_LINE_DOUBLE) {
			/* Interlaced + Line double flags means field replication.
			 * same lines are sent for both fields. Program the
 			 * second eye to be same as the first
			 */
			*stereo = base_offset;
		} else {
			/* Regular interlaced. Second eye starts on line 2.
			 * Skip every other line.
 			 */
			*stereo = base_offset + pitch;
			*stride = pitch * 2;
		}
	}

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 * kms_set_plane_pwr_vlv
 *
 * Turn off the display plane associated with the given CRTC.
 *
 * @param emgd_crtc [IN] Selected CRTC
 * @param enable    [IN]
 */
static void kms_set_plane_pwr_vlv(emgd_crtc_t *emgd_crtc, unsigned long enable,
								bool wait_for_vblank)
{
	unsigned long        plane_ctrl;
	unsigned long        cur_plane_ctrl;
	igd_display_plane_t *plane;
	igd_cursor_t        *cursor;
	igd_display_pipe_t  *pipe;
	igd_context_t       *context;
	unsigned char       *mmio;
	emgd_framebuffer_t  *fb_info;


	EMGD_TRACE_ENTER;

	EMGD_DEBUG("Program Plane: %s", enable?"ENABLE":"DISABLE");

	plane = PLANE(emgd_crtc);

	if (!plane) {
		EMGD_ERROR_EXIT("Trying to set power to a plane that is not tied"
			" to a crtc.");
		return;
	}

	pipe       = PIPE(emgd_crtc);
	cursor     = CURSOR(emgd_crtc);
	context    = emgd_crtc->igd_context;
	mmio       = MMIO(emgd_crtc);
	plane_ctrl = READ_MMIO_REG_VLV(mmio, plane->plane_reg);
	fb_info    = plane->fb_info;

	/* Do nothing if current power state is same as what we want to set */
	if ( (enable << 31) == (plane_ctrl & PLANE_ENABLE) ){
		EMGD_TRACE_EXIT;
		return;
	}

	if (FALSE == enable) {
		/*
		 *FIXME: This is being done a lot! Review and fix.
		 * Note: The vga programming code does not have an "off". So
		 * when programming the plane to off we make sure VGA is off
		 * as well.
		 */
		kms_disable_vga_vlv(context);

		/* Disable display plane */
		plane_ctrl &= ~BIT31;

		/* Disable cursor plane.  Bits 0,1,2 should be 0 */
		/* FIXME:  Should we be doing cursor (and sprite) plane power here? */
		cur_plane_ctrl = READ_MMIO_REG_VLV(mmio, cursor->cursor_reg);
		WRITE_MMIO_REG_VLV((cur_plane_ctrl & 0xfffffff8), mmio, cursor->cursor_reg);
	} else {
		/* Enable display plane */
		plane_ctrl |= BIT31;
	}

	/* Update plane control register.  The changes is triggered after
	 * writing to DSP_START_OFFSET */
	WRITE_MMIO_REG_VLV(plane_ctrl, mmio, plane->plane_reg);
	WRITE_MMIO_REG_VLV(READ_MMIO_REG_VLV(mmio, plane->plane_reg + DSP_START_OFFSET),
		mmio, plane->plane_reg + DSP_START_OFFSET);

	if (wait_for_vblank) {
		wait_for_vblank_vlv(mmio, pipe->pipe_reg);
	}

	EMGD_TRACE_EXIT;

	return;
}

/*!
 * s3d_update_ovlplane
 *
 * This is the non-mutex version of emgd_core_update_ovlplane.
 * It does almost the the same job as emgd_core_update_ovlplane except
 * mutex is not needed here to avoid deadlock, as it is already
 * mutex protected by upper layer in emgd_crtc_mode_set_base.
 * It will also update plane->enabled_by_s3d to indicate
 * the sprite plane is enable for S3D.
 *
 * @param emgd_crtc    [IN] Selected CRTC
 * @param plane        [IN] Sprite plane to be updated
 * @param newbufobj    [IN] Buffer to flip to
 * @param pin          [IN] Set to 1 to pin to BO
 * @param crtc_x/y/w/h [IN] Destination X, Y, width and height
 * @param src_x/y/w/h  [IN] Source X, Y, width and height in
 *                          16.16 fixed point format
 * @param src_pitch    [IN] Source pitch
 * @param drm_fmt      [IN] Source format from drm
 */
static int s3d_update_ovlplane(emgd_crtc_t *emgd_crtc, igd_ovl_plane_t * plane,
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
	igd_context_t *context = NULL;
	igd_display_port_t *port = NULL;

	EMGD_TRACE_ENTER;

	context = (igd_context_t *)emgd_crtc->igd_context;
	port = PORT(emgd_crtc);

	/* crtc_x and crtc_y passed in is relative to the framebuffer,
	 * let's adjust it so that it is relative to the display (screen)
	 */

	if(!port) {
		EMGD_ERROR("Invalid NULL parameters: port");
		EMGD_TRACE_EXIT;
		return -EINVAL;
	}

	crtc_x -= port->pt_info->x_offset;
	crtc_y -= port->pt_info->y_offset;

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

	if(plane->curr_surf.bo) {
		oldbufobj = (struct drm_i915_gem_object *)plane->curr_surf.bo;
	}
	if (oldbufobj != newbufobj) {
		if(pin) {
			ret = intel_pin_and_fence_fb_obj(emgd_crtc->base.dev, newbufobj, NULL);
			if (ret) {
				EMGD_ERROR("Failed to pin object!");
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

	ret = context->drv_dispatch.alter_ovl(
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
		plane->enabled_by_s3d = 1;
	} else {
		EMGD_ERROR("alter_ovl failed on plane id %d", plane->plane_id);
	}

	/* Unpin old obj after new one is active to avoid ugliness */
	if (oldbufobj && (oldbufobj != newbufobj)) {
		intel_unpin_fb_obj(oldbufobj);
		drm_gem_object_unreference(&oldbufobj->base);
	}

	EMGD_TRACE_EXIT;

	return ret;

}

/*!
 * s3d_disable_ovlplane
 *
 * This is the non-mutex version of emgd_core_disable_ovlplane.
 * It does almost the same job as emgd_core_disable_ovlplane except
 * mutex is not needed here to avoid deadlock, as it is already
 * mutex protected by upper layer in emgd_crtc_mode_set_base.
 * It will also update plane->enabled_by_s3d to indicate
 * the sprite plane is not used for S3D.
 *
 * @param emgd_crtc    [IN] Selected CRTC
 * @param plane        [IN] Sprite plane to be disable
 * @param uppin        [IN] Set to 1 to unpin to BO
 */
static int s3d_disable_ovlplane(emgd_crtc_t *emgd_crtc,
		igd_ovl_plane_t * plane, int unpin)
{
	igd_context_t *context = NULL;
	EMGD_TRACE_ENTER;

	context = (igd_context_t *)emgd_crtc->igd_context;

	if(plane->enabled) {

		context->drv_dispatch.alter_ovl(
				plane->emgd_crtc, plane->plane_id,
				NULL, NULL, NULL, NULL, NULL,
				IGD_OVL_ALTER_OFF);

		if(plane->curr_surf.bo != NULL) {
			if(unpin) {
				intel_unpin_fb_obj(plane->curr_surf.bo);
				drm_gem_object_unreference(&plane->curr_surf.bo->base);
			}
		}

		plane->enabled      = 0;
		plane->enabled_by_s3d = 0;
		plane->curr_surf.bo = NULL;
		plane->emgd_crtc    = NULL;

	}

	EMGD_TRACE_EXIT;

	return 0;
}

/*!
 * s3d_program_ovlplane_vlv
 *
 * Based on the S3D connector property set and buffer object added by user,
 * update or disable the sprite plane accordingly.
 *
 * @param emgd_crtc    [IN] Selected CRTC
 */
static void s3d_program_ovlplane_vlv(emgd_crtc_t *emgd_crtc)
{
	igd_timing_info_t   *timing;
	emgd_framebuffer_t  *fb_info;
	igd_ovl_plane_t     *sprite1, *sprite2;
	igd_display_pipe_t  *pipe;
	igd_display_port_t  *port;
	igd_context_t       *context;
	unsigned char       *mmio;
	unsigned long	s3d_mode_sel = 0;
	int				enable_sprite = 1;

	/* Source and destination for left image */
	unsigned int	crtc_x1, crtc_y1, crtc_w1, crtc_h1;
	unsigned int	src_x1, src_y1, src_w1, src_h1;

	/* Source and destination for right image */
	unsigned int	crtc_x2, crtc_y2, crtc_w2, crtc_h2;
	unsigned int	src_x2, src_y2, src_w2, src_h2;

	EMGD_TRACE_ENTER;

	pipe      = PIPE(emgd_crtc);
	port      = PORT(emgd_crtc);
	mmio      = MMIO(emgd_crtc);
	context   = emgd_crtc->igd_context;

	sprite1   = emgd_crtc->sprite1;
	sprite2   = emgd_crtc->sprite2;

	/* Validate bo size here? */

	if (!sprite1 || !sprite2) {
		EMGD_ERROR("Trying to program sprite plane that is not tied to a crtc.");
		EMGD_TRACE_EXIT;
		return;
	}

	timing        = pipe->timing;
	fb_info       = emgd_crtc->igd_plane->fb_info;

	if(pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_S3D_MODE_SET,
			0,		/*no PD_FLAG for S3D attr*/
			NULL,	/* don't need the attr ptr*/
			&s3d_mode_sel)) {
		EMGD_DEBUG("S3D_MODE_SET attributes not found, port doesn't support S3D");
		EMGD_TRACE_EXIT;
		return;
	}

	switch (s3d_mode_sel & 0xF0) {
	case PD_S3D_MODE_FRAME_PACKING:
		/* Since the primary height was adjusted (2X height + blanking) during timing
		 * fixed up for Framepacking format, need to find the height for left and right
		 * images without 3D format
		 */
		crtc_x1 = src_x1 = 0;
		crtc_y1 = src_y1 = 0;
		crtc_w1 = src_w1 = timing->width;
		crtc_h1 = src_h1 = timing->height - (timing->vtotal >> 1);

		crtc_x2 = 0;
		crtc_y2 = timing->vtotal >> 1;
		src_x2 = 0;
		src_y2 = 0;
		crtc_w2 = src_w2 = timing->width;
		crtc_h2 = src_h2 = timing->height - (timing->vtotal >> 1);

		EMGD_DEBUG("S3D Framepacking format");
		break;

	case PD_S3D_MODE_SBS_HALF:
		crtc_x1 = src_x1 = 0;
		crtc_y1 = src_y1 = 0;
		crtc_w1 = src_w1 = timing->width >> 1;	/* width divided by 2 */
		crtc_h1 = src_h1 = timing->height;

		crtc_x2 = timing->width >> 1;
		crtc_y2 = 0;
		src_x2 = 0;
		src_y2 = 0;
		crtc_w2 = src_w2 = timing->width >> 1;
		crtc_h2 = src_h2 = timing->height;

		EMGD_DEBUG("S3D Side-by-side Half format");
		break;

	case PD_S3D_MODE_TOP_BOTTOM:
		crtc_x1 = src_x1 = 0;
		crtc_y1 = src_y1 = 0;
		crtc_w1 = src_w1 = timing->width;
		crtc_h1 = src_h1 = timing->height >> 1; /* height divided by 2 */

		crtc_x2 = 0;
		crtc_y2 = timing->height >> 1;
		src_x2 = 0;
		src_y2 = 0;
		crtc_w2 = src_w2 = timing->width;
		crtc_h2 = src_h2 = timing->height >> 1;

		EMGD_DEBUG("S3D Top-and-Bottom format");
		break;

	case PD_S3D_MODE_SBS_FULL:
		/* Since the width was doubled during timing fixed up for
		 * Sided-by-side Full format , the width for left and right
		 * images is half of the primary width
		 */
		crtc_x1 = src_x1 = 0;
		crtc_y1 = src_y1 = 0;
		crtc_w1 = src_w1 = timing->width >> 1;	/* width divided by 2 */
		crtc_h1 = src_h1 = timing->height;

		crtc_x2 = timing->width >> 1;
		crtc_y2 = 0;
		src_x2 = 0;
		src_y2 = 0;
		crtc_w2 = src_w2 = timing->width >> 1;
		crtc_h2 = src_h2 = timing->height;

		EMGD_DEBUG("S3D Side-by-side Full format");
		break;

	default:
		crtc_x1 = src_x1 = 0;
		crtc_y1 = src_y1 = 0;
		crtc_w1 = src_w1 = 0;
		crtc_h1 = src_h1 = 0;

		crtc_x2 = src_x2 = 0;
		crtc_y2 = src_y2 = 0;
		crtc_w2 = src_w2 = 0;
		crtc_h2 = src_h2 = 0;

		enable_sprite = 0;
		EMGD_DEBUG("S3D property is set to disable or unsupported S3D format, disable sprite if necessary");
	}

	EMGD_DEBUG("S3D Left: x=%d, y=%d, w=%d, h=%d, Right: x=%d, y=%d, w=%d, h=%d",
				crtc_x1, crtc_y1, crtc_w1, crtc_h1, crtc_x2, crtc_y2, crtc_w2, crtc_h2);

	if(enable_sprite) {
		/* Program sprite plane for left image */
		s3d_update_ovlplane(emgd_crtc, sprite1, fb_info->other_bo[0], 1,
				   crtc_x1, crtc_y1, crtc_w1, crtc_h1,
				   src_x1 << 16, src_y1 << 16, src_w1 << 16, src_h1 << 16,
				   fb_info->base.pitches[1], fb_info->base.pixel_format);

		/* Program sprite plane for right image */
		s3d_update_ovlplane(emgd_crtc, sprite2, fb_info->other_bo[1], 1,
				   crtc_x2, crtc_y2, crtc_w2, crtc_h2,
				   src_x2 << 16, src_y2 << 16, src_w2 << 16, src_h2 << 16,
				   fb_info->base.pitches[2], fb_info->base.pixel_format);
	}
	else {
		/* We should disable the sprite only if it was previously
		 * enable by s3d_program_ovlplane
		 */
		if(sprite1->enabled_by_s3d) {
			s3d_disable_ovlplane(emgd_crtc,sprite1, 1);
		}
		if(sprite2->enabled_by_s3d) {
			s3d_disable_ovlplane(emgd_crtc,sprite2, 1);
		}
	}
	EMGD_TRACE_EXIT;
	return;
}

static int kms_calc_plane_vlv(emgd_crtc_t *emgd_crtc, bool preserve)
{
	igd_timing_info_t   *timing;
	emgd_framebuffer_t  *fb_info;
	igd_display_plane_t *plane;
	igd_display_pipe_t  *pipe;
	igd_context_t       *context;
	struct emgd_plane_regs *regs;

	EMGD_TRACE_ENTER;

	pipe      = PIPE(emgd_crtc);
	plane     = PLANE(emgd_crtc);
	context   = emgd_crtc->igd_context;

	if (!plane|| !pipe->timing) {
		EMGD_ERROR("Either plane is NULL or pipe->timing is NULL");
		return -EINVAL;
	}

	if (emgd_crtc->primary_regs) {
		regs = (struct emgd_plane_regs *)emgd_crtc->primary_regs;
	} else {
		EMGD_ERROR("emgd_crtc->primary_regs is NULL");
		return -EINVAL;
	}

	timing        = pipe->timing;
	fb_info       = plane->fb_info;
	regs->mmio    = MMIO(emgd_crtc);
	regs->plane_reg     = plane->plane_reg;
	regs->plane_control = READ_MMIO_REG_VLV(regs->mmio, regs->plane_reg);

	if (preserve != TRUE){
		if (DSPACNTR == regs->plane_reg) {
			regs->plane_control &= device_data->plane_a_preserve;
		} else { /* if it's plane b or plane c */
			regs->plane_control &= device_data->plane_b_c_preserve;
		}
	}

#ifndef CONFIG_MICRO
	/*pipe_timing = timing;*/
#endif

	/* There is a special case code for legacy VGA modes */
	while (timing->private.extn_ptr) {
		timing = (igd_timing_info_t *)timing->private.extn_ptr;
	}

	if(MODE_IS_VGA(timing)&& CHECK_VGA(pipe_timing)) {
		regs->is_mode_vga = true;
		EMGD_TRACE_EXIT;
		return 0;
	} else {
		kms_disable_vga_vlv(context);
	}

	/* FIXME: Programming the palette register causes driver to fail.
	 * Disabling gamma until palette register problem is solved
	 */
	/* plane_control |= (1<<30); */


	/* Here the settings:
 	 *   If line + pixel dbling, set 21,20 to 01b, and set Horz Multiply
	 *   If line dbling only,    set 21,20 to 11b
 	 *   If pixel dbling only,   set 21,20 to 00b, but set Horz Multiply
 	 *   If no doubling,         set 21,20 to 00b (no Horz Multiply)
 	 * For pixel doubling
 	 *           --> both progressive/interlaced modes
 	 * For Line doubling
 	 *           --> progressive modes only
 	 */

	regs->plane_control &= ~(BIT21|BIT20);
	if (!(timing->flags & IGD_SCAN_INTERLACE)) {
		/* Line doubling in progressive mode requires special bits */
		if (timing->flags & IGD_LINE_DOUBLE) {
			/* BIT 20 for line & pixel doubling*/
			regs->plane_control |= BIT20;
			/* check later, if no pixel doubling, set bit 21 too*/
		}
	}
	if (timing->flags & IGD_PIXEL_DOUBLE) {
		/* For line ONLY doubling, set bit 21 also '1' */
		regs->plane_control |= BIT21;
	}

	kms_mode_get_stride_stereo_vlv(emgd_crtc, &regs->stride, &regs->stereo, 0);


	/* set color depth */
	regs->plane_control &= ~(BIT29|BIT28|BIT27|BIT26);
	switch (IGD_PF_DEPTH(fb_info->igd_pixel_format)) {
	case PF_DEPTH_8:
		regs->plane_control |= BIT27;
		break;
	case PF_DEPTH_16:
		regs->plane_control |= BIT28 | BIT26;
		break;
	default:
	case PF_DEPTH_32:
		regs->plane_control |= BIT28 | BIT27;
		/* TODO:
		 * !!! Honestly, it is a workaround, important for FB_BLEND_OVL
		 * Real format as we see xRGB32 (BIT28 and BIT27 enabled only),
		 * but in fb_info->igd_pixel_format still default format IGD_PF_ARGB32,
		 * which was sent to here from up stair levels ( or from user level)
		 * As we see we always program  only xRGB32 (BIT28 and BIT27 enabled only)
		 * for DEPTH_32. Actually it is a bug */
		fb_info->igd_pixel_format = IGD_PF_xRGB32;

		/*
		 * Turn on alpha (BIT26) only when fb_blend_ovl_on is set; note that
		 * BIT26 is unconditonally cleared before this switch statement.
		 */
		if (emgd_crtc->fb_blend_ovl_on == 1) {
			regs->plane_control |= BIT26;
			fb_info->igd_pixel_format = IGD_PF_ARGB32;
		}
		break;
	}

	if(fb_info->igd_flags & IGD_ENABLE_DISPLAY_GAMMA) {
		regs->plane_control |= (BIT30);
	} else {
		regs->plane_control &= ~(BIT30);
	}

	if(fb_info->bo->tiling_mode != I915_TILING_NONE) {
		regs->plane_control |= (BIT10);
	} else {
		regs->plane_control &= ~BIT10;
	}

	regs->visible_offset = ((emgd_crtc->igd_plane->yoffset * fb_info->base.DRMFB_PITCH) +
			(emgd_crtc->igd_plane->xoffset * IGD_PF_BYPP(fb_info->igd_pixel_format)));

	if(fb_info->bo->tiling_mode != I915_TILING_NONE) {
		regs->tile_offset = (emgd_crtc->igd_plane->xoffset & 0x00000fff) |
							((emgd_crtc->igd_plane->yoffset & 0x00000fff) << 16);
	} else {
		regs->tile_offset = 0;
	}

	regs->base_offset = i915_gem_obj_ggtt_offset(fb_info->bo);
	/* Writing the base offset triggers the register updates */
	EMGD_TRACE_EXIT;

	return 0;
}

static void kms_commit_plane_vlv(emgd_crtc_t *emgd_crtc, void *primary_regs)
{
	struct emgd_plane_regs *regs = (struct emgd_plane_regs *)primary_regs;
	emgd_framebuffer_t  *fb_info;
	igd_timing_info_t   *timing;
	igd_display_plane_t *plane;
	igd_display_pipe_t *pipe;

	EMGD_TRACE_ENTER;

	plane   = PLANE(emgd_crtc);
	pipe    = PIPE(emgd_crtc);
	fb_info = plane->fb_info;

	if (!regs->is_mode_vga) {
		EMGD_DEBUG(" Plane Ctrl :  0x%lx", regs->plane_control);
		EMGD_DEBUG(" Plane Base :  0x%lx", regs->base_offset);
		EMGD_DEBUG(" X,Y Offset :  %d,%d",
			(int)emgd_crtc->igd_plane->xoffset, (int)emgd_crtc->igd_plane->yoffset);
		EMGD_DEBUG(" Plane Pitch:  0x%lx", (unsigned long)fb_info->base.DRMFB_PITCH);

		WRITE_MMIO_REG_VLV(regs->plane_control, regs->mmio, regs->plane_reg);
		WRITE_MMIO_REG_VLV(fb_info->base.DRMFB_PITCH, regs->mmio, regs->plane_reg + DSP_STRIDE_OFFSET);
		WRITE_MMIO_REG_VLV(regs->visible_offset, regs->mmio, regs->plane_reg + DSP_LINEAR_OFFSET);
		WRITE_MMIO_REG_VLV(regs->tile_offset, regs->mmio, regs->plane_reg + DSP_TOFF_OFFSET);

		/* Writing the base offset triggers the register updates */
		WRITE_MMIO_REG_VLV(regs->base_offset, regs->mmio, regs->plane_reg + DSP_START_OFFSET);
		emgd_crtc->active = true;
	} else {
		timing        = pipe->timing;

		/* There is a special case code for legacy VGA modes */
		while (timing->private.extn_ptr) {
			timing = (igd_timing_info_t *)timing->private.extn_ptr;
		}

		kms_program_plane_vga(regs->mmio, timing);
		regs->is_mode_vga = false;
	}

	/* Program the sprite plane for S3D if necessary */
	if(fb_info->igd_flags & IGD_HDMI_STEREO_3D_MODE &&
			fb_info->other_bo[0] && fb_info->other_bo[1]) {
		s3d_program_ovlplane_vlv(emgd_crtc);
	} else {
		/* if other_bo is NULL, sprite is not required.
		 * We should disable the sprite only if it was previously
		 * enable by s3d_program_ovlplane. This is required to ensure
		 * the sprite plane is disable when switching from S3D mode to
		 * normal mode.
		 */
		if(emgd_crtc->sprite1 && emgd_crtc->sprite1->enabled_by_s3d) {
			s3d_disable_ovlplane(emgd_crtc,emgd_crtc->sprite1, 1);
		}
		if(emgd_crtc->sprite2 && emgd_crtc->sprite2->enabled_by_s3d) {
			s3d_disable_ovlplane(emgd_crtc,emgd_crtc->sprite2, 1);
		}
	}

	EMGD_TRACE_EXIT;
}

/*!
 * kms_program_plane_vlv
 *
 * Program the plane.
 *
 * This assumes the plane should be enabled as part of the programming.
 * The set_plane_pwr function above should be used to enable/disable the
 * plane independent of the programming.
 */
int kms_program_plane_vlv(emgd_crtc_t *emgd_crtc, bool preserve)
{
	igd_display_pipe_t  *pipe;
	int ret = 0;

	pipe = PIPE(emgd_crtc);

	EMGD_TRACE_ENTER;

	ret = kms_calc_plane_vlv(emgd_crtc, preserve);
	if (ret) {
		return ret;
	}

	kms_commit_plane_vlv(emgd_crtc, emgd_crtc->primary_regs);	

	kms_set_plane_pwr_vlv(emgd_crtc, TRUE, TRUE);

	EMGD_TRACE_EXIT;
	return 0;
}



/**
 * kms_set_pipe_pwr_vlv
 *
 * Turns the pipe ON or OFF depending on the input
 *
 * @param emgd_crtc (IN) specifies the pipe to change
 * @param enable    (IN) TRUE to enable pipe, FALSE to disable
 *
 * @return
 */
static void kms_set_pipe_pwr_vlv(emgd_crtc_t *emgd_crtc, unsigned long enable)
{
	unsigned long       pipe_conf, dpll_ctrl;
	igd_display_pipe_t *pipe;
	igd_context_t      *context;
	unsigned char      *mmio;

	EMGD_TRACE_ENTER;

	pipe      = PIPE(emgd_crtc);
	mmio      = MMIO(emgd_crtc);
	context   = emgd_crtc->igd_context;
	pipe_conf = READ_MMIO_REG_VLV(mmio, pipe->pipe_reg);
	dpll_ctrl = READ_MMIO_REG_VLV(mmio, pipe->clock_reg->dpll_control);


	/* Do nothing if current power state is same as what we want to set */
	/* The PIPE_ENABLE bit is at bit-position 31 */
	if ( (enable << 31) == (pipe_conf & PIPE_ENABLE) ){
		EMGD_TRACE_EXIT;
		return;
	}

	if (!enable) {
		/* Disable pipe */
		WRITE_MMIO_REG_VLV((pipe_conf & ~PIPE_ENABLE), mmio, pipe->pipe_reg);

		/* Disable DPLL */
		WRITE_MMIO_REG_VLV((dpll_ctrl & ~BIT31),
				mmio, pipe->clock_reg->dpll_control);

		EMGD_DEBUG("Set Pipe Power: OFF");
	} else {
		/* Enable DPLL */
		WRITE_MMIO_REG_VLV((dpll_ctrl | BIT31),
				mmio, pipe->clock_reg->dpll_control);

		/* Enable pipe */
		WRITE_MMIO_REG_VLV(pipe_conf | PIPE_ENABLE, mmio, pipe->pipe_reg);

		EMGD_DEBUG("Set Pipe Power: ON");
	}

	EMGD_TRACE_EXIT;
	return;
}



/*!
 * kms_program_pipe_vlv
 *
 * This function programs the Timing registers and clock registers and
 * other control registers for PIPE.
 *
 * @param display
 * @param status
 *
 * @return void
 */
static void kms_program_pipe_vlv(emgd_crtc_t *emgd_crtc)
{
	unsigned long       timing_reg;
	unsigned long       pipe_conf;
	unsigned long       hactive, vactive;
	igd_timing_info_t  *current_timings = NULL;
	igd_display_port_t *port            = NULL;
	igd_display_pipe_t *pipe            = NULL;
	igd_display_plane_t *plane          = NULL;
	pd_timing_t        *vga_timing      = NULL;

	int            i;
	unsigned int   pipe_num;
	unsigned long  temp;
	unsigned char *mmio;	
	unsigned long dpcd_max_rate = 0;	
	unsigned long link_rate = 0;
	int ret = 0;
	unsigned long timeout, modPhy_ready_mask = 0x0;
	unsigned long dpioreg = 0x0;
	unsigned long preserve = 0xE0000001;

	EMGD_TRACE_ENTER;

	pipe            = PIPE(emgd_crtc);
	port            = PORT(emgd_crtc);
	plane     		= PLANE(emgd_crtc);
	pipe_num        = pipe->pipe_num;
	current_timings = pipe->timing;
	mmio            = MMIO(emgd_crtc);
	pipe_conf       = preserve & READ_MMIO_REG_VLV(mmio, pipe->pipe_reg);

	if (NULL == port) {
		EMGD_ERROR_EXIT("Invalid CRTC selected.");
		return;
	}

	if (pipe_conf & PIPE_ENABLE) {
		EMGD_DEBUG("ERROR: Pipe cannot be enabled while attempting to program it");
	}

	/* Debug messages */
	vga_timing = (pd_timing_t *)current_timings->private.extn_ptr;
	EMGD_DEBUG(
		"current_timings %ux%u mode_number = %u flags = 0x%lx, dclk = %lu",
		current_timings->width,
		current_timings->height,
		current_timings->mode_number,
		current_timings->flags,
		current_timings->dclk);

	if (vga_timing) {
		EMGD_DEBUG(
			"ext_timing %ux%u mode_number = %u flags= 0x%lx, dclk = %lu",
			vga_timing->width,
			vga_timing->height,
			vga_timing->mode_number,
			vga_timing->flags,
			vga_timing->dclk);
	}


	/*
	 * If the mode is VGA and the PD says it handles all VGA modes without
	 * reprogramming then just set the mode and leave centering off.
	 */
	if(current_timings->flags & IGD_MODE_VESA) {
		EMGD_DEBUG("IGD_MODE_VESA");

		if (current_timings->mode_number <= VGA_MODE_NUM_MAX) {
			EMGD_DEBUG("current_timings->mode_number <= VGA_MODE_NUM_MAX");

			program_pipe_vga_vlv(emgd_crtc);

			EMGD_TRACE_EXIT;
			return;
		}
	}

	if (port->pd_driver->type == PD_DISPLAY_DP_INT) { /* DP */
		/* Here we call DP read DPCD. */
		if (port->pd_driver->pipe_info){
			ret = port->pd_driver->pipe_info(
				port->pd_context,
				pipe->timing, 
				1<<pipe_num);

			if (ret != 0){
				EMGD_ERROR("Pre Link Training Error");
			}
		}

    	/* Find UPSCALING attr value*/
    	pi_pd_find_attr_and_value(port,
            PD_ATTR_ID_DPCD_MAX,
            0,/*no PD_FLAG for UPSCALING */
            NULL, /* dont need the attr ptr*/
	        &dpcd_max_rate);

	    if (dpcd_max_rate == DP_LINKBW_1_62_GBPS){
    	     link_rate = RATE_162_MHZ;
        	 EMGD_DEBUG ("dclk = 162000");
    	} else if (dpcd_max_rate == DP_LINKBW_2_7_GBPS) {
        	link_rate = RATE_270_MHZ;
         	EMGD_DEBUG ("dclk = 270000");
    	} else {
        	EMGD_ERROR ("Unsupported link rate.");
    	}

		if (port->pd_driver->pre_link_training){
			ret = port->pd_driver->pre_link_training(
				port->pd_context,
				pipe->timing, 
				1<<pipe_num);

			if (ret != 0){
				EMGD_ERROR("Pre Link Training Error");
			}
		}
		
		switch (port->bits_per_color){
			case IGD_DISPLAY_BPC_16:			
			case IGD_DISPLAY_BPC_14:			
			case IGD_DISPLAY_BPC_12:
				/*Set to max bpc supported by VLV.*/
				port->bits_per_color = IGD_DISPLAY_BPC_10; 			
			case IGD_DISPLAY_BPC_10:			
				pipe_conf |= BIT5;
				pipe_conf &= ~(BIT7|BIT6);
				EMGD_DEBUG ("BPC  is 10 bit");
				break;
			case IGD_DISPLAY_BPC_8:
				pipe_conf &= ~(BIT7|BIT6|BIT5);
				EMGD_DEBUG ("BPC  is 8 bit");
				break;
			default:
				port->bits_per_color = IGD_DISPLAY_BPC_6; 			
			case IGD_DISPLAY_BPC_6:
				pipe_conf |= BIT6;
				pipe_conf &= ~(BIT7|BIT5);
				EMGD_DEBUG ("BPC  is 6 bit");
				break;

		}

		/* UMG: Disable Pipe and reset DP related bits in PIPE */
		/* if plane bpc != pipe/port bpc, dither on */
		
		pipe_conf &= ~BIT(4);		/* Disable Dithering */
		EMGD_DEBUG ("plane pixel format = %lX", plane->fb_info->igd_pixel_format);

		if (IGD_PF_DEPTH((plane->fb_info->igd_pixel_format)) == PF_DEPTH_32){
			if (port->bits_per_color == IGD_DISPLAY_BPC_6){
				pipe_conf |= BIT(4);		/* Enable Dithering */
				pipe_conf &= ~(BIT(3)|BIT(2));		/* Dithering mode: Spatial. */
				EMGD_DEBUG ("Dithering enable with Spatial mode.");	
			}
		}
		pipe_conf &= ~BIT(1);		/* Disable DDAReset */
		EMGD_DEBUG ("DP port is using dpcd_max_rate as link_rate.");
	} else {
		link_rate = current_timings->dclk;
		EMGD_DEBUG ("VGA/HDMI port is using dclk as link_rate.");

	}


	if (link_rate != 0){
		EMGD_DEBUG ("Link Rate is %ld", link_rate);
		/* Program dot clock divisors. */
		program_clock_vlv(emgd_crtc, pipe->clock_reg, link_rate,
				PROGRAM_PRE_PIPE);
		if (port->pd_driver->type == PD_DISPLAY_DP_INT) { /* DP */
			
			unsigned long max_rate;				

			if (!port->pd_driver->link_training){
				return;
			}

			EMGD_DEBUG ("DP: link_rate = %ld", link_rate);
			if (link_rate == RATE_270_MHZ) {
				max_rate = DP_LINKBW_2_7_GBPS;
			} else {
				max_rate = DP_LINKBW_1_62_GBPS;
			}
			
			ret = port->pd_driver->link_training(
				port->pd_context,
				pipe->timing,
				max_rate, 
				1<<pipe_num);

			if (ret != 0){
				/* Link Training fails with the maximum supported link rate, will try with lower link rate */
				if(link_rate == RATE_270_MHZ) {
					/* If the current link rate used is 2.7Gps, we will try with 1.62Gps */
					program_clock_vlv(emgd_crtc, pipe->clock_reg, RATE_162_MHZ, PROGRAM_PRE_PIPE);
					/* MNTU Programmed in dp_program_dpll */
					/* dp_program_mntu_regs(p_ctx, p_mode, flags, DP_LINKBW_1_62_GBPS); */
					ret = port->pd_driver->link_training (port->pd_context, pipe->timing, DP_LINKBW_1_62_GBPS, 1<<pipe_num);
				} 
			}

		}
	} else {
		EMGD_ERROR ("Link Rate is 0. Something is wrong.");
		return;
	}

	/* Program timing registers for the pipe */
	timing_reg = pipe->timing_reg;
	if (current_timings->flags & IGD_PIXEL_DOUBLE) {
		hactive = (unsigned long)current_timings->width * 2 - 1;
	} else {
		hactive = (unsigned long)current_timings->width - 1;
	}

	if (current_timings->flags & IGD_LINE_DOUBLE) {
		vactive = (unsigned long)current_timings->height*2 - 1;
	} else {
		/* For vlv Hardware will automatically divide by 2 to
		   get the number of line in each field */
		vactive = (unsigned long)current_timings->height - 1;
	}

	/*TODO: Check if program_clock is OK before doing this. Else
	 * 	Board will crash.*/
	/* Reset the Palette */
	for (i = 0; i < 256; i++) {
		/* Program each of the 256 4-byte palette entries */
		WRITE_MMIO_REG_VLV(((i<<16) | (i<<8) | i),
			mmio, pipe->palette_reg + (i << 2));
	}

	if (port) {
		/* apply color correction */
		for( i = 0; PD_ATTR_LIST_END != port->attributes[i].id; i++ ) {

			if ((PD_ATTR_ID_FB_GAMMA      == (port->attributes[i].id)) ||
				(PD_ATTR_ID_FB_BRIGHTNESS == (port->attributes[i].id)) ||
				(PD_ATTR_ID_FB_CONTRAST == (port->attributes[i].id)))  {

				kms_set_color_correct_vlv(emgd_crtc);
			}
		}
	}

	/* This workaround is for eDP panels that doesn't support
	 * Standard Timings. With the upscaling flag enabled, the mode list is 
	 * similar to LVDS due to the filtering logic for 1366x768 is the same.
	 * However, all the modes except the Native DTD cannot be set successfully,
	 * showing corruption only. 
	 * Reference to UMG shows that they are always programming the 
	 * Native Timings and only alter the source image size (timing_reg + 1c)
	 * to display the mode the user is setting. Because of this
	 * UMG is capable of setting those standard timings for eDP
	 */

	/* If this is a EDID panel, the native timing will be the first 
	 * in the list, else if this is a EDID-less panel, the user DTD
	 * will be the first in the list as well
	 */

	/* Program Timings */
	temp = (unsigned long)(current_timings->htotal) << 16 | hactive;
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg);

	temp = ((unsigned long) current_timings->hblank_end << 16) |
		(unsigned long)(current_timings->hblank_start);
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x04);

	temp = ((unsigned long)(current_timings->hsync_end) << 16) |
		(unsigned long)(current_timings->hsync_start);
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x08);

	temp = ((unsigned long)(current_timings->vtotal) << 16) | vactive;
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x0C);

	temp = ((unsigned long)(current_timings->vblank_end) << 16) |
		(unsigned long)(current_timings->vblank_start);
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x10);

	temp = ((unsigned long)(current_timings->vsync_end)<< 16) |
		(unsigned long)(current_timings->vsync_start);
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x14);


	/*
	 * If there is a linked mode it is either the VGA or a scaled
	 * mode. If it is scaled then we need to use it as the source size.
	 */
	if(current_timings->private.extn_ptr) {
		igd_timing_info_t *scaled_timings =
			(igd_timing_info_t *)current_timings->private.extn_ptr;

		if((scaled_timings->flags & IGD_MODE_VESA) &&
			(scaled_timings->mode_number <= VGA_MODE_NUM_MAX)) {
			temp = (hactive << 16) | vactive;
			EMGD_DEBUG ("Scaled hactive = %ld, vactive = %ld", hactive, vactive);
		} else {
			EMGD_DEBUG(
				"scaled_timings->width [%d], scaled_timings->height [%d]",
				scaled_timings->width, scaled_timings->height);

			temp = (unsigned long)scaled_timings->width - 1;
			temp = (temp << 16) |
				(unsigned long)(scaled_timings->height - 1);
		}
	} else {
		EMGD_DEBUG ("hactive = %ld, vactive = %ld", hactive, vactive);
		temp = (hactive << 16) | vactive;
	}
	WRITE_MMIO_REG_VLV(temp, mmio, timing_reg + 0x1C);


	/* Put pipe in interlaced mode if requested:
	 *     should only happen for LVDS display if at all. */
	if (current_timings->flags & IGD_SCAN_INTERLACE) {
		pipe_conf |= (INTERLACE_EN);
	} else {
		pipe_conf &= ~(INTERLACE_EN);
	}

	/* Enable video on DisplayPort VLV spec.
	 * 0 = Video + Audio or Video only
	 * 1 = Audio only
	 */
	pipe_conf &= ~BIT26;

	/* Apply full color range VLV only */
	pipe_conf &= ~BIT13;

	/*
	 * VLV pipe config register has new bit definition to disable
	 * Display/Overlay/Cursor plane on the fly. VBIOS may have set
	 * these bits to disable the planes for inactive pipe, which may
	 * cause blank display when the pipe is active again if these bits
	 * are not reset. These bits must be set to zero for normal operation
	 */
	pipe_conf &= ~BIT19;
	pipe_conf &= ~BIT18;

	/* Before turning on the pipe, lets disable the GUnit display pipeline
	 * clock gating. This is a known silicon workaround.
	 * FIXME - See if we still need this workaround for VLV2
	 */
	temp = READ_MMIO_REG_VLV(mmio, GUNIT_CLK_GATING_DISABLE1);
	temp |= BIT12;
	WRITE_MMIO_REG_VLV(temp, mmio, GUNIT_CLK_GATING_DISABLE1);

	/*Before enabling PIPE. We need to make sure DPIO PhyStatus Ready.*/
	if (port->port_type == IGD_PORT_DIGITAL){	
		timeout = OS_SET_ALARM(200);
		if ((port->port_number == IGD_PORT_TYPE_SDVOB)||
			(port->port_number == IGD_PORT_TYPE_DPB)){
			EMGD_DEBUG ("PortB");
			modPhy_ready_mask = (BIT3|BIT2|BIT1|BIT0);
		} else if ((port->port_number == IGD_PORT_TYPE_SDVOC)||
   	   		(port->port_number == IGD_PORT_TYPE_DPC)) {
			EMGD_DEBUG ("PortC");
			modPhy_ready_mask = (BIT7|BIT6|BIT5|BIT4);
		}
		do {
			dpioreg = READ_MMIO_REG_VLV(mmio, 0x6014);
		}while( (dpioreg & (modPhy_ready_mask)) && !(OS_TEST_ALARM(timeout)) );
	
		EMGD_DEBUG ("DPIOREG = 0x%lX", dpioreg);
	
		if (dpioreg & modPhy_ready_mask){
			EMGD_ERROR ("Timeout occured waiting for ModPhy ready.");
		}
	}

	/* We may not want to actually enable the pipe here as CRTC can get enabled
	 * via a different KMS interface */
	pipe_conf |= PIPE_ENABLE;

	EMGD_DEBUG("PIPE_CONF: 0x%lx = 0x%lx", pipe->pipe_reg, pipe_conf);
	WRITE_MMIO_REG_VLV(pipe_conf, mmio, pipe->pipe_reg);

	/*
	 * Set the VGA address range to 0xa0000 so that a normal (not VGA)
	 * mode can be accessed through 0xa0000 in a 16bit world.
	 */
	WRITE_AR(mmio, 0x10, 0xb);
	WRITE_VGA(mmio, GR_PORT, 0x06, 0x5);
	WRITE_VGA(mmio, GR_PORT, 0x10, 0x1);

	if(current_timings->private.extn_ptr) {
		/* This means either internal scaling (LVDS) or centered VGA */
		current_timings = current_timings->private.extn_ptr;
		if(current_timings->private.extn_ptr) {
			/* This is both the scaled and centered VGA */
			current_timings = current_timings->private.extn_ptr;
		}
		if (current_timings->flags & IGD_MODE_VESA) {
			if (current_timings->mode_number <= VGA_MODE_NUM_MAX) {
				program_pipe_vga_vlv(emgd_crtc);
			} else {
				set_256_palette(mmio);
			}
		}
	}

	EMGD_TRACE_EXIT;
	return;
}



/*!
 * Status is currently not used
 *
 * @param display
 * @param port_number
 * @param status
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int kms_post_program_port_vlv(emgd_encoder_t * emgd_encoder,
	unsigned long status)
{
	int ret;
	igd_display_port_t *port	  = NULL;
	igd_display_pipe_t *pipe      = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;
	igd_timing_info_t  *timings   = NULL;
	unsigned char      *mmio      = NULL;
	/*unsigned long    portreg; */
	/* os_alarm_t         timeout; */
	/* unsigned long dc; */
	

	/*unsigned long pt = PORT_TYPE(display);*/

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

static int kms_program_port_vlv(emgd_encoder_t *emgd_encoder,
		unsigned long status)
{	
	igd_display_port_t *port      = NULL;
	int ret;

	EMGD_TRACE_ENTER;
	
	port = emgd_encoder->igd_port;

	if(port->port_type==IGD_PORT_ANALOG){
		ret = program_port_analog_vlv(emgd_encoder, status);
	}else if(port->port_type==IGD_PORT_DIGITAL){
		ret = program_port_digital_vlv(emgd_encoder, status);
	} else {
		ret = -IGD_ERROR_INVAL;
	}

	EMGD_TRACE_EXIT;
	return ret;

}



/*!
 * kms_get_vblank_counter_vlv
 *
 * This function returns the vblank counter number back to the caller.  If
 * the pipe is off, then this function returns 0.
 *
 * @param emgd_crtc [IN] The pipe to get frame value from
 *
 * @return 0 frame number, which can also be used as a vblank counter number
 */
static u32 kms_get_vblank_counter_vlv(emgd_crtc_t *emgd_crtc)
{
	unsigned long       high1, high2, low;
	struct drm_device  *dev;
	unsigned long       frame_high_reg, frame_low_reg;
	igd_context_t      *context;
	unsigned long       pipe_conf;
	igd_display_pipe_t *pipe;
	unsigned char      *mmio;


	dev     = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context = ((drm_emgd_private_t *)dev->dev_private)->context;
	pipe    = PIPE(emgd_crtc);
	mmio    = context->device_context.virt_mmadr;

	switch (emgd_crtc->igd_pipe->pipe_features & IGD_PORT_MASK) {
		case IGD_PORT_SHARE_DIGITAL:
			frame_high_reg = PIPEB_FRAME_HIGH;
			frame_low_reg  = PIPEB_FRAME_PIXEL;
			break;

		case IGD_PORT_SHARE_LVDS:
		default:
			frame_high_reg = PIPEA_FRAME_HIGH;
			frame_low_reg  = PIPEA_FRAME_PIXEL;
			break;
	}

	/* Only read registers if pipe is enabled */
	pipe_conf = READ_MMIO_REG_VLV(mmio, pipe->pipe_reg);
	if (0 == (pipe_conf & PIPE_ENABLE)) {
		EMGD_DEBUG("Cannot get frame count while pipe is off");
		return 0;
	}

	/*
	 * High and low register fields are not synchronized so it is possible
	 * that our low value is actually not from the same high value, e.g.
	 * going from "99" to "00" when transitioning 499 to 500.  To get
	 * around this, we will read until the two reads of high values stay
	 * the same.
	 */
	do {
		high1 = READ_MMIO_REG_VLV(mmio, frame_high_reg);
		high1 &= PIPE_FRAME_HIGH_MASK;

		low   = READ_MMIO_REG_VLV(mmio, frame_low_reg);
		low   &= PIPE_FRAME_LOW_MASK;

		high2 = READ_MMIO_REG_VLV(mmio, frame_high_reg);
		high2 &= PIPE_FRAME_HIGH_MASK;
	} while (high1 != high2);


	/* Frame count low is located at bits 24-31 */
	low >>= PIPE_FRAME_LOW_SHIFT;

	/* Low value is 8 bits long, so shift high by 8 bits */
	return (high1 << 8) | low;
}



static int program_port_analog_vlv(emgd_encoder_t * emgd_encoder,
		unsigned long status)
{
	unsigned long port_control;
	unsigned long pipe_number;
	unsigned long pd_powerstate = PD_POWER_MODE_D3;
	unsigned long preserve = 0;
	igd_display_port_t *port      = NULL;
	igd_context_t      *context   = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;
	igd_display_pipe_t *pipe      = NULL;
	unsigned char      *mmio      = NULL;
	pd_port_status_t   port_status = { 0 };
	int ret = 0;

	EMGD_TRACE_ENTER;

	port = emgd_encoder->igd_port;
	emgd_crtc = CRTC_FROM_ENCODER(emgd_encoder);
	if(emgd_crtc == NULL){
		EMGD_DEBUG("emgd_crtc is NULL");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}
	pipe = PIPE(emgd_crtc);
	context = emgd_crtc->igd_context;
	mmio = context->device_context.virt_mmadr;

	preserve = 0x3C80FFE5;

	/* port driver would have programmed BIT1 correctly
	 * for us, so preserve this here */

	port_control = preserve & READ_MMIO_REG_VLV(mmio, port->port_reg);

	if(status == FALSE) {
		port_control &= ~BIT31; /* VLV - No(H/V Sync) ACPI states */

		WRITE_MMIO_REG_VLV(port_control, mmio, port->port_reg);

		EMGD_TRACE_EXIT;
		return 0;
	}

	if(!(port->pt_info->flags & IGD_DISPLAY_ENABLE)) {
		EMGD_TRACE_EXIT;
		return 0;
	}

	pipe_number = pipe->pipe_num;
	port_control &= ~PORT_PIPE_SEL;
	port_control |= (pipe_number << PORT_PIPE_SEL_POS);
	
	if(pipe->timing->flags & IGD_VSYNC_HIGH) {
		port_control |= (1L<<4);
	}
	if(pipe->timing->flags & IGD_HSYNC_HIGH) {
		port_control |= (1L<<3);
	}

	/* Both IGD_ powerstates and PD_ powermodes have
	 * same definitions */
	pd_powerstate =
			(context->device_context.power_state > port->power_state) ?
			 context->device_context.power_state : port->power_state;

	switch(pd_powerstate) {
		case IGD_POWERSTATE_D0:

			/* if hot-plug and display is detached, don't mode set or enable port. */
			ret = port->pd_driver->pd_get_port_status(port->pd_context, &port_status);
			if ((ret == PD_SUCCESS) &&
				(config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) &&
				(port_status.connected == PD_DISP_STATUS_DETACHED)) {
					port_control &= ~BIT31;
			} else {
				port_control |= BIT31;
				ret = port->pd_driver->set_mode(port->pd_context, pipe->timing, 0);
				if (ret) {
					EMGD_ERROR_EXIT("PD set_mode returned: 0x%x", ret);
					return -IGD_ERROR_INVAL;
				}
			}
			break;
		case IGD_POWERSTATE_D1:
			break;
		case IGD_POWERSTATE_D2:
			break;
		case IGD_POWERSTATE_D3:
			break;
		default:
			EMGD_ERROR_EXIT("Invalid power state: 0x%lx",pd_powerstate);
			return -IGD_ERROR_INVAL;
	}

	EMGD_DEBUG("Port_control: 0x%lx = 0x%lx", port->port_reg, port_control);
	WRITE_MMIO_REG_VLV(port_control, mmio, port->port_reg);

	EMGD_TRACE_EXIT;

	return 0;
}

/* Disable HDMI port.
 *
 * Valleyview2 Display Registers specification has below comment for BIT31
 * of HDMI-B/HDMI-C register:
 * [DevIBX] When disabling the port, software must temporarily enable the
 * port with transcoder select (bit #30) cleared to ‘0’ after disabling the
 * port. This is workaround for hardware issue where the transcoder select
 * set to ‘1’ will prevent DPB/DPC from being enabled on transcoder A. 
 */
static void disable_hdmi_for_dp_vlv(unsigned char *mmio,
        unsigned long reg, unsigned long pipe_reg)
{
	unsigned long port_control;

	EMGD_TRACE_ENTER;

	port_control = READ_MMIO_REG_VLV(mmio, reg);

	/* Set BIT31 of HDMI port control register to 0, disable the port.
	 * Refer to i915 intel_disable_hdmi function, set BIT6 to 0, 
	 * disable audio output on this port, just in case. */
	port_control &= ~(BIT31 | BIT6);

	if( port_control & BIT30 )
	{
		port_control &= ~BIT30;
		WRITE_MMIO_REG_VLV(port_control, mmio, reg);
		READ_MMIO_REG_VLV(mmio, reg);

		/* Transcoder selection bits only update effectively on vblank.
		 * Wait for next vblank. If pipe is off then just delay 50 ms */
		if((1L<<31) & READ_MMIO_REG_VLV(mmio, pipe_reg))
			wait_for_vblank_vlv(mmio, pipe_reg);
		else
			mdelay(50);

		/* HW workaround. Valleyview2 Display Registers specification
		 * has below comment for BIT31 of HDMI-B/HDMI-C register:
		 * [DevIBX] Software must write this bit twice when enabling
		 * the port (setting to ‘1’) as a workaround for hardware
		 * issue that may result in first write getting masked. */
		WRITE_MMIO_REG_VLV((port_control | BIT31), mmio, reg);
		READ_MMIO_REG_VLV(mmio, reg);
		WRITE_MMIO_REG_VLV((port_control | BIT31), mmio, reg);
		READ_MMIO_REG_VLV(mmio, reg);
	}

	WRITE_MMIO_REG_VLV(port_control, mmio, reg);
	READ_MMIO_REG_VLV(mmio, reg);

	EMGD_TRACE_EXIT;
}

static int program_port_digital_vlv(emgd_encoder_t *emgd_encoder,
		unsigned long status)
{
	unsigned long pipe_number;
	unsigned long port_control, other_port_control = 0;
	unsigned long pd_powerstate = PD_POWER_MODE_D3;
	unsigned long preserve = 0;
	unsigned long upscale = 0;
	igd_timing_info_t  local_timing;
	igd_timing_info_t  *timing	= NULL;
	igd_display_port_t *port      = NULL;
	igd_context_t      *context   = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;
	igd_display_pipe_t *pipe      = NULL;
	unsigned char      *mmio      = NULL;
	int ret = 0;

	EMGD_TRACE_ENTER;

	port = emgd_encoder->igd_port;
	emgd_crtc = CRTC_FROM_ENCODER(emgd_encoder);
        if(emgd_crtc == NULL){
                EMGD_DEBUG("emgd_crtc is NULL");
                EMGD_TRACE_EXIT;
                return -IGD_ERROR_INVAL;
        }
	pipe = PIPE(emgd_crtc);
	context = emgd_crtc->igd_context;
	timing = pipe->timing;
	mmio = context->device_context.virt_mmadr;

	EMGD_DEBUG("Program Port: (%s)", status?"ENABLE":"DISABLE");
	EMGD_DEBUG("pd_flags: 0x%lx", port->pd_flags);

	port_control = preserve & READ_MMIO_REG_VLV(mmio, port->port_reg);

	/* If status is false, quickly disable the display */
	if(status == FALSE) {
		ret = port->pd_driver->set_power(
			port->pd_context,
			PD_POWER_MODE_D3);
		if (ret) {
			EMGD_ERROR_EXIT("PD set_power() returned: 0x%x", ret);
			return -IGD_ERROR_INVAL;
		}

		WRITE_MMIO_REG_VLV(port_control, mmio, port->port_reg);

		EMGD_TRACE_EXIT;
		return 0;
	}

	if(! (port->pt_info->flags & IGD_DISPLAY_ENABLE)) {
		EMGD_TRACE_EXIT;
		return 0;
	}

	/*TODO: Code below does not make sense.*/

	/*
	 * Added VGA SYNCS
	 */
	/* TODO: Check for vga_sync flag?*/
	if(pipe->timing->flags & IGD_VSYNC_HIGH) {
		port_control |= (1L<<4);
	}
	if(pipe->timing->flags & IGD_HSYNC_HIGH) {
		port_control |= (1L<<3);
	}

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

		if (pd_powerstate == IGD_POWERSTATE_D0){ 
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

			/* in Gen4 B-Specs, there are no bits,  *
			 * for data ordering/format for DVO data */
			pipe_number = pipe->pipe_num;
			port_control |= (pipe_number<<30);

			/* This is used to tell the port driver what pipe the port is connected to */
			port->callback->current_pipe = pipe->pipe_num;

			/* Note: HDMI and DP are sharing the same lanes, thus cannot be enabled
			 * simultaneously. Turning off the SDVO/HDMI B is DP B is being programmed or 
 			 * vice versa */
			if(port->pd_driver->type == PD_DISPLAY_DP_INT){
				/* Check if HDMIB and DPB is enabled simultaneously
				 * or HDMIC and DPC is enabled simultaneously. */
				if (port->port_reg == DPBCNTR) {
					/* Turn off SDVO/HDMI B if DP B is being programmed */
					disable_hdmi_for_dp_vlv(mmio, SDVOBCNTR, pipe->pipe_reg);
				} else if (port->port_reg == DPCCNTR) {
					/* Turn off SDVO/HDMI C if DP C is being programmed */
					disable_hdmi_for_dp_vlv(mmio, SDVOCCNTR, pipe->pipe_reg);
				} else {
					EMGD_ERROR_EXIT("Unidentified DisplayPort: Port %x", port->port_number);
					return -IGD_ERROR_INVAL;
				}

				/* Lane Count and Enhanced Framing is already set through dp_set_power */
				port_control = READ_MMIO_REG_VLV(mmio, port->port_reg);

				port_control &= ~PORT_PIPE_SEL;
				port_control |= (pipe_number << PORT_PIPE_SEL_POS);

				/* Link Training Pattern 1 Enabled*/
				port_control &= ~(BIT(29)|BIT(28));

				
				/* Enable audio for any DP port being programmed. It could be a case that
				 * audio on both DP port enabled in dual display. */
				EMGD_DEBUG("Enabling audio on 0x%lx", port->port_reg);
				port_control |= BIT(6);
				
				/* Digital Display B/C Detected (Default) */
				/* port_control |= BIT2; this is read only for VLV*/

			}else if(port->pd_driver->type == PD_DISPLAY_HDMI_INT){
				/* Check if HDMIB and DPB is enabled simultaneously
				   or HDMIC and DPC is enabled simultaneously. */
				if (port->port_reg == SDVOBCNTR) {
					/* Turn off DP B if SDVO/HDMI B is being programmed */
					other_port_control = READ_MMIO_REG_VLV(mmio, DPBCNTR);
					WRITE_MMIO_REG_VLV((other_port_control & ~BIT31), mmio, DPBCNTR);
				} else if (port->port_reg == SDVOCCNTR) {
					/* Turn off DP C if SDVO/HDMI C is being programmed */
					other_port_control = READ_MMIO_REG_VLV(mmio, DPCCNTR);
					WRITE_MMIO_REG_VLV((other_port_control & ~BIT31), mmio, DPCCNTR);
				} else {
					EMGD_ERROR_EXIT("Unidentified HDMI: Port %x", port->port_number);
					return -IGD_ERROR_INVAL;
				}


				/* Enable audio for any HDMI port being programmed. It could be a case that
				 * audio on both HDMI port enabled in dual display. */
				EMGD_DEBUG("Enabling audio on 0x%lx",port->port_reg);
				/* Enable audio */
				port_control |= BIT(6);
				/* Enable NULL Infoframes */
				port_control |= BIT(9);

				/* Set to TMDS encoding */
				port_control |= BIT(11);
				port_control &= (~BIT(10));

				/* enable the border */
				/* Centering would not work if this is disabled */
				port_control |= BIT7;

				/* disable the stall */
				/*FIXME: SPEC save reserved.*/
				port_control &= (~BIT29);
			}else{
				/*FIXME: What is this for?*/
				/* enable the border */
				/* Do we need to disable the SDVO border *
				 * for native VGA timings(i.e., use DE)? */
				port_control |= BIT7;

				/* enable the stall */
				port_control |= BIT29;
			}
			/* enable the register */
			port_control |= (BIT31);
		}
	}

	EMGD_DEBUG("Port_control: 0x%lx = 0x%lx", port->port_reg, port_control);

	WRITE_MMIO_REG_VLV(port_control, mmio, port->port_reg);

	EMGD_TRACE_EXIT;
	return 0;
}



/*!
 * kms_disable_vga_vlv
 *
 * Disables the VGA plane
 *
 * @param context [IN] Display context
 *
 * @return None
 */
static void kms_disable_vga_vlv(igd_context_t *context)
{
	unsigned char *mmio;
	unsigned long vga_ctrl;
	unsigned char sr01;


	EMGD_TRACE_ENTER;
	
	mmio = context->device_context.virt_mmadr;

	/*
	 * Disable VGA plane if it is enabled.
	 *
	 * Before modifying the VGA control register, the SR01 bit 5 must
	 * be set and wait 30us after it's set.
	 *
	 * The VGA plane is enabled when bit31 is cleared and disabled when
	 * bit31 is set.  This is backwards from the other plane controls.
	 */
	vga_ctrl = READ_MMIO_REG_VLV(mmio, VGACNTRL);
	if ((vga_ctrl & BIT31) == 0) {
		READ_VGA(mmio, SR_PORT, SR01, sr01); /* Read SR01 */
		WRITE_VGA(mmio, SR_PORT, SR01, sr01|BIT(5)); /* Turn on SR01 bit 5 */

		udelay(30); /* Wait for 30us */

		vga_ctrl |= BIT31;  /* set bit 31 to disable */
		vga_ctrl &= ~BIT30; /* clear bit 30 so VGA display is in normal size */
		WRITE_MMIO_REG_VLV(vga_ctrl, mmio, VGACNTRL);
	}

	EMGD_TRACE_EXIT;
}



/*!
 * kms_program_cursor_vlv
 *
 * This function programs the cursor registers
 *
 * @param display
 * @param status
 *
 * @return void
 */
static void kms_program_cursor_vlv(emgd_crtc_t *emgd_crtc,
	unsigned long status)
{
	unsigned long cursor_reg;
	unsigned long cursor_control = 0x00000000;
	unsigned long cursor_pos;
	unsigned long cursor_base;
	igd_cursor_info_t *cursor_info;
	int i;
	igd_context_t *context;
	unsigned char *mmio;

	context = (igd_context_t *)emgd_crtc->igd_context;
	mmio = context->device_context.virt_mmadr;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("Program Cursor: %s", status?"ENABLE":"DISABLE");

	cursor_reg = emgd_crtc->igd_cursor->cursor_reg;
	cursor_info = emgd_crtc->igd_cursor->cursor_info;

	/* Turn off cursor before changing anything */
	cursor_base = READ_MMIO_REG_VLV(mmio, cursor_reg + CUR_BASE_OFFSET);

	WRITE_MMIO_REG_VLV(cursor_control, mmio, cursor_reg);
	WRITE_MMIO_REG_VLV(cursor_base, mmio, cursor_reg + CUR_BASE_OFFSET);

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
		/* The cursor on VLV is in graphics memory */
		cursor_base = cursor_info->argb_offset;
		break;
	default:
		/* The cursor on VLV is in graphics memory */
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


	WRITE_MMIO_REG_VLV(cursor_pos, mmio, cursor_reg + CUR_POS_OFFSET);

	for(i=0; i<4; i++) {
		WRITE_MMIO_REG_VLV(cursor_info->palette[i], mmio, cursor_reg +
				CUR_PAL0_OFFSET + i * 4);
	}

	cursor_control = cursor_control | emgd_crtc->igd_pipe->pipe_num<<28;

	WRITE_MMIO_REG_VLV(cursor_control, mmio, cursor_reg);
	WRITE_MMIO_REG_VLV(cursor_base, mmio, cursor_reg + CUR_BASE_OFFSET);

	EMGD_TRACE_EXIT;
}


/*!
 * kms_alter_cursor_pos_vlv
 *
 * This function alters the position parameters associated with a cursor.
 *
 * @param display_handle
 * @param cursor_info
 *
 * @return 0
 */
static int kms_alter_cursor_pos_vlv(emgd_crtc_t *emgd_crtc)
{
	unsigned long cursor_reg;
	unsigned long new_pos;
	unsigned long cursor_base;
	unsigned char * mmio;

	cursor_reg = CURSOR(emgd_crtc)->cursor_reg;
	mmio = MMIO(emgd_crtc);

	/* To Fast For Tracing */

	/* To Fast For Tracing */

	if (0x27 & READ_MMIO_REG_VLV(mmio, cursor_reg)) {
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
			READ_MMIO_REG_VLV(mmio, cursor_reg + CUR_BASE_OFFSET);

		WRITE_MMIO_REG_VLV(new_pos, mmio, cursor_reg + CUR_POS_OFFSET);
		WRITE_MMIO_REG_VLV(cursor_base, mmio, cursor_reg + CUR_BASE_OFFSET);
	}


	return 0;
}



/*!
 * This function...
 *
 * @param display_handle
 * @param cursor_info
 *
 * @return 0
 */
static void kms_set_display_base_vlv(emgd_crtc_t *emgd_crtc,
		emgd_framebuffer_t *fb)
{
	unsigned long base;
	unsigned long ctrl;
	unsigned char *mmio;

	EMGD_TRACE_ENTER;

	mmio      = MMIO(emgd_crtc);
	ctrl = READ_MMIO_REG_VLV(mmio, PLANE(emgd_crtc)->plane_reg);

	/* Panning with tiled vs. untiled memory is handled differently
	 * in the Gen7 chipset
	 */
	if (fb->bo->tiling_mode != I915_TILING_NONE) {
		EMGD_DEBUG("Pan tiled");

		base = (emgd_crtc->igd_plane->xoffset & 0x00000fff) | ((emgd_crtc->igd_plane->yoffset & 0x00000fff) << 16);

		WRITE_MMIO_REG_VLV(base, mmio,
				PLANE(emgd_crtc)->plane_reg + DSP_TOFF_OFFSET);
		ctrl |= BIT10;

	} else {
		EMGD_DEBUG ("Pan linear");

		base = ((emgd_crtc->igd_plane->yoffset * fb->base.DRMFB_PITCH) +
				(emgd_crtc->igd_plane->xoffset * IGD_PF_BYPP(fb->igd_pixel_format)));

		WRITE_MMIO_REG_VLV(base, mmio,
				PLANE(emgd_crtc)->plane_reg + DSP_LINEAR_OFFSET);
		ctrl &= ~BIT10;
	}

	WRITE_MMIO_REG_VLV(ctrl, mmio, PLANE(emgd_crtc)->plane_reg);
	WRITE_MMIO_REG_VLV(fb->base.DRMFB_PITCH, mmio,
			PLANE(emgd_crtc)->plane_reg + DSP_STRIDE_OFFSET);
	WRITE_MMIO_REG_VLV(i915_gem_obj_ggtt_offset(fb->bo), mmio,
			PLANE(emgd_crtc)->plane_reg + DSP_START_OFFSET);

	PLANE(emgd_crtc)->fb_info = fb;

	//wait_for_vblank_vlv(MMIO(emgd_crtc), PIPE(emgd_crtc)->pipe_reg);

	EMGD_TRACE_EXIT;
}



/*!
 * kms_queue_flip_vlv
 *
 * Queues a flip instruction in the ring buffer.  This implementation
 * only works for full-screen flips.
 *
 * @param emgd_crtc [IN]  CRTC containing the buffer to flip
 * @param emgd_fb
 */
static int kms_queue_flip_vlv(emgd_crtc_t *emgd_crtc,
           	emgd_framebuffer_t *emgd_fb, 
          	struct drm_i915_gem_object *bo,
           	int plane_select)
{
	struct drm_device  *dev;
	drm_emgd_private_t *dev_priv;
	int                 ret;
	uint32_t            pitch, offset;

	struct intel_ring_buffer *ring;


	EMGD_TRACE_ENTER;

	dev      = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	dev_priv = (drm_emgd_private_t *) dev->dev_private;
	ring     = &dev_priv->ring[BCS];

	ret = intel_pin_and_fence_fb_obj(dev, bo, ring);
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
	if (emgd_crtc->freeze_state) {
		emgd_crtc->freeze_crtc = emgd_crtc;
		emgd_crtc->freeze_fb = emgd_fb;
		emgd_crtc->freeze_pitch = emgd_crtc->base.fb->DRMFB_PITCH;

		/* Sustitue our temporary frame buffer */
		pitch  = emgd_crtc->freezed_pitch | emgd_crtc->freeze_bo->tiling_mode;
		offset = i915_gem_obj_ggtt_offset(emgd_crtc->freeze_bo);

		/*
		 * If the surface that was copied into the temporary
		 * buffer was tiled, then our temporary buffer looks like
		 * it is tiled too.  The freezed_tiled flag is the indication
		 * that the instruction should set the tiled bit.
		 */
		pitch = emgd_crtc->freezed_pitch | emgd_crtc->freezed_tiled;
	} else {
       	if (emgd_fb->igd_flags & IGD_HDMI_STEREO_3D_MODE) {
       		if (plane_select == SPRITE_A || plane_select == SPRITE_C ) {
       			pitch = emgd_crtc->base.fb->pitches[1] | bo->tiling_mode;
       		}
       		else if (plane_select == SPRITE_B || plane_select == SPRITE_D) {
       			pitch = emgd_crtc->base.fb->pitches[2] | bo->tiling_mode;
       		}
       		else {
       			pitch = emgd_crtc->base.fb->pitches[0] | bo->tiling_mode;
       		}
        } else {
           	pitch = emgd_crtc->base.fb->DRMFB_PITCH | bo->tiling_mode;
       	}
	offset = i915_gem_obj_ggtt_offset(bo);
	}

#ifdef DONT_USE_MI_FOR_FLIP
	kms_set_display_base_vlv(emgd_crtc, emgd_fb);
#else

	ret = BEGIN_LP_RING(4);
	if(ret) {
		EMGD_ERROR_EXIT("Failed to allocate space on the ring for display flip!!");
		return ret;
	}

	OUT_RING(MI_DISPLAY_FLIP_I915 | MI_DISPLAY_FLIP_PLANE(plane_select));
	OUT_RING(pitch);
	OUT_RING(offset);
	OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();
#endif /* DONT_USE_MI_FOR_FLIP*/

	EMGD_TRACE_EXIT;

	return ret;
}



/*!
 *
 * im_reset_plane_pipe_ports_vlv
 *
 * Resets all the planes, pipes and ports
 * for this chips display pipelines
 *
 * @param context
 *
 * @return void
 */
static void im_reset_plane_pipe_ports_vlv(igd_context_t *context)
{
	igd_display_plane_t *plane;
	igd_display_pipe_t *pipe;
	igd_display_port_t *port;
	unsigned long temp;
	unsigned long i;
	unsigned char *mmio;
	igd_module_dispatch_t *module_dispatch;
	unsigned long lvdsInPortOrder = 0;
	unsigned long dpbInPortOrder = 0;
	unsigned long dpcInPortOrder = 0;

/*
	platform_context_vlv_t *platform =
		(platform_context_vlv_t *)context->platform_context;
	int j=(PIPE(display)->pipe_features & IGD_PIPE_IS_PIPEB)? 1:0;
*/
	//igd_display_context_t *display=(igd_display_context_t *)context;

	EMGD_TRACE_ENTER;

	/*
	 * Disable all plane, pipe and port registers because the
	 * bios may have been using a different set. Only unset the
	 * enable bit.
	 */
	mmio = context->device_context.virt_mmadr;
	module_dispatch = &context->module_dispatch;

	/* Only if quickboot port order only flag is enabled, then check port order to determine
	 * ports that are to be enabled. Will skip action for ports that are not in port order list.
	 */
	if(config_drm->hal_params[drm_emgd_configid-1].quickboot & IGD_QB_PORT_ORDER_ONLY)
	{
		for(i = 0; i < IGD_MAX_PORTS; i++)
		{
			switch(config_drm->hal_params[drm_emgd_configid-1].port_order[i])
			{
			case IGD_PORT_TYPE_LVDS:
				lvdsInPortOrder = 1;
				break;

			case IGD_PORT_TYPE_DPB:
				dpbInPortOrder = 1;
				break;

			case IGD_PORT_TYPE_DPC:
				dpcInPortOrder = 1;
				break;

			case IGD_PORT_TYPE_TV:
			case IGD_PORT_TYPE_SDVOB:
			case IGD_PORT_TYPE_SDVOC:
			case IGD_PORT_TYPE_ANALOG:
			default:
				break;
			}
		}
	}
	else { /* by default all are assume in port order */
		lvdsInPortOrder = dpbInPortOrder = dpcInPortOrder  = 1;
	} /* end of quickboot & IGD_QB_PORT_ORDER_ONLY */

	/* Turn off LVDS and SDVO ports */
	port = NULL;
	while((port = module_dispatch->dsp_get_next_port(context, port, 0))
			!= NULL) {
#ifndef CONFIG_FASTBOOT
		if (port->pd_driver) {
			port->pd_driver->set_power(port->pd_context, IGD_POWERSTATE_D3);
		}
#endif

		/* DisplayPort Specific Link Training Settings during Port Disabling*/
		if (((port->port_reg == DPBCNTR) && dpbInPortOrder) ||
		    ((port->port_reg == DPCCNTR) && (dpcInPortOrder))){
			if(port->port_reg == DPCCNTR) /* e-DP and DP-C are shared so only need to perform once */
			{
				dpcInPortOrder  = 0;
			}
			temp = READ_MMIO_REG_VLV(mmio, port->port_reg);
			temp &= ~BIT28;     /* Idle Pattern BIT29:28 = 10 */
			temp |= BIT29;
			WRITE_MMIO_REG_VLV(temp, mmio, port->port_reg);

			/* UMG: Delay 17ms */
			mdelay(17);

			temp = READ_MMIO_REG_VLV(mmio, port->port_reg);
			temp |= (BIT29|BIT28);  /* Link Not in Training = 11*/
			WRITE_MMIO_REG_VLV((temp & ~BIT31), mmio, port->port_reg);

		}	else {
			temp = READ_MMIO_REG_VLV(mmio, port->port_reg);
			WRITE_MMIO_REG_VLV((temp & ~BIT31), mmio, port->port_reg);
		}
	
		if (port->pd_driver) {
				port->pd_driver->set_power(port->pd_context, IGD_POWERSTATE_D3);
		}
		/* Program DDI0/DDI1 Tx Lane Resets set to default
		 *  (w/a for HDMI turn off flicker issue). This
		 *   will be re-checked on VLV A0 post power on.
		 */
		if (port->port_number == IGD_PORT_TYPE_SDVOB) {
			//kms_write_dpio_register(mmio, 0x8200, 0x00010080);
			//kms_write_dpio_register(mmio, 0x8204, 0x00600060);
			WRITE_MMIO_REG_VLV(0x00010080,mmio,0x8200);
			WRITE_MMIO_REG_VLV(0x00600060,mmio,0x8204);
		}
		if (port->port_number == IGD_PORT_TYPE_SDVOC) {
			//kms_write_dpio_register(mmio, 0x8400, 0x00010080);
			//kms_write_dpio_register(mmio, 0x8404, 0x00600060);
			WRITE_MMIO_REG_VLV(0x00010080,mmio,0x8400);
			WRITE_MMIO_REG_VLV(0x00600060,mmio,0x8404);
		}
	}

	/*
	 * Gen4 appears to require that plane B be shut off prior to
	 * shutting off plane A.  The normal get_next_plane returns them
	 * in order.  We need to read the list backwards.
	 */
	plane = NULL;
	while ((plane = module_dispatch->dsp_get_next_plane(context, plane, 1))
			!= NULL) {
		/*  This section only deals with display planes.
		 *  Leave cursor, VGA, overlay, sprite planes alone since they will
		 *  need a different disable bit/sequence.
		 */
		temp = READ_MMIO_REG_VLV(mmio, plane->plane_reg);
		if ((plane->plane_features & IGD_PLANE_DISPLAY)) {
			unsigned long preserve_plane_bit = 0;
			/* For VLV, the planes are hardwired to the pipes:
			 * Plane-A to Pipe-A and Plane-B to Pipe-B
			 */
			if(plane->plane_reg == DSPACNTR){
				preserve_plane_bit = device_data->plane_a_preserve;
				i = _PIPEACONF;
			} else {
				preserve_plane_bit = device_data->plane_b_c_preserve;
				i = _PIPEBCONF;
			}

			if ((temp & (~preserve_plane_bit)) != 0){
				/*If anything in plane_reg is changed.*/
				temp = temp & preserve_plane_bit;

			
			
				WRITE_MMIO_REG_VLV((temp & ~BIT31), mmio, plane->plane_reg);

				/* The B-Spec is ambiguous on which register is the trigger.
			 	* Testing has shown the the surface start address is the
			 	* correct trigger to disable the plane.
			 	*/
				WRITE_MMIO_REG_VLV(0, mmio, plane->plane_reg+DSP_START_OFFSET);

				/* Wait for VBLANK to ensure that the plane is really off */
				wait_for_vblank_vlv(mmio, i);

				EMGD_DEBUG("Plane disabled 0x%lx", plane->plane_reg);
			}
		} else if ((plane->plane_features & IGD_PLANE_CURSOR)) {
			WRITE_MMIO_REG_VLV((temp & 0xffffffe8),
				mmio, plane->plane_reg);
			WRITE_MMIO_REG_VLV(0, mmio, plane->plane_reg+4);
		}

	}

	/* Turn off pipes */
	pipe = NULL;
	while ((pipe = module_dispatch->dsp_get_next_pipe(context, pipe, 0))
			!= NULL) {

		/* Is this really required? Just waited for vblank above 2 times */
		wait_for_vblank_vlv(mmio, pipe->pipe_reg);
		temp = READ_MMIO_REG_VLV(mmio, pipe->pipe_reg);

		WRITE_MMIO_REG_VLV((temp & device_data->pipe_preserve),
			mmio, pipe->pipe_reg);

		/* Disable VGA display */
		kms_disable_vga_vlv(context);


		/* Disable DPLL */
		temp = READ_MMIO_REG_VLV(mmio, pipe->clock_reg->dpll_control);

		WRITE_MMIO_REG_VLV((temp & ~BIT31), mmio, pipe->clock_reg->dpll_control);
		
	}

	/* Disable Panel Fitter. This is needed to read
	 * the correct pipe CRC */
	temp = READ_MMIO_REG_VLV(mmio, PFIT_CONTROL);
	WRITE_MMIO_REG_VLV((temp & ~BIT31), mmio, PFIT_CONTROL);

	EMGD_TRACE_EXIT;
} /* end reset_plane_pipe_ports */


/*!
 *
 * im_filter_modes_vlv
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
static void im_filter_modes_vlv(igd_context_t *context,
		igd_display_port_t *port,
		pd_timing_t *in_list)
{
	EMGD_DEBUG("Max dclk is %ld", context->device_context.max_dclk);

	while (in_list->width != IGD_TIMING_TABLE_END) {
		/*
		 * Dotclock limits
		 * CRTC     20-355MHz
		 * HDMI     20-165MHz
		 * LVDS     20-112MHz
		 * DP       162,270MHz   How do we handle this?
		 *
		 * We also need reject anything higher than 90% of CDCLK.
		 */
		if (in_list->dclk > context->device_context.max_dclk) {
			in_list->flags &= ~IGD_MODE_SUPPORTED;
		} else {
			switch (port->port_type) {
				case IGD_PORT_DIGITAL:
					if (port->pd_driver->type == PD_DISPLAY_DP_INT){
						if ((in_list->dclk < 20000) || (in_list->dclk > 355000)) {
							in_list->flags &= ~IGD_MODE_SUPPORTED;
						}
					} else {
						if ((in_list->dclk < 20000) || (in_list->dclk > 165000)) {
							in_list->flags &= ~IGD_MODE_SUPPORTED;
						}
					}
					break;
				case IGD_PORT_LVDS:
					/* LVDS port driver should be limiting modes already. */
					break;
				case IGD_PORT_ANALOG:
					if ((in_list->dclk < 20000) || (in_list->dclk > 355000)) {
						in_list->flags &= ~IGD_MODE_SUPPORTED;
					}
					break;
				case IGD_PORT_TV:
				case IGD_PORT_MIPI_PORT:
					break;
			}
		}
		in_list++;
		if (in_list->width == IGD_TIMING_TABLE_END && in_list->private.extn_ptr) {
			in_list = in_list->private.extn_ptr;
		}
	}
	return;
}

#define single_plane_enabled(mask) is_power_of_2(mask)

/*!
 * kms_update_wm_vlv
 *
 * @param dev
 *
 * @return void
 */
static void kms_update_wm_vlv(struct drm_device *dev)
{
    static const int sr_latency_ns = 12000;
    struct drm_i915_private *dev_priv;
    int planea_wm, planeb_wm, cursora_wm, cursorb_wm;
    int plane_sr, cursor_sr;
    unsigned int enabled = 0;


	/* FIXME: For VLV, the default chicken register fuses are configured for 
	 * new PND timestamp-based B-Unit settings .. meaning this FW settings might 
	 * not really make much difference, we should however add code to check 
	 * current mode info and determine minimum ETA time for display requests from
	 * PND
	 */

	if (dev == NULL){
		EMGD_ERROR ("Device is null");
		return;
	}
	dev_priv = dev->dev_private;

    valleyview_update_dl(dev);

    if (g4x_compute_wm0(dev, 0,
                &valleyview_wm_info, latency_ns,
                &valleyview_cursor_wm_info, latency_ns,
                &planea_wm, &cursora_wm))
        enabled |= 1;

    if (g4x_compute_wm0(dev, 1,
                &valleyview_wm_info, latency_ns,
                &valleyview_cursor_wm_info, latency_ns,
                &planeb_wm, &cursorb_wm))
        enabled |= 2;

    plane_sr = cursor_sr = 0;
    if (single_plane_enabled(enabled) &&
        g4x_compute_srwm(dev, ffs(enabled) - 1,
                 sr_latency_ns,
                 &valleyview_wm_info,
                 &valleyview_cursor_wm_info,
                 &plane_sr, &cursor_sr))
        I915_WRITE(FW_BLC_SELF_VLV, FW_BLC_SELF_EN);
    else
        I915_WRITE(FW_BLC_SELF_VLV,
               I915_READ(FW_BLC_SELF_VLV) & ~FW_BLC_SELF_EN);

    DRM_DEBUG_KMS("Setting FIFO watermarks - A: plane=%d, cursor=%d, B: plane=%d, cursor=%d, SR: plane=%d, cursor=%d\n",
              planea_wm, cursora_wm,
              planeb_wm, cursorb_wm,
              plane_sr, cursor_sr);

    I915_WRITE(DSPFW1,
           (plane_sr << DSPFW_SR_SHIFT) |
           (cursorb_wm << DSPFW_CURSORB_SHIFT) |
           (planeb_wm << DSPFW_PLANEB_SHIFT) |
           planea_wm);
    I915_WRITE(DSPFW2,
           (I915_READ(DSPFW2) & DSPFW_CURSORA_MASK) |
           (cursora_wm << DSPFW_CURSORA_SHIFT));
    I915_WRITE(DSPFW3,
           (I915_READ(DSPFW3) | (cursor_sr << DSPFW_CURSOR_SR_SHIFT)));
}

static void valleyview_update_dl(struct drm_device *dev)
{
    struct drm_i915_private *dev_priv = dev->dev_private;
    int planea_prec, planea_dl, planeb_prec, planeb_dl;
    int cursora_prec, cursora_dl, cursorb_prec, cursorb_dl;
    int plane_prec_mult, cursor_prec_mult; /* Precision multiplier is
                            either 16 or 32 */

    /* For plane A, Cursor A */
    if (valleyview_compute_dl(dev, 0, &plane_prec_mult, &planea_dl,
                    &cursor_prec_mult, &cursora_dl)) {
        cursora_prec = (cursor_prec_mult == DRAIN_LATENCY_PRECISION_32) ?
            DDL_CURSORA_PRECISION_32 : DDL_CURSORA_PRECISION_16;
        planea_prec = (plane_prec_mult == DRAIN_LATENCY_PRECISION_32) ?
            DDL_PLANEA_PRECISION_32 : DDL_PLANEA_PRECISION_16;

        I915_WRITE(VLV_DDL1, cursora_prec |
                (cursora_dl << DDL_CURSORA_SHIFT) |
                planea_prec | planea_dl);
    }

    /* For plane B, Cursor B */
    if (valleyview_compute_dl(dev, 1, &plane_prec_mult, &planeb_dl,
                    &cursor_prec_mult, &cursorb_dl)) {
        cursorb_prec = (cursor_prec_mult == DRAIN_LATENCY_PRECISION_32) ?
            DDL_CURSORB_PRECISION_32 : DDL_CURSORB_PRECISION_16;
        planeb_prec = (plane_prec_mult == DRAIN_LATENCY_PRECISION_32) ?
            DDL_PLANEB_PRECISION_32 : DDL_PLANEB_PRECISION_16;

        I915_WRITE(VLV_DDL2, cursorb_prec |
                (cursorb_dl << DDL_CURSORB_SHIFT) |
                planeb_prec | planeb_dl);
    }
}

static int valleyview_compute_dl(struct drm_device *dev,
                    int plane,
                    int *plane_prec_mult,
                    int *plane_dl,
                    int *cursor_prec_mult,
                    int *cursor_dl)
{
    struct drm_crtc *crtc;
    int clock, pixel_size;
    int entries;

    crtc = intel_get_crtc_for_plane(dev, plane);
    if (!crtc || crtc->fb == NULL || !crtc->enabled)
        return false;

    clock = crtc->mode.clock;   /* VESA DOT Clock */

	if (clock == 0){
		EMGD_ERROR("Divide by Zero. Clock is Zero.");
		return false;
	}
	
    pixel_size = crtc->fb->bits_per_pixel / 8;  /* BPP */

    entries = (clock / 1000) * pixel_size;
    *plane_prec_mult = (entries > 256) ?
        DRAIN_LATENCY_PRECISION_32 : DRAIN_LATENCY_PRECISION_16;
    *plane_dl = (64 * *plane_prec_mult * 4) / ((clock / 1000) * pixel_size);

    entries = (clock / 1000) * 4;   /* BPP is always 4 for cursor */
    *cursor_prec_mult = (entries > 256) ?
        DRAIN_LATENCY_PRECISION_32 : DRAIN_LATENCY_PRECISION_16;
    *cursor_dl = (64 * *cursor_prec_mult * 4) / ((clock / 1000) * 4);

    return true;
}

static bool g4x_compute_wm0(struct drm_device *dev,
                int plane,
                const struct intel_watermark_params *display,
                int display_latency_ns,
                const struct intel_watermark_params *cursor,
                int cursor_latency_ns,
                int *plane_wm,
                int *cursor_wm)
{
    struct drm_crtc *crtc;
    int htotal, hdisplay, clock, pixel_size;
    int line_time_us, line_count;
    int entries, tlb_miss;

    crtc = intel_get_crtc_for_plane(dev, plane);
    if (!crtc || crtc->fb == NULL || !crtc->enabled) {
        *cursor_wm = cursor->guard_size;
        *plane_wm = display->guard_size;
        return false;
    }

    htotal = crtc->mode.htotal;
    hdisplay = crtc->mode.hdisplay;
    clock = crtc->mode.clock;
    pixel_size = crtc->fb->bits_per_pixel / 8;
    if (!clock) {
        *cursor_wm = cursor->guard_size;
        *plane_wm = display->guard_size;
		EMGD_ERROR ("Divide by Zero. Clock is Zero.");
        return false;
    }

    /* Use the small buffer method to calculate plane watermark */
    entries = ((clock * pixel_size / 1000) * display_latency_ns) / 1000;
    tlb_miss = display->fifo_size*display->cacheline_size - hdisplay * 8;
    if (tlb_miss > 0)
        entries += tlb_miss;
    entries = DIV_ROUND_UP(entries, display->cacheline_size);
    *plane_wm = entries + display->guard_size;
    if (*plane_wm > (int)display->max_wm)
        *plane_wm = display->max_wm;

    /* Use the large buffer method to calculate cursor watermark */
    line_time_us = ((htotal * 1000) / clock);
    if (!line_time_us)  {
        *cursor_wm = cursor->guard_size;
        *plane_wm = display->guard_size;
		EMGD_ERROR("Divide by Zero. line_time_us is Zero.");
        return false;
    }
    line_count = (cursor_latency_ns / line_time_us + 1000) / 1000;
    entries = line_count * 64 * pixel_size;
    tlb_miss = cursor->fifo_size*cursor->cacheline_size - hdisplay * 8;
    if (tlb_miss > 0)
        entries += tlb_miss;
    entries = DIV_ROUND_UP(entries, cursor->cacheline_size);
    *cursor_wm = entries + cursor->guard_size;
    if (*cursor_wm > (int)cursor->max_wm)
        *cursor_wm = (int)cursor->max_wm;

    return true;
}

static bool g4x_compute_srwm(struct drm_device *dev,
                 int plane,
                 int latency_ns,
                 const struct intel_watermark_params *display,
                 const struct intel_watermark_params *cursor,
                 int *display_wm, int *cursor_wm)
{
    struct drm_crtc *crtc;
    int hdisplay, htotal, pixel_size, clock;
    unsigned long line_time_us;
    int line_count, line_size;
    int small, large;
    int entries;

    if (!latency_ns) {
        *display_wm = *cursor_wm = 0;
        return false;
    }

    crtc = intel_get_crtc_for_plane(dev, plane);
	if (!crtc) {
		*display_wm = *cursor_wm = 0;
		return false;
	}

	hdisplay = crtc->mode.hdisplay;
    htotal = crtc->mode.htotal;
    clock = crtc->mode.clock;

	if (!clock){
		*display_wm = *cursor_wm = 0;
		EMGD_ERROR("Divide by Zero. Clock is Zero.");
		return false;
	}

    pixel_size = crtc->fb->bits_per_pixel / 8;

    line_time_us = (htotal * 1000) / clock;

	if (!line_time_us){
		*display_wm = *cursor_wm = 0;
		EMGD_ERROR("Divide by Zero. line_time_us is Zero.");
		return false;
	}

    line_count = (latency_ns / line_time_us + 1000) / 1000;
    line_size = hdisplay * pixel_size;

    /* Use the minimum of the small and large buffer method for primary */
    small = ((clock * pixel_size / 1000) * latency_ns) / 1000;
    large = line_count * line_size;

    entries = DIV_ROUND_UP(min(small, large), display->cacheline_size);
    *display_wm = entries + display->guard_size;

    /* calculate the self-refresh watermark for display cursor */
    entries = line_count * pixel_size * 64;
    entries = DIV_ROUND_UP(entries, cursor->cacheline_size);
    *cursor_wm = entries + cursor->guard_size;

    return g4x_check_srwm(dev,
                  *display_wm, *cursor_wm,
                  display, cursor);
}

/*
 * Check the wm result.
 *
 * If any calculated watermark values is larger than the maximum value that
 * can be programmed into the associated watermark register, that watermark
 * must be disabled.
 */
static bool g4x_check_srwm(struct drm_device *dev,
               int display_wm, int cursor_wm,
               const struct intel_watermark_params *display,
               const struct intel_watermark_params *cursor)
{
    DRM_DEBUG_KMS("SR watermark: display plane %d, cursor %d\n",
              display_wm, cursor_wm);

    if (display_wm > display->max_wm) {
        DRM_DEBUG_KMS("display watermark is too large(%d/%ld), disabling\n",
                  display_wm, display->max_wm);
        return false;
    }

    if (cursor_wm > cursor->max_wm) {
        DRM_DEBUG_KMS("cursor watermark is too large(%d/%ld), disabling\n",
                  cursor_wm, cursor->max_wm);
        return false;
    }

    if (!(display_wm || cursor_wm)) {
        DRM_DEBUG_KMS("SR latency is 0, disabling\n");
        return false;
    }

    return true;
}

unsigned long read_mmio_reg_vlv(unsigned char *mmio, unsigned long reg)
{
	if (IS_DISPLAYREG(reg)) {
		reg += 0x180000;
	}
	return EMGD_READ32(mmio + reg);
}

void write_mmio_reg_vlv(unsigned long value,
		unsigned char *mmio,
		unsigned long reg)
{
	if (IS_DISPLAYREG(reg)) {
		reg += 0x180000;
	}
	EMGD_WRITE32(value, mmio + reg);
}

unsigned char read_mmio_reg8_vlv(unsigned char *mmio, unsigned long reg)
{
	if (IS_DISPLAYREG(reg)) {
		reg += 0x180000;
	}
	return EMGD_READ8(mmio + reg);
}

void write_mmio_reg8_vlv(unsigned char value,
		unsigned char *mmio,
		unsigned long reg)
{
	if (IS_DISPLAYREG(reg)) {
		reg += 0x180000;
	}
	EMGD_WRITE8(value, mmio + reg);
}

/* This is special function for FB_BLEND_OVL feature
 * Function supports changing display's plane color format
 * from xRGB32 to ARGB32 and vice versa */
int kms_change_plane_color_format(emgd_crtc_t *emgd_crtc, unsigned int fb_blend_flag)
{
	int ret = 0;
	unsigned long plane_control = 0;
	unsigned long plane_offset = 0;
	unsigned long plane_reg = 0;
	unsigned long color_format = 0;
	emgd_framebuffer_t *fb_info;
	igd_display_plane_t *plane;
	igd_display_pipe_t *pipe;
	unsigned char *mmio;

	EMGD_TRACE_ENTER;

	pipe      = PIPE(emgd_crtc);
	plane     = PLANE(emgd_crtc);
	mmio      = MMIO(emgd_crtc);

	if ((NULL == plane) || (NULL == pipe->timing)) {
		EMGD_ERROR("Trying to program a plane that is not tied to a crtc.");
		EMGD_TRACE_EXIT;
		return 0;
	}

	fb_info       = plane->fb_info;
	plane_reg     = plane->plane_reg;
	plane_control = READ_MMIO_REG_VLV(mmio, plane_reg);
	EMGD_DEBUG("PA control:1:  0x%lx", plane_control);

	/*
	 * From the B-spec, display plane control register from BIT29 to BIT26,
	 * if the source pixel format is under Reserved, 8-bpp Indexed and
	 * 16 bit BGRX(565) then we don't change the display's color format.
	 */
	color_format = (plane_control >> 26) & 0xf;
	if(color_format <= 0x5) {
		EMGD_DEBUG("Display plane pixel format cannot support alpha blending!");
		EMGD_TRACE_EXIT;
		return 0;
	}


	if (fb_blend_flag) {
		if(!(plane_control & DSP_ALPHA)) {
			plane_control |= DSP_ALPHA;
			fb_info->igd_pixel_format |= PF_TYPE_ALPHA;
		} else {
			EMGD_TRACE_EXIT;
			return 0;
		}
	} else {
		if(plane_control & DSP_ALPHA) {
			plane_control &= ~DSP_ALPHA;
			fb_info->igd_pixel_format &= ~PF_TYPE_ALPHA;
		} else {
			EMGD_TRACE_EXIT;
			return 0;
		}
	}

	EMGD_DEBUG("PA control:2:  0x%lx", plane_control);
	WRITE_MMIO_REG_VLV(plane_control, mmio, plane_reg);
	/*to trig display's plane*/
	plane_offset = READ_MMIO_REG_VLV(mmio, plane_reg + DSP_START_OFFSET);
	WRITE_MMIO_REG_VLV(plane_offset, mmio, plane_reg + DSP_START_OFFSET);
	/* read value again, mostly for debug*/
	plane_control = READ_MMIO_REG_VLV(mmio, plane_reg);
	EMGD_DEBUG("PA control:3:  0x%lx", plane_control);

	EMGD_TRACE_EXIT;

	return ret;
}
