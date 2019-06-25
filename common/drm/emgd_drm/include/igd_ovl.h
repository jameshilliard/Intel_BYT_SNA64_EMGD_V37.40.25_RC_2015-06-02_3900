/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_ovl.h
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
 *  This is the external header file for overlay. It should be included
 *  by ial and/or other HAL modules that require overlay interaction.
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_OVL_H
#define _IGD_OVL_H

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
#include <drm/drm_flip.h>
#endif
/*!
 * @defgroup overlay_group Overlay
 * @ingroup video_group
 *
 * The overlay module is responsible for the hardware overlay and the secondary
 * overlay (sometimes referred to as plane C).
 *
 * <B>Relevent Dispatch Fucntions</B>
 * - _igd_dispatch::alter_ovl()
 * - _igd_dispatch::query_ovl()
 * - _igd_dispatch::query_max_size_ovl()
 * - _igd_dispatch::query_max_size_ovl()
 * @{
 */

/*----------------------------------------------------------------------------
 * Overlay Alter Flags
 *--------------------------------------------------------------------------*/
/*!
 * @name Alter Overlay Flags
 * @anchor alter_ovl_flags
 *
 * Flags for use with _igd_dispatch::alter_ovl()
 * - IGD_OVL_ALTER_OFF: Turn the overlay off
 * - IGD_OVL_ALTER_ON: Turn the overlay on
 *
 * - IGD_OVL_ALTER_PROGRESSIVE: The overlay is progressive.  Only valid when
 *     the overlay is on
 * - IGD_OVL_ALTER_INTERLEAVED: The overlay is interleaved/bobbed.  Only valid
 *     when the overlay is on
 *
 * - IGD_OVL_ALTER_FLIP_EVEN: Flip the even field.  Only valid when the overlay
 *     is on
 * - IGD_OVL_ALTER_FLIP_ODD: Flip the odd field.  Only valid when the overlay
 *     is on
 * - IGD_OVL_SPRITE2_ON_PIPE: Determines if this call to alter_overlay is
 *     being targetted to the 2nd sprite of this pipe. Newer chips has 2
 *     sprites per pipe today
 * @{
 */
#define IGD_OVL_ALTER_OFF           0x00000000
#define IGD_OVL_ALTER_ON            0x00000001

#define IGD_OVL_ALTER_PROGRESSIVE   0x00000000
#define IGD_OVL_ALTER_INTERLEAVED   0x00000002
#define IGD_OVL_ALTER_FLIP_EVEN     0x00000000
#define IGD_OVL_ALTER_FLIP_ODD      0x00000004

#define IGD_FW_VIDEO_OFF	        0x00000008

#define IGD_OVL_FORCE_USE_DISP      0x00000010
#define IGD_OVL_OSD_ON_SPRITEC      0x00000020
#define IGD_OVL_GET_SURFACE_DATA    0x00000040

#define IGD_OVL_SURFACE_ENCRYPTED   0x00000080

#define IGD_OVL_SPRITE1_ON_PIPE     0x00000000
#define IGD_OVL_SPRITE2_ON_PIPE     0x00000100

/*
 * the following values are needed because hardware seems to display yuv slightly dimmer
 * than RGB when color data is calculated out to be equal
 * */
#define MAX_CONTRAST_YUV	255
#define MID_CONTRAST_YUV	67
#define MIN_CONTRAST_YUV	0

#define MAX_SATURATION_YUV	1023
#define MID_SATURATION_YUV	145
#define MIN_SATURATION_YUV	0

#define MAX_BRIGHTNESS_YUV	127
#define MID_BRIGHTNESS_YUV	-5
#define MIN_BRIGHTNESS_YUV	-128

#define MAX_HUE_YUV		1023
#define MID_HUE_YUV		0
#define MIN_HUE_YUV		0

#define CONSTALPHA_MAX		255
#define CONSTALPHA_MIN		0

#define CONSTALPHA_ENABLE	1
#define CONSTALPHA_DISABLE	0

/* For checking valid range value */
#define IS_VALUE_VALID(value, min, max)\
	((value >= min ) && (value <= max))

/*! @} */

/* These are not actually used by any IAL
#define IGD_OVL_ALTER_MIRROR_NONE  0
#define IGD_OVL_ALTER_MIRROR_H     ?
#define IGD_OVL_ALTER_MIRROR_V     ?
#define IGD_OVL_ALTER_MIRROR_HV    (IGD_OVL_FLAGS_MIRROR_H|IGD_OVL_FLAGS_MIRROR_V) */

/*----------------------------------------------------------------------------
 * Overlay Plane(s) Pipe-Blending Control
 *--------------------------------------------------------------------------*/
/*!
 * @name Overlay Planes Pipe-Blending control
 * @anchor igd_ovl_pipeblend_t
 *
 * Flags for use with @ref igd_ovl_pipeblend_t member variables
 * This is only used with the HAL dispatch::alter_ovl_pipeblend calls
 * in the igd_ovl_pipeblend_t structure. These flags defines the
 * desired plane zorder layout for display and overlay planes, the
 * enablement or disablement of the FB (display) to overlay/sprite
 * blending knob and the constant alpha premult for the overlay data.
 * All these 3 functionalities will apply only to the current display
 * pipeline passed in furing alter_ovl. It can also be passed in
 * via a call to alter_ovl_pipeblend call which doesnt require passing in
 * the known surface, src rect, and dest rect info.
 *
 * **** For the variables bottom, middle and top ****
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!! Values does not used any more...
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * - IGD_OVL_ZORDER_DISPLAY1 used as value to put into igd_ovl_pipeblend_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Setting top to IGD_OVL_ZORDER_DISPLAY1 means Display Plane becomes
 *           top most. Setting bottom to IGD_OVL_ZORDER_DISPLAY1 Display Plane
 *           becomes bottom most. ... and etc... this also applies similiarly
 *           for IGD_OVL_ZORDER_SPRITE1 and IGD_OVL_ZORDER_SPRITE2.
 * - IGD_OVL_ZORDER_SPRITE1  used as value to put into igd_ovl_pipeblend_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 * - IGD_OVL_ZORDER_SPRITE2  used as value to put into igd_ovl_pipeblend_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 * - IGD_OVL_ZORDER_DEFAULT  used as value to put into igd_ovl_pipeblend_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 *
 * NOTE1: The default kernel module state if alter_ovl_pipeblend is never called:
 *		  SPRITE2=TOP, SPRITE1=MIDDLE, DISPLAY=BOTTOM.
 *
 * NOTE2: Using the IGD_OVL_OSD_ON_SPRITEC flag with an alter_ovl call
 * to either one of the sprites for that display pipeline will override
 * the current z_order for that display pipeline with the following:
 * DISPLAY=TOP, SPRITE2=MIDDLE, SPRITE1=BOTTOM
 * (where SPRITE1 "was" HW_OVL and SPRITE2 "was" SPRITE_C on prev chips).
 * This is for backwards compatibility.
 * This happens in VA driver when implementing subpicture using Sprites
 *
 * NOTE3: Using dest color keying for a particular video sprite will
 * make that sprite pop above the DISPLAY plane on next flip - changing
 * the current Z-order. HW limitation.
 *
 * NOTE4: WRT NOTE2 and NOTE3, if the IGD_OVL_PIPEBLEND_SET_ZORDER flag is
 * passed in on every flip, then the Zorder determined by igd_ovl_pipblend_t
 * zorder params will always override NOTE1, NOTE2 and NOTE3.
 *
 * NOTE4: Current XServer DDX default behavior (user space code) for Clone
 * display configuration is that the z-order will be mirror'ed to the
 * other display pipeline as well. XServer DDX Driver behavior can
 * Kernel module doesnt care about "DC" anymore so it will be up to XSErver
 * and its current XRandR configuration to handle this from client side.
 *
 * @{
 */
/* Definitions for bottom, middle and top
 * in zorder_combined, nibbles read right to left
 * = bottom, middle, top
 */
#define IGD_OVL_ZORDER_INVALID    0x0
/* will be returned by HW that doesnt support this*/
/*#define IGD_OVL_ZORDER_DISPLAY   0x01*/
/*#define IGD_OVL_ZORDER_SPRITE1    0x02*/
/*#define IGD_OVL_ZORDER_SPRITE2    0x04*/
/*#define DISP_SPR1_SPR2       0x00010204*/

/*see micro_spr_vlv.c for details*/
/*8&(9) - SA SB PA CA*/
//#define IGD_OVL_ZORDER_DEFAULT0x00020401
#define IGD_OVL_ZORDER_DEFAULT    0x000010204
/*use as "combined" in struct*/

/* Bitwise OR'able definitions for flags */
#define IGD_OVL_PIPEBLEND_SET_FBBLEND      0x00000001
#define IGD_OVL_PIPEBLEND_SET_ZORDER       0x00000002
#define IGD_OVL_PIPEBLEND_SET_CONSTALPHA   0x00000004

/*! @} */

/*!
 * @brief Overlay Plane Blending Control
 *
 * IGD_OVL_PIPEBLEND_SET_FBBLEND will update the state of the Fb-Blend-Ovl
 * depending on "fb_blend_ovl" param if igd_ovl_pipeblend_t. Enabling will
 * cause blending the contents of the overlay with the contents of the
 * frambuffer using the framebuffer's alpha channel (as if the framebuffer
 * is on top of the overlay). This feature requires
 * that dest colorkey be enabled. Blending will be per pixel. Even if
 * FB is XRGB, u can use this flag, and overlay module will change Display
 * Plane to ARGB - but the trick would be about how the IAL would allow
 * apps to get the right alpha into the FB.
 *
 * IGD_OVL_PIPEBLEND_SET_ZORDER updates the state of the current display pipes'
 * Zorder in relation to the other sprite and cursor planes on that pipe.
 * 3 nibbles will hold the bottom, middle and top most planes. These are the
 * variables "top", "middle" and "bottom" of the igd_ovl_pipeblend_t struct.
 * Each nibble can be either OVL_SPRITE1_ZORDER or OVL_SPRITE2_ZORDER or
 * OVL_DISPLAY_ZORDER.
 *
 * If flag IGD_OVL_PIPEBLEND_SET_CONSTALPHA is set,
 * then variable "has_const_alpha" (part of igd_ovl_pipeblend_t structure)
 * will be used to enable or disable constant alpha. If enabled, the
 * alpha value (from "const_alpha" variable) is used to multiply against
 * video pixels before blending in pipe. Used for video fade out effect
 *
 */
typedef struct _igd_ovl_pipeblend{
	union {
		struct {
			unsigned long top:4;
			unsigned long :4;
			unsigned long middle:4;
			unsigned long :4;
			unsigned long bottom:4;
			unsigned long :12;
		}; /* See above comment */
		unsigned char zorder_array[4];
		unsigned long zorder_combined;
	}; /* zorder impacts all planes of this pipe */
	unsigned long   fb_blend_ovl;
		/* fb-blend-ovl impact all planes of this pipe */
	unsigned long   has_const_alpha;
	unsigned long   const_alpha;
		/* const alpha impact only this ovl/sprite plane */
		/* cannot be used for querrying */
	unsigned long   enable_flags;
		/* flag bits indicates which variables are valid */
} igd_ovl_pipeblend_t;

/* FIXME!!! Cant seem to get a static declaration of above structure with
 * initialization working right with the compiler because if the union.
 * The following is a mirror structure that must be removed but is
 * needed for the time being to ensure default_config.c compiles properly
 */
typedef struct _igd_ovl_pipeblend_forinit{
	unsigned long   zorder_combined;
		/* zorder impacts all planes of this pipe */
	unsigned long   fb_blend_ovl;
		/* fb-blend-ovl impact all planes of this pipe */
	unsigned long   has_const_alpha;
	unsigned long   const_alpha;
		/* const alpha impact only this ovl/sprite plane */
		/* cannot be used for querrying */
	unsigned long   enable_flags;
		/* flag bits indicates which variables are valid */
} igd_ovl_pipeblend_forinit_t;

/*----------------------------------------------------------------------------
 * Overlay Query Flags
 *--------------------------------------------------------------------------*/
/*!
 * @name Query Overlay Flags
 * @anchor query_ovl_flags
 *
 * Flags for use with _igd_dispatch::query_ovl()
 * These flags ARE EXCLUSIVE - ONLY ONE AT A TIME can be used during a
 *    single call to igd_query_ovl
 *
 * - IGD_OVL_QUERY_IS_HW_SUPPORTED: Can the hardware support an overlay for
 *   this display_h?  This will return the same value no matter if the overlay
 *   is on or off, so the IAL must use some other method to determine if the
 *   overlay is in use.
 * - IGD_OVL_QUERY_IS_LAST_FLIP_DONE: Has the last flip completed?
 * - IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE: Wait until the last flip is complete.
 *   Returns TRUE if the last flip was successfully completed.  Returns FALSE
 *   if there was a HW issue where the last flip did not occur.
 * - IGD_OVL_QUERY_IS_ON: Is the hardware overlay currently on?  This does not
 *   include the secondary overlay, only the hardware overlay.
 * - IGD_OVL_QUERY_IS_GAMMA_SUPPORTED: Is the hardware supports GAMMA
 *   correction?
 * - IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED: Is the hardware supports video
 *   parameters (brightness, contrast, saturation) correction?
 * - IGD_OVL_QUERY_NUM_PLANES: Returns the number of hardware planes. Doesnt
 *   need a display (crtc) handle when calling with this flag
 * @{
 */
#define IGD_OVL_QUERY_IS_HW_SUPPORTED                     0x00000001
#define IGD_OVL_QUERY_IS_LAST_FLIP_DONE                   0x00000002
#define IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE                 0x00000003
#define IGD_OVL_QUERY_IS_ON                               0x00000004
#define IGD_OVL_QUERY_IS_GAMMA_SUPPORTED                  0x00000005
#define IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED            0x00000006
#define IGD_OVL_QUERY_NUM_PLANES                          0x00000007
#define IGD_OVL_QUERY_MASK                                0x0000000F
	/* Ensure no bits conflict with IGD_OVL_FORCE_USE_DISP above */
/*! @} */

/* This is now in igd_query_max_size_ovl
 * Get the maximum width/height for the src_surface pixel format
 * return: max width and height parameters; should always return TRUE.
#define IGD_OVL_QUERY_MAX_WIDTH_HEIGHT                    0x00000003
 */

/* This should not be needed.  Since the IAL is always passing in valid
 * gamma information, the IAL should not query the HAL for the information.
#define IGD_OVL_QUERY_COLOR_CORRECT_INFO                  0x00000005*/


/*----------------------------------------------------------------------------
 * Overlay Color Key
 *--------------------------------------------------------------------------*/
/*!
 * @name Overlay Color Key Flags
 * @anchor igd_ovl_color_key_info_flags
 *
 * Flags for use with @ref igd_ovl_color_key_info_t
 * Enables and disables the src and dest color key for the overlay
 *
 * @{
 */
#define IGD_OVL_SRC_COLOR_KEY_DISABLE		0x00000000
#define IGD_OVL_SRC_COLOR_KEY_ENABLE		0x00000001

#define IGD_OVL_DST_COLOR_KEY_DISABLE		0x00000000
#define IGD_OVL_DST_COLOR_KEY_ENABLE		0x00000002


/*! @} */

/*!
 * @brief Overlay Color Key
 *
 * The src_lo, src_hi, and dest color key are in the following pixel format
 * based on the pixel format of the src and dest surface.
 * Note: Alpha is always ignored.
 *
 * - xRGB32        = x[31:24] R[23:16] G[15:8] B[7:0]
 * - xRGB16_555    = x[31:15] R[14:10] G[9:5]  B[4:0]
 * - ARGB8_INDEXED = x[31:8] Index Color[7:0]
 * - YUV surf      = x[31:24] Y[23:16] U[15:8] V[7:0]
 *
 * If the source pixel (overlay) is within the src_lo and src_hi color key
 * (inclusive) for all color components, then the destination pixel is
 * displayed.  Otherwise the source pixel is displayed.
 *
 * If the source and dest color key are both enabled, what happens???
 */
typedef struct _igd_ovl_color_key_info {
	/*! Low end src color key value */
	unsigned long        src_lo;
	/*! High end src color key value */
	unsigned long        src_hi;
	/*! Dest color key value.  If the destination pixel matches the
	 *   dest color key then the souce pixel from the video surface is displayed.
	 *   Otherwise, the destination pixel is displayed */
	unsigned long        dest;
	/*! Enable and disable the src and dest color key.
	 *   See @ref igd_ovl_color_key_info_flags */
	unsigned long        flags;
} igd_ovl_color_key_info_t;

/*----------------------------------------------------------------------------
 * Overlay Video Quality
 *--------------------------------------------------------------------------*/
/*!
 * @brief Overlay Video Quality
 *
 * Brightness is a signed value with range [-127:127] while the
 * remaining plane color attributes are unsigned.
 */
typedef struct _igd_ovl_color_correct_info{
	short		        brightness;
	unsigned short		contrast;
	unsigned short		saturation;
	unsigned short		hue;
} igd_ovl_color_correct_info_t;


/*----------------------------------------------------------------------------
 *  Overlay Gamma
 *--------------------------------------------------------------------------*/
/*!
 * @name Overlay Gamma Flags
 * @anchor igd_ovl_gamma_info_flags
 *
 * Flags for use with @ref igd_ovl_gamma_info_t.
 * Enables and disables the gamma for the overlay
 *
 * @{
 */
#define IGD_OVL_GAMMA_DISABLE	0x00000000
#define IGD_OVL_GAMMA_ENABLE	0x00000001
/*! @} */

/*!
 * @name Overlay Gamma Min/Max
 * @anchor igd_ovl_gamma_info_range
 *
 * Minimum and maximum gamma values for red, green, and blue with
 * @ref igd_ovl_gamma_info_t.  These values are in 24i.8f format.
 * Minimum value is 0.6
 * Maximum value is 6.0
 *
 * @{
 */
#define IGD_OVL_GAMMA_DEFAULT   0x100  /* 1.0 */
#define IGD_OVL_GAMMA_MIN       0x09A  /* 0.6 */
#define IGD_OVL_GAMMA_MAX       0x600  /* 6.0 */
/*! @} */

/*!
 * @brief Overlay Gamma
 *
 * Red, green, and blue values must be between min and max values.
 * This value is in 24i.8f format.
 * See @ref igd_ovl_gamma_info_range
 */
typedef struct _igd_ovl_gamma_info{
	unsigned int		red;
	unsigned int		green;
	unsigned int		blue;
	/*! Enable and disable the gamma for the overlay.
	 *   See @ref igd_ovl_gamma_info_flags */
	unsigned int		flags;
} igd_ovl_gamma_info_t;
/*! @} */

#ifndef FWTOOLS
/*----------------------------------------------------------------------------
 * Overlay Info
 *--------------------------------------------------------------------------*/
/*!
 * @brief Overlay Information (includes color key, video quality, and gamma)
 */
typedef struct _igd_ovl_info{
	igd_ovl_color_key_info_t			color_key;
	igd_ovl_color_correct_info_t		color_correct;
	igd_ovl_gamma_info_t				gamma;
	igd_ovl_pipeblend_t                 pipeblend;
} igd_ovl_info_t;

#define OVL_PRIMARY   0
#define OVL_SECONDARY 1
#define IGD_OVL_MAX_HW    4  /* Maximum number of overlays */
/* in all variables with array IGD_OVL_MAX_HW,
 * index 0/1 = Ovl/Sprite A/B for Pipe A
 * index 2/3 = Ovl/Sprite C/D for Pipe B
 */
typedef struct _igd_ovl_plane{
	/* linkage back to the upper layer DRM plane structure*/
	struct drm_plane base;
	enum pipe pipe;
	struct drm_i915_gem_object *obj;
	/* this overlay plane's HAL id */
	int plane_id;
	char * plane_name;
	int enabled;

	/* The following are this plane's current state*/
	/* But these are emgd device specific references */
	void * emgd_crtc;//emgd_crtc_t * emgd_crtc;
			/* --> void typecast from emgd_crtc_t - map from drm_crtc */

	int src_has_alpha;

	/* the following are the "current" hal context for this ovl plane */
	emgd_framebuffer_t curr_surf;
	igd_ovl_info_t ovl_info;
	igd_rect_t src_rect;
	igd_rect_t dst_rect;
	unsigned long igd_flags;

	/* the following are this plane's capabilities */
	void * target_igd_pipe_ptr;
	int max_downscale; /* 0 means no limit */
	int max_upscale; /* 0 means no limit */
	int num_gamma;
	int has_ccorrect;
	int has_constalpha;
	unsigned long * supported_pixel_formats;/* ended by format of 0x0 */
	struct emgd_obj *sysfs_obj;
	/* zorder_control_bits for sprite control register */
	unsigned int zorder_control_bits;
#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	struct drm_flip_helper flip_helper;
#endif
	void *coords;
	void *regs;
	struct drm_i915_gem_object *oldbufobj;
	spinlock_t commit_lock_workers;
	struct _igd_ovl_plane *other_sprplane;

	/* Used to indicate this sprite is enable for S3D usage */
	int enabled_by_s3d;
} igd_ovl_plane_t;

/* User mode overlay context */
typedef struct _igd_ovl_caps {
	int num_planes;
	igd_ovl_plane_t * ovl_planes;
} igd_ovl_caps_t;
#endif

#endif /*_IGD_OVL_H*/
