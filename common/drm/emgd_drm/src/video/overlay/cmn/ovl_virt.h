/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: ovl_virt.h
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
 *  This is the internal header file for overlay. It should be not be
 *  by any other module besides the overlay module itself. It contains the
 *  neccessary hardware virtualized structures and functions internal to
 *  overlay
 *-----------------------------------------------------------------------------
 */

#ifndef _OVL_VIRT_H
#define _OVL_VIRT_H

/* None of the IALs use the OVL_SUPPORT_*.  Exclude it for now. */
/* None of the IALs use the OVL_RULE_MUST_*.  Exclude it for now. */

/* Overlay HW range of values for color control and gamma correction*/
#define OVL_HW_DEF_BRIGHT      750L
#define OVL_HW_MIN_BRIGHT      0L
#define OVL_HW_MAX_BRIGHT      10000L

#define OVL_HW_DEF_CONT	    10000L
#define OVL_HW_MIN_CONT	    0L
#define OVL_HW_MAX_CONT	    20000L

#define OVL_HW_DEF_SAT	        10000L
#define OVL_HW_MIN_SAT		    0L
#define OVL_HW_MAX_SAT	        20000L

#define OVL_HW_DEF_HUE			0L
#define OVL_HW_MIN_HUE			-180L
#define OVL_MHW_AX_HUE			180L

#define OVL_HW_DEF_GAMMA       1L
#define OVL_HW_MAX_GAMMA       500L
#define OVL_HW_MIN_GAMMA       1L

enum {
	OVL_STATE_OFF = 0,
	OVL_STATE_ON,
};

#define OVL_PRIMARY   0
#define OVL_SECONDARY 1
#define IGD_OVL_MAX_HW    4  /* Maximum number of overlays */
/* in all variables with array IGD_OVL_MAX_HW,
 * Earlier chips (1 ovl plane per pipe) -->
 *         index 0 = Ovl/Sprite A for Pipe A
 *         index 1 = Ovl/Sprite C for Pipe B
 *
 * Newer chips (2 ovl plane per pipe) -->
 *         index 0/1 = Ovl/Sprite A/B for Pipe A
 *         index 2/3 = Ovl/Sprite C/D for Pipe B
 */
#define PIPE_MAX_HW 2

typedef struct _ovl_context{
	ovl_dispatch_t (*dispatch)[];  /* Pointer to an array */
	unsigned int num_planes;
	unsigned int  state[IGD_OVL_MAX_HW];
	unsigned long sync[IGD_OVL_MAX_HW];
} ovl_context_t;

extern ovl_context_t ovl_context[];

#endif /*_OVL_VIRT_H*/
