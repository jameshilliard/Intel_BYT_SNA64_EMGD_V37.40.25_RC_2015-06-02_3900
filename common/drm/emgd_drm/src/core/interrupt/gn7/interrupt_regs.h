/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: interrupt_regs.h
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
 *  This file contains the interrupt related register definition  and
 *  macros for the Sandy Bridge platform.
 *-----------------------------------------------------------------------------
 */

#ifndef _INTERRUPT_REGS_GN7_H
#define _INTERRUPT_REGS_GN7_H

#include <general.h>
//#include <gn6/regs.h>

/*-----------------------------------------------------------------------------
 * GPU (Display+GTT+Error) + GTLC (3d/Video + PM) IRQ Registers and bits
 *---------------------------------------------------------------------------*/

/* GPU IRQ Registers */
/* Refer to i915/i915_regs.h */

/* GPU IRQ Bits Definition */
#define DSP_HOTSYNC_IRQ          BIT17
#define MASTER_ERROR             BIT15
#define DSP_PIPEA_VBLANK         BIT7
#define DSP_PIPEA_EVENT          BIT6
#define DSP_PIPEB_VBLANK         BIT5
#define DSP_PIPEB_EVENT          BIT4
#define DSP_A_DPBM               BIT3 /*DPST or BLM*/
#define DSP_B_DPBM               BIT2 /*DPST or BLM*/
#define ASLE                     BIT0

/* TODO: Its now handling hotplug and ASLE still needs to be handled in the future */
#define DSP_ENABLE_INTERRUPTS    (MASTER_ERROR | DSP_HOTSYNC_IRQ |\
                                   DSP_PIPEA_VBLANK | DSP_PIPEA_EVENT |\
                                   DSP_PIPEB_VBLANK | DSP_PIPEB_EVENT |\
                                   DSP_A_DPBM | DSP_B_DPBM /*| ASLE*/)
#define DSP_ALL_INTERRUPTS       (MASTER_ERROR | DSP_HOTSYNC_IRQ |\
                                   DSP_PIPEA_VBLANK | DSP_PIPEA_EVENT |\
                                   DSP_PIPEB_VBLANK | DSP_PIPEB_EVENT |\
                                   DSP_A_DPBM | DSP_B_DPBM | ASLE)


#define DSP_PIPEA_EVENTS         (DSP_PIPEA_VBLANK | DSP_PIPEA_EVENT | DSP_A_DPBM)
#define DSP_PIPEB_EVENTS         (DSP_PIPEB_VBLANK | DSP_PIPEB_EVENT | DSP_B_DPBM)
#define DSP_HOTPLUG_EVENTS	BIT17

/* GPU IRQ breakdown - Error Interrupt Registers */
#define ERR_IIR                  0x180000 + 0x20B0
#define ERR_IMR                  0x180000 + 0x20B4
#define ERR_ISR                  0x180000 + 0x20B8

/* GPU IRQ breakdown - Error Interrupt Bits Definition */
#define GUNIT_TLB_DATAE          BIT6
#define GUNIT_TLB_PTE            BIT5
#define PAGE_TABLE_ERROR         BIT4
#define CLAIM_ERROR              BIT0


/* GPU IRQ breakdown - Display Interrupt Registers */
#define PIPEASTAT               0x70024
#define PIPEBSTAT               0x71024
#define DPFLIPSTAT              0x70028
#define DPINVGTT                0x7002c /* Display Invalid GTT PTE Status */

#define DPFLIP_DSPTOGT_ALLEVENTS 0x3f3f0000

/* GPU IRQ breakdown - Display Interrupt Bits Definition */
/* -----------> PIPEASTAT --------->*/
#define FIFOA_UNDERUN            BIT31
#define SPRITEB_FLIPDONE_ENABLE  BIT30
#define CRCA_ERROR_ENABLE        BIT29
#define CRCA_DONE_ENABLE         BIT28
#define GMBUS_EVENT_ENABLE       BIT27
#define PLANEA_FLIPDONE_ENABLE   BIT26
#define VSYNCA_ENABLE            BIT25
#define DISPA_LINECOMP_ENABLE    BIT24
/*#define DPSTA_EVENT_ENABLE       BIT23 - not for VLV?*/
#define SPRITEA_FLIPDONE_ENABLE  BIT22
#define ODDFIELDA_EVENT_ENABLE   BIT21
#define EVENFIELDA_EVENT_ENABLE  BIT20
#define PERFCOUNT_EVENT_ENABLE   BIT19
#define START_VBLANKA_ENABLE     BIT18
#define FRAMESTARTA_ENABLE       BIT17
#define START_HBLANKA_ENABLE     BIT16
#define SPRITEB_FLIPDONE         BIT15
#define SPRITEA_FLIPDONE         BIT14
#define CRCA_ERROR               BIT13
#define CRCA_DONE                BIT12
#define GMBUS_EVENT              BIT11
#define PLANEA_FLIPDONE          BIT10
#define VSYNCA                   BIT9
#define DISPA_LINECOMP           BIT8
/*#define DPSTA_EVENT               BIT7 - not for VLV?*/
#define PIPEA_PSR                BIT6
#define ODDFIELDA_EVENT          BIT5
#define EVENFIELDA_EVENT         BIT4
#define PERFCOUNT_EVENT          BIT3
#define START_VBLANKA            BIT2
#define FRAMESTARTA              BIT1
#define START_HBLANKA            BIT0

/* TODO : For hotplug, PSR, stereo, we need more IRQs later */
#define DSPA_2NDLEVEL_IRQS_ENABLE (PLANEA_FLIPDONE_ENABLE | START_VBLANKA_ENABLE |\
                                    SPRITEA_FLIPDONE_ENABLE | SPRITEB_FLIPDONE_ENABLE)
/* Only handling the most frequently used interrupts for now */
#define DSPA_2NDLEVEL_IRQS_CLEAR   (PLANEA_FLIPDONE | START_VBLANKA | SPRITEA_FLIPDONE | SPRITEB_FLIPDONE)

/* -----------> PIPEBSTAT --------->*/
#define FIFOB_UNDERUN            BIT31
#define SPRITED_FLIPDONE_ENABLE  BIT30
#define CRCB_ERROR_ENABLE        BIT29
#define CRCB_DONE_ENABLE         BIT28
#define PERFCOUNT2_EVENT_ENABLE  BIT27
#define PLANEB_FLIPDONE_ENABLE   BIT26
#define VSYNCB_ENABLE            BIT25
#define DISPB_LINECOMP_ENABLE    BIT24
/*#define DPSTB_EVENT_ENABLE       BIT23 - not for VLV?*/
#define SPRITEC_FLIPDONE_ENABLE  BIT22
#define ODDFIELDB_EVENT_ENABLE   BIT21
#define EVENFIELDB_EVENT_ENABLE  BIT20
#define PIPEB_PSR_ENABLE         BIT19
#define START_VBLANKB_ENABLE     BIT18
#define FRAMESTARTB_ENABLE       BIT17
#define START_HBLANKB_ENABLE     BIT16
#define SPRITED_FLIPDONE         BIT15
#define SPRITEC_FLIPDONE         BIT14
#define CRCB_ERROR               BIT13
#define CRCB_DONE                BIT12
#define PERFCOUNT2_EVENT         BIT11
#define PLANEB_FLIPDONE          BIT10
#define VSYNCB                   BIT9
#define DISPB_LINECOMP           BIT8
/*#define DPST_EVENT               BIT7 - not for VLV?*/
/* MBZ?                          BIT6 */
#define ODDFIELDB_EVENT          BIT5
#define EVENFIELDB_EVENT         BIT4
#define PIPEB_PSR                BIT3
#define START_VBLANKB            BIT2
#define FRAMESTARTB              BIT1
#define START_HBLANKB            BIT0

/* HOTPLUG Events */
#define HPD_DIG_B_SHORT		BIT17
#define HPD_DIG_B_LONG		BIT18
#define HPD_DIG_C_SHORT		BIT19
#define HPD_DIG_C_LONG		BIT20
#define HPD_DPB_AUX		BIT4
#define HPD_DPC_AUX		BIT5
#define HPD_CRT_HIGH		BIT11


/* TODO : For hotplug, PSR, stereo, we need more IRQs later */
#define DSPB_2NDLEVEL_IRQS_ENABLE (PLANEB_FLIPDONE_ENABLE | START_VBLANKB_ENABLE |\
                                    SPRITEC_FLIPDONE_ENABLE | SPRITED_FLIPDONE_ENABLE)
/* Only handling the most frequently used interrupts for now */
#define DSPB_2NDLEVEL_IRQS_CLEAR   (PLANEB_FLIPDONE | START_VBLANKB | SPRITEC_FLIPDONE | SPRITED_FLIPDONE)

/* GPU IRQ breakdown - GTLC IRQ Register and Bit Definitions */
#define GTLC_MASTER_IRQ_CTRL     0x4400C
#define GTLC_MASTER_IRQ_ENABLE   BIT31

/* GPU IRQ breakdown - GTLC GT (2D/3D/Video) IRQ Registers */
#define GTLC_ISR                 0x44010  /* GT Interrupt Status (RO) */
#define GTLC_IMR                 0x44014  /* GT Interrupt Mask (RW) */
#define GTLC_IIR                 0x44018  /* GT Interrupt Identity (RWC) */
#define GTLC_IER                 0x4401C  /* GT Interrupt Enable (RW) */

/* GPU IRQ breakdown - GTLC 2nd level Blitter IRQ Registers */
#define BCS_IMR                  0x220A8
/* GPU IRQ breakdown - GTLC 2nd level Render IRQ Registers */
#define RCS_IMR                  0x020A8
/* GPU IRQ breakdown - GTLC 2nd level Video IRQ Registers */
#define VCS_IMR                  0x120A8

/* GPU IRQ breakdown - GTLC GT (2D/3D/Video) Bit Definitions */
#define GT_BLT_AS_CTX_SWITCH     BIT30    /* Blitter AS context switch */
#define GT_BLT_PD_FAULT          BIT29    /* Blitter Page Directory fault */
#define GT_BLT_MI_FLUSH_NOTIFY   BIT26    /* Blitter MI Flush DW notify */
#define GT_BLT_CMDSTR_ERR        BIT25    /* Blitter Command Streamer error */
#define GT_BLT_MMIO_SYNC_FLUSH   BIT24    /* Blitter MMIO Sync Flush */
#define GT_BLT_MI_USER_EVENT     BIT22    /* Blitter Command Stream User Interrupt */
#define GT_BLT_ALL_IRQS          (GT_BLT_AS_CTX_SWITCH | GT_BLT_PD_FAULT | GT_BLT_MI_FLUSH_NOTIFY |\
								  GT_BLT_CMDSTR_ERR | GT_BLT_MMIO_SYNC_FLUSH | GT_BLT_MI_USER_EVENT)

#define GT_VID_AS_CTX_SWITCH     BIT20    /* Video AS context switch */
#define GT_VID_PD_FAULT          BIT19    /* Video page directory fault */
#define GT_VID_CMDSTR_CNT_EXCEED BIT18    /* Video Command Stream Watchdog conter exceed */
#define GT_VID_RESERVED1         BIT17
#define GT_VID_MI_FLUSH_NOTIFY   BIT16    /* Video MI Flush DW notify */
#define GT_VID_CMDSTR_ERR        BIT15    /* Video Command Stream error */
#define GT_VID_MMIO_SYNC_FLUSH   BIT14    /* Video MMIO Sync Flush */
#define GT_VID_RESERVED2         BIT13
#define GT_VID_MI_USER_EVENT     BIT12    /* Video Command Stream User Event */
#define GT_VID_ALL_IRQS          (GT_VID_AS_CTX_SWITCH | GT_VID_PD_FAULT | GT_VID_CMDSTR_CNT_EXCEED |\
								 GT_VID_MI_FLUSH_NOTIFY | GT_VID_CMDSTR_ERR |\
								 GT_VID_MMIO_SYNC_FLUSH | GT_VID_MI_USER_EVENT)

/*#define GT_RND_MONITOR_BUF_HALF      BIT9   Render monitor buffer half full - not VLV?*/
#define GT_RND_AS_CTX_SWITCH     BIT8     /* Render AS context switch */
#define GT_RND_PD_FAULT          BIT7     /* Render page directory fault */
#define GT_RND_CMDSTR_CNT_EXCEED BIT6     /* Render Command Stream Watchdog conter exceed */
#define GT_RND_L3_PARITY_ERR     BIT5     /* Render Command L3 Parity Error */
#define GT_RND_PIPE_NOTIFY       BIT4     /* Render Pipe Control Notify */
#define GT_RND_CMDSTR_ERR        BIT3     /* Render Command Stream error */
#define GT_RND_MMIO_SYNC_FLUSH   BIT2     /* Render MMIO Sync Flush */
#define GT_RND_EU_DEBUG          BIT1     /* Render EU Debug */
#define GT_RND_MI_USER_EVENT     BIT0     /* Render Command Stream User Event */
#define GT_RND_ALL_IRQS          (GT_RND_AS_CTX_SWITCH | GT_RND_PD_FAULT | GT_RND_CMDSTR_CNT_EXCEED |\
								  GT_RND_L3_PARITY_ERR | GT_RND_PIPE_NOTIFY | GT_RND_CMDSTR_ERR |\
								  GT_RND_MMIO_SYNC_FLUSH | GT_RND_EU_DEBUG | GT_RND_MI_USER_EVENT )

#define GT_ALL_INTERRUPTS        0x675dd3df
#define GT_ENABLE_INTERRUPTS     (GT_BLT_MI_USER_EVENT | GT_VID_MI_USER_EVENT | GT_RND_MI_USER_EVENT)

/* GPU IRQ breakdown - GTLC PM (Power Mngt) IRQ Registers */
#define PM_ISR                   0x44020  /* Power Management Interrupt Status (RO)*/
#define PM_IMR                   0x44024  /* Power Management Interrupt Mask (RW) */
#define PM_IIR                   0x44028  /* Power Management Interrupt Identity (RWC) */
#define PM_IER                   0x4402C  /* Power Management Interrupt Enable (RW) */

/* GPU IRQ breakdown - GTLC PM (Power Mngt) Bit Definitions */
#define PM_RND_FREQ_RC6_DOWN_TIMEOUT BIT6
#define PM_RP_UP_EVENT           BIT5
#define PM_RP_DOWN_EVENT         BIT4
#define PM_RND_GEYSERVILLE_UP_EVAL BIT2
#define PM_RND_GEYSERVILLE_DOWN_EVAL BIT1

#define PM_ALL_INTERRUPTS        0x00000076
#define PM_ENABLE_INTERRUPTS     0x00000000

#define HWSTAM                   0x02098  /* Hardware Status Mask */
#define VIDEO_HWSTAM             0x12098  /* Video Stream Hardware Status Mask */
#define BLITTER_HWSTAM           0x22098  /* Blitter Hardware Status Mask */


#endif
