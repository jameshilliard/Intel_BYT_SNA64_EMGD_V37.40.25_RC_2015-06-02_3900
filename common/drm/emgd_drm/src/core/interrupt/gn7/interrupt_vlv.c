/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: interrupt_vlv.c
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
 *  This file contains SNB HAL specific implementations for IGD interrupt functions.
 *-----------------------------------------------------------------------------
 */


#include <linux/spinlock.h>
#include <linux/io-mapping.h>

#include <igd_core_structs.h>
#include "../cmn/interrupt_dispatch.h"
#include "interrupt_regs.h"
#include "../../i915/i915_reg.h"

static interrupt_data_t priv;
static unsigned char *virt_mmadr;

#define IRQ_LOOP_STRESS			0

#define IGD_POWERGATE_DECODE_MSK	0xC

#define VLV_WRITE32(reg, val)   WRITE_MMIO_REG_VLV(val, virt_mmadr, reg)
#define VLV_READ32(reg)         READ_MMIO_REG_VLV(virt_mmadr, reg)

void init_int_vlv(unsigned char *mmadr)
{
	virt_mmadr = (unsigned char *) mmadr;
	priv.irq_display = 0;
	priv.irq_graphtrans = 0;
	priv.irq_enabled = 0;
	priv.irq_pm = 0;
	priv.irq_pmiir = 0;
	priv.irq_hpd = 0;

}

void stop_irq_vlv(oal_callbacks_t *oal_callbacks_table)
{
	/* Disable HW Status register mask for GPU Render, Blitter and Video */
	/* For Render its special - it overlaps with main display registers in VLV *
	 * with the 0x180000 offset so we cant use VLV_READ32 or VLV_WRITE32         */
	EMGD_WRITE32(0xffffffff, virt_mmadr + HWSTAM);
	VLV_WRITE32(BLITTER_HWSTAM, 0xffffffff);
	VLV_WRITE32(VIDEO_HWSTAM, 0xffffffff);

	/* Disable Display Interrupts */
	VLV_WRITE32(VLV_IMR, 0xffffffff);
	VLV_WRITE32(VLV_IER, 0);
	VLV_READ32(VLV_IER);

	/* Disable GTLC GT (2d/3d/Video) Interrupts */
	VLV_WRITE32(GTLC_IMR, 0xffffffff);
	VLV_WRITE32(GTLC_IER, 0);
	VLV_READ32(GTLC_IER);

	/* Disable PM (GT Power Management) Interrupts */
	VLV_WRITE32(PM_IMR, 0xffffffff);
	VLV_WRITE32(PM_IER, 0);
	VLV_READ32(PM_IER);

	/* Disable Error Interrupts */
	VLV_WRITE32(ERR_IMR, 0xffffffff);
	/*VLV_WRITE32(ERR_IER, 0);
	 *  - no such register? always enabled?
	VLV_READ32(ERR_IER);*/

	/* Disable the master IRQ interface between GTLC and VLV IRQ */
	VLV_WRITE32(GTLC_MASTER_IRQ_CTRL, 0);
	VLV_READ32(GTLC_MASTER_IRQ_CTRL);

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		/* Disabled hotplug interrupt status */
		VLV_WRITE32(PORT_HOTPLUG_EN, 0);
		VLV_READ32(PORT_HOTPLUG_EN);
	}

	priv.irq_enabled = 0;
}

void start_irq_vlv(oal_callbacks_t *oal_callbacks_table)
{
	unsigned long deimr;

	/* Workaround stalls observed on Sandy Bridge GPUs by
	 * making the blitter command streamer generate a
	 * write to the Hardware Status Page for
	 * MI_USER_INTERRUPT.  This appears to serialize the
	 * previous seqno write out before the interrupt
	 * happens.
	 */
	/* For Render its special - it overlaps with main display registers in VLV *
	 * with the 0x180000 offset so we cant use VLV_READ32 or VLV_WRITE32       */
	EMGD_WRITE32(~GT_RND_MI_USER_EVENT, virt_mmadr + HWSTAM);
	VLV_WRITE32(BLITTER_HWSTAM, ~GT_BLT_MI_USER_EVENT);
	VLV_WRITE32(VIDEO_HWSTAM, ~GT_VID_MI_USER_EVENT);

	/* Clear left over interrupts -----------------------> */
	/* 1st, turn off all IMRs to stop getting any new ones */
	VLV_WRITE32(VLV_IMR, DSP_ALL_INTERRUPTS);
	VLV_WRITE32(GTLC_IMR, GT_ALL_INTERRUPTS);
	VLV_WRITE32(PM_IMR, PM_ALL_INTERRUPTS);
	/* 2nd, Disable & Clear 2nd level display interrupt status for Pipe A*/
	VLV_WRITE32(PIPEASTAT, DSPA_2NDLEVEL_IRQS_CLEAR);
	/* 3rd, Disable & Clear 2nd level display interrupt status for Pipe B*/
	VLV_WRITE32(PIPEBSTAT, DSPB_2NDLEVEL_IRQS_CLEAR);
	/* 4th, Clear IRQs from main interrupt identify register */
	VLV_WRITE32(VLV_IIR, VLV_READ32(VLV_IIR));
	/* 5th, Clear IRQs from GTLS interrupt identify register */
	VLV_WRITE32(GTLC_IIR, VLV_READ32(GTLC_IIR));
	/* 6th, Clear IRQs from PM interrupt identity register */
	VLV_WRITE32(PM_IIR, VLV_READ32(PM_IIR));

	/* Enable GTLC Interrupts - but only reflects on Status */
	VLV_WRITE32(GTLC_IER, GT_ENABLE_INTERRUPTS);
	VLV_READ32(GTLC_IER);
	VLV_WRITE32(PM_IER, PM_ENABLE_INTERRUPTS);
	VLV_READ32(PM_IER);
	/* No enable bits for Error-Interrupts
	 * VLV_WRITE32(ERR_IER, ERR_ENABLE_INTERRUPTS);
	 * VLV_READ32(ERR_IER);*/

	/* Enable first line Display Interrupts */
	VLV_WRITE32(VLV_IER, DSP_ENABLE_INTERRUPTS);
	VLV_READ32(VLV_IER);

	/* Enable second line Display Interrupts for Pipe A*/
	VLV_WRITE32(PIPEASTAT, DSPA_2NDLEVEL_IRQS_ENABLE);
	/* Enable second line Display Interrupts for Pipe B*/
	VLV_WRITE32(PIPEBSTAT, DSPB_2NDLEVEL_IRQS_ENABLE);

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		/* Enable second line Hot Plug Detect for DP, HDMI and VGA */
		VLV_WRITE32(PORT_HOTPLUG_EN, VLV_READ32(PORT_HOTPLUG_EN) |
							HDMIB_HOTPLUG_INT_EN |
							DPC_HOTPLUG_INT_EN);
		VLV_READ32(PORT_HOTPLUG_EN);

		/*
		 * Enable CRT Hotplug Detection. This is to enable hotplug
		 * analog interrupt in the case where we do not insmod with
		 * VGA initially boot up
		 */
		VLV_WRITE32(ADPA, VLV_READ32(ADPA) | ADPA_CRT_HPD_EN);
		VLV_READ32(ADPA);
	}

	/* Shouldn't we also enable Gunit to pulse display events to Gen7GT?*/
	/* VLV_WRITE32(DPFLIPSTAT, DPFLIP_DSPTOGT_ALLEVENTS); */

	/*FIXME: we are disabling both pipe A/B horizontal blank interrupt  *
	 * and vertical blank interrupt so that we enable enough interrupts *
	 * in order to not only get into Rc6 state but also able to startx  *
	 * with fallback mode off without any issue                         */
	VLV_WRITE32(DPFLIPSTAT, 0x27270000);

	/* Enable the 2nd level GTLC Interrupt mask registers for Blitter*/
	VLV_WRITE32(BCS_IMR, 0);
	VLV_READ32(BCS_IMR);
	/* Enable the 2nd level GTLC Interrupt mask registers for Render    *
	 * Special - it overlaps with the main display IMR for VLV with the *
	 * 0x180000 offset so we cant use VLV_READ32 or VLV_WRITE32         */
	EMGD_WRITE32(0, virt_mmadr + RCS_IMR);
	EMGD_READ32(virt_mmadr + RCS_IMR);
	/* Enable the 2nd level GTLC Interrupt mask registers for Video*/
	VLV_WRITE32(VCS_IMR, 0);
	VLV_READ32(VCS_IMR);

	VLV_WRITE32(GTLC_IMR, ~GT_ENABLE_INTERRUPTS);
	VLV_READ32(GTLC_IMR);
	VLV_WRITE32(PM_IMR, ~PM_ENABLE_INTERRUPTS);
	VLV_READ32(PM_IMR);

	/* Enable the GTLC Master IRQ interface between GTLC and VLV IRQ*/
	VLV_WRITE32(GTLC_MASTER_IRQ_CTRL, GTLC_MASTER_IRQ_ENABLE);
	VLV_READ32(GTLC_MASTER_IRQ_CTRL);

	/* Keep Display Mask IRQ off - DRM will turn on / off on the fly */
	VLV_WRITE32(VLV_IMR, DSP_ENABLE_INTERRUPTS);
	VLV_READ32(VLV_IMR);

	/* unmasked DSP_HOTSYNC_IRQ bit */
	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		deimr = VLV_READ32(VLV_IMR);
		deimr &= ~DSP_HOTSYNC_IRQ;
		VLV_WRITE32(VLV_IMR, deimr);
	}

	priv.irq_enabled = 1;
}

/*!
 *
 * @param oal_callbacks_t *oal_callbacks_table
 *
 * @return 0 on the interrupt was handled and we need to queue a DPC for it
 * @return 1 on the interrupt was handled but wasn't one to do further
 *  processing on
 * @return 2 on the interrupt was not ours
 */
int isr_helper_vlv(oal_callbacks_t *oal_callbacks_table)
{
	unsigned long deiir = 0, gtiir = 0, deier = 0, pmiir = 0;
	unsigned long dspa_status = 0, dspb_status = 0;
	u32 hpd_stat = 0;

	deier = VLV_READ32(VLV_IER);

	/* Temporarily turn off all display interrupts */
	VLV_WRITE32(VLV_IER, 0);
	VLV_READ32(VLV_IER);
	/* Temporarily turn off master GTLC interrupt control (2d/3d/video/pm) */
	VLV_WRITE32(GTLC_MASTER_IRQ_CTRL, 0);

	/* Sample our sticky IIR bits for display, GT and PM*/
	deiir = VLV_READ32(VLV_IIR);
	gtiir = VLV_READ32(GTLC_IIR);
	pmiir = VLV_READ32(PM_IIR);

	/*pmiir will be reseted later. This is keep the pmiir.*/
	priv.irq_pmiir=pmiir;

	if (pmiir & PM_RND_GEYSERVILLE_UP_EVAL) {
		priv.irq_pm |= IGD_IRQ_PM_UP_EVENT;
		/*EMGD_ERROR("PM_RND_GEYSERVILLE_UP_EVAL detected, pmiir=%lx", pmiir);*/
	}

	if (deiir == 0 && gtiir == 0 && pmiir == 0) {
		VLV_WRITE32(GTLC_MASTER_IRQ_CTRL, GTLC_MASTER_IRQ_ENABLE);
		VLV_WRITE32(VLV_IER, deier);
		VLV_READ32(VLV_IER);
		return ISR_NOT_OURS;
	}

	/* check if we have pipe a events, read the pipe a status and clear that first */
	if(deiir & DSP_PIPEA_EVENTS){
		dspa_status = VLV_READ32(PIPEASTAT);
		VLV_WRITE32(PIPEASTAT, dspa_status);
	}
	/* check if we have pipe b events, read the pipe b status and clear that first */
	if(deiir & DSP_PIPEB_EVENTS){
		dspb_status = VLV_READ32(PIPEBSTAT);
		VLV_WRITE32(PIPEBSTAT, dspb_status);
	}

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		if (deiir & DSP_HOTPLUG_EVENTS) {
			hpd_stat = VLV_READ32(PORT_HOTPLUG_STAT);

			/* clear the hotplug stat */
			EMGD_DEBUG("hotplug event received, stat 0x%08x\n", hpd_stat);
			VLV_WRITE32(PORT_HOTPLUG_STAT, hpd_stat);
		}
	}

	/* should clear PCH hotplug event before clear CPU irq */
	VLV_WRITE32(VLV_IIR, deiir);
	VLV_WRITE32(GTLC_IIR, gtiir);
	VLV_WRITE32(PM_IIR, pmiir);

	VLV_WRITE32(GTLC_MASTER_IRQ_CTRL, GTLC_MASTER_IRQ_ENABLE);
	VLV_WRITE32(VLV_IER, deier);
	VLV_READ32(VLV_IER);

	if (gtiir & (GT_RND_MI_USER_EVENT | GT_RND_PIPE_NOTIFY)) {
		priv.irq_graphtrans |= IGD_IRQ_GT_RND;
	}
	if (gtiir & GT_VID_MI_USER_EVENT) {
		priv.irq_graphtrans |= IGD_IRQ_GT_VID;
	}
	if (gtiir & GT_BLT_MI_USER_EVENT) {
		priv.irq_graphtrans |= IGD_IRQ_GT_BLT;
	}

	if (dspa_status & START_VBLANKA) {
		priv.irq_display |= IGD_IRQ_DISP_VBLANK_A;
	}
	if (dspb_status & START_VBLANKB) {
		priv.irq_display |= IGD_IRQ_DISP_VBLANK_B;
	}
	if (dspa_status & PLANEA_FLIPDONE) {
		priv.irq_display |= IGD_IRQ_DISP_FLIP_A;
	}
	if (dspb_status & PLANEB_FLIPDONE) {
		priv.irq_display |= IGD_IRQ_DISP_FLIP_B;
	}
	if (dspa_status & SPRITEA_FLIPDONE) {
		EMGD_DEBUG("Sprite A flip done detected");
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_A;
	}
	if (dspa_status & SPRITEB_FLIPDONE) {
                EMGD_DEBUG("Sprite B flip done detected");
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_B;
	}
	if (dspb_status & SPRITEC_FLIPDONE) {
                EMGD_DEBUG("Sprite C flip done detected");
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_C;
	}
	if (dspb_status & SPRITED_FLIPDONE) {
                EMGD_DEBUG("Sprite D flip done detected");
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_D;
	}

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		if (hpd_stat & HPD_DIG_B_SHORT || hpd_stat & HPD_DIG_B_LONG)
			priv.irq_hpd |= IGD_IRQ_HPD_DIG_B;

		if (hpd_stat & HPD_DIG_C_SHORT || hpd_stat & HPD_DIG_C_LONG)
			priv.irq_hpd |= IGD_IRQ_HPD_DIG_C;

		if (hpd_stat & HPD_CRT_HIGH)
			priv.irq_hpd |= IGD_IRQ_HPD_CRT;
	}

	return ISR_HANDLED;
}

/*!
 *
 * @param oal_callbacks_t *oal_callbacks_table
 *
 * @return void
 */
void mask_int_helper_vlv(oal_callbacks_t *oal_callbacks_table,
	interrupt_data_t *int_mask)
{
	unsigned long deimr = VLV_READ32(VLV_IMR);
	unsigned long dspa_status = VLV_READ32(PIPEASTAT);
	unsigned long dspb_status = VLV_READ32(PIPEBSTAT);

	if (int_mask->irq_display & IGD_IRQ_HPD_GENERAL) {
		EMGD_DEBUG("Disable HPD interrupt");
		deimr |= DSP_HOTSYNC_IRQ;
	}

	if (int_mask->irq_display & IGD_IRQ_DISP_VBLANK_A) {
		/* turn on vblanka done event */
		dspa_status |= START_VBLANKA_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEASTAT, dspa_status);
		VLV_READ32(PIPEASTAT);
		deimr &= ~DSP_PIPEA_VBLANK;
		deimr &= ~DSP_PIPEA_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_DISP_VBLANK_B) {
		/* turn on vblankb done event */
		dspb_status |= START_VBLANKB_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEBSTAT, dspb_status);
		VLV_READ32(PIPEBSTAT);
		deimr &= ~DSP_PIPEB_VBLANK;
		deimr &= ~DSP_PIPEB_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_DISP_FLIP_A) {
		/* turn on flip-a done event */
		dspa_status |= PLANEA_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEASTAT, dspa_status);
		VLV_READ32(PIPEASTAT);
		deimr &= ~DSP_PIPEA_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_SPRITE_FLIP_A) {
		/* turn on flip-spr-a done event */
		dspa_status |= SPRITEA_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEASTAT, dspa_status);
		VLV_READ32(PIPEASTAT);
		deimr &= ~DSP_PIPEA_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_SPRITE_FLIP_B) {
		/* turn on flip-spr-b done event */
		dspa_status |= SPRITEB_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEASTAT, dspa_status);
		VLV_READ32(PIPEASTAT);
		deimr &= ~DSP_PIPEA_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_DISP_FLIP_B) {
		/* turn on flip-b done event */
		dspb_status |= PLANEB_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEBSTAT, dspb_status);
		VLV_READ32(PIPEBSTAT);
		deimr &= ~DSP_PIPEB_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_SPRITE_FLIP_C) {
		/* turn on flip-spr-c done event */
		dspb_status |= SPRITEC_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEBSTAT, dspb_status);
		VLV_READ32(PIPEBSTAT);
		deimr &= ~DSP_PIPEB_EVENT;
	}
	if (int_mask->irq_display & IGD_IRQ_SPRITE_FLIP_D) {
		/* turn on flip-spr-d done event */
		dspb_status |= SPRITED_FLIPDONE_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEBSTAT, dspb_status);
		VLV_READ32(PIPEBSTAT);
		deimr &= ~DSP_PIPEB_EVENT;
	}

	VLV_WRITE32(VLV_IMR, deimr);
	VLV_READ32(VLV_IMR);
}

/*!
 *
 * @param oal_callbacks_t *oal_callbacks_table
 *
 * @return void
 */
void unmask_int_helper_vlv(oal_callbacks_t *oal_callbacks_table,
	interrupt_data_t *int_unmask)
{
	unsigned long deimr = VLV_READ32(VLV_IMR);
	unsigned long dspa_status = VLV_READ32(PIPEASTAT);
	unsigned long dspb_status = VLV_READ32(PIPEBSTAT);

	if (int_unmask->irq_display & IGD_IRQ_HPD_GENERAL) {
		EMGD_DEBUG("Enable HPD interrupt");
		deimr &= DSP_HOTSYNC_IRQ;
	}

	if (int_unmask->irq_display & IGD_IRQ_DISP_VBLANK_A) {
		/* turn off start vblank on 2nd level */
		dspa_status &= ~START_VBLANKA_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEASTAT, dspa_status);
		VLV_READ32(PIPEASTAT);
		deimr |= DSP_PIPEA_VBLANK;
	}
	if (int_unmask->irq_display & IGD_IRQ_DISP_VBLANK_B) {
		/* turn off start vblank on 2nd level */
		dspb_status &= ~START_VBLANKB_ENABLE;
		/* dont clear any interupts - we're not handling that here*/
		dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
		VLV_WRITE32(PIPEBSTAT, dspb_status);
		VLV_READ32(PIPEBSTAT);
		deimr |= DSP_PIPEB_VBLANK;
	}
	if (int_unmask->irq_display & IGD_IRQ_DISP_FLIP_A) {
		if((dspa_status & DSPA_2NDLEVEL_IRQS_ENABLE) == PLANEA_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-a done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEASTAT, dspa_status);
			VLV_READ32(PIPEASTAT);
			deimr |= DSP_PIPEA_EVENT;
		}
	}
	if (int_unmask->irq_display & IGD_IRQ_SPRITE_FLIP_A) {
		if((dspa_status & DSPA_2NDLEVEL_IRQS_ENABLE) == SPRITEA_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-spr-a done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEASTAT, dspa_status);
			VLV_READ32(PIPEASTAT);
			deimr |= DSP_PIPEA_EVENT;
		}
	}
	if (int_unmask->irq_display & IGD_IRQ_SPRITE_FLIP_B) {
		if((dspa_status & DSPA_2NDLEVEL_IRQS_ENABLE) == SPRITEB_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-spr-b done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspa_status &= ~DSPA_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEASTAT, dspa_status);
			VLV_READ32(PIPEASTAT);
			deimr |= DSP_PIPEA_EVENT;
		}
	}
	if (int_unmask->irq_display & IGD_IRQ_DISP_FLIP_B) {
		if((dspb_status & DSPB_2NDLEVEL_IRQS_ENABLE) == PLANEB_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-a done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEBSTAT, dspb_status);
			VLV_READ32(PIPEBSTAT);
			deimr |= DSP_PIPEB_EVENT;
		}
	}
	if (int_unmask->irq_display & IGD_IRQ_SPRITE_FLIP_C) {
		if((dspb_status & DSPB_2NDLEVEL_IRQS_ENABLE) == SPRITEC_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-a done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEBSTAT, dspb_status);
			VLV_READ32(PIPEBSTAT);
			deimr |= DSP_PIPEB_EVENT;
		}
	}
	if (int_unmask->irq_display & IGD_IRQ_SPRITE_FLIP_D) {
		if((dspb_status & DSPB_2NDLEVEL_IRQS_ENABLE) == SPRITED_FLIPDONE_ENABLE){
			/* right now we're only asking for flip-a done,
			 * if we dont want it, we can disable all events on pipea */
			/* turn off all events */
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_ENABLE;
			/* dont clear any interupts - we're not handling that here*/
			dspb_status &= ~DSPB_2NDLEVEL_IRQS_CLEAR;
			VLV_WRITE32(PIPEBSTAT, dspb_status);
			VLV_READ32(PIPEBSTAT);
			deimr |= DSP_PIPEB_EVENT;
		}
	}

	VLV_WRITE32(VLV_IMR, deimr);
	VLV_READ32(VLV_IMR);
}

/*!
 *
 * @param void *int_info_data
 *
 * @return 1 is interrupt type supported and events occured previously.
 * @return 0 is interrupt type supported but events have not occur.
 * @return -1 is interrupt type not supported.
 */
int request_int_info_helper_vlv(interrupt_data_t *int_info_data)
{
	int_info_data->irq_display = priv.irq_display;
	int_info_data->irq_graphtrans = priv.irq_graphtrans;
	int_info_data->irq_enabled = priv.irq_enabled;
	int_info_data->irq_pm = priv.irq_pm;
	int_info_data->irq_pmiir = priv.irq_pmiir;
	int_info_data->irq_hpd = priv.irq_hpd;

	return 0;
}

/*!
 *
 * @param void *int_info_data
 *
 * @return 1 void.
 */
void clear_occured_int_helper_vlv(void *int_info_data)
{
}

void clear_cache_int_helper_vlv(oal_callbacks_t *oal_callbacks_table)
{
	priv.irq_display = 0;
	priv.irq_graphtrans = 0;
	priv.irq_pm = 0;
	priv.irq_pmiir=0;
	priv.irq_hpd = 0;
}

/*!
 *
 * @param VLV_interrupt_data_t **int_data
 *
 * @return 1 on failure
 * @return 0 on success
 */
int dpc_helper_vlv(void *p)
{
	interrupt_data_t **int_data = p;

	if(int_data) {
		*int_data = &priv;

		return 0;
	}

	return 1;
}

void uninstall_int_helper_vlv(oal_callbacks_t *oal_callbacks_table)
{
	/* Disable HW Status register mask for GPU Render, Blitter and Video */
	/* For Render its special - it overlaps with main display registers in VLV *
	 * with the 0x180000 offset so we cant use VLV_READ32 or VLV_WRITE32         */
	EMGD_WRITE32(0xffffffff, virt_mmadr + HWSTAM);
	VLV_WRITE32(BLITTER_HWSTAM, 0xffffffff);
	VLV_WRITE32(VIDEO_HWSTAM, 0xffffffff);

	/* Disable and clear Display Interrupts */
	VLV_WRITE32(VLV_IMR, 0xffffffff);
	VLV_WRITE32(VLV_IER, 0);
	VLV_READ32(VLV_IER);
	VLV_WRITE32(VLV_IIR, VLV_READ32(VLV_IIR));

	/* Disable and clear GT Interrupts */
	VLV_WRITE32(GTLC_IMR, 0xffffffff);
	VLV_WRITE32(GTLC_IER, 0);
	VLV_READ32(GTLC_IER);
	VLV_WRITE32(GTLC_IIR, VLV_READ32(GTLC_IIR));

	/* Disable and clear PM Interrupts */
	VLV_WRITE32(PM_IMR, 0xffffffff);
	VLV_WRITE32(PM_IER, 0);
	VLV_READ32(PM_IER);
	VLV_WRITE32(PM_IIR, VLV_READ32(PM_IIR));
}

interrupt_helper_dispatch_t interrupt_helper_vlv = {
	init_int_vlv,
	stop_irq_vlv,
	start_irq_vlv,
	isr_helper_vlv,
	mask_int_helper_vlv,
	unmask_int_helper_vlv,
	request_int_info_helper_vlv,
	clear_occured_int_helper_vlv,
	clear_cache_int_helper_vlv,
	dpc_helper_vlv,
	uninstall_int_helper_vlv,
};
