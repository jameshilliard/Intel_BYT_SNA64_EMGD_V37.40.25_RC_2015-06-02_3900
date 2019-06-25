/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd_drm.h
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
#ifndef _EMGD_DRM_H_
#define _EMGD_DRM_H_

#include <drm/drm.h>
#include <igd_core_structs.h>
#include <linux/version.h>

/* GEM-related defines from libdrm */
#include <i915_drm.h>

enum {
	CHIP_SNB_0102 = 3,
	CHIP_IVB_0152 = 4,
	CHIP_VLV_0f30 = 5,
	CHIP_VLV2_0f31 = 6,
	CHIP_VLV3_0f32 = 7,
	CHIP_VLV4_0f33 = 8,
};

/*
 * This is where all the data structures used by the Koheo DRM interface are
 * defined.  These data structures are shared between and are used to pass data
 * between the user-space & kernel-space code for each ioctl.
 *
 * The naming convention is:  emgd_drm_<HAL-procedure-pointer-name>_t
 */


/* Set the destination color key on a given sprite */
/* NOT IMMEDIATE: Needs a subsequent flip (set_plane) */
/*
 * Intel sprite handling
 *
 * Color keying works with a min/mask/max tuple.  Both source and destination
 * color keying is allowed.
 *
 * Source keying:
 * Sprite pixels within the min & max values, masked against the color channels
 * specified in the mask field, will be transparent.  All other pixels will
 * be displayed on top of the primary plane.  For RGB surfaces, only the min
 * and mask fields will be used; ranged compares are not allowed.
 *
 * Destination keying:
 * Primary plane pixels that match the min value, masked against the color
 * channels specified in the mask field, will be replaced by corresponding
 * pixels from the sprite plane.
 *
 * Note that source & destination keying are exclusive; only one can be
 * active on a given plane.
 */
#define I915_SET_COLORKEY_NONE		(1<<0) /* disable color key matching */
#define I915_SET_COLORKEY_DESTINATION	(1<<1)
#define I915_SET_COLORKEY_SOURCE	(1<<2)

/* Set or get the current color correction info on a given sprite */
/* This MUST be an immediate update because of customer requirements */
struct drm_intel_sprite_colorcorrect {
	__u32 plane_id;
	__s32 brightness;  /* range = -128 to 127, default = -5 */
	__u32 contrast;    /* range = 0 to 255, default = 67 */
	__u32 saturation;  /* range = 0 to 1023, default = 145 */
	__u32 hue;         /* range = 0 to 1023, default = 0 */
	/*
	 * 1. Take note that depending on the HW, there are usage limitations -
	 * end user needs to be aware of that. These interface umbrella most HW caps.
	 * 2. Also, user space client should "get" first then "set" with incremental
	 * changes since kernel driver could have OEM specific initial values (depending
	 * on use case).
	 */
};

#define I915_SPRITEFLAG_PIPEBLEND_FBBLENDOVL    0x00000001
#define I915_SPRITEFLAG_PIPEBLEND_CONSTALPHA    0x00000002
#define I915_SPRITEFLAG_PIPEBLEND_ZORDER        0x00000004
/* Set or Get the current color correction info on a given sprite */
/* This MUST be an immediate update because of customer requirements */
struct drm_intel_sprite_pipeblend {
	__u32 plane_id;
	__u32 crtc_id;
	__u32 enable_flags;
		/* I915_SPRITEFLAG_PIPEBLEND_FBBLENDOVL = 0x00000001*/
		/* I915_SPRITEFLAG_PIPEBLEND_CONSTALPHA = 0x00000002*/
		/* I915_SPRITEFLAG_PIPEBLEND_ZORDER     = 0x00000004*/
	__u32 fb_blend_ovl;
	__u32 has_const_alpha;
	__u32 const_alpha; /* 8 LSBs is alpha channel*/
	__u32 zorder_value;
};


#ifdef SPLASH_VIDEO_SUPPORT
/*
 * This struct is used to pass/register the frame buffers added thru addFb2
 * to the driver for splash video usage.
 */
#define SPVIDEO_MAX_FB_SUPPORTED	(10)
struct spvideo_fb_list {
        __u32 fb_ids[SPVIDEO_MAX_FB_SUPPORTED];
        __u32 fbs_allocated;
};
#endif

/*
 * This is where all the IOCTL's used by the egd DRM interface are
 * defined.  This information is shared between the user space code and
 * the kernel module.
 */

#define BASE DRM_COMMAND_BASE

/*
 * EMGD-specific ioctls.  These get mapped to the device specific range between
 * 0x40 and 0x99.  That means our offsets here can range from 0x00 - 0x59
 *
 * Since our goal is to re-use i915 GEM "as is," including the libdrm
 * interfaces to it, we need to make sure that some GEM ioctl's are at the same
 * place in the table as OTC's driver (since libdrm defines those positions).
 * Client driver must use these values!
 */
/**************************************************************
 *
 * IMPORTANT NOTE FOR I915 DRM IOCTL COMPLIANCE -->
 *
 *     I915 USES IOCTL CODES FROM 0x0 THRU 0x32 (as of 3.7.0)
 *     0x33 TO 0x59 ARE FREE, BUT OTC WILL SURE GROW EM
 *     0x10 AND 0x12 ARE HOLES, BUT WE'LL IGNORE THOSE
 *     SO LETS START WITH 0x40 FOR EGD SPECIFIC ONES
 *     (IF WE EVER REALLY NEED TO)
 *
 *************************************************************
 */
/* 0X06 - reserve for "GETPARAM" */
/* 0x13 - 0x26 are reserved for GEM */
/* 0x29 is reserved for GEM_EXECBUFFER2 */


#ifdef SPLASH_VIDEO_SUPPORT
/* For splash video to overlay without being master */
#define DRM_IGD_SPVIDEO_INIT_BUFFERS 0x47
#define DRM_IGD_SPVIDEO_DISPLAY      0x48
#endif

#define DRM_IGD_MODE_SET_PIPEBLEND        0x45
#define DRM_IGD_MODE_GET_PIPEBLEND        0x46
#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
#define DRM_IGD_SET_SPRITE_COLORCORRECT   0x4b
#define DRM_IGD_GET_SPRITE_COLORCORRECT   0x4c
#endif

/************************************************ 
 *
 * BACK PORTING i915 IOCTLs for earlier kernels
 *
 ************************************************/

#define DRM_I915_GET_SPRITE_COLORKEY      0x2a
#define DRM_I915_SET_SPRITE_COLORKEY      0x2b

/*
 * egd IOCTLs.
 */


/*
 * LTSI kernel support
 * Linux Kernel LTSI is not typical kernel EMGD used to support.  The
 * kernel is a moving target.  It may contains latest upstream change such as
 * driver which include kernel DRM tree.  Current version for LTSI supported is
 * 3.10.25/28.
 */
#ifdef LTSI_KERNEL
#define LTSI_VERSION LINUX_VERSION_CODE
#else
#define LTSI_VERSION KERNEL_VERSION(0, 0, 0)
#endif

#ifdef SPLASH_VIDEO_SUPPORT
/* For splash video to overlay without being master */
#define DRM_IOCTL_IGD_SPVIDEO_INIT_BUFFERS     DRM_IOWR(BASE + DRM_IGD_SPVIDEO_INIT_BUFFERS,\
			struct spvideo_fb_list)
#define DRM_IOCTL_IGD_SPVIDEO_DISPLAY          DRM_IOWR(BASE + DRM_IGD_SPVIDEO_DISPLAY,\
			struct drm_mode_set_plane)
#endif

#define DRM_IOCTL_IGD_MODE_SET_PIPEBLEND         DRM_IOWR(BASE + DRM_IGD_MODE_SET_PIPEBLEND,\
			struct drm_intel_sprite_pipeblend)
#define DRM_IOCTL_IGD_MODE_GET_PIPEBLEND         DRM_IOWR(BASE + DRM_IGD_MODE_GET_PIPEBLEND,\
			struct drm_intel_sprite_pipeblend)
#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
#define DRM_IOCTL_IGD_SET_SPRITE_COLORCORRECT    DRM_IOWR(BASE + DRM_IGD_SET_SPRITE_COLORCORRECT,\
			struct drm_intel_sprite_colorcorrect)
#define DRM_IOCTL_IGD_GET_SPRITE_COLORCORRECT    DRM_IOWR(BASE + DRM_IGD_GET_SPRITE_COLORCORRECT,\
			struct drm_intel_sprite_colorcorrect)
#endif

/* i915 GEM ioctls are defined in i915_drm.h */


/** @{
 * Intel memory domains
 *
 * Most of these just align with the various caches in
 * the system and are used to flush and invalidate as
 * objects end up cached in different domains.
 */
/** CPU cache */
#define I915_GEM_DOMAIN_CPU		0x00000001
/** Render cache, used by 2D and 3D drawing */
#define I915_GEM_DOMAIN_RENDER		0x00000002
/** Sampler cache, used by texture engine */
#define I915_GEM_DOMAIN_SAMPLER		0x00000004
/** Command queue, used to load batch buffers */
#define I915_GEM_DOMAIN_COMMAND		0x00000008
/** Instruction cache, used by shader programs */
#define I915_GEM_DOMAIN_INSTRUCTION	0x00000010
/** Vertex address cache */
#define I915_GEM_DOMAIN_VERTEX		0x00000020
/** GTT domain - aperture and scanout */
#define I915_GEM_DOMAIN_GTT		0x00000040
/** @} */

#define I915_TILING_NONE	0
#define I915_TILING_X		1
#define I915_TILING_Y		2

#define I915_BIT_6_SWIZZLE_NONE		0
#define I915_BIT_6_SWIZZLE_9		1
#define I915_BIT_6_SWIZZLE_9_10		2
#define I915_BIT_6_SWIZZLE_9_11		3
#define I915_BIT_6_SWIZZLE_9_10_11	4
/* Not seen by userland */
#define I915_BIT_6_SWIZZLE_UNKNOWN	5
/* Seen by userland. */
#define I915_BIT_6_SWIZZLE_9_17		6
#define I915_BIT_6_SWIZZLE_9_10_17	7


#define DRMFB_PITCH pitches[0]
#define DRMMODE_HANDLE handles[0]
#define DRM_MODE_FB_CMD_TYPE drm_mode_fb_cmd2

#endif

/* These are S3D flags used by drm_display_mode */
#ifndef DRM_MODE_FLAG_3D_TOP_AND_BOTTOM
#define DRM_MODE_FLAG_3D_TOP_AND_BOTTOM          (1<<14)
#endif
#ifndef DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF
#define DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF       (1<<15)
#endif
#ifndef DRM_MODE_FLAG_3D_FRAME_PACKING
#define DRM_MODE_FLAG_3D_FRAME_PACKING           (1<<16)
#endif
#ifndef DRM_MODE_FLAG_3D_MASK
#define DRM_MODE_FLAG_3D_MASK   (DRM_MODE_FLAG_3D_TOP_AND_BOTTOM |     \
                                 DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF |  \
                                 DRM_MODE_FLAG_3D_FRAME_PACKING)
#endif

/* For checking the string1 and string2 same or not */
#define CHECK_NAME(M, N) (strncmp(M, N, sizeof(N)) == 0)

