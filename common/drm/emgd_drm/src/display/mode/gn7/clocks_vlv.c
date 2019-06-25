/*
 *-----------------------------------------------------------------------------
 * Filename: clocks_vlv.c
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
 *  Clock programming for VLV
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.mode

#include <io.h>
#include <ossched.h>
#include <memory.h>

#include <igd_core_structs.h>
#include <dsp.h>
#include <pi.h>

#include <gn7/regs.h>
#include "instr_common.h"
#include "regs_common.h"

#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "drm_emgd_private.h"

/*!
 * @addtogroup display_group
 * @{
 */

#if defined(CONFIG_VLV)
/*===========================================================================
; File Global Data
;--------------------------------------------------------------------------*/
#ifdef CONFIG_FIXED_TABLE

typedef struct _fixed_clock {
	unsigned long dclk;
	unsigned long n;
	unsigned long m1;
	unsigned long m2;
	unsigned long p1;
	unsigned long p2;
}fixed_clock_t;

static fixed_clock_t fixed_clock_table[] =  {
	/* Clock     N     M1  		M2     P1    P2 */
	{108000L,     4,    90, 	0,   2,    10},
	{0xffffffffL, 0x00, 0x00, 0x00, 0x00, 0x00}
}; /*FIXME: Not sure*/

#endif

/* #define TARGET_ERROR 48 */
#define TARGET_ERROR 5000

/*  Definitions for VLV PLL hardware */

/* #define REF_FREQ 96000L  FIXME: VLV Not sure, default VLV is 100 MHz, external is 27 Mhz(TV)*/
#define REF_FREQ 100000L  /*FIXME: VLV Not sure, default VLV is 100 MHz, external is 27 Mhz(TV)*/

#define M1_BITS_MASK    0x00000700  /* bits 10-8=m1-div */
#define M2_BITS_MASK    0x000000FF  /* bits 31-24=m-div */

#define N_BITS_MASK		0x0000F000 /* bits 15:12 n-div select*/

#define P1_BITS_MASK	0x00E00000  /* bits 23-21 */

#define P2_BITS_MASK	0x0001F000  /* bits 20:16 */

#define K_BITS_MASK		0x0F000000   /* bits 27:24 */

#define ENABLE_DCLKP	0x01000000   /* bits 24 */

#define REF_FREQ_SEL_MASK	 0x0D73CC00   /* bit 27: PLLREFSEL Override Enable
										 bit 17:16 PLL Reference Clock Select
												0 0 core ref clock
												0 1 refclka
												1 0	refclk from PLL[X]	
												1 1 alt core ref clock*/ 
#define DG_FILTER_FIXVALUE_MASK 0x001F0077

#define POST_DIV_SEL_BITS_MASK 0x70000000

typedef const struct _min_max {
	unsigned long min;
	unsigned long max;
} min_max_t;



typedef const struct _vlv_limits {
	unsigned long ref_freq;
	min_max_t m1;
	min_max_t m2;
	min_max_t n;
	min_max_t p1;
	min_max_t p2;
	min_max_t vco;

} vlv_limits_t;

/* FIXME: all the limit down here may not be accurate*/
static vlv_limits_t vlv_dac_limits[] =
{
	/*  ref   	m1     	m2    		n     		p1  		p2    		vco */
	{  19200L, 	{2, 3}, {2, 255},	{1, 10}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  27000L, 	{2, 3}, {2, 255},	{1, 10}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  96000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  100000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  120000L, 	{2, 3},	{2, 255},	{1, 10}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  135000L, 	{2, 3},	{2, 255},	{1, 10}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
}; /*Based on Excel sheet.*/

static vlv_limits_t vlv_hdmi_limits[] =
{
	/*  ref   	m1     	m2    		n     		p1  		p2    		vco */
	{  19200L, 	{2, 3}, {11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  27000L, 	{2, 3}, {11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  96000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  100000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{2, 20},	{4000000L, 6000000L}},
	{  120000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  135000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},

}; /*Based on Excel sheet. */

static vlv_limits_t vlv_dp_limits[] =
{
	/*  ref   	m1     	m2    		n     		p1  		p2    		vco */
	{  19200L, 	{2, 3}, {11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  27000L, 	{2, 3}, {11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  96000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  100000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  120000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},
	{  135000L, 	{2, 3},	{11, 156},	{1, 7}, 	{2, 3}, 	{1, 20},	{4000000L, 5994000L}},

}; /*Based on Excel sheet. */


/* Helper functions to send dividers value via IOSF SB */
/* Not needed for now*/
static unsigned long kms_read_dpio_register(
			unsigned char * mmio, unsigned long reg);
static int kms_write_dpio_register(unsigned char * mmio,
			unsigned long reg, unsigned long value);
static unsigned long find_GCD(unsigned long larger_num, unsigned long smaller_num);
static void pipe_protect(emgd_crtc_t *emgd_crtc, bool protect);


/* REGISTER DEFINITIONS for VLV */
/********************************/

/* Sideband Register Offsets - DPLL A */
#define DPLLA_M_K_N_P_DIV       0x800C    /* M-K-N-P-Divisor */
#define DPLLA_REF_CLOCK_SRC		0x8014		/*write this register with 0x0570CC00*/
#define DPLLA_DG_LOOP_FILTER	0x8048		/*Digital Loop Filter*/
#define DPLLA_EN_DCLKP			0x801C		/*Enable dclkp to core*/
#define DPLLA_CML_TLINE			0x804C      /*CML Tline need to set to x8 lane*/

/* Sideband Register Offsets - DPLL B */
#define DPLLB_M_K_N_P_DIV       0x802C    /* M-Divisor */
#define DPLLB_REF_CLOCK_SRC		0x8034		/*write this register with 0x0570CC00*/
#define DPLLB_DG_LOOP_FILTER	0x8068		/*Digital Loop Filter*/
#define DPLLB_EN_DCLKP			0x803C		/*Enable dclkp to core*/
#define DPLLB_CML_TLINE			0x806C      /*CML Tline need to set to x8 lane*/

#define DPLL_SEL_REFCLKA        0x01
#define DPLL_SEL_REFCLKB        0x10

#define DPLL_LANE_CTRL0_REG             0x0220
#define DPLL_LANE_CTRL1_REG             0x0420
#define DPLL_LANE_CTRL2_REG             0x2620
#define DPLL_LANE_CTRL3_REG             0x2820


static int calculate_clock_vlv(
	unsigned long dclk,
	unsigned long ref_freq,
	vlv_limits_t *l,
	unsigned long *m1,
	unsigned long *m2,
	unsigned long *n,
	unsigned long *p1,
	unsigned long *p2,
	unsigned long *post_div_sel,
	unsigned long *actual_dclk,
	unsigned long *target_vco,
	unsigned long port_type,
	unsigned long pd_type)
{

	unsigned long current_m1, current_m2, current_n, current_p1, current_p2;
	unsigned long actual_freq = 0;
	long freq_error = 0, min_error = 0, freq_error_min = 10000;
	unsigned long freq_error_mod, least_freq_error_mod = 0;
	unsigned long ulGCDFactor;
	unsigned long temp_data_rate = 0;
	unsigned long data_rate = 0, fastclk = 0, update_rate = 0;
	
	EMGD_TRACE_ENTER;

	min_error = 100000L;

	*m1 = 0;
	*m2 = 0;
	*n = 0;
	*p1 = 0;
	*p2 = 0;
	*post_div_sel = 0;

	/*
	 * P2 Values
	 * ---------
	 * 1. DP   - P2Div = 10
	 * 2. DAC  - 10 (< 225MHz), 5 (> 225MHz)
	 * 3. HDMI - 10 (< 225MHz), 5 (> 225MHz)
	 * 4. LVDS - 14 (Single Channel)
	 *
	 * P2 Divider value
	 * ----------------
	 * 00 - HDMI/DP/DAC (25M - 225M)  divide by 10 <<spec
	 *      specifies P2=01 which is wrong>>
	 * 01 - DAC (225M - 400M rate)  divide by 5
	 * 10 - LVDS  single (enable the 7x clock)  divide by 14
	 */
	/*FIXME: Yet to be determined.*/
	*post_div_sel = 0x001; /*FIXME: why this bit need to set to 1*/
	data_rate = dclk * 10;
	switch (port_type) {
		case IGD_PORT_ANALOG:
			if (dclk > 225000L){
				EMGD_DEBUG("High Rest DAC data_rate = dclk * 10");
			}
			break;
		case IGD_PORT_DIGITAL:
			if (pd_type == PD_DISPLAY_DP_INT) {
				*post_div_sel = 0x001;			
				data_rate = dclk * 5;
				EMGD_DEBUG("data_rate = dclk * 5");
			} else if (pd_type == PD_DISPLAY_HDMI_INT) {
				*post_div_sel = 0x001;			
				/*data_rate = dclk * 5;*/
				data_rate = dclk * 10;
				EMGD_DEBUG("data_rate = dclk * 5");
			}
			break;
		default:
			EMGD_ERROR("Invalid port type. %lu", port_type);
			EMGD_TRACE_EXIT;
	}


	if (pd_type == PD_DISPLAY_DP_INT){
		if (dclk == RATE_270_MHZ) {
			if (ref_freq == 27000){			
				*m1 = 2;
				*m2 = 100;
				*n= 1;
				*p1= 2;
				*p2= 2;
			} else if(ref_freq == 96000){
				*m1 = 3;
				*m2 = 75;
				*n= 4;
				*p1= 2;
				*p2= 2;
			} else if(ref_freq == 100000){
				*m1 = 2;
				*m2 = 27;
				*n= 1;
				*p1= 2;
				*p2= 2;
			}else {
				EMGD_ERROR ("Clock for DP does not supported.");
			}
 
		} else if (dclk == RATE_162_MHZ) {
			if (ref_freq == 27000)	{		
				*m1 = 2;
				*m2 = 90;
				*n= 1;
				*p1= 3;
				*p2= 2;
			}else if(ref_freq == 96000){
				*m1 = 2;
				*m2 = 76;
				*n= 3;
				*p1= 3;
				*p2= 2;
			}else if(ref_freq == 100000){
				*m1 = 3;
				*m2 = 81;
				*n= 5;
				*p1= 3;
				*p2= 2;
			}else {
				EMGD_ERROR ("Clock for DP does not supported.");
			}
		} else {
			EMGD_ERROR ("Clock for DP does not supported.");
		}
		*target_vco = (ref_freq/(*n)) * (*m1) * (*m2); /*FIXME: Based on formula in excel.Need to re-confirm.*/
		min_error = 0;
		freq_error = freq_error_min;
	} else {

		/*TODO:  Cedarview has different formula fo DP. We are unsure about VLV. 
 		* Will revisit this later. */
		/* Get best m,n,p1,p2 combinations by trying all the valid combinations */
		min_error = TARGET_ERROR;
		least_freq_error_mod = 10000;
 
		for(current_m1 = l->m1.min; current_m1 <= l->m1.max; current_m1 ++) {
			for(current_m2 = l->m2.min; current_m2 <= l->m2.max; current_m2 ++) {
				for(current_n = l->n.min; current_n <= l->n.max; current_n++) {
					update_rate = ref_freq/current_n;
			
					if ((update_rate < 19200) || (update_rate > 135000)){
						continue;
					}

					*target_vco = (update_rate) * current_m1 * current_m2; /*FIXME: Based on formula in excel.Need to re-confirm.*/
					if  ((*target_vco < l->vco.min) ||(*target_vco > l->vco.max)) {
						continue;
					}

					for(current_p1 = l->p1.min; current_p1 <= l->p1.max; current_p1++) {
						for(current_p2 = l->p2.min; current_p2 <= l->p2.max; current_p2++) {
							if ((current_p2 == 11) || 
								(current_p2 == 13) ||
								(current_p2 == 15) ||
								(current_p2 == 17) ||
								(current_p2 == 19) || current_p2 >= 21){
								/* Reserved value */
								continue;
							}
							fastclk = (*target_vco)/(current_p1*current_p2);
							/* Fast clock minimum is 124.8 Mhz but max is 
							 * different between DAC and HDMI */
							if (fastclk < 124800) { 
								continue;
							}
							if (port_type == IGD_PORT_ANALOG) {
								/* this is not in modphy excel spec - its a guess */
								if (fastclk > 2650000) {
									continue;
								}
							} else if (fastclk > 1804800 ) {
								continue;
							}

							actual_freq = 2 * (fastclk); /*From excel sheet*/

							/* Minimum clocks are at 25Mhz, but maximum is different 
							 * for HDMI vs DAC */
							if (actual_freq < 249600) {
								continue;
							}
							if (port_type == IGD_PORT_ANALOG) {
								/* this is not in modphy excel spec - its a guess */
								if( actual_freq > 3500000){
									continue;
								}
							} else if (actual_freq > 1804800 ) {
								continue;
							}

							if (data_rate <= actual_freq) {
								freq_error = actual_freq - data_rate;
							} else if (actual_freq < data_rate) {
								freq_error = data_rate - actual_freq;
							}

							/*Based on Excel sheet error more than 9MHz never exist.*/
							if (freq_error > 9000){
								continue;
							}

							/* From UMG's code: As per experiment 
 							 * it was observed that for dot clocks
		 					 * greater than 350Mhz, the 32 bit not was getting overflowed
	 						 * because of multiplication with with 1000. Added GCD for
							 * modes greater that 350M to solve this issue.
	 						 * This is to cover the decimal accuracy
							 * because of non-floating point calculation
		 					 */
							freq_error_mod = freq_error;
							ulGCDFactor = 1;
							if(data_rate >= 1290000)
							{
								ulGCDFactor = find_GCD(freq_error,actual_freq);
							}

						   	freq_error = freq_error/ulGCDFactor;
						   	temp_data_rate = data_rate/ulGCDFactor;
						
							/* Prevent data get overflow causing the invalid value freq_error */	
							if(freq_error > 4294){
								/* "Potential overflow in clock calculation." */
								if(port_type == IGD_PORT_ANALOG){
									/*EMGD_ERROR("freq error OVERFLOW?!");*/
								}
								continue;
							}

						    /* From UMG's codes: (eq 1)tries to find the error based upon
		 					 * the difference between difference of actual and target freq 
	 						*/
							freq_error = 1000000*freq_error/temp_data_rate;

							if (freq_error > TARGET_ERROR){
								continue;
							}

							if ((freq_error < min_error)) {				
								/* values that get into registers */
								*m1 = current_m1;
								*m2 = current_m2;
								*n = current_n;
								*p1 = current_p1;
								*p2 = current_p2;
								freq_error_min = freq_error;
								least_freq_error_mod = freq_error_mod;
								min_error = freq_error;
							
							} else if ((freq_error == freq_error_min) &&
					   			(freq_error_mod <= least_freq_error_mod)){
								/* values that get into registers */
								*m1 = current_m1;
								*m2 = current_m2;
								*n = current_n;
								*p1 = current_p1;
								*p2 = current_p2;

								least_freq_error_mod = freq_error_mod;
								min_error = freq_error;

							}

						} /*p2*/
					}/*p1*/
				} /*n*/
			} /*m2*/
		} /*m1*/
	}
	/*
	 * No clock found that meets error requirement
	 */
	if (freq_error < freq_error_min) {
		EMGD_ERROR ("No clock found.");
		EMGD_TRACE_EXIT;
		return 1;
	}

	EMGD_DEBUG("PLL--> dclk= %lu, n = %lu, m1= %lu, m2 = %lu, p1= %lu, p2=%lu, ref_clk=%lu\n  ", dclk, *n, *m1, *m2, *p1, *p2, ref_freq );
	EMGD_DEBUG ("Actual Frequency = %ld, data_rate requested = %ld", (2 *( ( (ref_freq / (*n)) * (*m1) * (*m2)  ) /((*p1) * (*p2))) ), data_rate);
	EMGD_DEBUG("min_error:%ld", min_error);
	EMGD_TRACE_EXIT;
	return 0;
}



static int get_clock_vlv(
	unsigned long dclk,
	unsigned long ref_freq,
	vlv_limits_t *l,
	unsigned long *m1,
	unsigned long *m2,
	unsigned long *n,
	unsigned long *p1,
	unsigned long *p2,
	unsigned long *post_div_sel,
	unsigned long *actual_dclk,
	unsigned long *target_vco,
	unsigned long port_type,
	unsigned long pd_type)
{
#ifdef CONFIG_FIXED_TABLE
	fixed_clock_table *fixed;
	EMGD_TRACE_ENTER;
	/* Enable this if calculated values are wrong and required fixed table */

	/* First check for a fixed clock from the table. These are ones that
	 * can't be calculated correctly. */
	while (fixed->dclk != 0xffffffff) {
		if (fixed->dclk == dclk) {
			EMGD_DEBUG("Using Fixed Clock From table for clock %ld", dclk);
			*m = fixed->m1;
			*m = fixed->m2;
			*n = fixed->n;
			*p1 = fixed->p1;
			*p2 = fixed->p2;
			EMGD_TRACE_EXIT;
			return 0;
		}
		fixed++;
	}
#endif

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("Calculating dynamic clock for clock %ld", dclk);

	if (calculate_clock_vlv(dclk, ref_freq, l, m1, m2, n, p1, p2,
		post_div_sel,
		actual_dclk, target_vco, port_type, pd_type)) {
		EMGD_ERROR("Could not calculate clock %ld, returning default.", dclk);
		EMGD_TRACE_EXIT;
		return 1;
	}

	EMGD_TRACE_EXIT;
	return 0;

}


int program_lane_vlv(emgd_crtc_t *emgd_crtc, igd_clock_t *clock){
	igd_display_pipe_t *pipe         = NULL;
	igd_display_port_t *port         = NULL;
	unsigned char      *mmio         = NULL;
	unsigned long 		temp 		 = 0x0;	

	if(!emgd_crtc) {
		EMGD_ERROR("Invalid NULL parameters: emgd_crtc");
		return -EINVAL;
	}

	if(!clock) {
		EMGD_ERROR("Invalid NULL parameters: clock");
		return -EINVAL;
	}

	pipe     = PIPE(emgd_crtc);
	mmio     = MMIO(emgd_crtc);
	port     = PORT(emgd_crtc);

	if (port->pd_driver->type == PD_DISPLAY_HDMI_INT) {
		
		/* Program Lane for HDMI */
		if(port->port_number == IGD_PORT_TYPE_SDVOB && clock->dpll_control == DPLLACNTR){
			kms_write_dpio_register(mmio, 0x0220, 0x001000C4);
			kms_write_dpio_register(mmio, 0x0420, 0x001000C4);
			kms_write_dpio_register(mmio, 0x8220, 0x001000C4);
		}
		else if(port->port_number == IGD_PORT_TYPE_SDVOC && clock->dpll_control == DPLLBCNTR){
			kms_write_dpio_register(mmio, 0x2620, 0x003000C4);
			kms_write_dpio_register(mmio, 0x2820, 0x003000C4);
			kms_write_dpio_register(mmio, 0x8420, 0x003000C4);
		}
		else if(port->port_number == IGD_PORT_TYPE_SDVOC && clock->dpll_control == DPLLACNTR){
			kms_write_dpio_register(mmio, 0x2620, 0x001000C4);
			kms_write_dpio_register(mmio, 0x2820, 0x001000C4);
			kms_write_dpio_register(mmio, 0x8420, 0x001000C4);
		}
		else if(port->port_number == IGD_PORT_TYPE_SDVOB && clock->dpll_control == DPLLBCNTR){
			kms_write_dpio_register(mmio, 0x0220, 0x003000C4);
			kms_write_dpio_register(mmio, 0x0420, 0x003000C4);
			kms_write_dpio_register(mmio, 0x8220, 0x003000C4);
		}
	}

	/* Program Lane for DP*/
	if(port->pd_driver->type == PD_DISPLAY_DP_INT) {
		if(clock->dpll_control == DPLLBCNTR){
			/* PIPE_B	*/
			EMGD_DEBUG ("Programming Pipe B");
			temp = kms_read_dpio_register(mmio, DPLL_LANE_CTRL2_REG);
			temp |= BIT(20);
			kms_write_dpio_register(mmio, DPLL_LANE_CTRL2_REG, temp);

			temp = kms_read_dpio_register(mmio, DPLL_LANE_CTRL3_REG);
			temp |= BIT(20);
			kms_write_dpio_register(mmio, DPLL_LANE_CTRL3_REG, temp);

			if(port->port_number == IGD_PORT_TYPE_DPB){
				kms_write_dpio_register(mmio, 0x8220, 0x003000C4);
			} else {
				kms_write_dpio_register(mmio, 0x8420, 0x003000C4);
			}
				
		} else { 
			/* PIPE_A	*/
			EMGD_DEBUG ("Programming Pipe A");
			temp = kms_read_dpio_register(mmio, DPLL_LANE_CTRL0_REG);
			temp |= BIT(20);
			kms_write_dpio_register(mmio, DPLL_LANE_CTRL0_REG, temp);

			temp = kms_read_dpio_register(mmio, DPLL_LANE_CTRL1_REG);
			temp |= BIT(20);
			kms_write_dpio_register(mmio, DPLL_LANE_CTRL1_REG, temp);

			if(port->port_number == IGD_PORT_TYPE_DPB){
				kms_write_dpio_register(mmio, 0x8220, 0x001000C4);
			} else {
				kms_write_dpio_register(mmio, 0x8420, 0x001000C4);
			}
		
		}

	}
	return 0;
}


int program_clock_vlvonvlv(emgd_crtc_t *emgd_crtc,
	igd_clock_t *clock, unsigned long dclk,
	unsigned short operation)
{

	igd_display_pipe_t *pipe         = NULL;
	igd_display_port_t *port         = NULL;
	unsigned char      *mmio         = NULL;

	/*VLV code below.*/
	unsigned long m1, m2, n, temp;
	unsigned long div_regs = 0L, en_idclkp = 0L;
	unsigned long ref_clk_src = 0L, dg_loop_filter = 0L;
	unsigned long cml_tline = 0L;
	unsigned long p1, p2;
	unsigned long ref_freq, actual_dclk;
	unsigned long target_vco;
	unsigned long dpllmd = 0;
	unsigned long mult = 0;
	unsigned long count;
	unsigned long post_div_sel = 0L;
	vlv_limits_t *l = NULL;
	int ret;
	unsigned long dpio_ctl, dpll;
	os_alarm_t timeout;
	struct drm_device  *dev          = NULL;
	igd_context_t      *context      = NULL;

	EMGD_TRACE_ENTER;

	pipe     = PIPE(emgd_crtc);
	mmio     = MMIO(emgd_crtc);
	port     = PORT(emgd_crtc);

	dev             = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context         = ((drm_emgd_private_t *)dev->dev_private)->context;

	if (!port) {
		EMGD_ERROR_EXIT("No port being used.");
		return -IGD_ERROR_INVAL;
	}

	/* Write protect key off. */
	pipe_protect(emgd_crtc, FALSE);


	/* FIXME:Need a way to tell which Ref Freq are we using on VLV. */
	ref_freq = REF_FREQ; /*Default 96 MHz*/

	/* LVDS reference clock can be 96 or 100 MHz. However there is
	 * no mention in the specification to specify which register
	 * to select/set this.
	 */

	/* This is from VLV Display BSpec
	 * There are two external differential inputs to the DPIO, 
	 * where each of the DPLLs can use any of the two 
	 * as reference clock (more options as internal 
	 * reference clock are for DFT modes). External clk
	 * src will be connected to CK505 that can supply 25Mhz
	 * and internal clock will supply 100Mhz reference. 
	 * Internal clocks - 2 types - (SSC and Bend).
	 * Can configure either for any port via DPIO. 
	 */

	/* For DP, there are only 2 target dclk
	 * on a range of frequency.
	 * FIXME: Not sure still the same with VLV. */
	if(port->pd_driver->type == PD_DISPLAY_DP_INT) {
		EMGD_DEBUG ("PLL ref clock for DP is 100,000.");
		ref_freq = 100000L;
	} else if((port->pd_driver->type == PD_DISPLAY_TVOUT_EXT)||
			(port->pd_driver->type == PD_DISPLAY_HDMI_EXT)) {
		ref_freq = 27000L;
	}
	/*FIXME: VLV have more to consider for ref_freq */
  	 
	if(port->pd_driver->type == PD_DISPLAY_HDMI_INT) {
  		mult = 1;
	}

	switch (port->port_type) {
		case IGD_PORT_ANALOG:
			for(count=0; count<6; count++) {
				if (vlv_dac_limits[count].ref_freq == ref_freq) {
					l = &vlv_dac_limits[count];
					break;
				}
			}
			break;
		case IGD_PORT_DIGITAL:
			if (port->pd_driver->type == PD_DISPLAY_DP_INT) {
				for (count=0; count<6; count++) {
					if (vlv_dp_limits[count].ref_freq == ref_freq) {
						l = &vlv_dp_limits[count];
						break;
					}
				}
			} else if (port->pd_driver->type == PD_DISPLAY_HDMI_INT) {
				for (count=0; count<6; count++) {
					if (vlv_hdmi_limits[count].ref_freq == ref_freq) {
						l = &vlv_hdmi_limits[count];
						break;
					}
				}
			}
			break;
	}

	if (l == NULL){
		EMGD_ERROR("ref_freq is not found in the table.");
		return 1;
	}

	/* For all other display types, calculate the MNP values */
	ret = get_clock_vlv(dclk, ref_freq, l, &m1, &m2, &n, &p1, &p2,
		&post_div_sel,
		&actual_dclk, &target_vco, port->port_type,port->pd_driver->type);
	if (ret) {
		EMGD_ERROR_EXIT("Clock %ld could not be programmed", dclk);
		return ret;
	}

	/* Disable DPLL */
	temp = READ_MMIO_REG_VLV(mmio, clock->dpll_control);
	temp &= ~BIT31;
	WRITE_MMIO_REG_VLV(temp, mmio, clock->dpll_control);
	udelay(150);

	/* Need to set the DPIO reference clock BIT29 for side band to work */
	temp= READ_MMIO_REG_VLV(mmio, clock->dpll_control);
	temp|= 0x20000000;
	WRITE_MMIO_REG_VLV(temp, mmio, clock->dpll_control);
	
	dpio_ctl = READ_MMIO_REG_VLV(mmio, DPIO_RESET_REG);
	EMGD_DEBUG("READ #1 DPIO_RESET_REG=0x%lx", dpio_ctl);
	if (!(dpio_ctl & 0x1)){
		WRITE_MMIO_REG_VLV(0x00, mmio, DPIO_RESET_REG);
		WRITE_MMIO_REG_VLV(0x01, mmio, DPIO_RESET_REG);
		dpio_ctl = READ_MMIO_REG_VLV(mmio, DPIO_RESET_REG);
		EMGD_DEBUG("READ #2 DPIO_RESET_REG=0x%lx", dpio_ctl);
	}
	temp = READ_MMIO_REG_VLV(mmio, DPIO_RESET_REG);

	if (clock->dpll_control == DPLLACNTR) {
		dpll = 0x70002000;
	} else {
		dpll = 0x70006000;
	}
		
	WRITE_MMIO_REG_VLV(dpll, mmio, clock->dpll_control);
	timeout = OS_SET_ALARM(200);
	do {
		temp = READ_MMIO_REG_VLV(mmio, clock->dpll_control) & BIT15;
	} while ((temp != 0x0) && (!OS_TEST_ALARM(timeout)));

	if (temp != 0) {
		EMGD_ERROR("Wait for DPLL unlock timed out!");
	}

	/* Map sideband address registers for divisor programming based on
	 * DPLL A or DPLL B */
	if (clock->dpll_control == DPLLACNTR) {
		div_regs = DPLLA_M_K_N_P_DIV;
		ref_clk_src = DPLLA_REF_CLOCK_SRC;		/*write this register with 0x0570CC00*/
		dg_loop_filter = DPLLA_DG_LOOP_FILTER;		/*Digital Loop Filter*/
		en_idclkp = DPLLA_EN_DCLKP;
		cml_tline = DPLLA_CML_TLINE;
	} else if (clock->dpll_control == DPLLBCNTR) {
		div_regs = DPLLB_M_K_N_P_DIV;
		ref_clk_src = DPLLB_REF_CLOCK_SRC;		/*write this register with 0x0570CC00*/
		dg_loop_filter = DPLLB_DG_LOOP_FILTER;		/*Digital Loop Filter*/
		en_idclkp = DPLLB_EN_DCLKP;
		cml_tline = DPLLB_CML_TLINE;
	}


	/* Program DDI0/DDI1 Tx Lane Resets set to default
	 *  (w/a for HDMI turn off flicker issue). This
	 *   will be re-checked on VLV A0 post power on.
	 */
    if (port->port_number == IGD_PORT_TYPE_SDVOB) {
		kms_write_dpio_register(mmio, 0x8200, 0x00010080);
		kms_write_dpio_register(mmio, 0x8204, 0x00600060);
	}
    if (port->port_number == IGD_PORT_TYPE_SDVOC) {
		kms_write_dpio_register(mmio, 0x8400, 0x00010080);
		kms_write_dpio_register(mmio, 0x8404, 0x00600060);
	}

	/* This is workaround for B0. */
	if ((port->port_number == IGD_PORT_TYPE_DPC) || 
			(port->port_number == IGD_PORT_TYPE_SDVOC)) {
		kms_write_dpio_register(mmio, 0x8430, 0x00750F00);
		kms_write_dpio_register(mmio, 0x84AC, 0x00001500);
		kms_write_dpio_register(mmio, 0x84B8, 0x40400000);

	} else if ((port->port_number == IGD_PORT_TYPE_DPB) ||
		(port->port_number == IGD_PORT_TYPE_SDVOB))  {
		kms_write_dpio_register(mmio, 0x8230, 0x00750F00);
		kms_write_dpio_register(mmio, 0x82AC, 0x00001500);
		kms_write_dpio_register(mmio, 0x82B8, 0x40400000);
	}

	/* to turn on all lanes for DP/eDP. */
	if (port->port_number == IGD_PORT_TYPE_DPC) {
		kms_write_dpio_register(mmio, 0x2600, 0x10080);
		kms_write_dpio_register(mmio, 0x2604, 0x600060);
		kms_write_dpio_register(mmio, 0x2800, 0x10080);
		kms_write_dpio_register(mmio, 0x2804, 0x600060);
	} else if (port->port_number == IGD_PORT_TYPE_DPB)  {
		kms_write_dpio_register(mmio, 0x200, 0x10080);
		kms_write_dpio_register(mmio, 0x204, 0x600060);
		kms_write_dpio_register(mmio, 0x400, 0x10080);
		kms_write_dpio_register(mmio, 0x404, 0x600060);
	}
	/*BIT 12: Fast PLL Lock Timer Disabled, set to 0. DPIO_FASTCLK_DISABLE*/
	/*BIT 5: PLL Adaptive clocking, ACA for bit 5 is currently not needed, set to 1 to Bypass ACA.
 	* Adaptive clocking means u can change m1, n1, p etc
 	* Without disabling DPLL. 	
 	* BIT 4: SFR State Bypass, set to 1 to bypass SFR*/
	kms_write_dpio_register(mmio, 0x8100, 0x610); /* 0x630 */

	/* Program 0x8048/8068 Digital loop Filter, we just hard code here.*/
	temp = 0x009f0003; /*VLV2 hardcoded value from excel sheet*/
	switch(port->pd_driver->type) {
		case PD_DISPLAY_DP_INT:
			if(dclk ==  RATE_270_MHZ) {
				/* TODO: Check if DP/eDP port driver is already handling this
				 * if HBR mode, then set dg_loop_filter to 0x00D0000F
				 */
				EMGD_DEBUG ("LOOP FILTER: DP 270 MHz");
				temp = 0x00D0000F;
			}
			break;
		case PD_DISPLAY_HDMI_INT:
			if(context->device_context.did == PCI_DEVICE_ID_VGA_VLV2 &&
				context->device_context.rid == 2) {
				/* VLV2 A0 and B0/B1/B1 have similar device ID (0xf31) but different
				 * rev ID. If the rev ID is A0, let's use the older value here */
				temp = 0x005f0051; /*VLV2 hardcoded value from excel sheet*/
			}
			break;
	}
	kms_write_dpio_register(mmio, dg_loop_filter, temp);
	EMGD_DEBUG("%s: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx", port->port_name, dg_loop_filter, temp);


	/*FIXME: ref clock select here for VLV.*/
	/*0x8014/8034 bit 27 - PLLREFSEL Override Enable. Default 1 - core clock 
 	* 17:16 PLL Reference Clock Select Harcode 0x00
 	* The rest we hardcoded to 0x0570CC00 .
 	* Both need to be written for VLV2. 
 	* The value should be from the choice for Ref Clock Source.*/
	if (clock->dpll_control == DPLLACNTR){
		switch(port->pd_driver->type) {
			case PD_DISPLAY_DP_INT:
				temp = 0x0DF40000; //using iCLK - ssc
				break;
			default:
				temp = 0x0DF70000; //using iCLK - bend
				break;
		}
	} else {
		switch(port->pd_driver->type) {
			case PD_DISPLAY_DP_INT:
				temp = 0x0DF70000; //using iCLK - ssc
				break;
			default:
				temp = 0x0DF40000; //using iCLK - bend
				break;
		}
	}
	kms_write_dpio_register(mmio, ref_clk_src, temp);
	EMGD_DEBUG("%s: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx", port->port_name, (unsigned long)  ref_clk_src, temp);

	/* Program Divider M K N P 0x800C*/
	EMGD_DEBUG ("PLL --> p1 = %lu, p2 = %lu, n = %lu, m1 = %lu, m2 = %lu"
		, p1, p2, n, m1, m2);

	temp  = 0x01000800;
	temp |= 0x70000000 & (post_div_sel << 28);
	temp |= 0x00E00000 &  (p1 << 21)  ;
	temp |= 0x001F0000 & (p2 << 16) ;
	temp |= 0x0000F000 & (n <<  12);
	temp |= 0x00000700 & (m1 << 8);
	temp |= 0xFF & (m2 << 0);
		

	kms_write_dpio_register(mmio, div_regs, temp);
	EMGD_DEBUG("%s: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx", port->port_name, div_regs, temp);


	
	/*Enable bit 24, Enable idclkp.*/
	temp = 0x01C00000; /*Enable bit 24, Enable idclkp.*/
	kms_write_dpio_register(mmio, en_idclkp, temp);
	EMGD_DEBUG("%s: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx", port->port_name, en_idclkp, temp);

	temp = 0x87871000; 
	kms_write_dpio_register(mmio, cml_tline, temp);
	EMGD_DEBUG("%s: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx", port->port_name, cml_tline, temp);

	/* FIXME : Is this a HACK? */
	dpll = READ_MMIO_REG_VLV(mmio, clock->dpll_control);
	dpll |= (1 << 31);
	WRITE_MMIO_REG_VLV(dpll, mmio, clock->dpll_control);

	timeout = OS_SET_ALARM(200);
	do {
		temp = READ_MMIO_REG_VLV(mmio, clock->dpll_control) & BIT15;
	} while ((temp == 0x00) && (!OS_TEST_ALARM(timeout)));

	if (temp == 0) {
		EMGD_ERROR("Wait for DPLL lock timed out!");
	}

	temp = READ_MMIO_REG_VLV(mmio, clock->dpll_control);
	EMGD_DEBUG("DPLL_CTRL(read): 0x%lx = 0x%lx", clock->dpll_control, temp);


	program_lane_vlv(emgd_crtc, clock);
	if (port->pd_driver->port_clock){
		unsigned long flags = (1<<pipe->pipe_num); 
		ret = port->pd_driver->port_clock(
		port->pd_context,
		pipe->timing,
		dclk,
		flags,
		port->bits_per_color);

		if (ret != 0){
			EMGD_ERROR("Port Clock problem");
		}
	}

	/* Tx buffer enable staggering (to avoid all Tx buffers waking up
	 *  at same time which may cause supply noise induced jitter issues). 
	 */
	if (port->port_number == IGD_PORT_TYPE_SDVOB || port->port_number == IGD_PORT_TYPE_DPB) {
		kms_write_dpio_register(mmio, 0x0230, 0x00400F00);
		kms_write_dpio_register(mmio, 0x0430, 0x00540F00);

        } else if (port->port_number == IGD_PORT_TYPE_SDVOC || port->port_number == IGD_PORT_TYPE_DPC) {
		kms_write_dpio_register(mmio, 0x2630, 0x00400F00);
		kms_write_dpio_register(mmio, 0x2830, 0x00540F00);
	}
	/* HDMI setting, 1.0V 0dB and there is no training
	 *  needed here as in DP/eDP one swing value would suffice.
	 */
	if (port->port_number == IGD_PORT_TYPE_SDVOB){
                kms_write_dpio_register(mmio, 0x8294, 0x00000000);//reset calcinit to 0
                kms_write_dpio_register(mmio, 0x8290, 0x2B245F5F);//set 3.5dB de-emph to be equal for both full rate and half rate
                kms_write_dpio_register(mmio, 0x8288, 0x5578B83A);//set 1.0V swing settings
                kms_write_dpio_register(mmio, 0x828c, 0x0C782040);//disable scaled comp method if already set
                kms_write_dpio_register(mmio, 0x0690, 0x2B247878);//disable deemph for HDMI clock
                kms_write_dpio_register(mmio, 0x822c, 0x00030000);//setup PCS control over-ride
                kms_write_dpio_register(mmio, 0x8224, 0x00002000);//enable 0dB de-emph
                kms_write_dpio_register(mmio, 0x8294, 0x80000000);//enable calcinit
        } else if (port->port_number == IGD_PORT_TYPE_SDVOC){
                kms_write_dpio_register(mmio, 0x8494, 0x00000000);//reset calcinit to 0
                kms_write_dpio_register(mmio, 0x8490, 0x2B245F5F);//set 3.5dB de-emph to be equal for both full rate and half rate
                kms_write_dpio_register(mmio, 0x8488, 0x5578B83A);//set 1.0V swing settings
                kms_write_dpio_register(mmio, 0x848c, 0x0C782040);//disable scaled comp method if already set
                kms_write_dpio_register(mmio, 0x2A90, 0x2B247878);//disable deemph for HDMI clock
                kms_write_dpio_register(mmio, 0x842c, 0x00030000);//setup PCS control over-ride
                kms_write_dpio_register(mmio, 0x8424, 0x00002000);//enable 0dB de-emph
                kms_write_dpio_register(mmio, 0x8494, 0x80000000);//enable calcinit
        }

	/* Program the Multiplier after enabling and warming up */
	if(port->pd_driver->type == PD_DISPLAY_HDMI_INT) {
		dpllmd = (mult - 1) | ((mult - 1) << 8);
		EMGD_DEBUG ("Program DPLLMD with 0x%08X", (unsigned int)dpllmd);
		WRITE_MMIO_REG_VLV(dpllmd, mmio, clock->dpll_control + 8);
	}

	EMGD_TRACE_EXIT;
	return 0;
}

int program_clock_vlv(emgd_crtc_t *emgd_crtc,
	igd_clock_t *clock, unsigned long dclk,
	unsigned short operation)
{
	struct drm_device  *dev          = NULL;
	igd_context_t      *context      = NULL;

	dev             = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	context         = ((drm_emgd_private_t *)dev->dev_private)->context;

	return program_clock_vlvonvlv(emgd_crtc, clock, dclk, operation);

}

static void pipe_protect(emgd_crtc_t *emgd_crtc, bool protect)
{
	unsigned long reg = 0 ;
	unsigned long pipe_reg_offset = 0x0;
	igd_display_pipe_t *pipe         = NULL;
	unsigned char      *mmio         = NULL;

	EMGD_TRACE_ENTER;
	
	pipe     = PIPE(emgd_crtc);
	mmio     = MMIO(emgd_crtc);

	if (pipe->clock_reg->dpll_control == DPLLACNTR){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}

	reg = READ_MMIO_REG_VLV(mmio, pipe_reg_offset + 4);

	if(FALSE == protect) {
		EMGD_DEBUG ("Turn off write protect.");
		reg = 0xABCD0000 | (reg & 0x0000FFFF);
	} else if(TRUE == protect) {
		EMGD_DEBUG ("Turn on write protect.");
		reg = reg & 0x0000FFFF;
	}

	WRITE_MMIO_REG_VLV(reg, mmio, pipe_reg_offset + 4);
	EMGD_TRACE_EXIT;
}

/*!
 *  * Routine to calculate GCD
 *   * @param larger_num
 *    * @param smaller_num
 *     *
 *      */
static unsigned long find_GCD(unsigned long larger_num, unsigned long smaller_num)
{
	if (smaller_num == 0)
		return larger_num;
	return (find_GCD(smaller_num, (larger_num % smaller_num)));
}






/*!
 * Helper function to access DPIO register
 * @param display
 * @param reg
 * @param value
 *
 * @return value from DPIO register
 */
static unsigned long kms_read_dpio_register(unsigned char *mmio,
		   unsigned long reg)
{
	unsigned long temp, value;
	os_alarm_t timeout;

	/* Polls SB_PACKET_REG and wait for Busy bit (bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Timeout waiting for Busy bit to be cleared!! Program clock might not work properly");
		return -1;
	}

	/* Write SB_ADDRESS_REG with destination offset */
	WRITE_MMIO_REG_VLV(reg, mmio, SB_ADDRESS_REG);

	/* Construct SB_Packet_REG
	 * SidebandRid = 0
	 * SidebandPort = 0x88 << 8
	 * SidebandByteEnable = 0xF << 4
	 * SidebandOpcode = 0 (READ) << 16
	 */
	/* WRITE_MMIO_REG_VLV(0x88F0L, mmio, SB_PACKET_REG);*/
	WRITE_MMIO_REG_VLV((0x00<<24 )|(0x0<<16)|(0x12<<8)|(0xF<<4), mmio, SB_PACKET_REG);

	/* Polls SB_PACKET_REG and wait for Busy bit (bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Timeout waiting for Busy bit to be cleared!! Program clock might not work properly");
		return -1;
	}

	/* Read SB_DATA_REG to obtain desired register data */
	value = READ_MMIO_REG_VLV(mmio, SB_DATA_REG);

	return value;

}

/*!
 * Helper function to write DPIO register
 * @param display
 * @param reg
 * @param value
 *
 * @return 0 on success
 */
static int kms_write_dpio_register(unsigned char * mmio,
		   unsigned long reg, unsigned long value)
{
	unsigned long temp;
	os_alarm_t timeout;

	/* Polls SB_PACKET_REG and wait for Busy bit(bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Timeout waiting for Busy bit to be cleared!! Program clock might not work properly");
		return -1;
	}

	/* Write SB_ADDRESS_REG with destination offset */
	WRITE_MMIO_REG_VLV(reg, mmio, SB_ADDRESS_REG);
	/* Write the value to be written to SB_DATA_REG */
	WRITE_MMIO_REG_VLV(value, mmio, SB_DATA_REG);

	/* Construct SB_Packet_REG
	 * SidebandRid = 0
	 * SidebandPort = 0x88 << 8
	 * SidebandByteEnable = 0xF << 4
	 * SidebandOpcode = 1 (WRITE) << 16
	 */
	/* WRITE_MMIO_REG_VLV(0x188F0L, mmio, SB_PACKET_REG);*/
	WRITE_MMIO_REG_VLV((0x0<<24)|(0x1<<16)|(0x12<<8)|(0xf<<4) , 
		mmio, SB_PACKET_REG);	
	return 0;
}


#endif

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: clocks_vlv.c,v 1.1.2.47 2012/06/21 06:20:13 slim50 Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/display/mode/gn7/Attic/clocks_vlv.c,v $
 *----------------------------------------------------------------------------
 */
