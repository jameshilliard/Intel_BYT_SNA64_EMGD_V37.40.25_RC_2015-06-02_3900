/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: instr_common.h
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
 *-----------------------------------------------------------------------------
 * Description:
 *  Common tools for hardware that uses instruction engines.
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_INSTR_COMMON_H
#define _IGD_INSTR_COMMON_H

/* Write instructions macro. */
#ifdef DEBUG_BUILD_TYPE
#define INSTR_WRITE(data, addr)											\
	EMGD_DEBUG_S("    addr:%p   data:0x%8.8x", ((void *)(addr)),		\
			(unsigned int)(data));										\
	EMGD_WRITE32 (data, (addr));										\
	addr++;

#define IGD_PRINT_INSTR(name)                                    \
	EMGD_DEBUG_S("GMCH Instruction: %s", name);
#else
#define INSTR_WRITE(data, addr) EMGD_WRITE32(data, (addr)++);
#define IGD_PRINT_INSTR(name)
#endif

/* Definitions used by IS_DISPLAYREG, which is currently called by common
 * code.
 */
#define GEN6_GDRST  0x941c
#define  GEN6_GRDOM_FULL        (1 << 0)
#define  GEN6_GRDOM_RENDER      (1 << 1)
#define  GEN6_GRDOM_MEDIA       (1 << 2)
#define  GEN6_GRDOM_BLT         (1 << 3)

#define FENCE_REG_SANDYBRIDGE_0     0x100000

#define PGTBL_ER    0x02024
#define RENDER_RING_BASE    0x02000
#define BSD_RING_BASE       0x04000
#define GEN6_BSD_RING_BASE  0x12000
#define BLT_RING_BASE       0x22000
#define RENDER_HWS_PGA_GEN7 (0x04080)
#define BSD_HWS_PGA_GEN7    (0x04180)
#define BLT_HWS_PGA_GEN7    (0x04280)

#define IPEIR_I965  0x02064

#define MI_MODE     0x0209c
#define GFX_MODE_GEN7   0x0229c

#define GEN6_BLITTER_HWSTAM 0x22098
#define GEN6_BLITTER_IMR    0x220a8
#define GEN6_BLITTER_ECOSKPD    0x221d0
#define GEN6_BSD_SLEEP_PSMI_CONTROL 0x12050

#define GEN6_BSD_HWSTAM         0x12098
#define GEN6_BSD_IMR            0x120a8

#define GEN6_BSD_RNCID          0x12198

#define _DPLL_A 0x06014
#define _DPLL_B 0x06018


#define VLV_MASTER_IER          0x4400c /* Gunit master IER */

#define GTISR   0x44010
#define GTIMR   0x44014
#define GTIIR   0x44018
#define GTIER   0x4401c

#define  FORCEWAKE              0xA18C
#define  FORCEWAKE_VLV              0x1300b0
#define  FORCEWAKE_ACK_VLV          0x1300b4
#define  FORCEWAKE_ACK              0x130090
#define  FORCEWAKE_MT               0xa188 /* multi-threaded */
#define  FORCEWAKE_MT_ACK           0x130040
#define  ECOBUS                 0xa180
#define    FORCEWAKE_MT_ENABLE          (1<<5)

#define GEN6_PMISR              0x44020
#define GEN6_PMIER              0x4402C

#endif
