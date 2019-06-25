/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: interrupt_regs.h
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
 *  This file contains the interrupt related register definition  and
 *  macros for the Sandy Bridge platform.
 *-----------------------------------------------------------------------------
 */

#ifndef _INTERRUPT_REGS_GN6_H
#define _INTERRUPT_REGS_GN6_H

#include <general.h>
//#include <gn6/regs.h>

/*-----------------------------------------------------------------------------
 * Instruction and Interrupt Control Registers (01000h - 02FFFh)
 *---------------------------------------------------------------------------*/
#define HWSTAM                   0x02098  /* Hardware Status Mask */
#define VIDEO_HWSTAM             0x12098  /* Video Stream Hardware Status Mask */
#define BLITTER_HWSTAM           0x22098  /* Blitter Hardware Status Mask */

/* Display Engine Interrupt Registers */
#define DEISR                    0x44000  /* Display Interrupt Status (RO) */
#define DEIMR                    0x44004  /* Display Interrupt Mask (RW) */
#define DEIIR                    0x44008  /* Display Interrupt Identity (RWC) */
#define DEIER                    0x4400C  /* Display Interrupt Enable (RW) */

/* Display Engine Interrupt Bits Definition */
#define DE_MASTER_IRQ_ENABLE     BIT31    /* This only apply to DEIER */
#define DE_SPRITE_B_FLIPPED      BIT29
#define DE_SPRITE_A_FLIPPED      BIT28
#define DE_PLANE_B_FLIPPED       BIT27
#define DE_PLANE_A_FLIPPED       BIT26
#define DE_PCU_EVENT             BIT25
#define DE_GTT_FAULT             BIT24
#define DE_POISON                BIT23
#define DE_PERFORM_COUNTER       BIT22
#define DE_PCH_DISPLAY_EVENT     BIT21
#define DE_AUX_CHANNEL_A         BIT20
#define DE_DP_A_HOTPLUG          BIT19
#define DE_GSE                   BIT18
#define DE_PIPE_B_VBLANK         BIT15
#define DE_PIPE_B_EVEN_FIELD     BIT14
#define DE_PIPE_B_ODD_FIELD      BIT13
#define DE_PIPE_B_LINE_COMPARE   BIT12
#define DE_PIPE_B_VSYNC          BIT11
#define DE_PIPE_B_CRC_DONE       BIT10
#define DE_PIPE_B_CRC_ERROR      BIT9
#define DE_PIPE_B_FIFO_UNDERRUN  BIT8
#define DE_PIPE_A_VBLANK         BIT7
#define DE_PIPE_A_EVEN_FIELD     BIT6
#define DE_PIPE_A_ODD_FIELD      BIT5
#define DE_PIPE_A_LINE_COMPARE   BIT4
#define DE_PIPE_A_VSYNC          BIT3
#define DE_PIPE_A_CRC_DONE       BIT2
#define DE_PIPE_A_CRC_ERROR      BIT1
#define DE_PIPE_A_FIFO_UNDERRUN  BIT0

#define DE_ALL_INTERRUPTS        0x3fffffff
#define DE_ENABLE_INTERRUPTS     (DE_MASTER_IRQ_ENABLE | DE_PCH_DISPLAY_EVENT | DE_GSE | DE_PLANE_A_FLIPPED | DE_PLANE_B_FLIPPED | DE_PIPE_A_VBLANK | DE_PIPE_B_VBLANK | DE_SPRITE_A_FLIPPED | DE_SPRITE_B_FLIPPED)
#define DE_IDENTITY_INTERRUPTS   (DE_PCH_DISPLAY_EVENT | DE_GSE | DE_PLANE_A_FLIPPED | DE_PLANE_B_FLIPPED | DE_SPRITE_A_FLIPPED | DE_SPRITE_B_FLIPPED)


/* Graphic Transactions Interrupt Registers */
#define GTISR                    0x44010  /* GT Interrupt Status (RO) */
#define GTIMR                    0x44014  /* GT Interrupt Mask (RW) */
#define GTIIR                    0x44018  /* GT Interrupt Identity (RWC) */
#define GTIER                    0x4401C  /* GT Interrupt Enable (RW) */

/* Graphic Transactons Interrupt Bits Definition */
#define GT_BLT_AS_CTX_SWITCH     BIT30    /* Blitter AS context switch */
#define GT_BLT_PD_FAULT          BIT29    /* Blitter Page Directory fault */
#define GT_BLT_MI_FLUSH_NOTIFY   BIT26    /* Blitter MI Flush DW notify */
#define GT_BLT_CMDSTR_ERR        BIT25    /* Blitter Command Streamer error */
#define GT_BLT_MMIO_SYNC_FLUSH   BIT24    /* Blitter MMIO Sync Flush */
#define GT_BLT_MI_USER_EVENT     BIT22    /* Blitter Command Stream User Interrupt */
#define GT_VID_AS_CTX_SWITCH     BIT20    /* Video AS context switch */
#define GT_VID_PD_FAULT          BIT19    /* Video page directory fault */
#define GT_VID_CMDSTR_CNT_EXCEED BIT18    /* Video Command Stream Watchdog conter exceed */
#define GT_VID_MI_FLUSH_NOTIFY   BIT16    /* Video MI Flush DW notify */
#define GT_VID_CMDSTR_ERR        BIT15    /* Video Command Stream error */
#define GT_VID_MMIO_SYNC_FLUSH   BIT14    /* Video MMIO Sync Flush */
#define GT_VID_MI_USER_EVENT     BIT12    /* Video Command Stream User Event */
#define GT_BSP_CNT_EXCEED        BIT9     /* Bit Stream Pipeline Counter Exceeded */
#define GT_RND_AS_CTX_SWITCH     BIT8     /* Render AS context switch */
#define GT_RND_PD_FAULT          BIT7     /* Render page directory fault */
#define GT_RND_CMDSTR_CNT_EXCEED BIT6     /* Render Command Stream Watchdog conter exceed */
#define GT_RND_PIPE_NOTIFY       BIT4     /* Render Pipe Control Notify */
#define GT_RND_CMDSTR_ERR        BIT3     /* Render Command Stream error */
#define GT_RND_MMIO_SYNC_FLUSH   BIT2     /* Render MMIO Sync Flush */
#define GT_RND_EU_DEBUG          BIT1     /* Render EU Debug */
#define GT_RND_MI_USER_EVENT     BIT0     /* Render Command Stream User Event */

#define GT_ALL_INTERRUPTS        0x675dd3df
#define GT_ENABLE_INTERRUPTS     (GT_BLT_MI_USER_EVENT | GT_VID_MI_USER_EVENT | GT_RND_MI_USER_EVENT)

/* Gen 6 Power Management Interrupt Register */
#define PMIIR                    0x44028

#endif
