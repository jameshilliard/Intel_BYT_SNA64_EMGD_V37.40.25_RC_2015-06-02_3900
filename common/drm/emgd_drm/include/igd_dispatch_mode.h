/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_dispatch_mode.h
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
 *  This header file defines the interfaces that can be called directly by
 *  the upper KMS layer of the emgd driver - this contains both the DI
 *  (Device Independent) and DD (Device Dependent) layer of the mode module
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPATCH_MODE_H
#define _IGD_DISPATCH_MODE_H

#include <igd_mode.h>


typedef struct _igd_mode_dispatch {
	/*
	 * ------------------------------------------------------------
	 *       DEVICE-INDEPENDENT KMS EXPORTED FUNCTIONS --->
	 *       PREFIX = kms_ SUFFIX = NONE
	 * ------------------------------------------------------------
	 * These are the Device Independant functions that KMS can call
	 * Please carefully add new interfaces to the right location to
	 * ensure no duplication of code that is common across hardware
	 */
	int (*kms_match_mode)(emgd_encoder_t *emgd_encoder,
			emgd_framebuffer_t *fb_info, igd_timing_info_t **timing);
	/*
	 * ------------------------------------------------------------
	 *        DEVICE-DEPENDENT KMS EXPORTED FUNCTIONS --->
	 *        PREFIX = kms_ HIDDEN_SUFFIX = _corename (_snb / _vlv)
	 * ------------------------------------------------------------
	 * These are the Device Dependant functions that KMS can call
	 * Please carefully add new interfaces to the right location to
	 * ensure no duplication of code that is common across hardware
	 */
	void (*kms_program_pipe) (emgd_crtc_t *emgd_crtc);
	void (*kms_set_pipe_pwr) (emgd_crtc_t *emgd_crtc, unsigned long enable);
	int __must_check (*kms_program_plane)(emgd_crtc_t *emgd_crtc, bool preserve);
	int  (*kms_calc_plane)(emgd_crtc_t *emgd_crtc, bool preserve);
	void (*kms_commit_plane)(emgd_crtc_t *emgd_crtc, void *regs);
	void (*kms_set_plane_pwr)(emgd_crtc_t *emgd_crtc, unsigned long enable, bool wait_for_vblank);
	int  (*kms_program_port) (emgd_encoder_t *emgd_encoder,
		unsigned long status);
	int  (*kms_post_program_port)(emgd_encoder_t *emgd_encoder,
		unsigned long status);
	u32  (*kms_get_vblank_counter)(emgd_crtc_t *emgd_crtc);
	void (*kms_disable_vga)(igd_context_t * context);
	void (*kms_program_cursor)(emgd_crtc_t *emgd_crtc, unsigned long status);
	int (*kms_alter_cursor_pos)(emgd_crtc_t *emgd_crtc);
	int (*kms_queue_flip)(emgd_crtc_t *emgd_crtc,
			emgd_framebuffer_t *emgd_fb,
          	struct drm_i915_gem_object *bo,
           	int plane_select);
	int (*kms_set_palette_entries)(emgd_crtc_t *emgd_crtc,
			unsigned int start_index, unsigned int count);
	void (*kms_set_display_base)(emgd_crtc_t *emgd_crtc,
			emgd_framebuffer_t *fb);
	int (*kms_set_color_correct)(emgd_crtc_t *emgd_crtc);
	void (*kms_update_wm)(struct drm_device *dev);
	/* This is special function for FB_BLEND_OVL feature
	 * Function supports changing display's plane color format
	 * from xRGB32 to ARGB32 and vice versa */
	int (*kms_change_plane_color_format) (emgd_crtc_t *emgd_crtc, unsigned int fb_blend_flag);
	/*
	 * ------------------------------------------------------------
	 *    DEVICE-DEPENDENT INTERMODULE EXPORTED FUNCTIONS --->
	 *    PREFIX = im_ HIDDEN_SUFFIX = _corename (_snb / _vlv)
	 * ------------------------------------------------------------
	 * These are the Device Dependant functions that KMS DOES NOT call
	 * Other HAL modules need to use for internally managed functionality
	 * Please carefully add new interfaces to the right location to
	 * ensure no duplication of code that is common across hardware
	 */
	void (*im_reset_plane_pipe_ports)(igd_context_t *context);
	void (*im_filter_modes)(igd_context_t *context,
		igd_display_port_t *port, pd_timing_t *in_list);
	int (*kms_get_brightness )(emgd_crtc_t *emgd_crtc);
	int (*kms_get_max_brightness) (emgd_crtc_t *emgd_crtc);
	int (*kms_set_brightness) (emgd_crtc_t *emgd_crtc, int level); 
	int (*kms_blc_support) (emgd_crtc_t *emgd_crtc, unsigned long *port_number);

} igd_mode_dispatch_t;


/*
 * Firmware(VBIOS or EFI Video Driver) related information
 * that needs to be populated  before the driver re-programs
 * the Hardware Registers. This information is needed to provide
 * seamless transition from firmware to driver.
 */
typedef struct _fw_info {

	/* TODO: Fill this up */
	unsigned long fw_dc; /* The dsp module already has this value */

	/* Plane information */
	//igd_framebuffer_info_t fb_info[2]; /* one for each plane */

	/* Pipe information */
	igd_display_info_t timing_arr[2]; /* one for each pipe */

	/* Port information */

	/* if the plane registers needs an update, set this field to 1 */
	int program_plane;

} fw_info_t;



typedef struct _mode_context {
	/*
	 * All of the below values will be initialized in mode module
	 * init function mode_init().
	 */
	unsigned long first_alter;
	igd_context_t *context;
	fw_info_t* fw_info; /* This needs to be zero for VBIOS */

    /* quickboot options */
    unsigned long quickboot;
    int seamless;
    unsigned long video_input;
    int splash;
	unsigned long ref_freq;
	int tuning_wa;
	unsigned long clip_hw_fix;
	unsigned long async_flip_wa;

	/*
	 * Enable override of following registers when en_reg_override=1.
	 * Display Arbitration, FIFO Watermark Control, GVD HP_CONTROL,
	 * Bunit Chickenbits, Bunit Write Flush, Display Chickenbits
	 */
	unsigned long en_reg_override;
	unsigned long disp_arb;
	unsigned long fifo_watermark1;
	unsigned long fifo_watermark2;
	unsigned long fifo_watermark3;
	unsigned long fifo_watermark4;
	unsigned long fifo_watermark5;
	unsigned long fifo_watermark6;
	unsigned long gvd_hp_control;
	unsigned long bunit_chicken_bits;
	unsigned long bunit_write_flush;
	unsigned long disp_chicken_bits;

} mode_context_t;

extern int full_mode_init(igd_context_t *context,
	mode_context_t *mode_context);

extern int full_mode_query(igd_driver_h driver_handle, unsigned long dc,
	igd_display_info_t **mode_list, igd_display_port_t *port);

extern int query_seamless(unsigned long dc,
		int index,
		igd_timing_info_t *pt,
		emgd_framebuffer_t *pf,
		unsigned long flags);

extern void swap_fb_cursor( void );

extern int set_color_correct(igd_display_context_t *display,
	const igd_range_attr_t *attr_to_set);

/*
 * NOTE: Some of these externs are declared with the struct name because the
 * contents of that struct are unavailable at the DI layer. The symbol
 * is used as the generic mode_dispatch_t which is a subset.
 */

#ifdef CONFIG_SNB
extern igd_mode_dispatch_t mode_kms_dispatch_snb;
#endif
#ifdef CONFIG_VLV
extern igd_mode_dispatch_t mode_kms_dispatch_vlv;
#endif


/*******************************************************************************
 *
 * The following macros provide part of the "protected" interface to support
 * mode-module "requests" for VBlank interrupts.
 *
 * Requests are for a "who" and "what."  The "who" is what type of code is
 * making the request, and the "what" is the port that the requestor wants to
 * know about VBlanks for.  Here is additional information:
 *
 * - Who - which software is asking:
 *   - WAIT - The code that implements the wait_vblank() function pointer.
 *            When interrupts are requested for WAIT, the interrupt handler
 *            makes note of when VBlanks occur.  The WAIT code queries (polls)
 *            whether a VBlank has occured since its request.
 *   - FLIP - The code that implements the {check|set}_flip_pending() function
 *            pointers.  When interrupts are requested for FLIP, the interrupt
 *            handler makes note of when VBlanks occur.  The FLIP code queries
 *            (polls) whether a VBlank has occured since its request.
 *   - CB -   The non-HAL code that registers a VBlank interrupt "callback"
 *            (CB).  When interrupts are requested for CB, the interrupt
 *            handler calls the callback when VBlanks occur.
 *
 * - What - which port (Note: space is reserve for 4 ports, even only two exist
 *   at this time):
 *   - PORT2 (Port 2, Pipe A, SDVO-B)
 *   - PORT4 (Port 4, Pipe B, Int-LVDS)
 *
 * Note: internally, the requests are stored in bits within an unsigned long.
 * This helps explain the way the macros are implemented:
 *
 ******************************************************************************/

/* A requestor uses this macro to generate the bit request for who and what: */
#define VBINT_REQUEST(who,port) ((port) << (who))

/* A requestor uses one of these macros to specify a what (i.e. port): */
#define VBINT_PORT2 0x01
#define VBINT_PORT4 0x02
/* Note: the following 2 macros reserve space for 2 more (future) ports: */
#define VBINT_PORTn 0x04
#define VBINT_PORTm 0x08

/* A requestor uses one of these macros to identify itself (the what): */
/* Note: Each "who" has 4 bits (for 4 ports); the value is a shift amount: */
#define VBINT_WAIT 0
#define VBINT_FLIP 4
#define VBINT_CB   8


/*******************************************************************************
 *
 * The following macros provide part of the "private" interface to support
 * mode-module "requests" for VBlank interrupts.  The VBlank-interrupt code
 * uses these macros to manage requests, and to record VBlanks that occur
 * (a.k.a. "answers").
 *
 * Other parts of the "mode" module should not use these macros.
 *
 ******************************************************************************/

/* Answers for a request are stored in bits to the left of the request bits: */
#define VBINT_ANSWER_SHIFT 12
#define VBINT_ANSWER(who,port) (((port) << (who)) << VBINT_ANSWER_SHIFT)
#define VBINT_ANSWER4_REQUEST(request) ((request) << VBINT_ANSWER_SHIFT)

/* The following special bit is used by disable_vblank_interrupts_{plb|tnc}() to
 * disable the hardware, but not unregister the never-registered interrupt
 * handler:
 */
#define VBLANK_DISABLE_HW_ONLY BIT31

/* The following macros aggregate all of the who's can enable interrupts for a
 * given port:
 */
#define VBLANK_INT4_PORT2 (VBINT_REQUEST(VBINT_WAIT, VBINT_PORT2) | \
	VBINT_REQUEST(VBINT_FLIP, VBINT_PORT2) |						\
	VBINT_REQUEST(VBINT_CB, VBINT_PORT2))
#define VBLANK_INT4_PORT4 (VBINT_REQUEST(VBINT_WAIT, VBINT_PORT4) | \
	VBINT_REQUEST(VBINT_FLIP, VBINT_PORT4) |						\
	VBINT_REQUEST(VBINT_CB, VBINT_PORT4))

/* The following macros aggregate all of the whats (ports) that can enable
 * interrupts for a given who (they aren't used, but do help document that
 * 4 bits are reserved for each "who"):
 */
/* FIXME -- KEEP THESE??? */
#define VBLANK_INT4_WAIT 0x0000000f
#define VBLANK_INT4_FLIP 0x000000f0
#define VBLANK_INT4_CB   0x00000f00

/* The following macros tell whether interrupts are enabled, either in general,
 * or for a certain port.
 */
#define VBLANK_INTERRUPTS_ENABLED \
	(vblank_interrupt_state & (VBLANK_INT4_PORT2 | VBLANK_INT4_PORT4 | \
		VBLANK_DISABLE_HW_ONLY))
#define VBLANK_INTERRUPTS_ENABLED4_PORT2 \
	(vblank_interrupt_state & VBLANK_INT4_PORT2)
#define VBLANK_INTERRUPTS_ENABLED4_PORT4 \
	(vblank_interrupt_state & VBLANK_INT4_PORT4)

#endif /*_IGD_DISPATCH_MODE_H*/
