/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: clocks_snb.c
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
 *  Clock programming for SNB
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.mode

#include <io.h>
#include <ossched.h>
#include <memory.h>

#include <igd_core_structs.h>
#include <dsp.h>
#include <pi.h>

#include <gn6/regs.h>
#include "instr_common.h"

#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "regs_common.h"
#include "drm_emgd_private.h"


/*!
 * @addtogroup display_group
 * @{
 */

#if defined(CONFIG_SNB)
/*===========================================================================
; File Global Data
;--------------------------------------------------------------------------*/

/* Ironlake / Sandybridge */
/* as we calculate clock using (register_value + 2) for
   N/M1/M2, so here the range value for them is (actual_value-2).
 */
#define IRONLAKE_DOT_MIN         25000
#define IRONLAKE_DOT_MAX         350000
#define IRONLAKE_VCO_MIN         1760000
#define IRONLAKE_VCO_MAX         3510000
#define IRONLAKE_M1_MIN          12
#define IRONLAKE_M1_MAX          22
#define IRONLAKE_M2_MIN          5
#define IRONLAKE_M2_MAX          9
#define IRONLAKE_P2_DOT_LIMIT    225000 /* 225Mhz */

/* DAC & HDMI Refclk 120Mhz */
#define SNB_DAC_N_MIN	3
#define SNB_DAC_N_MAX	8
#define IRONLAKE_DAC_M_MIN	79
#define IRONLAKE_DAC_M_MAX	127
#define IRONLAKE_DAC_P_MIN	5
#define IRONLAKE_DAC_P_MAX	80
#define IRONLAKE_DAC_P1_MIN	1
#define IRONLAKE_DAC_P1_MAX	8
#define IRONLAKE_DAC_P2_SLOW	10
#define IRONLAKE_DAC_P2_FAST	5

/* LVDS single-channel 120Mhz refclk */
#define IRONLAKE_LVDS_S_N_MIN	1
#define IRONLAKE_LVDS_S_N_MAX	3
#define IRONLAKE_LVDS_S_M_MIN	79
#define IRONLAKE_LVDS_S_M_MAX	118
#define IRONLAKE_LVDS_S_P_MIN	28
#define IRONLAKE_LVDS_S_P_MAX	112
#define IRONLAKE_LVDS_S_P1_MIN	2
#define IRONLAKE_LVDS_S_P1_MAX	8
#define IRONLAKE_LVDS_S_P2_SLOW	14
#define IRONLAKE_LVDS_S_P2_FAST	14

typedef struct _clock_attrib {
	unsigned short did;
	unsigned int min_m;
	unsigned int max_m;
	unsigned int min_n;
	unsigned int max_n;
	unsigned int min_p1;
	unsigned int max_p1;
	unsigned int fix_p2_lo;
	unsigned int fix_p2_hi;
	unsigned int lvds_fix_p2_lo;
	unsigned int lvds_fix_p2_hi;
	unsigned long min_vco;
	unsigned long max_vco;
	unsigned int min_m1;
	unsigned int max_m1;
	unsigned int min_m2;
	unsigned int max_m2;
	unsigned int target_error;
} clock_attrib_t;

static clock_attrib_t *clock_info = NULL;

/* FIXME(SNB): Value based on GN4, need to be checked for SNB */
static clock_attrib_t clock_table[] = {
	{PCI_DEVICE_ID_VGA_SNB,
	IRONLAKE_DAC_M_MIN,
	IRONLAKE_DAC_M_MAX,
	SNB_DAC_N_MIN,
	SNB_DAC_N_MAX,
	IRONLAKE_DAC_P1_MIN,
	IRONLAKE_DAC_P1_MAX,
	IRONLAKE_DAC_P2_FAST,
	IRONLAKE_DAC_P2_SLOW,
	IRONLAKE_LVDS_S_P2_FAST,
	IRONLAKE_LVDS_S_P2_SLOW,
	IRONLAKE_VCO_MIN,
	IRONLAKE_VCO_MAX,
	IRONLAKE_M1_MIN,
	IRONLAKE_M1_MAX,
	IRONLAKE_M2_MIN,
	IRONLAKE_M2_MAX,
	48},
};

#define CLOCK_TABLE_SIZE  (sizeof(clock_table)/sizeof(clock_attrib_t))

/*!
 * This function translates from the calculated M value to the M1, M2
 * register values.
 *
 * @param m
 * @param *m1
 * @param *m2
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int calculate_m1_m2(unsigned long m,
	unsigned long *m1,
	unsigned long *m2)
{
	unsigned long current_m1, current_m2;

	EMGD_TRACE_ENTER;

	/*                  M1 Range     M2 Range
	 *                  --------     --------
	 * CRT              12-22        5-9
	 * SDVO/HDMI/DVI
	 * LVDS(1)
	 * LVDS(2)
	 */
	for(current_m1 = (clock_info->min_m1 + 2); current_m1 <= (clock_info->max_m1 + 2); current_m1++) {
		for(current_m2 = (clock_info->min_m2 + 2); current_m2 <= (clock_info->max_m2 + 2); current_m2++)  {
			if ((current_m1 * 5 + current_m2) == m) {
				*m1 = current_m1 - 2;
				*m2 = current_m2 - 2;
				EMGD_TRACE_EXIT;
				return 0;
			}
		}
	}

	EMGD_DEBUG("M1, M2 not found for M == %ld", m);
	EMGD_TRACE_EXIT;
	return 1;
}

#define REF_FREQ        120000   /* 120Mhz refclk */
#define MIN_VCO         IRONLAKE_VCO_MIN
#define MAX_VCO         IRONLAKE_VCO_MAX
#define TARGET_ERROR        48   /*   48 KHz */

/*!
 *
 * @param dclk
 * @param ref_freq
 * @param m1
 * @param m2
 * @param n
 * @param p
 * @param min_vco
 * @param target_error
 * @param port_type
 * @param dual_channel
 * @param max_fp
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int calculate_clock(unsigned long dclk,
	unsigned long ref_freq,
	unsigned long *m1,
	unsigned long *m2,
	unsigned long *n,
	unsigned long *p,
	unsigned long min_vco,
	unsigned long target_error,
	unsigned long port_type,
	unsigned long dual_channel,
	unsigned long max_fp)

{
	unsigned long p1;
	unsigned long p2;
	/* Parameters */
	unsigned long freqmult_p2;

	/* Intermidiate variables */
	unsigned long  pdiv;
	unsigned long  target_vco, actual_freq;
	long freq_error, min_error;
	unsigned long dclk_10000;

	unsigned long current_m, current_n, current_p1;
	unsigned long best_m = 0;
	unsigned long best_n = 0;
	unsigned long best_p1 = 0;

	EMGD_TRACE_ENTER;

	min_error = 100000;

	if (port_type == IGD_PORT_LVDS) {
		/* For LVDS port */
		/* Test if we are dual channel */
		if (dual_channel) {
			freqmult_p2 = clock_info->lvds_fix_p2_lo;
			p2 = 1;
		} else{
			freqmult_p2 = clock_info->lvds_fix_p2_hi;
			p2 = 0;
		}
	} else {
		/* For non-LVDS port */
		if (dclk > max_fp) {
			freqmult_p2 = clock_info->fix_p2_lo;
			p2 = 1;
		} else {
			freqmult_p2 = clock_info->fix_p2_hi;
			p2 = 0;
		}
	}

	/* dclk * 10000, so it does not have to be calculated within the
	 * loop */
	dclk_10000 = dclk * 10000;

	for(current_n = clock_info->min_n; current_n <= clock_info->max_n; current_n++) {
		for(current_m = clock_info->min_m; current_m <= clock_info->max_m; current_m++) {
			for(current_p1 = clock_info->min_p1; current_p1 <= clock_info->max_p1; current_p1++) {
				pdiv = freqmult_p2 * current_p1;
				target_vco = dclk * pdiv;

				if (target_vco < min_vco || target_vco > clock_info->max_vco) {
					/* target_vco continues to increase, so start with
					 * next current_n */
					continue;
				}

				actual_freq = (ref_freq * current_m) / (current_n * pdiv);
				freq_error = 10000 - (dclk_10000 / actual_freq);

				if (freq_error < 0) {
					freq_error = -freq_error;
				}
				if (freq_error < min_error)  {
					best_n = current_n;
					best_m = current_m;
					best_p1 = current_p1;
					min_error = freq_error;
				}
				if (min_error == 0) {
					break;
				}
			}
			if (min_error == 0) {
				break;
			}
		}
		if (min_error == 0) {
			break;
		}
	}
	/*
	 * No clock found that meets error requirement
	 */
	if (min_error > (long)target_error) {
		EMGD_TRACE_EXIT;
		return 1;
	}

	/* Translate M,N,P to m1,m2,n,p register values */
	*n = best_n - 2;
	if (calculate_m1_m2(best_m, m1, m2)) {
		/* No M1, M2 match for M */
		EMGD_TRACE_EXIT;
		return 1;
	}

	p1 = (1 << (best_p1-1));
	/* p2 determined above */
	*p = (  p1 | (p2<<8) );

	EMGD_DEBUG("n:%ld   m1:0x%lx  m2:%ld", *n, *m1, *m2);
	EMGD_DEBUG("p1:%ld  p2:%ld", best_p1, freqmult_p2);
	EMGD_DEBUG("min_error:%ld", min_error);

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 *
 * @param dclk
 * @param ref_freq
 * @param m1
 * @param m2
 * @param n
 * @param p
 * @param target_error
 * @param port_type
 * @param dual_channel
 * @param max_fp
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int get_clock(unsigned long dclk,
	unsigned long ref_freq,
	unsigned long *m1,
	unsigned long *m2,
	unsigned long *n,
	unsigned long *p,
	unsigned long target_error,
	unsigned long port_type,
	unsigned long dual_channel,
	unsigned long max_fp)
{

	EMGD_TRACE_ENTER;

	/* No fixed clock found so calculate one. */
	EMGD_DEBUG("Calculating dynamic clock for clock %ld", dclk);

	/* For all ports work with 96MHz vco, so use it. */
	if (calculate_clock(dclk, ref_freq, m1, m2, n, p, clock_info->min_vco,
			target_error, port_type, dual_channel, max_fp)) {
		/* No usable clock.  Use 640x480@60 as default. */
		EMGD_ERROR("Could not calculate clock %ld, returning default.",dclk);
		*m1 = 0x11;
		*m2 = 0x8;
		*n = 0x3;
		*p = 0x2;
		EMGD_TRACE_EXIT;
		return 1;
	}

	EMGD_TRACE_EXIT;
	return 0;
}


fdi_m_n fdi_mn_pipea_temp_new = {0,0,0,0,0,0,0};
fdi_m_n * fdi_mn_new_pipea = &fdi_mn_pipea_temp_new;
fdi_m_n fdi_mn_pipeb_temp_new = {0,0,0,0,0,0,0};
fdi_m_n * fdi_mn_new_pipeb = &fdi_mn_pipeb_temp_new;

fdi_m_n fdi_mn_pipea_ctx = {0,0,0,0,0,0,0};
fdi_m_n * fdi_mn_curr_pipea = &fdi_mn_pipea_ctx;
fdi_m_n fdi_mn_pipeb_ctx = {0,0,0,0,0,0,0};
fdi_m_n * fdi_mn_curr_pipeb = &fdi_mn_pipeb_ctx;

static void fdi_reduce_ratio(unsigned long *num, unsigned long *den)
{
	while (*num > 0xffffff || *den > 0xffffff) {
		*num >>= 1;
		*den >>= 1;
	}
}

static void gen6_compute_fdi_m_n(int bits_per_pixel, int nlanes, int pixel_clock,
		     int link_clock, fdi_m_n * m_n)
{
	m_n->tu = 64; /* default size */

	/* BUG_ON(pixel_clock > INT_MAX / 36); */
	m_n->gmch_m = bits_per_pixel * pixel_clock;
	m_n->gmch_n = link_clock * nlanes * 8;
	fdi_reduce_ratio(&m_n->gmch_m, &m_n->gmch_n);

	m_n->link_m = pixel_clock;
	m_n->link_n = link_clock;
	fdi_reduce_ratio(&m_n->link_m, &m_n->link_n);
}

static void kms_gn6_fdi_pre_pipe_enable(emgd_crtc_t *emgd_crtc,
				fdi_m_n * curr_fdi_mn_ctx)
{
	igd_display_pipe_t *pipe    = NULL;
	unsigned char      *mmio;
	unsigned int pipe_num;
	unsigned long reg, temp;

	pipe            = PIPE(emgd_crtc);
	pipe_num        = pipe->pipe_num;
	mmio            = MMIO(emgd_crtc);

	/* Write the TU size bits so error detection works */
	EMGD_WRITE32(((curr_fdi_mn_ctx->tu - 1) << 25),
		mmio + FDI_RX_TUSIZE1(pipe_num));

	/* enable PCH FDI RX PLL, wait warmup plus DMI latency */
	reg = FDI_RX_CTL(pipe_num);

	/* clear out bits we wanna touch */
	temp = EMGD_READ32(mmio + reg);
	temp &= ~((0x7 << 19) | (0x7 << 16));

	/* get the num of fdi lanes */
	temp |= (curr_fdi_mn_ctx->num_lanes - 1) << 19;

	/* FIXME: For now, because we currently only support 24BPP and HDMI
	 * we can skip checking the BPC and leave this as default = 8BPC */
	temp |= (PIPE_8BPC << 16);

	/* lets enable the FDI Rx PLL! */
	temp |= FDI_RX_PLL_ENABLE;

	EMGD_WRITE32(temp, mmio + reg);

	EMGD_DEBUG(" FDI_RX_CTL write: 0x%lx", temp);
	udelay(200); /* FIXME - why 200 usecs??? spec says only need 25 */

	/* Switch from Rawclk to PCDclk */
	temp = EMGD_READ32(mmio + reg);
	EMGD_WRITE32(temp | FDI_PCDCLK, mmio + reg);

	EMGD_DEBUG(" FDI_RX_CTL read: 0x%lx", temp);
	udelay(200); /* FIXME - why 200 usecs??? spec says less */

	/* Enable CPU FDI TX PLL, always on for Ironlake */
	reg = FDI_TX_CTL(pipe_num);

	/* clear out bits we wanna touch */
	temp = EMGD_READ32(mmio + reg);
	temp &= ~(0x7 << 19);

	/* get the num of fdi lanes */
	temp |= (curr_fdi_mn_ctx->num_lanes - 1) << 19;

	if ((temp & FDI_TX_PLL_ENABLE) == 0) {
		EMGD_WRITE32(temp | FDI_TX_PLL_ENABLE, mmio + reg);

		EMGD_DEBUG(" FDI_TX_CTL: 0x%lx", temp);
		udelay(100); /* FIXME - why 100 usecs??? spec says only need 10 */
	}
}



static const int const snb_b_fdi_train_param [] = {
	FDI_LINK_TRAIN_400MV_0DB_SNB_B,
	FDI_LINK_TRAIN_400MV_6DB_SNB_B,
	FDI_LINK_TRAIN_600MV_3_5DB_SNB_B,
	FDI_LINK_TRAIN_800MV_0DB_SNB_B,
};


/* The FDI link training functions for SNB/Cougarpoint. */
static void kms_gen6_fdi_link_train(emgd_crtc_t *emgd_crtc,
				fdi_m_n * curr_fdi_mn_ctx)
{
	igd_display_pipe_t *pipe         = NULL;
	igd_display_port_t *port         = NULL;
	unsigned char      *mmio;
	unsigned int pipe_num;
	unsigned long reg, temp, i;

	if(!emgd_crtc) {
		EMGD_ERROR("Invalid NULL parameters: emgd_crtc");
		return;
	}

	if(!curr_fdi_mn_ctx) {
		EMGD_ERROR("Invalid NULL parameters: curr_fdi_mn_ctx");
		return;
	}

	pipe            = PIPE(emgd_crtc);
	pipe_num        = pipe->pipe_num;
	port            = PORT(emgd_crtc);
	mmio            = MMIO(emgd_crtc);

	/*** FDI Train - 1: For Display Port, enable display port first ***/

	if(PD_DISPLAY_DP_INT == port->pd_driver->type){

		EMGD_ERROR("FDI Training codes not supporting Display Port yet!");

		/* FIXME: for DP on PCH, need to enable port before FDI training*/

		/* FIXME: for DP on PCH, setup DP_CTL pre-emphasis voltage swing levels*/
	}

	/*** FDI Train - 2: umask FDI RX Interrupt status bits ***/

	/* unset symbol_lock and bit_lock bit for accurate train result feedback*/
	reg = FDI_RX_IMR(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_RX_SYMBOL_LOCK;
	temp &= ~FDI_RX_BIT_LOCK;
	EMGD_WRITE32(temp, mmio + reg);

	EMGD_READ32(mmio +  reg);
	udelay(150); /* why 150usecs? Spec
		doesnt say anything about this for IMR? */

	/*** FDI Train - 3: enable CPU FDI TX for Pattern 1 Train***/

	/* FDI_TX_CTL, set num of lanes a.k.a. port_width*/
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~(7 << 19);
	temp |= (curr_fdi_mn_ctx->num_lanes - 1) << 19;

	/* FDI_TX_CTL, need to setup the voltage swing and pre-emphasis level */
	temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
	temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;

	/* FDI_TX_CTL, setup for train pattern 1 first */
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_PATTERN_1;

	/* enable the FDI TX side */
	temp |= FDI_TX_ENABLE;
	EMGD_WRITE32(temp, mmio + reg);

	/*** FDI Train - 4: enable PCH FDI RX for Pattern 1 Train***/

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);

	/* FDI_TX_CTL, setup for train pattern 1 first */
	temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
	temp |= FDI_LINK_TRAIN_PATTERN_1_CPT;

	/* enable the FDI RX side */
	temp |=  FDI_RX_ENABLE;
	EMGD_WRITE32(temp, mmio +  reg);
	EMGD_READ32(mmio +  reg);
	/*
	 * FIXME: spec says u need 10usec for CPU FDI and 25 usecs for
	 * PCH FDI warm ups.
	 */
	udelay(150);

	/*** FDI Train - 5: Block on Pattern 1 Train ***/
	for (i = 0; i < 4; i++ ) {
		reg = FDI_TX_CTL(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
		temp |= snb_b_fdi_train_param[i];
		EMGD_WRITE32(temp, mmio + reg);

		EMGD_READ32(mmio +  reg);
		udelay(500); /* why 500usec?, Spec indicates 32usec */

		reg = FDI_RX_IIR(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		EMGD_DEBUG("FDI_RX_IIR 0x%lx", temp);

		if (temp & FDI_RX_BIT_LOCK) {
			EMGD_WRITE32(temp | FDI_RX_BIT_LOCK, mmio + reg);
			EMGD_DEBUG("FDI train 1 done.");
			break;
		}
	}
	if (i == 4) {
		EMGD_ERROR("FDI train 1 fail!");
	}

	/*** FDI Train - 6: enable CPU FDI TX for Pattern 2 Train***/
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);

	/* FDI_TX_CTL, setup the voltage swing and pre-emphasis level */
	temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
	temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;

	/* FDI_TX_CTL, setup for train pattern 2 next*/
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_PATTERN_2;

	EMGD_WRITE32(temp, mmio + reg);

	/*** FDI Train - 7: enable CPU FDI RX for Pattern 2 Train***/
	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);

	/* FDI_RX_CTL, setup for train pattern 2 next*/
	temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
	temp |= FDI_LINK_TRAIN_PATTERN_2_CPT;

	EMGD_WRITE32(temp, mmio + reg);
	EMGD_READ32(mmio +  reg);
	udelay(150); /* why 150usec here?? PCH FDI already warmed up*/

	/*** FDI Train - 8: Block on Pattern 2 Train ***/
	for (i = 0; i < 4; i++ ) {
		reg = FDI_TX_CTL(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
		temp |= snb_b_fdi_train_param[i];
		EMGD_WRITE32(temp, mmio + reg);

		EMGD_READ32(mmio +  reg);
		udelay(500); /* why 500usec?, Spec indicates 32usec */

		reg = FDI_RX_IIR(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		EMGD_DEBUG("FDI_RX_IIR 0x%lx", temp);

		if (temp & FDI_RX_SYMBOL_LOCK) {
			EMGD_WRITE32(temp | FDI_RX_SYMBOL_LOCK, mmio + reg);
			EMGD_DEBUG("FDI train 2 done.");
			break;
		}
	}
	if (i == 4) {
		EMGD_ERROR("FDI train 2 fail!");
	}

	EMGD_DEBUG("FDI train done.");
}

static void kms_gn6_fdi_complete_train(emgd_crtc_t *emgd_crtc)
{
	struct drm_device  *dev          = NULL;
	igd_display_pipe_t *pipe         = NULL;
	igd_context_t      *context      = NULL;
	unsigned char      *mmio;

	unsigned int pipe_num;
	unsigned long reg, temp;

	pipe            = emgd_crtc->igd_pipe;
	pipe_num        = pipe->pipe_num;
	dev             = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context         = ((drm_emgd_private_t *)dev->dev_private)->context;
	mmio            = context->device_context.virt_mmadr;


	/* enable normal train */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_NONE | FDI_TX_ENHANCE_FRAME_ENABLE;
	EMGD_WRITE32(temp, mmio + reg);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
	temp |= FDI_LINK_TRAIN_NORMAL_CPT;
	EMGD_WRITE32(temp | FDI_RX_ENHANCE_FRAME_ENABLE, mmio + reg);

	/* wait one idle pattern time */
	EMGD_READ32(mmio +  reg);
	udelay(1000); //why 1 second wait time here?!
}


static void kms_gen6_fdi_post_pipe_enable(emgd_crtc_t *emgd_crtc,
				fdi_m_n * curr_fdi_mn_ctx)
{
	struct drm_device  *dev          = NULL;
	igd_context_t      *context      = NULL;
	unsigned char      *mmio;

	dev             = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context         = ((drm_emgd_private_t *)dev->dev_private)->context;
	mmio            = context->device_context.virt_mmadr;

	/* For non-DP output, clear any trans DP clock recovery setting.*/
	if (emgd_crtc->igd_pipe->pipe_num == 0) {
		EMGD_WRITE32(0, mmio + TRANSA_DATA_M1);
		EMGD_WRITE32(0, mmio + TRANSA_DATA_N1);
		EMGD_WRITE32(0, mmio + TRANSA_DP_LINK_M1);
		EMGD_WRITE32(0, mmio + TRANSA_DP_LINK_N1);
	} else {
		EMGD_WRITE32(0, mmio + TRANSB_DATA_M1);
		EMGD_WRITE32(0, mmio + TRANSB_DATA_N1);
		EMGD_WRITE32(0, mmio + TRANSB_DP_LINK_M1);
		EMGD_WRITE32(0, mmio + TRANSB_DP_LINK_N1);
	}
}

int kms_gen6_pch_dpll_enable(emgd_crtc_t *emgd_crtc,
		igd_clock_t *clock, unsigned long dclk,
		fdi_m_n * curr_fdi_mn_ctx)
{
	struct drm_device  *dev          = NULL;
	igd_display_port_t *port         = NULL;
	unsigned char      *mmio;

	unsigned long m1, m2, n, p, fp;
	unsigned long control;
	unsigned long ref_freq;
	unsigned long target_error;
	unsigned long mult;
	unsigned long dual_channel;
	unsigned long max_fp;
	unsigned long max_dclk;
	int factor, ret, index;

	if(!emgd_crtc) {
		EMGD_ERROR("Invalid NULL parameters: emgd_crtc");
		return -EINVAL;
	}

	if(!clock) {
		EMGD_ERROR("Invalid NULL parameters: clock");
		return -EINVAL;
	}

	if(!curr_fdi_mn_ctx) {
		EMGD_ERROR("Invalid NULL parameters: curr_fdi_mn_ctx");
		return -EINVAL;
	}

	dev             = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	mmio            = MMIO(emgd_crtc);
	port            = PORT(emgd_crtc);

	/*** PCH DPLL - 1 Data Preparation ***/
	ref_freq = REF_FREQ;
	dual_channel=0;
	factor = 21;		/* M/N ratio for DAC/HDMI/SDVO CB_Tuning */

	/* Depending on the port being targetted, lets set the multiplier, max_fp and max_dclk */
	if(PD_DISPLAY_HDMI_INT == port->pd_driver->type) {
		max_fp   = 225000;
		max_dclk = 225000;
		if (dclk < 25000) {          /* 0 - 25 MHz */
			EMGD_ERROR_EXIT("unsupported dot clock below 25Mhz");
			return -IGD_INVAL;
		} else if (dclk <= 50000) {   /* 25 - 50 MHz */
			mult = 4;
		} else if (dclk <= 100000) {  /* 50 - 100Mhz */
			mult = 2;
		} else if (dclk <= 225000) {  /* 100 - 225Mhz */
			mult = 1;
		} else {                     /* > 225 Mhz MAX*/
			EMGD_ERROR_EXIT("unsupported dot clock above 225Mhz");
			return -IGD_INVAL;
		}
	} else if (IGD_PORT_ANALOG == port->port_type) {
		max_fp   = 350000;
		max_dclk = 350000;
		if (dclk < 25000) {          /* 0 - 25 MHz */
			EMGD_ERROR_EXIT("unsupported dot clock below 25Mhz");
			return -IGD_INVAL;
		} else if (dclk <= 50000) {   /* 25 - 50 MHz */
			mult = 4;
		} else if (dclk <= 100000) {  /* 50 - 100Mhz */
			mult = 2;
		} else if (dclk <= 350000) {  /* 100 - 225Mhz */
			mult = 1;
		} else {                     /* > 225 Mhz MAX*/
			EMGD_ERROR_EXIT("unsupported dot clock above 225Mhz");
			return -IGD_INVAL;
		}
	} else {
		EMGD_ERROR_EXIT("unsupported port driver");
		return -IGD_INVAL;
	}

	mult = 1;
	curr_fdi_mn_ctx->port_mult = mult;

	/* Check for max dclk */
	if (dclk > max_dclk) {
		EMGD_ERROR_EXIT("dot clock exceeded");
		return -IGD_INVAL;
	}
	dclk *= mult;

	if (port->pd_flags & PD_FLAG_CLK_SOURCE) {
		/* For external clock sources always use ref_clock == dclk */
		ref_freq = dclk;
		/* When the clock source is provided externally by the port driver,
		 * the allowed error range is 0. */
		target_error = 0;
	} else {
		target_error = clock_info->target_error;
	}

	/* HDMI port */
	if( (port->pd_driver->type == PD_DISPLAY_HDMI_INT) ||
		(IGD_PORT_ANALOG == port->port_type) ){
		/* calculate the MNP values */
		ret = get_clock(dclk, ref_freq, &m1, &m2, &n, &p, target_error,
			port->port_type, dual_channel, max_fp);
		if (ret) {
			EMGD_ERROR_EXIT("Clock %ld could not be programmed", dclk);
			return -IGD_INVAL;
		}
	} else {
		EMGD_ERROR_EXIT("Unsupported pd_driver-type");
		return -IGD_INVAL;
	}

	/* Program N, M1,and M2 */
	fp =  (n<<16) | (m1<<8) | m2;
	/* Enable autotuning of the PLL clock (if permissible) */
	if (m1 < (factor * n)) {
		fp |= FP_CB_TUNE;
	}
	EMGD_WRITE32(fp, mmio + clock->mnp);

	control = 0;
	/* Enable DPLL, Disable VGA Mode and stick in new P values */
	if (port->port_type == IGD_PORT_LVDS) {
		/* If LVDS set the appropriate bits for mode select */
		control = BIT27 | (p<<clock->p_shift);
		if(port->attr_list) {
			for(index = 0; index < port->attr_list->num_attrs; index++) {
				/* Set spread spectrum and pulse phase */
				if(port->attr_list->attr[index].id == PD_ATTR_ID_SSC) {
					/* Pulse Phase only has valid values between 4 and 13 */
					if(port->attr_list->attr[index].value >= 4 &&
						port->attr_list->attr[index].value <= 13) {
						control |= BIT13 | BIT14;
						/* Set the Pulse Phase to the clock phase specified by the user */
						control |= (port->attr_list->attr[index].value<<9);
					}
					break;
				}
			}
		}
	} else {
		/* else DAC/SDVO-HDMI-DVI mode */
		control = BIT26 | (p<<clock->p_shift);
	}
	/* Set the clock source correctly based on (LVDS/DispPort) PD settings */
	/* FIXME! - check the port attribute for spread spectrum or super spread
	 * spectrum attr and OR-in SSC_CLK or SUPER_SSC_CLK accordingly.
	 */

	/* If it's SDVO/HDMI-DVI/DP port, enable SDVO High Speed Clock */
	if (port->port_type == IGD_PORT_DIGITAL) {
		control |= BIT30;
	}

	/* For first write bits 30:0, then write bit31 */
	EMGD_WRITE32(control, mmio + clock->dpll_control);
	EMGD_WRITE32(EMGD_READ32(mmio + clock->dpll_control)|BIT31,
			mmio + clock->dpll_control);

	/* Wait 150us for the DPLL to stabilize */
	udelay(150); /* why 150 usec? Spec states 50 usecs suffice */

	return 0;
}

/*!
 * program_clock_snb
 *
 * @param emgd_crtc
 * @param clock
 * @param dclk
 *
 * @return 0 on success
 * @return 1 on failure
 */
int program_clock_snb(emgd_crtc_t *emgd_crtc,
		igd_clock_t *clock,
		unsigned long dclk,
		unsigned short operation)
{
	struct drm_device  *dev          = NULL;
	igd_display_pipe_t *pipe         = NULL;
	igd_display_port_t *port         = NULL;
	struct drm_encoder *encoder      = NULL;
	emgd_encoder_t     *emgd_encoder = NULL;
	igd_context_t      *context      = NULL;
	unsigned char      *mmio;

	int i;
	fdi_m_n * temp_fdi_mn_ctx;
	fdi_m_n * live_fdi_mn_ctx;
	unsigned long temp, bps;
	unsigned long target_clock;
	unsigned long reg;
	os_alarm_t timeout;

	/* These are related to FDI link enablement / training */
	int port_is_on_pch, pipe_num;
	int pixel_multiplier = 0; /* = intel_mode_get_pixel_multiplier(adjusted_mode); */
	int lane = 0, link_bw, bpp;

	EMGD_TRACE_ENTER;

	pipe     = emgd_crtc->igd_pipe;
	dev      = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context  = ((drm_emgd_private_t *)dev->dev_private)->context;
	mmio     = context->device_context.virt_mmadr;
	pipe_num = pipe->pipe_num;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if(encoder != NULL) {
			if (((struct drm_crtc *)(&emgd_crtc->base)) == encoder->crtc) {
				emgd_encoder = container_of(encoder, emgd_encoder_t, base);
				port = emgd_encoder->igd_port;
				break;
			}
		}
	}

	if (!port) {
		EMGD_ERROR_EXIT("No port being used.");
		return -IGD_ERROR_INVAL;
	}


	port_is_on_pch = (port->port_features & IGD_IS_PCH_PORT)? 1: 0;

	/*
	 * EagleLake is same as Cantiga so just reuse the code. Not sure how much
	 * the code size will increase for VBIOS.
	 */
	if(!clock_info) {
		for(i = 0; i < CLOCK_TABLE_SIZE; i++) {
			if(context->device_context.did == clock_table[i].did) {
				clock_info = &clock_table[i];
				break;
			}
		}
	}

	/*** INITIAL DATA PREPARATION! ***/

	/*** DATA 1 - choose correct fdi_m_n context ***/
	if (0 == pipe_num) {
		temp_fdi_mn_ctx = fdi_mn_new_pipea;
		live_fdi_mn_ctx = fdi_mn_curr_pipea;
	} else {
		temp_fdi_mn_ctx = fdi_mn_new_pipeb;
		live_fdi_mn_ctx = fdi_mn_curr_pipeb;
	}

	/*** DATA 2 - identify the correct link bw ***/
	/* FDI is a binary signal running at ~2.7GHz, encoding
	 * each output octet as 10 bits. The actual frequency
	 * is stored as a divider into a 100MHz clock, and the
	 * mode pixel clock is stored in units of 1KHz.
	 * Hence the bw of each lane in terms of the mode signal
	 * is:
	 */
	link_bw = 27 * 10000; /* see comments above for explaination */
	/* link_bw is number of thousand (kilo) pixel bytes per second*/

	/*** DATA 3 - bits per pixel ***/
	/* We need to know the bits per channel of the pipe before
	 * calculating the link speed and number of lanes
	 * we should NOT be reading the registers like OTC, we should
	 * be getting this data from the current display FB pixel format
	 * which currently is either 565 or 8888 - for now we dont care
	 * about 2101010 or 16161616. So we can simply assume bpp = 24.
	 */
	EMGD_ERROR("SNB clock - Assuming bits per pipe channel is 24!!!");
	bpp = 24;

	/*** DATA 4 - target dot clock***/
	target_clock = dclk;

	/*** DATA 5 - num of lanes ***/
	/* Account for spread spectrum to avoid
	 * oversubscribing the link. Max center spread
	 * is 2.5%; use 5% for safety's sake.*/
	bps = target_clock * bpp * 21 / 20;/* bps is thousand(kilo) bits per sec */
	lane = bps / (link_bw * 8) + 1;
	/*intel_crtc->fdi_lanes = lane;*/
	temp_fdi_mn_ctx->num_lanes = lane;
	if (pixel_multiplier > 1) {
		link_bw *= pixel_multiplier;
	}

	if(PROGRAM_PRE_PIPE == operation){

		EMGD_DEBUG("CLOCK Programming Pre-pipe");

		/* PRE-PIPE Programming = FDI link */

		/* Part of the FDI link includes the PCH Transcoder
		 * Data and Link M/N / TU values. For these values,
		 * We need to know the link bandwidth and the number
		 * of lanes (where link bandwidth is a function of
		 * the dotclock, bits per pixels). We got this data
		 * from above (DATA prep code). Now we can
		 * calculate M/N values for FDI PCH Rx / CPU Tx regs
		 */

		/*** FDI - 1 Determine the M/N and TUX values */
		gen6_compute_fdi_m_n(bpp, lane, target_clock, link_bw, temp_fdi_mn_ctx);

		/*** FDI - 2 Enable PCH Reference Clock and modulators ***/

		/* Ironlake: try to setup display ref clock before DPLL
		 * enabling. This is only under driver's control after
		 * PCH B stepping, previous chipset stepping should be
		 * ignoring this setting.
		 */
		temp = EMGD_READ32(mmio + PCH_DREF_CONTROL);
		/* Always enable nonspread source */
		temp &= ~DREF_NONSPREAD_SOURCE_MASK;
		temp |= DREF_NONSPREAD_SOURCE_ENABLE;
		temp &= ~DREF_SSC_SOURCE_MASK;
		temp |= DREF_SSC_SOURCE_ENABLE;
		EMGD_WRITE32(temp, mmio + PCH_DREF_CONTROL);

		EMGD_READ32(mmio +  PCH_DREF_CONTROL);
		udelay(200); /* spec says only need 1 usec for PCH ref clk enablement*/

		/*** FDI - 5 if non-eDP, enable PCH FDI RX PLL ***/
		/* This is only for HDMI on SNB PCH. For eDP on SNB CPU
		 * need  to disable CPU FDI tx and PCH FDI rx */
		if(port_is_on_pch) {
			kms_gn6_fdi_pre_pipe_enable(emgd_crtc, temp_fdi_mn_ctx);
		}
	} else if(PROGRAM_POST_PIPE == operation){

		EMGD_DEBUG("CLOCK Programming Post-pipe");

		if(port_is_on_pch) {

			if(PD_DISPLAY_DP_INT == port->pd_driver->type){
				/* FIXME: need to turn on the Display Port before training */
				/*if (has_edp_encoder && !intel_encoder_is_pch_edp(&has_edp_encoder->base)) {
					ironlake_set_pll_edp(crtc, adjusted_mode->clock);
				}*/
			}

			/*** PCH PLL 1 - Train FDI ***/
			/* For PCH output, training FDI link */
			kms_gen6_fdi_link_train(emgd_crtc, temp_fdi_mn_ctx);

			/*** PCH PLL 2 - Configure and Enable PCH DPLL***/
			if (kms_gen6_pch_dpll_enable(emgd_crtc,
					clock, dclk, temp_fdi_mn_ctx)){
				EMGD_ERROR("failed to enable PCH dpll");
			}

			/*** PCH PLL 3 - Enable DPLL to Transcoder (and Port selection) ***/
			temp = EMGD_READ32(mmio + PCH_DPLL_SEL);
			if (pipe_num == 0 && (temp & TRANSA_DPLL_ENABLE) == 0)
				temp |= (TRANSA_DPLL_ENABLE | TRANSA_DPLLA_SEL);
			else if (pipe_num == 1 && (temp & TRANSB_DPLL_ENABLE) == 0)
				temp |= (TRANSB_DPLL_ENABLE | TRANSB_DPLLB_SEL);
			EMGD_WRITE32(temp, mmio + PCH_DPLL_SEL);
			udelay(10);

			if(port->pd_driver->type == PD_DISPLAY_HDMI_INT){
				temp = EMGD_READ32(mmio + 0xC6014);
				temp |= ((temp_fdi_mn_ctx->port_mult - 1) << 9);
				EMGD_WRITE32(temp, mmio + 0xC6014);
			} else if(IGD_PORT_ANALOG == port->port_type){
				temp = EMGD_READ32(mmio + 0xC6014);
				temp |= ((temp_fdi_mn_ctx->port_mult - 1) << 9);
				EMGD_WRITE32(temp, mmio + 0xC6014);
			} else {
				/* have to handle for other ports */
			}
			/*** PCH PLL 4 - Configure Transcoder timings ***/
			EMGD_WRITE32(EMGD_READ32(mmio + HTOTAL(pipe_num)),
					mmio + TRANS_HTOTAL(pipe_num));
			EMGD_WRITE32(EMGD_READ32(mmio + HBLANK(pipe_num)),
					mmio + TRANS_HBLANK(pipe_num));
			EMGD_WRITE32(EMGD_READ32(mmio + HSYNC(pipe_num)),
					mmio + TRANS_HSYNC(pipe_num));
			EMGD_WRITE32(EMGD_READ32(mmio + VTOTAL(pipe_num)),
					mmio + TRANS_VTOTAL(pipe_num));
			EMGD_WRITE32(EMGD_READ32(mmio + VBLANK(pipe_num)),
					mmio + TRANS_VBLANK(pipe_num));
			EMGD_WRITE32(EMGD_READ32(mmio + VSYNC(pipe_num)),
					mmio + TRANS_VSYNC(pipe_num));

			/*** PCH PLL 5 - Mark Complete Training and start pixels ***/
			kms_gn6_fdi_complete_train(emgd_crtc);

			/*** PCH PLL 6 - Enable PCH Transcoder ***/
			reg = TRANSCONF(pipe_num);
			temp = EMGD_READ32(mmio + reg);
			/* make BPC in transcoder be consistent with pipeconf reg */
			temp &= ~PIPE_BPC_MASK;
			temp |= EMGD_READ32(mmio + PIPECONF(pipe_num)) & PIPE_BPC_MASK;
			EMGD_WRITE32(temp | TRANS_ENABLE, mmio + reg);

			/* Wait for Pipe enable/disable, about 50 msec (20Hz). */
			timeout = OS_SET_ALARM(100);
			do {
				temp = EMGD_READ32(mmio + reg) & TRANS_STATE_ENABLE;
				/* Check for timeout */
			} while ((temp != TRANS_STATE_ENABLE) && (!OS_TEST_ALARM(timeout)));

			if (temp != TRANS_STATE_ENABLE) {
				EMGD_ERROR_EXIT("Timeout waiting for transcoder enable status!");
				return -1;
			}

			/*** PCH PLL 7 - Configure Display Port Transcoder ***/
			if(PD_DISPLAY_DP_INT == port->pd_driver->type){
				/* FIXME: need to configure and enable Display Port transcoder */
			}

			/*** PCH PLL 8 - reset M/N/T on Transcoder ***/
			kms_gen6_fdi_post_pipe_enable(emgd_crtc, temp_fdi_mn_ctx);

			OS_MEMCPY(live_fdi_mn_ctx, temp_fdi_mn_ctx, sizeof(fdi_m_n));
		}
	}

	EMGD_TRACE_EXIT;
	return 0;
}

#endif /* defined(CONFIG_SNB) */

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: clocks_snb.c,v 1.1.2.20 2012/04/30 08:41:41 aalteres Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/display/mode/gn6/Attic/clocks_snb.c,v $
 *----------------------------------------------------------------------------
 */
