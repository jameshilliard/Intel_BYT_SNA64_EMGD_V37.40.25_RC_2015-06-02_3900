/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: reg_snb.c
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
 *  This is the implementation file for the register module's main API's called
 *  by external devices.
 *-----------------------------------------------------------------------------
 */

#include <io.h>
#include <memory.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <gn6/regs.h>
#include <gn6/context.h>
#include "regs_common.h"
#include "drm_emgd_private.h"

#include <dsp.h>
#include <vga.h>
#include "../cmn/reg_dispatch.h"

/*!
 * @addtogroup state_group
 * @{
 */

#define PLANE_LATCH_COUNT  4

/*#define RING_BUFFER        1*/
#define MMIO_MISC          1

static reg_buffer_t *reg_alloc_snb(igd_context_t *context,
	unsigned long flags, void *_platform_context);
static void reg_free_snb(igd_context_t *context, reg_buffer_t *reg_buffer,
	void *_platform_context);
static int reg_save_snb(igd_context_t *context, reg_buffer_t *reg_buffer,
	void *_platform_context);
static int reg_restore_snb(igd_context_t *context, reg_buffer_t *reg_buffer,
	void *_platform_context);

/* GR registers being saved */
static unsigned char gr_regs_snb[] = {
	0x00, 0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08,
	0x10, 0x11,
	0x18,
	0xff
};
#define GR_REG_COUNT sizeof(gr_regs_snb)

/* SR registers being saved */
static unsigned char sr_regs_snb[] = {
	0x00, 0x01, 0x02, 0x03, 0x04,
	0x07,
	0xff
};
#define SR_REG_COUNT sizeof(sr_regs_snb)

/* AR registers being saved */
static unsigned char ar_regs_snb[] = {
	0x00, 0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08, 0x09,
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13,
	0x14,
	0xff,
};
#define AR_REG_COUNT sizeof(ar_regs_snb)

/* CR registers being saved */
static unsigned char cr_regs_snb[] = {
	0x00, 0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08, 0x09,
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x15, 0x16, 0x17, 0x18,
	0xff
};
#define CR_REG_COUNT sizeof(cr_regs_snb)


/* MMIO states register to be saved */
static unsigned long mmio_regs_snb[] = {

	/* Instruction and Interrupt Control */
	/*PGTBL_CTL,*/

	/* PCH Clock Reference Source */
	PCH_DREF_CONTROL,

	/* 1st instance of FDI control to enable FDI RX and TX PLL
	 * do not change the sequence here */
	_FDI_RXA_TUSIZE1, _FDI_RXB_TUSIZE1,
	_FDI_RXA_CHICKEN, _FDI_RXB_CHICKEN,
	_FDI_RXA_CTL, _FDI_TXA_CTL,
	_FDI_RXB_CTL, _FDI_TXB_CTL,

	/* reset_plane_pipe_port must be called before restoring registers */
	PCH_FPA0, PCH_FPA1, PCH_FPB0, PCH_FPB1,

	DISP_ARB_CTL,
	ILK_DISPLAY_CHICKEN1,
	ILK_DISPLAY_CHICKEN2,

	/* Panel Fitter */
	PFA_PWR_GATE_CTRL, PFA_CTL_1, PFA_WIN_POS, PFA_WIN_SZ,
	PFB_PWR_GATE_CTRL, PFB_CTL_1, PFB_WIN_POS, PFB_WIN_SZ,

	/* 2nd instance of FDI control to enable FDI and perform training
	 * do not change the sequence here */
	_FDI_RXA_CTL, _FDI_TXA_CTL, _FDI_RXB_CTL, _FDI_TXB_CTL,

	/* This will enable the PCH DPLL, this can be done anytime before
	 * enable PCH transcoder */
	PCH_DPLL_A, PCH_DPLL_B,
	PCH_DPLL_SEL,

	TRANS_DP_CTL_A, TRANS_DP_CTL_B,

	/* Transcoder */
	TRANS_HTOTAL_A, TRANS_HBLANK_A, TRANS_HSYNC_A, TRANS_VTOTAL_A,
	TRANS_VBLANK_A, TRANS_VSYNC_A,
	TRANS_HTOTAL_B, TRANS_HBLANK_B, TRANS_HSYNC_B, TRANS_VTOTAL_B,
	TRANS_VBLANK_B, TRANS_VSYNC_B,

	TRANSA_DATA_M1, TRANSA_DATA_N1, TRANSA_DATA_M2, TRANSA_DATA_N2,
	TRANSA_DP_LINK_M1, TRANSA_DP_LINK_N1, TRANSA_DP_LINK_M2, TRANSA_DP_LINK_N2,

	TRANSB_DATA_M1, TRANSB_DATA_N1, TRANSB_DATA_M2, TRANSB_DATA_N2,
	TRANSB_DP_LINK_M1, TRANSB_DP_LINK_N1, TRANSB_DP_LINK_M2, TRANSB_DP_LINK_N2,

	TRANSACONF, TRANSBCONF,

	/* Program Pipe A */
	PIPEAGCMAXRED, PIPEAGCMAXGRN, PIPEAGCMAXBLU, PIPEA_STAT,
	HTOTAL_A, HBLANK_A, HSYNC_A, VTOTAL_A, VBLANK_A, VSYNC_A, PIPEASRC,
	PIPEA_DATA_M1, PIPEA_DATA_N1, PIPEA_DATA_M2, PIPEA_DATA_M2,
	PIPEA_LINK_M1, PIPEA_LINK_N1, PIPEA_LINK_M2, PIPEA_LINK_N2,
	CRCCTRLREDA, CRCCTRLGREENA,	CRCCTRLBLUEA, CRCCTRLRESA,

	/* Program Pipe B */
	PIPEBGCMAXRED, PIPEBGCMAXGRN, PIPEBGCMAXBLU, PIPEB_STAT,
	HTOTAL_B, HBLANK_B, HSYNC_B, VTOTAL_B, VBLANK_B, VSYNC_B, PIPEBSRC,
	PIPEB_DATA_M1, PIPEB_DATA_N1, PIPEB_DATA_M2, PIPEB_DATA_M2,
	PIPEB_LINK_M1, PIPEB_LINK_N1, PIPEB_LINK_M2, PIPEB_LINK_N2,
	CRCCTRLREDB, CRCCTRLGREENB, CRCCTRLBLUEB, CRCCTRLRESB,

	/* Enable Pipes */
	_PIPEACONF, _PIPEBCONF,

	/* Display A plane control */
	DSPASTRIDE,

	/* Display B plane control */
	DSPBSTRIDE,

	/* Enable Plane B + Triggers */
	DSPBCNTR, DSPBTILEOFF, DSPBLINOFF, DSPBSURF,

	/* Enable Plane A + Triggers */
	DSPACNTR, DSPATILEOFF, DSPALINOFF, DSPASURF,

	/* Enable VGA Plane */
	VGACNTRL,

	/* Enable Ports */
	HDMIB, HDMIC, HDMID, PCH_ADPA,

	/* Cursor A&B */
	CURSOR_A_BASE, CURSOR_A_POS, CURSOR_A_PAL0, CURSOR_A_PAL1,
	CURSOR_A_PAL2, CURSOR_A_PAL3,
	CURSOR_B_BASE, CURSOR_B_POS, CURSOR_B_PAL0, CURSOR_B_PAL1,
	CURSOR_B_PAL2, CURSOR_B_PAL3,
	/* No more... */
	0xffffffff
};


typedef struct _reg_platform_context_snb {
	unsigned long *mmio_regs;
	unsigned long mmio_size;
	unsigned char *ar;
	unsigned char *cr;
	unsigned char *sr;
	unsigned char *gr;
	unsigned long pipes;
} reg_platform_context_snb_t;


typedef struct _vga_state_snb  {
	unsigned char fcr; // Feature Control register
	unsigned char msr; // Miscellaneous Output register
	unsigned char sr_index; // Sequencer index register
	unsigned char cr_index; // CRT Controller index register
	unsigned char ar_index; // Attribute Controller index register
	unsigned char gr_index; // Graphics Controller index register
	unsigned char ar[AR_REG_COUNT];  // Attribute Contr regs (AR00-AR14)
	unsigned char sr[SR_REG_COUNT];  // Sequencer registers (SR01-SR04)
	unsigned char cr[CR_REG_COUNT];  // CRT Controller regs (CR00-CR18)
	unsigned char gr[GR_REG_COUNT];  // Graphics Contr regs
	unsigned char plane_latches[PLANE_LATCH_COUNT];
} vga_state_snb_t;


#define DAC_DATA_COUNT  256  /* 256 sets of (R, G, B) data */
typedef struct _dac_state_snb {
	unsigned long palette_a[DAC_DATA_COUNT]; /* Pipe A palette data */
	unsigned long palette_b[DAC_DATA_COUNT]; /* Pipe B palette data */
	unsigned char mode;  // DAC state register
	unsigned char index; // DAC index register
	unsigned char mask;  // 0x3C6, Palette Pixel Mask Register
} dac_state_snb_t;

/* Structure to store the 3D registers during power management
 * These are 3D specific registers, but named as D3D because
 * variabl names cannot start with a number*/
typedef struct _d3d_state_snb {
	unsigned long cin_ctl;
	unsigned long bin_scene;
	unsigned long bmp_buffer;
	unsigned long bmp_get;
	unsigned long bmp_put;
} d3d_state_snb_t;

typedef struct _reg_buffer_snb {
	unsigned long *mmio_state;
	unsigned long gtt[512*1024];  /* For Cantiga max aperture is 2GB but
								   * current supported max is 512MB */
	void *vga_mem;
	vga_state_snb_t vga_state;
	dac_state_snb_t dac_state;
	d3d_state_snb_t d3d_state;
	void *cmd_state;
} reg_buffer_snb_t;

/* FIXME: Probably need one of these for each platform... */
static reg_platform_context_snb_t reg_platform_context_snb = {
	mmio_regs_snb,
	(unsigned long) sizeof(mmio_regs_snb),
	ar_regs_snb,
	cr_regs_snb,
	sr_regs_snb,
	gr_regs_snb,
	2,
};

reg_dispatch_t reg_dispatch_snb = {
	reg_alloc_snb,
	reg_free_snb,
	reg_save_snb,
	reg_restore_snb,
	&reg_platform_context_snb
};

/******************************************************************************
 *  Local Functions
 *****************************************************************************/
static int reg_save_gtt_snb(igd_context_t *context, unsigned char *mmio,
	reg_buffer_snb_t *reg_args);
static int reg_restore_gtt_snb(igd_context_t *context,
	reg_buffer_snb_t *reg_args);

/*!
 * This procedure simply waits for the next vertical syncing (vertical retrace)
 * period. If the display is already in a vertical syncing period, this
 * procedure exits unlike "util_Wait_VSync_Start()" function that waits for
 * the beginning of the next vertical sync period.
 *
 * Note: A timeout is included to prevent an endless loop.
 *
 * @param context
 *
 * @return FALSE if timed out
 * @return TRUE otherwise
 */
int reg_wait_vsync_snb ( igd_context_t *context)
{
	unsigned long i = 0;
	unsigned char *mmio;

	mmio = context->device_context.virt_mmadr;

	if((EMGD_READ32(mmio + VGACNTRL) & BIT31)) {
		return 1;
	}

	while((i++ < 0x100000) &&  /* Check for timeout */
		((EMGD_READ8_P(mmio + STATUS_REG_01) & BIT3) == 0x00)) {
		;
	}

	if(i >= 0x100000) {
		return 0;
	}

	return 1;

}

/*!
 * Saves the current VGA register state of the video chipset into the
 * given state buffer.
 *
 * This function first saves the 4 plane latches, and then it saves
 * the SR, GR, AR, CR registers.
 *
 * @param context the current device context
 * @param vga_buffer this is where the VGA register state is saved
 * @param ar_regs AR registers to save
 * @param cr_regs CR registers to save
 * @param sr_regs SR registers to save
 * @param gr_regs GR registers to save
 *
 * @return 0
 */
static int reg_save_vga_snb(
	igd_context_t *context,
	vga_state_snb_t *vga_buffer,
	unsigned char *ar_regs,
	unsigned char *cr_regs,
	unsigned char *sr_regs,
	unsigned char *gr_regs)
{
	unsigned char *mmio;
	int i;

	EMGD_TRACE_ENTER;

	mmio = context->device_context.virt_mmadr;

	/* First, save a few registers that will be modified to read the latches.
	 * We need to use GR04 to go through all the latches.  Therefore, we must
	 * first save GR04 before continuing.  However, GR04 will only behave the
	 * way we want it to if we are not in Odd/Even mode and not in Chain 4
	 * mode.  Therefore, we have to make sure GR05 (GFX mode reg),
	 * and SR04 (Mem Mode reg) are set properly.
	 * According to B-spec, we are not supposed to program GR06 via MMIO.
	 */
	READ_VGA2(mmio, GR_PORT, GR04, vga_buffer->gr[0x04]);
	READ_VGA2(mmio, SR_PORT, SR04, vga_buffer->sr[0x04]); /* Memory Mode */
	READ_VGA2(mmio, GR_PORT, GR05, vga_buffer->gr[0x05]); /* GFX Mode Reg */
	READ_VGA2(mmio, GR_PORT, GR06, vga_buffer->gr[0x06]); /* Misc Reg */

	WRITE_VGA2(mmio, SR_PORT, SR04, 0x06);
	WRITE_VGA2(mmio, GR_PORT, GR05, 0x00);
	WRITE_VGA2(mmio, GR_PORT, GR06, 0x05);

	/* Save Memory Latch Data latches */
	for(i=0; i<PLANE_LATCH_COUNT; i++) {
		WRITE_VGA2(mmio, GR_PORT, GR04, (unsigned char)i);
		READ_VGA2(mmio, CR_PORT, CR22, vga_buffer->plane_latches[i] );
	}

	/* Restore the modified regs */
	WRITE_VGA2(mmio, GR_PORT, GR06, vga_buffer->gr[0x06]);
	WRITE_VGA2(mmio, GR_PORT, GR05, vga_buffer->gr[0x05]);
	WRITE_VGA2(mmio, GR_PORT, GR04, vga_buffer->gr[0x04]);
	WRITE_VGA2(mmio, SR_PORT, SR04, vga_buffer->sr[0x04]);


	/* Save Feature Controller register. */
	vga_buffer->fcr = EMGD_READ8_P(mmio + FEATURE_CONT_REG_READ);

	/* Save Miscellaneous Output Register. */
	vga_buffer->msr = EMGD_READ8_P(mmio + MSR_READ_PORT);

	/* Save index registers. */
	vga_buffer->sr_index = EMGD_READ8_P(mmio + SR_PORT);
	vga_buffer->cr_index = EMGD_READ8_P(mmio + CR_PORT);
	vga_buffer->gr_index = EMGD_READ8_P(mmio + GR_PORT);

#if 0
	/*
	 * Save the AR index register and last write status. Not sure that
	 * this is really necessary so skip it for now.
	 */
	READ_VGA2(mmio, CR_PORT, CR24, isARData);  // Next write to AR index reg
	isARData &= 0x80;

	// Save AR index and last write status
	vga_buffer->ar_index = EMGD_READ8_P(mmio + AR_PORT) | isARData;
#endif



	/* Save CRT Controller registers. */
	for(i=0; cr_regs[i] != 0xff; i++) {
		READ_VGA2(mmio, CR_PORT, cr_regs[i], vga_buffer->cr[i]);
	}

	/* Save Attribute Controller registers. */
	for(i=0; ar_regs[i] != 0xff; i++) {
		reg_wait_vsync_snb(context);
		READ_AR2(mmio, (unsigned char)i, vga_buffer->ar[i]);
	}

	/* Save Graphics Controller registers. */
	for(i=0; gr_regs[i] != 0xff; i++) {
		READ_VGA2(mmio, GR_PORT, gr_regs[i], vga_buffer->gr[i]);
	}

	/* Save Sequencer registers. */
	for(i=0; sr_regs[i] != 0xff; i++) {
		READ_VGA2(mmio, SR_PORT, sr_regs[i], vga_buffer->sr[i]);
	}

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * Saves the current VGA register state of the video chipset into the
 * given state buffer.
 *
 * @param context the current device context
 * @param vga_buffer this is where the VGA register state is saved
 * @param ar_regs AR registers to save
 * @param cr_regs CR registers to save
 * @param sr_regs SR registers to save
 * @param gr_regs GR registers to save
 *
 * @return 0
 */
static int reg_restore_vga_snb(
	igd_context_t *context,
	vga_state_snb_t *vga_buffer,
	unsigned char *ar_regs,
	unsigned char *cr_regs,
	unsigned char *sr_regs,
	unsigned char *gr_regs)
{
	unsigned long i;
	unsigned char *mmio;
	unsigned long bit_mask;
	mmio = context->device_context.virt_mmadr;

	/* Restore the plane latches.
	 *
	 * BP: I don't understand what this block is doing and it doesn't
	 * seem necessary. Should check this against the spec and figure
	 * out what it really does.
	 */

	/* Memory Mode Register */
	WRITE_VGA2(mmio, SR_PORT, SR04, 0x06);
	/* GR05, Graphics Mode Register */
	WRITE_VGA2(mmio, GR_PORT, GR05, 0x00);
	/* GR06, Micsellaneous Register */
	WRITE_VGA2(mmio, GR_PORT, GR06, 0x05);
	/* GR08, Bit Mask Register */
	WRITE_VGA2(mmio, GR_PORT, GR08, 0xFF);

	for(i=0, bit_mask=1; i<PLANE_LATCH_COUNT; i++, bit_mask<<= 1)  {
		/* Set plane select register */
		WRITE_VGA2(mmio, GR_PORT, GR04, i);
		/* Plane/Map mask register */
		WRITE_VGA2(mmio, SR_PORT, SR02, bit_mask);
	}

	for(i=0, bit_mask=1; i<PLANE_LATCH_COUNT; i++, bit_mask<<= 1)  {
		/* Plane/Map mask register again?? */
		WRITE_VGA2(mmio, SR_PORT, SR02, bit_mask);
	}

	/* Restore standard VGA registers.
	 * 2) Sequence registers
	 * 1) MSR register
	 * 3) CRTC registers
	 * 4) Graphics registers
	 * 5) Attribute registers
	 * 6) VGA Feature register
	 * 7) Index restisters
	 */


	WRITE_VGA2(mmio, SR_PORT, SR00, 01); /* Do sync reset */

	for(i=0; sr_regs[i] != 0xff; i++) {
		WRITE_VGA2(mmio, SR_PORT, sr_regs[i], vga_buffer->sr[i]);
	}


	EMGD_WRITE8_P(vga_buffer->msr, mmio + MSR_PORT);
	WRITE_VGA2(mmio, SR_PORT, SR00, 0x03); /* Set to normal operation */

	/* Unlock CRTC */
	WRITE_VGA2(mmio, CR_PORT, CR11, vga_buffer->cr[0x11] & 0x7F);
	for(i=0; cr_regs[i] != 0xff; i++) {
		WRITE_VGA2(mmio, CR_PORT, cr_regs[i], vga_buffer->cr[i]);
	}

	for(i=0; gr_regs[i] != 0xff; i++) {
		WRITE_VGA2(mmio, GR_PORT, gr_regs[i], vga_buffer->gr[i]);
	}

	for(i=0; ar_regs[i] != 0xff; i++) {
		WRITE_AR2(mmio, ar_regs[i], vga_buffer->ar[i]);
	}

	EMGD_WRITE8_P(vga_buffer->fcr, mmio + FEATURE_CONT_REG);

	/* Restore index registers. Is this necessary?  */
	EMGD_WRITE8_P(vga_buffer->sr_index, mmio + SR_PORT);
	EMGD_WRITE8_P(vga_buffer->cr_index, mmio + CR_PORT);
	EMGD_WRITE8_P(vga_buffer->gr_index, mmio + GR_PORT);

	/* Lock CRTC */
	WRITE_VGA2(mmio, CR_PORT, CR11, vga_buffer->cr[0x11] | 0x80);

	return 0;
}

/*!
 * Saves the DAC registers and lookup table.
 *
 * @param context
 * @param dac_state
 * @param platform_context
 *
 * @return 0
 */
static int reg_save_dac_snb(
	igd_context_t *context,
	dac_state_snb_t *dac_state,
	reg_platform_context_snb_t *platform_context)
{
	unsigned char *mmio;
	int	i;

	mmio = context->device_context.virt_mmadr;

	/* Save DACMASK register */
	dac_state->mask = EMGD_READ8_P(mmio + DAC_PEL_MASK);

	/* Save DAC State register */
	dac_state->mode = EMGD_READ8_P(mmio + DAC_STATE);

	/* Save DAC Index register */
	dac_state->index = EMGD_READ8_P(mmio + DAC_WRITE_INDEX);

	/*
	 * Save DAC data
	 * Start from first DAC location
	 */
	EMGD_WRITE8_P(0, mmio + DAC_WRITE_INDEX);

	/* Save Pipe A Palette */
	/* Or if Pipe A is used for VGA */
	if(((EMGD_READ32(mmio + _PIPEACONF) & PIPE_ENABLE) &&
		(EMGD_READ32(mmio + PIPEASRC)) ) ||
		(( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x80000000) ) &&
		( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x20000000) )) ) {
		for(i=0; i<DAC_DATA_COUNT; i++)  {
			dac_state->palette_a[i] = EMGD_READ32(mmio + i*4 + LGC_PALETTE_A);
		}
	}

	if(platform_context->pipes == 2) {
		/* If Pipe B is enabled, save Pipe B Palette */
		/* Or if Pipe B is used for VGA */
		if(((EMGD_READ32(mmio + _PIPEBCONF) & PIPE_ENABLE) &&
			(EMGD_READ32(mmio + PIPEBSRC)))  ||
			(( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x80000000) ) &&
			( ( EMGD_READ32(mmio + VGACNTRL) & 0x20000000) )) ) {

			for(i=0; i<DAC_DATA_COUNT; i++)  {
				dac_state->palette_b[i] = EMGD_READ32(mmio + i*4 + LGC_PALETTE_B);
			}
		}
	}

	return 0;
}

/*!
 * Restore previously saved DAC palette from the specifed state buffer.
 *
 * @param context
 * @param dac_state
 * @param platform_context
 *
 * @return 0
 */
static int reg_restore_dac_snb(
	igd_context_t *context,
	dac_state_snb_t *dac_state,
	reg_platform_context_snb_t *platform_context)
{
	int i;
	unsigned char *mmio;
	unsigned char temp;

	mmio = context->device_context.virt_mmadr;

	/* If Pipe A is enabled, restore Palette */
	/* Or if Pipe A is used for VGA */
	if(((EMGD_READ32(mmio + _PIPEACONF) & PIPE_ENABLE) &&
	(EMGD_READ32(mmio + PIPEASRC)) ) ||
		(( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x80000000) ) &&
		( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x20000000) )) ) {

		/*
		 * Restore DAC data
		 * Start from first DAC location
		 */
		EMGD_WRITE8_P(0, mmio + DAC_WRITE_INDEX);

		/* Restore Pipe A Palette */
		for(i=0; i<DAC_DATA_COUNT; i++)  {
			EMGD_WRITE32(dac_state->palette_a[i], mmio + i*4 + LGC_PALETTE_A);
		}
	}

	/* If this is a single pipe device. */
	if(platform_context->pipes == 2) {
		/* If Pipe B is enabled, restore Palette */
		/* Or if Pipe B is used for VGA */
		if(((EMGD_READ32(mmio + _PIPEBCONF) & PIPE_ENABLE) &&
			(EMGD_READ32(mmio + PIPEBSRC )))  ||
			(( ! ( EMGD_READ32(mmio + VGACNTRL) & 0x80000000) ) &&
			( ( EMGD_READ32(mmio + VGACNTRL) & 0x20000000) )) ) {

			for(i=0; i<DAC_DATA_COUNT; i++)  {
				EMGD_WRITE32(dac_state->palette_b[i], mmio + i*4 + LGC_PALETTE_B);
			}
		}
	}

	/* Restore DACMASK register */
	EMGD_WRITE8_P(dac_state->mask, mmio + DAC_PEL_MASK);

	/* Restore DAC Index register */
	if(dac_state->mode & 1) { /* Last write was to "write index register" */
		EMGD_WRITE8_P(dac_state->index, mmio + DAC_WRITE_INDEX);
	} else {  /* Last index write was to "read index register" */
		EMGD_WRITE8_P(dac_state->index - 1, mmio + DAC_READ_INDEX);

		/* Dummy read to set DACSTATE register and to increment read index to
		 * last saved.
		 */
		temp = EMGD_READ8_P(mmio + DAC_DATA_REG);
	}

	return 0;
}

/*!
 * Saves the d3d States.
 *
 * @param context Pointer to igd_context
 * @param mmio Pointer to mmio address
 * @param d3d_state
 *
 * @return 0
 */
static int reg_save_d3d_snb(igd_context_t *context, unsigned char *mmio,
	d3d_state_snb_t *d3d_state)
{
	/* Do we actually need to do anything here? */

	return 0;
}

/*!
 * Restore previously saved D3D registers.
 *
 * @param context Pointer to igd_context
 * @param d3d_state
 *
 * @return 0
 */
static int reg_restore_d3d_snb(
	igd_context_t *context,
	d3d_state_snb_t *d3d_state)
{
	/* Do we need to do anything here? */

	return 0;
}

/*!
 * Initialises memory to store register values
 *
 * @param context pointer to igd_context
 * @param flags indicate which register type to save
 * @param _platform_context
 *
 * @return pointer to structure which will eventually be saved data defining
 * 	register state
 * @return NULL on failure
 */
static reg_buffer_t *reg_alloc_snb(igd_context_t *context,
	unsigned long flags,
	void *_platform_context)
{
	reg_buffer_snb_t* reg_args;
	reg_buffer_t *reg_buffer;
	reg_platform_context_snb_t *platform_context =
		(reg_platform_context_snb_t *)_platform_context;

	EMGD_DEBUG("Entry - reg_alloc");

	reg_buffer = (reg_buffer_t*)OS_ALLOC(sizeof(reg_buffer_t));
	if(reg_buffer == NULL) {
		return NULL;
	}

	reg_args = (reg_buffer_snb_t*)OS_ALLOC_LARGE(sizeof(reg_buffer_snb_t));
	if(reg_args == NULL) {
		OS_FREE(reg_buffer);
		return NULL;
	}
	reg_buffer->mode_buffer = NULL;
	reg_buffer->platform_buffer = reg_args;

	OS_MEMSET(reg_args, 0, sizeof(reg_buffer_snb_t));
	reg_buffer->flags = flags;

	reg_args->mmio_state = (void*)OS_ALLOC(platform_context->mmio_size);

	if(!reg_args->mmio_state) {
		EMGD_DEBUG("Failed Allocating mmio memory");
		OS_FREE(reg_buffer);
		OS_FREE_LARGE(reg_args);
		return NULL;
	}

	return reg_buffer;
}

/*!
 * This function calculates the size of memory to store data
 *
 * @param context void pointer to main igd context
 * @param reg_buffer pointer to register structure returned from a reg_alloc
 * @param _platform_context
 *
 * @return 0
 */
static void reg_free_snb(igd_context_t *context,
	reg_buffer_t *reg_buffer,
	void *_platform_context)
{
	reg_buffer_snb_t* reg_args;

	EMGD_DEBUG("Entry - reg_free");

	if(reg_buffer) {
		reg_args = (reg_buffer_snb_t*)reg_buffer->platform_buffer;

		if(reg_args){
			if(NULL != reg_args->mmio_state) {
				OS_FREE(reg_args->mmio_state);
			}

			if(NULL != reg_args->vga_mem) {
				OS_FREE(reg_args->vga_mem);
			}

			if(NULL != reg_args->cmd_state) {
				OS_FREE(reg_args->cmd_state);
			}

			OS_FREE_LARGE(reg_args);
		}

		OS_FREE(reg_buffer);
	}

	return;
}

/*!
 *
 * @param context void pointer to main igd context
 * @param reg_buffer pointer to register structure returned from a reg_alloc
 * @param _platform_context
 *
 * @return 0 on success
 * @return -1 on failure
 */
static int reg_save_snb(igd_context_t *context,
	reg_buffer_t *reg_buffer,
	void *_platform_context)
{
	reg_buffer_snb_t           *reg_args;
	unsigned long              *buffer;
	reg_platform_context_snb_t *platform_context =
		(reg_platform_context_snb_t *)_platform_context;
	int                        i;
	unsigned char              *mmio;

	EMGD_TRACE_ENTER;

	if(reg_buffer == NULL){
		EMGD_TRACE_EXIT;
		return 0;
	}

	reg_args = (reg_buffer_snb_t *)reg_buffer->platform_buffer;
	if(reg_args == NULL){
		EMGD_TRACE_EXIT;
		return 0;
	}

	mmio = context->device_context.virt_mmadr;


	/* Save vga registers */
	if(reg_buffer->flags & IGD_REG_SAVE_VGA) {
		EMGD_DEBUG("Saving VGA registers");
		reg_save_vga_snb(context, &reg_args->vga_state,
			platform_context->ar, platform_context->cr, platform_context->sr,
			platform_context->gr);
	}

	/* Save mmio registers */
	if(reg_buffer->flags & IGD_REG_SAVE_MMIO) {
		EMGD_DEBUG("Saving MMIO registers");
		buffer = reg_args->mmio_state;
		for(i=0; platform_context->mmio_regs[i] != 0xffffffff; i++) {
			*buffer++ = EMGD_READ32(mmio + platform_context->mmio_regs[i]);
		}
	}

	/* Save GTT */
	if(reg_buffer->flags & IGD_REG_SAVE_GTT) {
		EMGD_DEBUG("Saving GTT");
		reg_save_gtt_snb( context, mmio, reg_args );
	}

	/* Save DAC registers */
	if(reg_buffer->flags & IGD_REG_SAVE_DAC) {
		EMGD_DEBUG("Saving DAC registers");
		reg_save_dac_snb(context, &reg_args->dac_state, platform_context);
	}

	/* Reset ring buffer and save appcontext */
	/* FIXME(SNB): This is in command module */	
/* 	if(reg_buffer->flags & IGD_REG_SAVE_RB) {
		EMGD_DEBUG("Saving ring buffer registers");
		cmd_save(context, &(reg_args->cmd_state));
	}
*/
	/* Save Mode state */
	if(reg_buffer->flags & IGD_REG_SAVE_MODE) {
		EMGD_DEBUG("Saving mode state");
		/* At driver initialization though mode_save is requested, mode isn't
		 * initialized. So skip the mode_save if dispatch function isn't
		 * available. In this case, mode_save() will be done as part of
		 * mode_init(). */
		if (context->module_dispatch.mode_save && !reg_buffer->mode_buffer) {
			context->module_dispatch.mode_save(context,
				&reg_buffer->mode_buffer,
				&reg_buffer->flags);
		} else {
			EMGD_DEBUG("mode_save() is skipped as mode_init() isn't done.");
		}
	}

	/* Save D3D state registers */
	if(reg_buffer->flags & IGD_REG_SAVE_3D) {
		EMGD_DEBUG("Saving D3D registers");
		reg_save_d3d_snb(context, mmio, &reg_args->d3d_state);
	}

	EMGD_TRACE_EXIT;
	return 0;
}

static const int const snb_b_fdi_train_param [] = {
	FDI_LINK_TRAIN_400MV_0DB_SNB_B,
	FDI_LINK_TRAIN_400MV_6DB_SNB_B,
	FDI_LINK_TRAIN_600MV_3_5DB_SNB_B,
	FDI_LINK_TRAIN_800MV_0DB_SNB_B,
};

static void gen6_fdi_link_train2(unsigned int pipe_num, unsigned char *mmio, int lane)
{
	unsigned long reg, temp, i;

	if (pipe_num > 1) {
		EMGD_ERROR("Invalid pipe number: %d", pipe_num);
		return;
	}

	/* Train 1: umask FDI RX Interrupt symbol_lock and bit_lock bit
	 * for train result */
	reg = FDI_RX_IMR(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_RX_SYMBOL_LOCK;
	temp &= ~FDI_RX_BIT_LOCK;
	EMGD_WRITE32(temp, mmio + reg);

	/*POSTING_READ(reg);*/
	udelay(150);

	/* enable CPU FDI TX and PCH FDI RX */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~(7 << 19);
	temp |= (lane - 1) << 19;
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_PATTERN_1;
	temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
	/* SNB-B */
	temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;
	EMGD_WRITE32(temp | FDI_TX_ENABLE, mmio + reg);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
	temp |= FDI_LINK_TRAIN_PATTERN_1_CPT;
	EMGD_WRITE32(temp | FDI_RX_ENABLE, mmio +  reg);

	/*POSTING_READ(reg);*/
	udelay(150);

	for (i = 0; i < 4; i++ ) {
		reg = FDI_TX_CTL(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
		temp |= snb_b_fdi_train_param[i];
		EMGD_WRITE32(temp, mmio + reg);

		/*POSTING_READ(reg);*/
		udelay(500);

		reg = FDI_RX_IIR(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		EMGD_DEBUG("FDI_RX_IIR = 0x%lx", temp);

		if (temp & FDI_RX_BIT_LOCK) {
			EMGD_WRITE32(temp | FDI_RX_BIT_LOCK, mmio + reg);
			EMGD_DEBUG("FDI train 1 done.");
			break;
		}
	}
	if (i == 4) {
		EMGD_DEBUG("FDI train 1 fail!");
	}

	/* Train 2 */
	reg = FDI_TX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_NONE;
	temp |= FDI_LINK_TRAIN_PATTERN_2;
	temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
	/* SNB-B */
	temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;
	EMGD_WRITE32(temp, mmio + reg);

	reg = FDI_RX_CTL(pipe_num);
	temp = EMGD_READ32(mmio + reg);
	temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
	temp |= FDI_LINK_TRAIN_PATTERN_2_CPT;
	EMGD_WRITE32(temp, mmio + reg);

	/*POSTING_READ(reg);*/
	udelay(150);

	for (i = 0; i < 4; i++ ) {
		reg = FDI_TX_CTL(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
		temp |= snb_b_fdi_train_param[i];
		EMGD_WRITE32(temp, mmio + reg);

		/*POSTING_READ(reg);*/
		udelay(500);

		reg = FDI_RX_IIR(pipe_num);
		temp = EMGD_READ32(mmio + reg);
		EMGD_DEBUG("FDI_RX_IIR = 0x%lx", temp);

		if (temp & FDI_RX_SYMBOL_LOCK) {
			EMGD_WRITE32(temp | FDI_RX_SYMBOL_LOCK, mmio + reg);
			EMGD_DEBUG("FDI train 2 done.");
			break;
		}
	}
	if (i == 4) {
		EMGD_DEBUG("FDI train 2 fail!");
	}

	EMGD_DEBUG("FDI train done.");
}

static void intel_fdi_normal_train2(unsigned int pipe_num, unsigned char *mmio)
{
	unsigned long reg, temp;

	if (pipe_num > 1) {
		EMGD_ERROR("Invalid pipe number: %d", pipe_num);
		return;
	}

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
	/*POSTING_READ(reg);*/
	mdelay(1);
}

void print_reg_val(unsigned long reg,
		unsigned long *buffer)
{
	if(reg == PCH_FPA0) {
		EMGD_DEBUG("PCH_FPA0=0x%lx", *buffer);
	} else if(reg == PCH_FPA1) {
		EMGD_DEBUG("PCH_FPA1=0x%lx", *buffer);
	} else if(reg == PCH_FPB0) {
		EMGD_DEBUG("PCH_FPB0=0x%lx", *buffer);
	} else if(reg == PCH_FPB1) {
		EMGD_DEBUG("PCH_FPB1=0x%lx", *buffer);
	} else if(reg == PCH_DPLL_A) {
		EMGD_DEBUG("PCH_DPLL_A=0x%lx", *buffer);
	} else if(reg == PCH_DPLL_B) {
		EMGD_DEBUG("PCH_DPLL_B=0x%lx", *buffer);
	} else if(reg == PCH_DPLL_SEL) {
		EMGD_DEBUG("PCH_DPLL_SEL=0x%lx", *buffer);
	} else if(reg == VGACNTRL) {
		EMGD_DEBUG("VGACNTRL=0x%lx", *buffer);
	} else if(reg == HDMIB) {
		EMGD_DEBUG("HDMIB=0x%lx", *buffer);
	} else if(reg == HDMIC) {
		EMGD_DEBUG("HDMIC=0x%lx", *buffer);
	} else if(reg == HDMID) {
		EMGD_DEBUG("HDMID=0x%lx", *buffer);
	} else if(reg == _PIPEACONF) {
		EMGD_DEBUG("_PIPEACONF=0x%lx", *buffer);
	} else if(reg == _PIPEBCONF) {
		EMGD_DEBUG("_PIPEBCONF=0x%lx", *buffer);
	} else if(reg == HTOTAL_A) {
		EMGD_DEBUG("HTOTAL_A=0x%lx", *buffer);
	} else if(reg == VTOTAL_A) {
		EMGD_DEBUG("VTOTAL_A=0x%lx", *buffer);
	} else if(reg == PIPEASRC) {
		EMGD_DEBUG("PIPEASRC=0x%lx", *buffer);
	} else if (reg == TRANS_HTOTAL_A) {
		EMGD_DEBUG("TRANS_HTOTAL_A=0x%lx", *buffer);
	} else if(reg == TRANS_VTOTAL_A) {
		EMGD_DEBUG("TRANS_VTOTAL_A=0x%lx", *buffer);
	} else if(reg == _FDI_RXA_CTL) {
		EMGD_DEBUG("_FDI_RXA_CTL=0x%lx", *buffer);
		EMGD_DEBUG("_FDI_TXA_CTL=0x%lx", *(buffer+1));
	} else if(reg == _FDI_RXB_CTL) {
		EMGD_DEBUG("_FDI_RXB_CTL=0x%lx", *buffer);
		EMGD_DEBUG("_FDI_TXB_CTL=0x%lx", *(buffer+1));
	} else	if (reg == PFA_CTL_1) {
		EMGD_DEBUG("PFA_CTL_1=0x%lx", *buffer);
	} else if(reg == PFA_WIN_SZ) {
		EMGD_DEBUG("PFA_WIN_SZ=0x%lx", *buffer);
	} else if(reg == PFA_WIN_POS) {
		EMGD_DEBUG("PFA_WIN_POS=0x%lx", *buffer);
	} else {
		EMGD_DEBUG("Reg(0x%lx)=0x%lx", reg, *buffer);
	}
	return;
}

/*!
 * This function restores the regs states
 *
 * @param context void pointer to main igd context
 * @param reg_buffer pointer to register structure returned from a reg_alloc
 * @param _platform_context
 *
 * @return 0 on success
 * @return -1 on failure
 */
int reg_restore_snb(igd_context_t *context,
	reg_buffer_t *reg_buffer,
	void *_platform_context)
{
	reg_buffer_snb_t* reg_args;
	reg_platform_context_snb_t *platform_context =
		(reg_platform_context_snb_t *)_platform_context;
	unsigned char *mmio;
	unsigned long *buffer;
	unsigned long temp = 0;
	int lane;
	int flag;
	int i;

	EMGD_DEBUG("Entry - reg_restore");

	if(reg_buffer == NULL){
		return 0;
	}

	reg_args = (reg_buffer_snb_t *)reg_buffer->platform_buffer;
	if(reg_args == NULL){
		return 0;
	}

	EMGD_DEBUG(" flags = 0x%lx", reg_buffer->flags);

	mmio = context->device_context.virt_mmadr;

	/* Restore MMIO registers */
	if(reg_buffer->flags & IGD_REG_SAVE_MMIO) {

#if 0
#ifdef MMIO_MISC
		/* Invalidate the fence registers by writing 0 to the lower 32-bits
		 * of each 64-bit register.  This will allow us to restore the upper
		 * 32-bits before the register is active again.
		 */

		EMGD_DEBUG("Invalidate Fences");
		for (i=FENCE0; i <= FENCE15; i += 8 * sizeof(unsigned char)) {
			EMGD_WRITE32(0, mmio + i);
		}
#endif
#endif

		EMGD_DEBUG("Restoring MMIO registers");
		flag = 0;
		buffer = reg_args->mmio_state;
		for(i=0; platform_context->mmio_regs[i] != 0xffffffff; i++) {

			/* DEBUG: Print the reg val */
			print_reg_val(platform_context->mmio_regs[i], buffer);

			if (platform_context->mmio_regs[i] == INSTPM ||
				platform_context->mmio_regs[i] == MI_ARB_STATE ||
				platform_context->mmio_regs[i] == MI_RDRET_STATE ) {

				EMGD_DEBUG("Handle special masked register case");
				EMGD_WRITE32(0xffff0000 | *buffer++,
						mmio + platform_context->mmio_regs[i]);

			} else if (platform_context->mmio_regs[i] == _FDI_RXA_CTL ||
				platform_context->mmio_regs[i] == _FDI_RXB_CTL) {

				if (flag < 2) {
					EMGD_DEBUG("Handle special %s PLL enable",
						(platform_context->mmio_regs[i] == _FDI_RXA_CTL) ?
						"FDI_A":"FDI_B");
					/* Clear the FDI RX enable and PCDCLK, only RX PLL is enable */
					EMGD_WRITE32((*buffer) & ~(FDI_RX_ENABLE | FDI_PCDCLK),
						mmio + platform_context->mmio_regs[i]);
					udelay(200);

					/* Clear the FDI RX enable only, switch to PCDCLK */
					EMGD_WRITE32((*buffer++) & ~(FDI_RX_ENABLE),
						mmio + platform_context->mmio_regs[i]);
					udelay(200);
					temp = EMGD_READ32(mmio + platform_context->mmio_regs[i]);
					EMGD_DEBUG("FDI_RX_CTL read=0x%lx", temp);

					/* Point to next mmio register, which is FDI_TX_CTL */
					i++;
					/* Clear the FDI TX enable, only TX PLL is enable */
					EMGD_WRITE32((*buffer++) & ~(FDI_TX_ENABLE),
						mmio + platform_context->mmio_regs[i]);
					udelay(100);
					temp = EMGD_READ32(mmio + platform_context->mmio_regs[i]);
					EMGD_DEBUG("FDI_TX_CTL read=0x%lx",temp);

				} else {
					EMGD_DEBUG("Handle special FDI enable and link training");
					if (*buffer & FDI_RX_ENABLE) {
						/* FDI was previously enable, need FDI training */
						lane = ((*(buffer+1) >> 19) & 7) + 1;
						EMGD_DEBUG("lane=%d",lane);

						if (platform_context->mmio_regs[i] == _FDI_RXA_CTL) {
							EMGD_DEBUG("FDI_A link training");
							gen6_fdi_link_train2(0, mmio, lane);
						} else {
							EMGD_DEBUG("FDI_B link training");
							gen6_fdi_link_train2(1, mmio, lane);
						}
						buffer++;

						/* FDI_TX_CTL was already handled in link training */
						i++;
						buffer++;

					} else {
						/* FDI was not enable, just restore the value */
						if (platform_context->mmio_regs[i] == _FDI_RXA_CTL) {
							EMGD_DEBUG("FDI_RX_A not enable, no link training");
						} else {
							EMGD_DEBUG("FDI_RX_B not enable, no link training");
						}
						/* FDI_RX_CTL */
						EMGD_WRITE32(*buffer++,
							mmio + platform_context->mmio_regs[i]);
						/* Point to next mmio register, which is FDI_TX_CTL */
						i++;
						EMGD_WRITE32(*buffer++,
							mmio + platform_context->mmio_regs[i]);
						udelay(150);
					}
				}

				flag++;

			} else if (platform_context->mmio_regs[i] == _PIPEACONF ||
				platform_context->mmio_regs[i] == _PIPEBCONF) {

				EMGD_DEBUG("Handle special PIPE enable");
				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);
				mdelay(50);

			} else if (platform_context->mmio_regs[i] == PCH_DPLL_A ||
				platform_context->mmio_regs[i] == PCH_DPLL_B) {

				EMGD_DEBUG("Handle special PCH_DPLL enable");
				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);
				udelay(200);

			} else if (platform_context->mmio_regs[i] == TRANSACONF ||
				platform_context->mmio_regs[i] == TRANSBCONF) {

				EMGD_DEBUG("Handle special Transcoder enable");
				if(*buffer & TRANS_ENABLE) {
					if (platform_context->mmio_regs[i] == TRANSACONF) {
						EMGD_DEBUG("FDI_A normal train");
						intel_fdi_normal_train2(0, mmio);
					} else {
						EMGD_DEBUG("FDI_B normal train");
						intel_fdi_normal_train2(1, mmio);
					}
				}

				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);
				udelay(100);

			} else if (platform_context->mmio_regs[i] == VGACNTRL) {
				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);

			} else if (platform_context->mmio_regs[i] == PIPEASRC) {
				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);

			} else {
				EMGD_WRITE32(*buffer++, mmio + platform_context->mmio_regs[i]);
			}

			/* DEBUG */
			/*
			temp = EMGD_READ32(mmio + platform_context->mmio_regs[i]);
			EMGD_DEBUG("Write(0x%lx): 0x%lx",
					platform_context->mmio_regs[i], *(buffer -1));
			EMGD_DEBUG("Read (0x%lx): 0x%lx",
					platform_context->mmio_regs[i], temp);
			*/
		}

		/* restore GTT */
		reg_restore_gtt_snb(context, reg_args);
	}

	/* Restore DAC registers */
	if(reg_buffer->flags & IGD_REG_SAVE_DAC) {
		reg_restore_dac_snb(context, &reg_args->dac_state, platform_context);
	}

	/* Restore VGA registers */
	if(reg_buffer->flags & IGD_REG_SAVE_VGA) {
		reg_restore_vga_snb(context, &reg_args->vga_state,
			platform_context->ar, platform_context->cr, platform_context->sr,
			platform_context->gr);

	}

	/* Restore ring buffer Registers */
/*	if(reg_buffer->flags & IGD_REG_SAVE_RB) {
		cmd_restore(context, &(reg_args->cmd_state));
	}
*/
	/* Restore D3D registers */
	if(reg_buffer->flags & IGD_REG_SAVE_3D) {
		EMGD_DEBUG("Restoring D3D registers");
		reg_restore_d3d_snb(context, &reg_args->d3d_state);
	}

	/* Restore Mode state */
	if(reg_buffer->flags & IGD_REG_SAVE_MODE) {
		if(reg_buffer->mode_buffer) {
			context->module_dispatch.mode_restore(context,
				&reg_buffer->mode_buffer,
				&reg_buffer->flags);
		} else {
			EMGD_DEBUG("mode_restore() already happened in mode_shutdown().");
		}
	}

	return 0;
} /* end reg_restore */

/*!
 * This function saves the GTT table entries into a buffer so that the GTT
 * can be restored later.
 *
 * @param context needs this to get the GTT table size and to get the
 *	virtual address to the GTT table
 * @param mmio virtual address to MMIO
 * @param reg_args a pre-allocated context where GTT entries are saved
 *
 * @return 0 on success
 * @return -1 on failure
 */
static int reg_save_gtt_snb(igd_context_t *context, unsigned char *mmio,
	reg_buffer_snb_t *reg_args)
{
	unsigned int i;

	/* Read the GTT entries from GTT ADR and save it in the array. */
	for(i = 0; i < (context->device_context.gatt_pages); i++) {
		reg_args->gtt[i] = EMGD_READ32(
			context->device_context.virt_gttadr + i);
	}

	return 0;
}

/*!
 *  This function restores the GTT table entries.
 *
 * @param context the context contains the GTT size and address
 * @param reg_args this has the saved GTT entries
 *
 * @return 0 on success
 * @return -1 on failure
 */
static int reg_restore_gtt_snb(igd_context_t *context,
	reg_buffer_snb_t *reg_args)
{
	unsigned int i;

	/* if the first element is 0, then nothing was saved */
	if (0 == reg_args->gtt[0]) {
		return 0;
	}

	/* restore all gtt_size 4-byte entries */
	for (i=0; i < context->device_context.gatt_pages; i++) {
		/* Reads physical location of GTT and store it in the GTT table
		 * entry that has the page we allocated earlier*/
		EMGD_WRITE32(reg_args->gtt[i], context->device_context.virt_gttadr + i);
	}

	return 0;
}



