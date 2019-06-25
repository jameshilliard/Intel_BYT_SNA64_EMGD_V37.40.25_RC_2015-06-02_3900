/*
 *-----------------------------------------------------------------------------
 * Filename: instr.h
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
 *  
 *-----------------------------------------------------------------------------
 */

#ifndef _INSTR_H
#define _INSTR_H

#include <instr_common.h>

/*
 * Memory interface instructions used by the kernel
 */
#define MI_INSTR(opcode, flags)       (((opcode) << 23) | (flags))

#define MI_NOOP                       MI_INSTR(0, 0)
#define MI_USER_INTERRUPT             MI_INSTR(0x02, 0)
#define MI_WAIT_FOR_EVENT             MI_INSTR(0x03, 0)
#define MI_WAIT_FOR_OVERLAY_FLIP      (1<<16)
#define MI_WAIT_FOR_PLANE_B_FLIP      (1<<6)
#define MI_WAIT_FOR_PLANE_A_FLIP      (1<<2)
#define MI_WAIT_FOR_PLANE_A_SCANLINES (1<<1)
#define MI_FLUSH                      MI_INSTR(0x04, 0) /*This is legacy gen6 opcode*/
#define MI_READ_FLUSH                 (1 << 0)
#define MI_EXE_FLUSH                  (1 << 1)
#define MI_NO_WRITE_FLUSH             (1 << 2)
#define MI_SCENE_COUNT                (1 << 3) /* just increment scene count */
#define MI_END_SCENE                  (1 << 4) /* flush binr & incr scene cnt */
#define MI_INVALIDATE_ISP             (1 << 5) /* inval indirect st ptrs */
#define MI_BATCH_BUFFER_END           MI_INSTR(0x0a, 0)
#define MI_SUSPEND_FLUSH              MI_INSTR(0x0b, 0)
#define MI_SUSPEND_FLUSH_EN           (1<<0)
#define MI_REPORT_HEAD                MI_INSTR(0x07, 0)
#define MI_DISPLAY_FLIP               MI_INSTR(0x14, 2)
#define MI_DISPLAY_FLIP_I915          MI_INSTR(0x14, 1)
#define MI_DISPLAY_FLIP_PLANE(n)      ((n) << 19)
#define MI_SET_CONTEXT                MI_INSTR(0x18, 0)
#define MI_SAVE_EXT_STATE_EN          (1<<3)
#define MI_RESTORE_EXT_STATE_EN       (1<<2)
#define MI_FORCE_RESTORE              (1<<1)
#define MI_RESTORE_INHIBIT            (1<<0)
#define MI_STORE_DWORD_IMM            MI_INSTR(0x20, 1)
#define MI_STORE_DWORD_INDEX          MI_INSTR(0x21, 1)
#define MI_STORE_DWORD_INDEX_SHIFT    2 /*Legacy*/
/* Official intel docs are somewhat sloppy concerning MI_LOAD_REGISTER_IMM:
 * - Always issue a MI_NOOP _before_ the MI_LOAD_REGISTER_IMM - otherwise hw
 *   simply ignores the register load under certain conditions.
 * - One can actually load arbitrary many arbitrary registers: Simply issue x
 *   address/value pairs. Don't overdue it, though, x <= 2^4 must hold!
 */
#define MI_LOAD_REGISTER_IMM(x)       MI_INSTR(0x22, 2*x-1)
#define MI_FLUSH_DW                   MI_INSTR(0x26, 1) /* for GEN6 */
#define MI_INVALIDATE_TLB             (1<<18)
#define MI_INVALIDATE_BSD             (1<<7)
#define MI_BATCH_BUFFER               MI_INSTR(0x30, 1)
#define MI_BATCH_NON_SECURE           (1)
#define MI_BATCH_NON_SECURE_I965      (1<<8)
#define MI_BATCH_BUFFER_START         MI_INSTR(0x31, 0)
#define MI_SEMAPHORE_MBOX             MI_INSTR(0x16, 1) /* gen6+ */
#define MI_SEMAPHORE_GLOBAL_GTT       (1<<22)
#define MI_SEMAPHORE_UPDATE           (1<<21)
#define MI_SEMAPHORE_COMPARE          (1<<20)
#define MI_SEMAPHORE_REGISTER         (1<<18)
/*
 * 3D instructions used by the kernel
 */
#define GFX_INSTR(opcode, flags) ((0x3 << 29) | ((opcode) << 24) | (flags))

#define GFX_OP_RASTER_RULES          ((0x3<<29)|(0x7<<24))
#define GFX_OP_SCISSOR               ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define SC_UPDATE_SCISSOR            (0x1<<1)
#define SC_ENABLE_MASK               (0x1<<0)
#define SC_ENABLE                    (0x1<<0)
#define GFX_OP_LOAD_INDIRECT         ((0x3<<29)|(0x1d<<24)|(0x7<<16))
#define GFX_OP_SCISSOR_INFO          ((0x3<<29)|(0x1d<<24)|(0x81<<16)|(0x1))
#define SCI_YMIN_MASK                (0xffff<<16)
#define SCI_XMIN_MASK                (0xffff<<0)
#define SCI_YMAX_MASK                (0xffff<<16)
#define SCI_XMAX_MASK                (0xffff<<0)
#define GFX_OP_SCISSOR_ENABLE        ((0x3<<29)|(0x1c<<24)|(0x10<<19))
#define GFX_OP_SCISSOR_RECT          ((0x3<<29)|(0x1d<<24)|(0x81<<16)|1)
#define GFX_OP_COLOR_FACTOR          ((0x3<<29)|(0x1d<<24)|(0x1<<16)|0x0)
#define GFX_OP_STIPPLE               ((0x3<<29)|(0x1d<<24)|(0x83<<16))
#define GFX_OP_MAP_INFO              ((0x3<<29)|(0x1d<<24)|0x4)
#define GFX_OP_DESTBUFFER_VARS       ((0x3<<29)|(0x1d<<24)|(0x85<<16)|0x0)
#define GFX_OP_DESTBUFFER_INFO       ((0x3<<29)|(0x1d<<24)|(0x8e<<16)|1)
#define GFX_OP_DRAWRECT_INFO         ((0x3<<29)|(0x1d<<24)|(0x80<<16)|(0x3))
#define GFX_OP_DRAWRECT_INFO_I965    ((0x7900<<16)|0x2)
#define SRC_COPY_BLT_CMD             ((2<<29)|(0x43<<22)|4)
#define XY_SRC_COPY_BLT_CMD          ((2<<29)|(0x53<<22)|6)
#define XY_MONO_SRC_COPY_IMM_BLT     ((2<<29)|(0x71<<22)|5)
#define XY_SRC_COPY_BLT_WRITE_ALPHA  (1<<21)
#define XY_SRC_COPY_BLT_WRITE_RGB    (1<<20)
#define BLT_DEPTH_8                  (0<<24)
#define BLT_DEPTH_16_565             (1<<24)
#define BLT_DEPTH_16_1555            (2<<24)
#define BLT_DEPTH_32                 (3<<24)
#define BLT_ROP_GXCOPY               (0xcc<<16)
#define CMD_OP_DISPLAYBUFFER_INFO    ((0x0<<29)|(0x14<<23)|2)
#define ASYNC_FLIP                   (1<<22)
#define DISPLAY_PLANE_A              (0<<20)
#define DISPLAY_PLANE_B              (1<<20)
#define PIPE_CONTROL_QW_WRITE        (1<<14)
#define PIPE_CONTROL_DEPTH_STALL     (1<<13)
#define PIPE_CONTROL_WC_FLUSH        (1<<12)
#define PIPE_CONTROL_IS_FLUSH        (1<<11) /* MBZ on Ironlake */
#define PIPE_CONTROL_ISP_DIS         (1<<9)
#define PIPE_CONTROL_NOTIFY          (1<<8)
#define PIPE_CONTROL_GLOBAL_GTT      (1<<2) /* in addr dword */
#define PIPE_CONTROL_STALL_EN        (1<<1) /* in addr word, Ironlake+ only */


#endif /* _INSTR_H */
