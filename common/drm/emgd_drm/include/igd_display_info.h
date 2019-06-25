/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_display_info.h
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
 *
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPLAY_INFO_H_
#define _IGD_DISPLAY_INFO_H_


/*!
 * @name Display Info Flags
 *
 * Flags used with Display Info data structure to control boolean information.
 *
 * @note These flags are identical to the flags used with the port driver
 *  SDK. If any of these flags change, the corresponding flag in pd.h must
 *  also change.
 *
 * @{
 */

/* Enable/Disable Display  */
#define IGD_DISPLAY_DISABLE       0x00000000
#define IGD_DISPLAY_ENABLE        0x00000001
#define IGD_SCAN_MASK             0x80000000
#define IGD_SCAN_PROGRESSIVE      0x00000000

#define IGD_SCAN_INTERLACE        0x80000000
#define PD_SCAN_INTERLACE         0x80000000

#define IGD_PIX_LINE_DOUBLE_MASK  0x60000000
#define IGD_DOUBLE_LINE_AND_PIXEL 0x60000000

#define IGD_LINE_DOUBLE           0x40000000
#define PD_LINE_DOUBLE            0x40000000

#define IGD_PIXEL_DOUBLE          0x20000000
#define PD_PIXEL_DOUBLE           0x20000000

#define IGD_MODE_TEXT             0x10000000
#define PD_MODE_TEXT              0x10000000  /* VGA Text mode */

#define IGD_VSYNC_HIGH            0x08000000
#define PD_VSYNC_HIGH             0x08000000
#define IGD_HSYNC_HIGH            0x04000000
#define PD_HSYNC_HIGH             0x04000000
#define IGD_BLANK_LOW             0x02000000
#define PD_BLANK_LOW              0x02000000
#define IGD_MODE_VESA             0x01000000 /* VGA/VESA mode number is valid*/
#define PD_MODE_VESA              0x01000000 /* VGA/VESA mode number is valid */

#define IGD_MODE_STALL            0x00800000  /* Flag to enable stall signal */
#define PD_MODE_STALL             0x00800000   /* Flag to enable stall signal */
#define IGD_MODE_SCALE            0x00400000  /* Request NATIVE pipe timings */
#define PD_MODE_SCALE             0x00400000   /* Request NATIVE timings */

#define IGD_ASPECT_16_9           0x00200000  /* 16:9 aspect ratio */
#define PD_ASPECT_16_9            0x00200000  /* 16:9 aspect ratio, otherwise it is 4:3 */
#define IGD_MODE_CEA              0x00100000
#define PD_MODE_CEA				  0x00100000

#define IGD_MODE_DTD              0x00080000   /* Read from EDID */
#define PD_MODE_DTD               0x00080000   /* Read from EDID */
#define IGD_MODE_DTD_USER         0x00040000   /* User defined timing */
#define PD_MODE_DTD_USER          0x00040000   /* User defined timing */
#define IGD_MODE_DTD_FP_NATIVE    0x00020000   /* Native fp timing */
#define PD_MODE_DTD_FP_NATIVE     0x00020000   /* Native fp timing */
#define IGD_MODE_SUPPORTED        0x00010000
#define PD_MODE_SUPPORTED         0x00010000

#define IGD_MODE_FACTORY          0x00008000   /* Factory supported mode */
#define PD_MODE_FACTORY           0x00008000   /* Factory supported mode */
#define IGD_MODE_RB               0x00004000   /* Reduced blanking mode */
#define PD_MODE_RB                0x00004000   /* Reduced blanking mode */

#define IGD_MODE_INFO_ROTATE_MASK 0x00000F00   /* informative - future HW feature? */
#define PD_MODE_INFO_ROTATE_MASK  0x00000F00   /* informative - future HW feature? */
#define IGD_MODE_INFO_ROTATE_0    0x00000000   /* informative - future HW feature? */
#define PD_MODE_INFO_ROTATE_0     0x00000000   /* informative - future HW feature? */
#define IGD_MODE_INFO_ROTATE_90   0x00000100   /* informative - future HW feature? */
#define PD_MODE_INFO_ROTATE_90    0x00000100   /* informative - future HW feature? */
#define IGD_MODE_INFO_ROTATE_180  0x00000200   /* informative - future HW feature? */
#define PD_MODE_INFO_ROTATE_180   0x00000200   /* informative - future HW feature? */
#define IGD_MODE_INFO_ROTATE_270  0x00000300   /* informative - future HW feature? */
#define PD_MODE_INFO_ROTATE_270   0x00000300   /* informative - future HW feature? */

/*!
 * @brief Display timing information for use with
 *   _igd_dispatch::alter_displays()
 */
typedef struct _igd_display_info_ext {
	short mode_number;
	void *extn_ptr; /* INTERNAL pointer for use by main driver only */
}igd_display_info_ext;

typedef struct _igd_display_info {
	unsigned short width;
	unsigned short height;
	unsigned short refresh;
	unsigned long dclk;              /* in KHz */
	unsigned short htotal;
	unsigned short hblank_start;
	unsigned short hblank_end;
	unsigned short hsync_start;
	unsigned short hsync_end;
	unsigned short vtotal;
	unsigned short vblank_start;
	unsigned short vblank_end;
	unsigned short vsync_start;
	unsigned short vsync_end;
	short mode_number;
	unsigned long flags; /* Valid Flags
									   - PD_SCAN_INTERLACE
									   - PD_LINE_DOUBLE
									   - PD_HSYNC_HIGH
									   - PD_MODE_SUPPORTED
									   - PD_MODE_DTD
									   - etc... etc...*/
	unsigned short x_offset;
	unsigned short y_offset;
	/* unsigned short port_number; */
	/* void *pd_private_ptr; */  /* Pointer for use by the PD for any purpose. */

	unsigned short reserved_dd;     /* Reserved for device dependant layer */
	unsigned short reserved_dd_ext; /* Reserved for device dependant layer */

	igd_display_info_ext private;
} igd_display_info_t, pd_timing_t, *pigd_display_info_t;

typedef struct _igd_display_info _pd_timing;

/* This structure is used for the mode table which is a list of all
 * supported modes. */


typedef pd_timing_t igd_timing_info_t, *pigd_timing_info_t;

/*!
 * @name timing table end of list marker
 *
 * This value is used to mark the end of a timing list (igd_display_info_t).
 * It is stored in the width field.  This must match the value for
 * PD_TIMING_LIST_END in src/include/pd.h
 *
 * @{
 */
#define IGD_TIMING_TABLE_END     0xffff
/*! @} */

#endif /* _IGD_DISPLAY_INFO_H_ */
