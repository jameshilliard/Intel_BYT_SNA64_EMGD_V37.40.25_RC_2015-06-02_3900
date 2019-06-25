/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: spr_regs_vlv.h
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
 *  This is the internal header file for gen7 ValleyView sprite engine. It should
 *  not be usd by any other module besides the overlay module itself. It contains the
 *  neccessary hardware virtualized structures and functions internal to
 *  the ValleyView core's sprite engine
 *-----------------------------------------------------------------------------
 */
#ifndef _SPRITE_REGS_VLV_H
#define _SPRITE_REGS_VLV_H


#define HACKY_FLIP_PENDING 1

/* Color Correction */
#define SPR_RGB_COLOR_DEF_CONT_BRGHT    0x1000000
#define SPR_RGB_COLOR_DEF_SATN_HUE      0x0000080

/* Overlay Command Definitions */
/* Source Format */

#define SPR_CMD_SRCFMTMASK          0x3c3F0000

#define SPR_CMD_RGB_CONVERSION      0x00080000

#define SPR_CMD_YUV422              0x00000000
#define SPR_CMD_xBGR32_2101010      0x20000000
#define SPR_CMD_ABGR32_2101010      0x24000000
#define SPR_CMD_xRGB32              0x18000000
#define SPR_CMD_ARGB32              0x1c000000
#define SPR_CMD_xBGR32              0x38000000
#define SPR_CMD_ABGR32              0x3c000000
#define SPR_CMD_RGB565              0x14000000
#define SPR_CMD_ARGB8IDX            0x08000000
/* FOR ABOVE NAMES EMGD vs B-SPEC - RGB Channel Order */
/* dont get confused between the spec and memory layout.
 * If spec says "RGB" - thats big endian RGB so memory layout
 *       is actually XBGR
 * If spec says "BGR" - thats big endian BGR so memory layout
 *       is actually XRGB
 * But!! our codes define as per IA little endian layout.
 */

/* YUV 422 Swap */
#define SPR_CMD_YUYV                0x00000000
#define SPR_CMD_UYVY                0x00010000
#define SPR_CMD_YVYU                0x00020000
#define SPR_CMD_VYUY                0x00030000

#define SPR_REG_GAMMA_NUM           6

#endif /* _SPR_REGS_VLV_H */
