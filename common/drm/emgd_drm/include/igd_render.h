/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_render.h
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
 *  This is a header file for the Intel GFX commands.
 *  This includes commands specific to Intel hardware and structures specific
 *  to Intel hardware.  All other commands and structures are available
 *  through GFX.
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_RENDER_H
#define _IGD_RENDER_H

/*
 * Flags passed to 2d and 3d render functions.
 * Note: These flags MUST not conflict the the blend flags in igd_blend.h
 */
#define IGD_RENDER_BLOCK       0x00000000
#define IGD_RENDER_NONBLOCK    0x00000001
/* Render to Framebuffer: Causes a flush afterward */
#define IGD_RENDER_FRAMEBUFFER 0x00000002
/* Should this be an immediate blit? */
#define IGD_RENDER_IMMEDIATE   0x00000004
/* Render should prevent on-screen tearing */
#define IGD_RENDER_NOTEAR      0x00000008

/*
 * Command priority for use with 2d and 3d render functions.
 */
#define IGD_PRIORITY_NORMAL    0x0
#define IGD_PRIORITY_INTERRUPT 0x1
#define IGD_PRIORITY_POWER     0x2
#define IGD_PRIORITY_BIN       0x3
#define IGD_PRIORITY_BB        0x4


typedef struct _igd_yuv_coeffs {
	char ry;
	char ru;
	char rv;
   	char gy;
	char gu;
	char gv;
	char by;
	char bu;
	char bv;

	short r_const;
	short g_const;
	short b_const;

	unsigned char r_shift;
	unsigned char g_shift;
	unsigned char b_shift;
} igd_yuv_coeffs, *pigd_yuv_coeffs;

typedef struct _igd_palette_info {
	unsigned long *palette;
	int palette_id;
	int palette_type;
	int size;

	igd_yuv_coeffs yuv_coeffs;
} igd_palette_info_t, *pigd_palette_info_t;

/*
 *
 * 2D blit => igd_coord + igd_surface => igd_rect + igd_surface
 * 3D blend => (igd_rect + igd_surface + igd_rect)*n => igd_rect + igd_surface
 *   Blend: N source surfaces with a source rect.
 *          N destination rects.
 *          1 Destination surface.
 *          1 Destination clip rect.
 */

typedef struct _igd_coord {
	unsigned int x;
	unsigned int y;
} igd_coord_t, *pigd_coord_t;

typedef struct _igd_rect {
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
} igd_rect_t, *pigd_rect_t;


/*
 * Region structure. Holds information about non-surface memory
 * allocations.
 */
#define IGD_REGION_UNFINISHED 0x00000000
#define IGD_REGION_READY      0x00000001
#define IGD_REGION_QUEUED     0x00000002
#define IGD_REGION_ABANDONED  0x00000004
typedef struct _igd_region {
	unsigned long offset;
	char *virt;
	unsigned long size;
	unsigned long type;
	unsigned long flags;
} igd_region_t;

typedef struct _igd_surface_list {
    unsigned long offset;
    unsigned long size;
} igd_surface_list_t, *pigd_surface_list_t;

/*!
 * @name Surface Render Ops
 * @anchor render_ops
 *
 * These flags apply to a surface and indicate features of the surface
 * that should be considered during render operations such as
 * _igd_dispatch::blend()
 *
 * - IGD_RENDER_OP_BLEND: This surface should be blended with other
 *  surfaces during rendering. In calls to _igd_dispatch::blend() this
 *  surface will blend with those below it in the stack or the destination
 *  surface (when the surface is the bottom-most in the stack).
 *   Note: xRGB surfaces used with this render op will convert to ARGB
 *   surfaces with an alpha of 1.0. Without this render op xRGB surfaces
 *   will preserve the x value from the source.
 *
 * - IGD_RENDER_OP_PREMULT: The surface contains a pre-multipled alpha
 *  value. This indicates that the per-pixel color values have already been
 *  multipled by the per-pixel alpha value. Without this bit set it is
 *  necessary to perform this multiplication as part of a blend operation.
 *
 * - IGD_RENDER_OP_ALPHA: The surface contains a global alpha. All pixel
 *  values are multiplied against the global alpha as part of any blend
 *  operation. Global alpha is used exclusive of Diffuse color.
 *
 * - IGD_RENDER_OP_DIFFUSE: The surface contains a single diffuse color.
 *  This flag is used with alpha-only surfaces or surfaces of constant
 *  color. Diffuse is used exclusively of Global Alpha.
 *
 * - IGD_RENDER_OP_CHROMA: THe surface contains a chroma-key. Pixel
 *  values between  chroma_high and chroma_low will be filtered out.
 *
 * - IGD_RENDER_OP_COLORKEY: The surface contains a colorkey. This is used
 *  on mask surfaces to indicate the location of pixels that may be drawn
 *  on the destination. Only pixels with values between chroma_high and
 *  chroma_low should "pass through" the mask to the destination. Other pixels
 *  will be filered out.
 *
 * @{
 */

#define IGD_RENDER_OP_BLEND      0x1
#define IGD_RENDER_OP_PREMULT    0x2
#define IGD_RENDER_OP_ALPHA      0x4
#define IGD_RENDER_OP_DIFFUSE    0x8
#define IGD_RENDER_OP_CHROMA     0x20
#define IGD_RENDER_OP_COLORKEY   0x40

/*
 * Make sure rotate and flip flags are not moved. Blend uses them as
 * array indexes
 */
#define IGD_RENDER_OP_ROT_MASK   0x000300
#define IGD_RENDER_OP_ROT_0      0x000000
#define IGD_RENDER_OP_ROT_90     0x000100
#define IGD_RENDER_OP_ROT_180    0x000200
#define IGD_RENDER_OP_ROT_270    0x000300
#define IGD_RENDER_OP_FLIP       0x000400
#define IGD_RENDER_OP_SKIP_ROT	 0x000800

/*
 * flags for post process - Deinterlacing and ProcAmpControl.
 */
#define IGD_RENDER_OP_INTERLACED 0x00001000
#define IGD_RENDER_OP_BOB_EVEN   0x00002000
#define IGD_RENDER_OP_BOB_ODD    0x00004000
#define IGD_RENDER_OP_PROCAMP    0x00010000

/* @} */

#endif /* _IGD_RENDER_H */
