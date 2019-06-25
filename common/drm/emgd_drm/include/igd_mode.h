/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_mode.h
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

#ifndef _IGD_MODE_H_
#define _IGD_MODE_H_

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*!
 * @defgroup pixel_formats Pixel Format Definitions
 * @ingroup display_group
 *
 * Pixel Format Definitions used in FB info data structure and
 * throughout IGD API functions. Pixel formats are comprised
 * of a Depth and Colorspace component combined (OR'd) with
 * a unique number. Pixel formats and their components are
 * defined with the following defines.
 *
 * @{
 */

/*!
 * This is a just to make use of the Pixel format explicit. This could
 * be replaced with an ENUM in the future.
 */
typedef unsigned long igd_pf_t;

/*!
 * @name Masks
 * Masks that may be used to seperate portions of the pixel format
 * definitions.
 *
 * @note Pixel Formats are maintained such that
 * (pixel_format & IGD_PF_FMT_MASK) >> IGD_PF_FMT_SHIFT
 * will always provide a unique index that can be used in a
 * lookup table.
 * @{
 */
#define IGD_PF_MASK                             0x0000FFFF
#define IGD_PF_TYPE_MASK                        0x0000FF00
#define IGD_PF_DEPTH_MASK                       0x000000FF
#define IGD_PF_FMT_MASK                         0x00FF0000
#define IGD_PF_FMT_SHIFT                        16
/*! @} */

/*!
 * @name Depths
 * Pixel Format depths in Bits per pixel.
 * @{
 */
#define PF_DEPTH_1                              0x00000001
#define PF_DEPTH_2                              0x00000002
#define PF_DEPTH_4                              0x00000004
#define PF_DEPTH_8                              0x00000008
#define PF_DEPTH_12                             0x0000000c
#define PF_DEPTH_16                             0x00000010
#define PF_DEPTH_24                             0x00000018
#define PF_DEPTH_32                             0x00000020
#define PF_DEPTH_64                             0x00000040
/*! @} */

/* Unknown Pixel Format */
#define IGD_PF_UNKNOWN							0x00000000

/*!
 * @name Colorspace
 * Colorspace components of the overall pixel format definition.
 * Several types may be OR'd together.
 * To check for equivalent types use  IGD_PF_TYPE(pf) == TYPE
 *
 * @{
 */
#define PF_TYPE_ALPHA                           0x00000100
#define PF_TYPE_RGB                             0x00000200
#define PF_TYPE_YUV                             0x00000400
#define PF_TYPE_PLANAR                          0x00000800
#define PF_TYPE_OTHER                           0x00001000
#define PF_TYPE_RGB_XOR                         0x00002000 /* Cursor */
#define PF_TYPE_COMP                            0x00004000 /* Compressed */

#define PF_TYPE_ARGB (PF_TYPE_ALPHA | PF_TYPE_RGB)
#define PF_TYPE_YUV_PACKED (PF_TYPE_YUV)
#define PF_TYPE_YUV_PLANAR (PF_TYPE_YUV | PF_TYPE_PLANAR)

/*! @} */

/*!
 * @name 8 Bit RGB Pixel Formats
 * @note Depth is bits per index (8)
 * @{
*/
#define IGD_PF_ARGB8_INDEXED     (PF_DEPTH_8  | PF_TYPE_ARGB | 0x00010000)
#define IGD_PF_ARGB4_INDEXED     (PF_DEPTH_4  | PF_TYPE_ARGB | 0x00020000)
/*! @} */

/*!
 * @name 16 Bit RGB Pixel Formats
 * @note Depth is bits per pixel (16)
 * @{
 */
#define IGD_PF_ARGB16_4444       (PF_DEPTH_16 | PF_TYPE_ARGB | 0x00030000)
#define IGD_PF_ARGB16_1555       (PF_DEPTH_16 | PF_TYPE_ARGB | 0x00040000)
#define IGD_PF_RGB16_565         (PF_DEPTH_16 | PF_TYPE_RGB  | 0x00050000)
#define IGD_PF_xRGB16_555        (PF_DEPTH_16 | PF_TYPE_RGB  | 0x00060000)
/*! @} */

/*!
 * @name 24 Bit RGB Pixel Formats
 * @note Depth is bits per pixel (24)
 * @{
 */
#define IGD_PF_RGB24             (PF_DEPTH_24 | PF_TYPE_RGB  | 0x00070000)
/*! @} */

/*!
 * @name 32 Bit RGB Pixel Formats
 * @note Depth is bits per pixel including unused bits (32).
 * @{
 */
#define IGD_PF_xRGB32_8888       (PF_DEPTH_32 | PF_TYPE_RGB  | 0x00080000)
#define IGD_PF_xRGB32            IGD_PF_xRGB32_8888
#define IGD_PF_ARGB32            (PF_DEPTH_32 | PF_TYPE_ARGB | 0x00090000)
#define IGD_PF_ARGB32_8888       IGD_PF_ARGB32
#define IGD_PF_xBGR32_8888       (PF_DEPTH_32 | PF_TYPE_RGB  | 0x000a0000)
#define IGD_PF_xBGR32            IGD_PF_xBGR32_8888
#define IGD_PF_ABGR32            (PF_DEPTH_32 | PF_TYPE_ARGB | 0x000b0000)
#define IGD_PF_ABGR32_8888       IGD_PF_ABGR32
/*! @} */

/*!
 * @name YUV Packed Pixel Formats
 * @note Depth is Effective bits per pixel
 *
 * @{
 */
#define IGD_PF_YUV422_PACKED_YUY2 (PF_DEPTH_16| PF_TYPE_YUV_PACKED| 0x000c0000)
#define IGD_PF_YUV422_PACKED_YVYU (PF_DEPTH_16| PF_TYPE_YUV_PACKED| 0x000d0000)
#define IGD_PF_YUV422_PACKED_UYVY (PF_DEPTH_16| PF_TYPE_YUV_PACKED| 0x000e0000)
#define IGD_PF_YUV422_PACKED_VYUY (PF_DEPTH_16| PF_TYPE_YUV_PACKED| 0x000f0000)
#define IGD_PF_YUV411_PACKED_Y41P (PF_DEPTH_12| PF_TYPE_YUV_PACKED| 0x00100000)
#define IGD_PF_YUV444_PACKED_AYUV (PF_DEPTH_32| PF_TYPE_YUV_PACKED| 0x00340000)
/*! @} */

/*!
 * @name YUV Planar Pixel Formats
 * @note Depth is Y plane bits per pixel
 * @{
 */
#define IGD_PF_YUV420_PLANAR_I420 (PF_DEPTH_8| PF_TYPE_YUV_PLANAR| 0x00110000)
#define IGD_PF_YUV420_PLANAR_IYUV IGD_PF_YUV420_PLANAR_I420
#define IGD_PF_YUV420_PLANAR_YV12 (PF_DEPTH_8| PF_TYPE_YUV_PLANAR| 0x00120000)
#define IGD_PF_YUV410_PLANAR_YVU9 (PF_DEPTH_8| PF_TYPE_YUV_PLANAR| 0x00130000)
#define IGD_PF_YUV420_PLANAR_NV12 (PF_DEPTH_8| PF_TYPE_YUV_PLANAR| 0x00140000)
/*! @} */

/*!
 * @name Cursor Pixel Formats
 * @{
 *
 * RGB_XOR_2: Palette = {Color 0, Color 1, Transparent, Invert Dest }
 * RGB_T_2: Palette = {Color 0, Color 1, Transparent, Color 2 }
 * RGB_2: Palette = {Color 0, Color 1, Color 2, Color 3 }
 */
#define IGD_PF_RGB_XOR_2      (PF_DEPTH_2  | PF_TYPE_RGB_XOR | 0x00150000)
#define IGD_PF_RGB_T_2        (PF_DEPTH_2  | PF_TYPE_RGB     | 0x00160000)
#define IGD_PF_RGB_2          (PF_DEPTH_2  | PF_TYPE_RGB     | 0x00170000)
/*! @} */

/*!
 * @name Bump Pixel Formats
 * @note Depth is bits per pixel
 * @{
 */
#define IGD_PF_DVDU_88             (PF_DEPTH_16 | PF_TYPE_OTHER | 0x00180000)
#define IGD_PF_LDVDU_655           (PF_DEPTH_16 | PF_TYPE_OTHER | 0x00190000)
#define IGD_PF_xLDVDU_8888         (PF_DEPTH_32 | PF_TYPE_OTHER | 0x001a0000)
/*! @} */

/*!
 * @name Compressed Pixel Formats
 * @note Depth is effective bits per pixel
 * @{
 */
#define IGD_PF_DXT1               (PF_DEPTH_4 | PF_TYPE_COMP | 0x001b0000)
#define IGD_PF_DXT2               (PF_DEPTH_8 | PF_TYPE_COMP | 0x001c0000)
#define IGD_PF_DXT3               (PF_DEPTH_8 | PF_TYPE_COMP | 0x001d0000)
#define IGD_PF_DXT4               (PF_DEPTH_8 | PF_TYPE_COMP | 0x001e0000)
#define IGD_PF_DXT5               (PF_DEPTH_8 | PF_TYPE_COMP | 0x001f0000)
/*! @} */

/*!
 * @name Depth Buffer Formats
 * @note Depth is bits per pixel
 * @{
 */
#define IGD_PF_Z16               (PF_DEPTH_16 | PF_TYPE_OTHER | 0x00200000)
#define IGD_PF_Z24               (PF_DEPTH_32 | PF_TYPE_OTHER | 0x00210000)
#define IGD_PF_S8Z24             (PF_DEPTH_32 | PF_TYPE_OTHER | 0x00220000)
/*! @} */

/*!
 * @name Other Pixel Formats
 *
 * - I8 8bit value replicated to all color channels.
 * - L8 8bit value replicated to RGB color channels, Alpha is 1.0.
 * - A8 8bit Alpha with RGB = 0.
 * - AL88 8bit Alpha, 8bit color replicated to RGB channels.
 * - AI44 4bit alpha 4bit palette color.
 * - IA44 4bit alpha 4bit palette color.
 * @{
 */
#define IGD_PF_I8               (PF_DEPTH_8 | PF_TYPE_OTHER | 0x00230000)
#define IGD_PF_L8               (PF_DEPTH_8 | PF_TYPE_OTHER | 0x00240000)
#define IGD_PF_A8               (PF_DEPTH_8 | PF_TYPE_ALPHA | 0x00250000)
#define IGD_PF_AL88             (PF_DEPTH_16 | PF_TYPE_ALPHA | PF_TYPE_OTHER | 0x00260000)
#define IGD_PF_AI44             (PF_DEPTH_8 | PF_TYPE_OTHER | 0x00270000)
#define IGD_PF_IA44             (PF_DEPTH_8 | PF_TYPE_OTHER | 0x00280000)

#define IGD_PF_L16              (PF_DEPTH_16 | PF_TYPE_OTHER | 0x00290000)
#define IGD_PF_ARGB32_2101010   (PF_DEPTH_32 | PF_TYPE_ARGB  | 0x002a0000)
/* defined below
 * #define IGD_PF_xRGB32_2101010   (PF_DEPTH_32 | PF_TYPE_RGB   | 0x00370000)
 * #define IGD_PF_xBGR32_2101010   (PF_DEPTH_32 | PF_TYPE_RGB   | 0x00380000) */
#define IGD_PF_AWVU32_2101010	(PF_DEPTH_32 | PF_TYPE_OTHER | 0x002b0000)
#define IGD_PF_QWVU32_8888		(PF_DEPTH_32 | PF_TYPE_OTHER | 0x002c0000)
#define IGD_PF_GR32_1616        (PF_DEPTH_32 | PF_TYPE_OTHER | 0x002d0000)
#define IGD_PF_VU32_1616        (PF_DEPTH_32 | PF_TYPE_OTHER | 0x002e0000)

/*!
 * @name Floating-Point formats
 *
 * - R16F Floating-point format 16-bits for the Red Channel
 * - GR32_1616F 32-bit float format using 16 bits for red and green channel
 * - R32F IEEE Floating-Point s23e8 32-bits for the Red Channel
 * - ABGR32_16161616F - This is a 64-bit FP format.
 * - Z32F 32-bit float 3D depth buffer format.
 * @{
 */
#define IGD_PF_R16F                 (PF_DEPTH_16 | PF_TYPE_OTHER | 0x002f0000)
#define IGD_PF_GR32_1616F           (PF_DEPTH_32 | PF_TYPE_OTHER | 0x00300000)
#define IGD_PF_R32F                 (PF_DEPTH_32 | PF_TYPE_OTHER | 0x00310000)
#define IGD_PF_ABGR64_16161616F     (PF_DEPTH_64 | PF_TYPE_ARGB  | 0x00320000)
#define IGD_PF_Z32F                 (PF_DEPTH_32 | PF_TYPE_OTHER | 0x00330000)
#define IGD_PF_xBGR64_16161616F     (PF_DEPTH_64 | PF_TYPE_RGB   | 0x00340000)
#define IGD_PF_ARGB64_16161616F     (PF_DEPTH_64 | PF_TYPE_ARGB  | 0x00350000)
#define IGD_PF_xRGB64_16161616F     (PF_DEPTH_64 | PF_TYPE_RGB   | 0x00360000)
/*! @} */

/*!
 * @name Other Pixel Formats 2 - more formats
 * @{
 */
#define IGD_PF_ABGR32_2101010   (PF_DEPTH_32 | PF_TYPE_RGB   | 0x00370000)
#define IGD_PF_xRGB32_2101010   (PF_DEPTH_32 | PF_TYPE_RGB   | 0x00380000)
#define IGD_PF_xBGR32_2101010   (PF_DEPTH_32 | PF_TYPE_RGB   | 0x00390000)
/*! @} */

/*!
 * @name IGD_PF_NEXT array length helper
 *
 * This helper should always be set to the next available PF id. In this
 * manner any array that uses the id as an index can be defined as
 * unsigned long lookup_table[IGD_PF_NEXT] = {...}; and will then generate
 * compile warnings if the pixel format list length changes.
 */
#define IGD_PF_NEXT 0x3B
/*!
 * @name Helper Macros
 * @{
 */

/*! Gets bits per pixel from pixel format */
#define IGD_PF_BPP(pf)   ((pf) & IGD_PF_DEPTH_MASK)
/*! Gets bits per pixel from pixel format */
#define IGD_PF_DEPTH(pf) IGD_PF_BPP(pf)
/*! Gets Bytes per pixel from pixel format */
#define IGD_PF_BYPP(pf)  ((IGD_PF_DEPTH(pf)+0x7)>>3)
/*! Gets Bytes required for line of pixels */
#define IGD_PF_PIXEL_BYTES(pf, np)  (((IGD_PF_BPP(pf)*np) +0x7)>>3)
/*! Gets numeric pf */
#define IGD_PF_NUM(pf) ((pf>>16) & 0xff)
/*! Gets pf type */
#define IGD_PF_TYPE(pf)  (pf & IGD_PF_TYPE_MASK)

/*! @} */
/*! @} */


/*
 * NOTE: When Adding pixel formats you must add correct definitions to
 * any pixel format lookup tables. See igd_2d.c
 */


/*!
 * @addtogroup display_group
 *
 * <B>Relavent Dispatch Functions</B>
 * - _igd_dispatch::query_dc()
 * - _igd_dispatch::query_mode_list()
 * - _igd_dispatch::free_mode_list()
 * - _igd_dispatch::alter_displays()
 *
 * @{
 */

/*!
 * The maximum number of displays available in the display configurations
 * below.
 */
#define IGD_MAX_DISPLAYS 2

/*!
 * @name Display Configuration Definition
 * @anchor dc_defines
 *
 * The display configuration (dc) is a unique 32bit identifier that fully
 * describes all displays in use and how they are attached to planes and
 * pipes to form Single, Clone, Twin and Extended display setups.
 *
 * The DC is treated as 8 nibbles of information (nibble = 4 bits). Each
 * nibble position in the 32bit DC corresponds to a specific role as
 * follows:
 *
<PRE>
    0x12345678
      ||||||||-- Legacy Display Configuration (Single, Twin, Clone, Ext)
      |||||||--- Port Number for Primary Pipe Master
      ||||||---- Port Number for Primary Pipe Twin 1
      |||||----- Port Number for Primary Pipe Twin 2
      ||||------ Port Number for Primary Pipe Twin 3
      |||------- Port Number for Secondary Pipe Master
      ||-------- Port Number for Secondary Pipe Twin 1
      |--------- Port Number for Secondary Pipe Twin 2
</PRE>
 *
 * The legacy Display Configuration determines if the display is in Single
 * Twin, Clone or extended mode using the following defines. When a complex
 * (>2 displays) setup is defined, the legacy configuration will describe
 * only a portion of the complete system.
 *
 * - IGD_DISPLAY_CONFIG_SINGLE: A single primary display only.
 * - IGD_DISPLAY_CONFIG_CLONE: Two (or more) displays making use of two
 *    display pipes. In this configuration two sets of display timings are
 *    used with a single source data plane.
 * - IGD_DISPLAY_CONFIG_TWIN: Two (or more) displays using a single display
 *    pipe. In this configuration a single set of display timings are
 *    used for multiple displays.
 * - IGD_DISPLAY_CONFIG_EXTENDED: Two (or more) displays making use of two
 *    display pipes and two display planes. In this configuration two sets
 *    of display timings are used and two source data planes.
 *
 * @{
 */
#define IGD_DISPLAY_CONFIG_SINGLE   0x1
#define IGD_DISPLAY_CONFIG_CLONE    0x2
#define IGD_DISPLAY_CONFIG_TWIN     0x4
#define IGD_DISPLAY_CONFIG_EXTENDED 0x8
#define IGD_DISPLAY_CONFIG_MASK     0xf
/*! @} */

/*!
 * @name IGD_DC_PORT_NUMBER
 *
 * Given a display configuration value and an index, return the port
 * number at that position.
 */
#define IGD_DC_PORT_NUMBER(dc, i) (unsigned short) ((dc >> (i * 4)) & 0x0f)
#define IGD_DC_PRIMARY(dc) (IGD_DC_PORT_NUMBER(dc,1))
#define IGD_DC_SECONDARY(dc) (IGD_DC_PORT_NUMBER(dc,5))


/*!
 * @name IGD_DC_SINGLE
 * For a given dc, return true if it is in single display mode
 */
#define IGD_DC_SINGLE(dc)   ((dc & 0xf) == IGD_DISPLAY_CONFIG_SINGLE)
/*!
 * @name IGD_DC_TWIN
 * For a given dc, return true if it is in twin display mode
 */
#define IGD_DC_TWIN(dc)     ((dc & 0xf) == IGD_DISPLAY_CONFIG_TWIN)
/*!
 * @name IGD_DC_CLONE
 * For a given dc, return true if it is in clone display mode
 */
#define IGD_DC_CLONE(dc)    ((dc & 0xf) == IGD_DISPLAY_CONFIG_CLONE)
/*!
 * @name IGD_DC_EXTENDED
 * For a given dc, return true if it is in extended display mode
 */
#define IGD_DC_EXTENDED(dc) ((dc & 0xf) == IGD_DISPLAY_CONFIG_EXTENDED)

/*!
 * @name Query DC Flags
 * @anchor query_dc_flags
 *
 *   - IGD_QUERY_DC_ALL: Query All usable DCs.
 *   - IGD_QUERY_DC_PREFERRED: Returns only preferred dc's from the list
 *      of all usable dc's. Twin mode dc's are eliminated if an equivalent
 *      Clone dc is available. Only the most useful/flexible combination
 *      of 3 displays is returned rather than all combinations of the 3
 *      displays.
 *   - IGD_QUERY_DC_INIT: Returns a pointer to the initial dc to be used.
 *   - IGD_QUERY_DC_EXISTING: Returns a pointer to the initial dc that
 *      most closely matches the DC in use by the hardware. This can be
 *      used by an IAL to "discover" what the vBIOS has arranged and use
 *      that configuration. (NOT IMPLEMENTED)
 * @{
 */
#define IGD_QUERY_DC_ALL        0x0
#define IGD_QUERY_DC_PREFERRED  0x1
#define IGD_QUERY_DC_INIT       0x2
#define IGD_QUERY_DC_EXISTING   0x3
/*! @} */


/*!
 * @name DC index Flags
 * @anchor dc_index_flags
 *
 *  - IGD_DC_IDX_PRIMARY_MASTER: index to the primary display pipe master
 *  - IGD_DC_IDX_PRIMARY_TWIN1: index to the first primary display pipe's twin
 *  - IGD_DC_IDX_PRIMARY_TWIN2: index to the second primary display pipe's twin
 *  - IGD_DC_IDX_PRIMARY_TWIN3: index to the third primary display pipe's twin
 *  - IGD_DC_IDX_SECONDARY_MASTER: index to the secondary display pipe master
 *  - IGD_DC_IDX_SECONDARY_TWIN1: index to the first secondary display pipe's
 *     twin
 *  - IGD_DC_IDX_SECONDARY_TWIN2: index to the second secondary display pipe's
 *     twin
 * @{
 */
#define IGD_DC_IDX_PRIMARY_MASTER        0x1
#define IGD_DC_IDX_PRIMARY_TWIN1         0x2
#define IGD_DC_IDX_PRIMARY_TWIN2         0x3
#define IGD_DC_IDX_PRIMARY_TWIN3         0x4
#define IGD_DC_IDX_SECONDARY_MASTER      0x5
#define IGD_DC_IDX_SECONDARY_TWIN1       0x6
#define IGD_DC_IDX_SECONDARY_TWIN2       0x7
/*! @} */


/*!
 * @name Framebuffer Info Flags
 * @anchor fb_info_flags
 *
 * These bits may be set in the flags member of the _igd_framebuffer_info
 * data structure to alter the HALs behavior when calling
 * dispatch::alter_displays() All other flag bits must be 0 when calling.
 *
 *  - IGD_VBIOS_FB: The framebuffer info is not populated because the mode
 *     is being set with a VGA/VESA mode number. The HAL should instead
 *     return the framebuffer information to the caller when the mode
 *     has been set.
 *  - IGD_ENABLE_DISPLAY_GAMMA: The framebuffer data will be color corrected
 *     by using the 3*256 entry palette lookup table. Red, Green, and Blue
 *     8 bit color values will be looked up and converted to another value
 *     during display. The pixels are not modified. The caller must also
 *     set the palette using _igd_dispatch::set_palette_entries()
 *  - IGD_REUSE_FB: The HAL will use the incoming framebuffer info as the
 *     framebuffer. The HAL will allocate a framebuffer only if
 *     frambuffer_info.allocated == 0 and this is the first use of the
 *     display. If the offset has been changed, the caller is now responsible
 *     for freeing the previous framebuffer surface, and the HAL now fully
 *     owns the incoming surface, freeing it on shutdown etc.
 *  - IGD_MIN_PITCH: The allocation will use the input value
 *      of pitch as the minimum acceptable pitch. This allows a caller
 *      to allocate a surface with "extra" room between the width and
 *      pitch so that the width may later be expanded (use with caution).
 *  - IGD_HDMI_STEREO_3D_MODE: The framebuffer info will contains multiple
 *      buffers to use for stereo 3D.  First buffer for framebuffer,
 *      second buffer for left content and third buffer for right content.
 *
 * Additionally all surface flags will be populated by the HAL and
 * returned.
 * See: @ref surface_info_flags
 *
 * @{
 */
#define IGD_VBIOS_FB                            0x80000000
#define IGD_ENABLE_DISPLAY_GAMMA				0x40000000
#define IGD_REUSE_FB                            0x20000000
#define IGD_MIN_PITCH                           0x10000000
#define IGD_FB_FLAGS_MASK                       0xf0000000
#define IGD_HDMI_STEREO_3D_MODE					0x01000000
/* @} */

/*!
 * @name Query Modes Flags (deprecated)
 * @deprecated To be removed in IEGD 5.1
 *
 * Flags for use in igd_query_display function call
 *
 * @{
 */
#define IGD_QUERY_ALL_MODES   0x01
#define IGD_QUERY_PORT_MODES  0x00
/*! @} */

/*!
 * @name Query Mode List Flags
 * @anchor query_mode_list_flags
 *
 * Flags for use with _igd_dispatch::query_mode_list()
 *  - IGD_QUERY_PRIMARY_MODES: Query mode list for the primary pipe.
 *  - IGD_QUERY_SECONDARY_MODES: Query modes list for the secondary pipe.
 *  - IGD_QUERY_LIVE_MODES: Query the live modes instead of filtering
 *      and allocing a new list. This may be ORed with other bits.
 * @{
 */
#define IGD_QUERY_PRIMARY_MODES   0x0
#define IGD_QUERY_SECONDARY_MODES 0x1
#define IGD_QUERY_LIVE_MODES      0x2
/*! @} */

/*!
 * Display handle list and Display info list should be terminated with
 * this tag. List may contain nulls, they will be skipped.
 *
 * @note This value is -1 so it will work the same on 32 and 64 bit platforms
 */
#define IGD_LIST_END -1


#define IGD_INVALID_MODE 0

#define DP_DEF_PANEL_FIT       1      /* Default panel fit in off = 0 */
#define DP_MAINTAIN_ASPECT_RATIO       1      /* Default aspect ratio in off = 0 */
#define DP_TEXT_TUNING_FUZZY       0      /* Default text tuning is Fuzzy filtering*/
#define DP_TEXT_TUNING_CRISP       1      /* Crisp edge enhanceing filtering */
#define DP_TEXT_TUNING_MEDIAN       2      /* Media between fuzzy and crisp filtering */


#endif /* _IGD_MODE_H_ */
