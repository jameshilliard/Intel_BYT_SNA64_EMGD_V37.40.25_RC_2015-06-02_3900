/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: interrupt_snb.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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


static interrupt_data_t priv;
static unsigned char *virt_mmadr;

#define IRQ_LOOP_STRESS			0

#define IGD_POWERGATE_DECODE_MSK	0xC

#define SNB_WRITE32(reg, val)   writel(val, virt_mmadr + reg)
#define SNB_READ32(reg)         readl(virt_mmadr + reg)

void init_int_snb(unsigned char *mmadr)
{
	virt_mmadr = (unsigned char *) mmadr;
	priv.irq_display = 0;
	priv.irq_graphtrans = 0;
	priv.irq_enabled = 0;
}

void stop_irq_snb(oal_callbacks_t *oal_callbacks_table)
{
	SNB_WRITE32(HWSTAM, 0xffffffff);

	/* Disable Display Interrupts */
	SNB_WRITE32(DEIMR, 0xffffffff);
	SNB_WRITE32(DEIER, 0);
	SNB_READ32(DEIER);

	/* Disable GT Interrupts */
	SNB_WRITE32(GTIMR, 0xffffffff);
	SNB_WRITE32(GTIER, 0);
	SNB_READ32(GTIER);

	priv.irq_enabled = 0;
}

void start_irq_snb(oal_callbacks_t *oal_callbacks_table)
{
	SNB_WRITE32(HWSTAM, 0x0000effe);

	/* Workaround stalls observed on Sandy Bridge GPUs by
	 * making the blitter command streamer generate a
	 * write to the Hardware Status Page for
	 * MI_USER_INTERRUPT.  This appears to serialize the
	 * previous seqno write out before the interrupt
	 * happens.
	 */
	SNB_WRITE32(BLITTER_HWSTAM, ~GT_BLT_MI_USER_EVENT);
	SNB_WRITE32(VIDEO_HWSTAM, ~GT_VID_MI_USER_EVENT);

	/* Clear left over interrupts */
	SNB_WRITE32(DEIIR, SNB_READ32(DEIIR));
	SNB_WRITE32(DEIMR, ~DE_IDENTITY_INTERRUPTS);

	/* Disable GT Interrupts */
	SNB_WRITE32(GTIIR, SNB_READ32(GTIIR));
	SNB_WRITE32(GTIMR, GT_ALL_INTERRUPTS);

	SNB_WRITE32(GTIER, GT_ENABLE_INTERRUPTS);
	SNB_READ32(GTIER);
	SNB_WRITE32(DEIER, DE_ENABLE_INTERRUPTS);
	SNB_READ32(DEIER);

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
int isr_helper_snb(oal_callbacks_t *oal_callbacks_table)
{
	unsigned long deiir = 0, gtiir = 0, deier = 0, pmiir = 0;

	deier = SNB_READ32(DEIER);

	SNB_WRITE32(DEIER, deier & ~DE_MASTER_IRQ_ENABLE);
	SNB_READ32(DEIER);

	deiir = SNB_READ32(DEIIR);
	gtiir = SNB_READ32(GTIIR);
	pmiir = SNB_READ32(PMIIR);

	if (deiir == 0 && gtiir == 0 && pmiir == 0) {
		SNB_WRITE32(DEIER, deier);
		SNB_READ32(DEIER);

		return ISR_NOT_OURS;
	}

	/* should clear PCH hotplug event before clear CPU irq */
	SNB_WRITE32(DEIIR, deiir);
	SNB_WRITE32(GTIIR, gtiir);
	SNB_WRITE32(PMIIR, pmiir);

	SNB_WRITE32(DEIER, deier);
	SNB_READ32(DEIER);

	if (gtiir & (GT_RND_MI_USER_EVENT | GT_RND_PIPE_NOTIFY)) {
		priv.irq_graphtrans |= IGD_IRQ_GT_RND;
	}
	if (gtiir & GT_VID_MI_USER_EVENT) {
		priv.irq_graphtrans |= IGD_IRQ_GT_VID;
	}
	if (gtiir & GT_BLT_MI_USER_EVENT) {
		priv.irq_graphtrans |= IGD_IRQ_GT_BLT;
	}

	if (deiir & DE_PIPE_A_VBLANK) {
		priv.irq_display |= IGD_IRQ_DISP_VBLANK_A;
	}
	if (deiir & DE_PIPE_B_VBLANK) {
		priv.irq_display |= IGD_IRQ_DISP_VBLANK_B;
	}

	if (deiir & DE_PLANE_A_FLIPPED) {
		priv.irq_display |= IGD_IRQ_DISP_FLIP_A;
	}
	if (deiir & DE_PLANE_B_FLIPPED) {
		priv.irq_display |= IGD_IRQ_DISP_FLIP_B;
	}
	if (deiir & DE_SPRITE_A_FLIPPED) {
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_A;
	}
	if (deiir & DE_SPRITE_B_FLIPPED) {
		priv.irq_display |= IGD_IRQ_SPRITE_FLIP_B;
	}

	return ISR_HANDLED;
}

/*!
 *
 * @param oal_callbacks_t *oal_callbacks_table
 *
 * @return void
 */
void mask_int_helper_snb(oal_callbacks_t *oal_callbacks_table,
	interrupt_data_t *int_mask)
{
	unsigned long deimr = SNB_READ32(DEIMR);

	if (int_mask->irq_display & IGD_IRQ_DISP_VBLANK_A) {
		deimr &= ~DE_PIPE_A_VBLANK;
	}

	if (int_mask->irq_display & IGD_IRQ_DISP_VBLANK_B) {
		deimr &= ~DE_PIPE_B_VBLANK;
	}

	SNB_WRITE32(DEIMR, deimr);
	SNB_READ32(DEIMR);
}

/*!
 *
 * @param oal_callbacks_t *oal_callbacks_table
 *
 * @return void
 */
void unmask_int_helper_snb(oal_callbacks_t *oal_callbacks_table,
	interrupt_data_t *int_unmask)
{
	unsigned long deimr = SNB_READ32(DEIMR);

	if (int_unmask->irq_display & IGD_IRQ_DISP_VBLANK_A) {
		deimr |= DE_PIPE_A_VBLANK;
	}

	if (int_unmask->irq_display & IGD_IRQ_DISP_VBLANK_B) {
		deimr |= DE_PIPE_B_VBLANK;
	}

	SNB_WRITE32(DEIMR, deimr);
	SNB_READ32(DEIMR);
}

/*!
 *
 * @param void *int_info_data
 *
 * @return 1 is interrupt type supported and events occured previously.
 * @return 0 is interrupt type supported but events have not occur.
 * @return -1 is interrupt type not supported.
 */
int request_int_info_helper_snb(interrupt_data_t *int_info_data)
{
	int_info_data->irq_display = priv.irq_display;
	int_info_data->irq_graphtrans = priv.irq_graphtrans;
	int_info_data->irq_enabled = priv.irq_enabled;

	return 0;
}

/*!
 *
 * @param void *int_info_data
 *
 * @return 1 void.
 */
void clear_occured_int_helper_snb(void *int_info_data)
{
}

void clear_cache_int_helper_snb(oal_callbacks_t *oal_callbacks_table)
{
	priv.irq_display = 0;
	priv.irq_graphtrans = 0;
}

/*!
 *
 * @param snb_interrupt_data_t **int_data
 *
 * @return 1 on failure
 * @return 0 on success
 */
int dpc_helper_snb(void *p)
{
	interrupt_data_t **int_data = p;

	if(int_data) {
		*int_data = &priv;

		return 0;
	}

	return 1;
}

void uninstall_int_helper_snb(oal_callbacks_t *oal_callbacks_table)
{
	SNB_WRITE32(HWSTAM, 0xffffffff);

	/* Disable and clear Display Interrupts */
	SNB_WRITE32(DEIMR, 0xffffffff);
	SNB_WRITE32(DEIER, 0);
	SNB_READ32(DEIER);
	SNB_WRITE32(DEIIR, SNB_READ32(DEIIR));

	/* Disable and clear GT Interrupts */
	SNB_WRITE32(GTIMR, 0xffffffff);
	SNB_WRITE32(GTIER, 0);
	SNB_READ32(GTIER);
	SNB_WRITE32(GTIIR, SNB_READ32(GTIIR));
}

interrupt_helper_dispatch_t interrupt_helper_snb = {
	init_int_snb,
	stop_irq_snb,
	start_irq_snb,
	isr_helper_snb,
	mask_int_helper_snb,
	unmask_int_helper_snb,
	request_int_info_helper_snb,
	clear_occured_int_helper_snb,
	clear_cache_int_helper_snb,
	dpc_helper_snb,
	uninstall_int_helper_snb,
};
