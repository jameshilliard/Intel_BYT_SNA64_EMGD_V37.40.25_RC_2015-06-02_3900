/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: spr_regs_snb.h
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
 *  This is the internal header file for sandybridge sprite engine. It should
 *  not be used by any other module besides the overlay module itself. It
 *  contains the neccessary hardware virtualized structures and functions
 *  internal to the gen6 sandybridge core's sprite engine
 *-----------------------------------------------------------------------------
 */
#ifndef _SPRITE_REGS_SNB_H
#define _SPRITE_REGS_SNB_H


#define HACKY_FLIP_PENDING 1

/* Color Correction */
#define SPR_RGB_COLOR_DEF_CONT_BRGHT    0x1000000
#define SPR_RGB_COLOR_DEF_SATN_HUE      0x0000080

/* Overlay Command Definitions */
/* Source Format */

#define SPR_CMD_SRCFMTMASK          0x061F0000

#define SPR_CMD_YUV422              0x00000000
#define SPR_CMD_xRGB32_2101010      0x02000000
#define SPR_CMD_xRGB32              0x04000000
#define SPR_CMD_xRGB64_16161616F    0x06000000

/* RGB Channel Order */
/* dont get confused between the spec and memory layout.
 * If spec says "RGB" - thats big endian RGB so memory layout
 *       is actually XBGR
 * If spec says "BGR" - thats big endian BGR so memory layout
 *       is actually XRGB
 * But!! our codes define as per IA little endian layout.
 */
#define SPR_CMD_RGB                 0x00000000
#define SPR_CMD_BGR                 0x00100000

/* YUV 422 Swap */
#define SPR_CMD_YUYV                0x00000000
#define SPR_CMD_UYVY                0x00010000
#define SPR_CMD_YVYU                0x00020000
#define SPR_CMD_VYUY                0x00030000

#define SPR_REG_GAMMA_NUM           16
#define SPR_REG_OFFSET_GAMMAMAX     0x40

#endif /* _SPR_REGS_SNB_H */
