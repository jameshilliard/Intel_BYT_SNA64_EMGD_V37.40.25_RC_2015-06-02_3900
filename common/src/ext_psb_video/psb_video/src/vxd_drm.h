/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics Inc.  Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#ifndef _VXD_DRM_H_
#define _VXD_DRM_H_

#include <drm/drm.h>
#include <linux/version.h>


#define PSB_RELOC_MAGIC         0x67676767
#define PSB_RELOC_SHIFT_MASK    0x0000FFFF
#define PSB_RELOC_SHIFT_SHIFT   0
#define PSB_RELOC_ALSHIFT_MASK  0xFFFF0000
#define PSB_RELOC_ALSHIFT_SHIFT 16

#define PSB_RELOC_OP_OFFSET     0	/* Offset of the indicated
					 * buffer
					 */
struct psb_mmu_driver;

struct drm_psb_reloc {
	uint32_t reloc_op;
	uint32_t where;		/* offset in destination buffer */
	uint32_t buffer;	/* Buffer reloc applies to */
	uint32_t mask;		/* Destination format: */
	uint32_t shift;		/* Destination format: */
	uint32_t pre_add;	/* Destination format: */
	uint32_t background;	/* Destination add */
	uint32_t dst_buffer;	/* Destination buffer. Index into buffer_list */
	uint32_t arg0;		/* Reloc-op dependant */
	uint32_t arg1;
};

#define PSB_GPU_ACCESS_READ         (1ULL << 32)
#define PSB_GPU_ACCESS_WRITE        (1ULL << 33)
#define PSB_GPU_ACCESS_MASK         (PSB_GPU_ACCESS_READ | PSB_GPU_ACCESS_WRITE)

#define PSB_ENGINE_2D 2
#define PSB_ENGINE_DECODE 0
#define PSB_ENGINE_VIDEO 0
#define VSP_ENGINE_VPP 6

/*
 * For this fence class we have a couple of
 * fence types.
 */
#define _PSB_FENCE_EXE_SHIFT           0
#define _PSB_FENCE_TYPE_EXE         (1 << _PSB_FENCE_EXE_SHIFT)

struct drm_psb_getpageaddrs_arg {
	uint32_t handle;
	unsigned long *page_addrs;
	unsigned long gtt_offset;
};

struct drm_psb_extension_rep {
	int32_t exists;
	uint32_t driver_ioctl_offset;
	uint32_t sarea_offset;
	uint32_t major;
	uint32_t minor;
	uint32_t pl;
};

#define DRM_PSB_EXT_NAME_LEN 128

/* IOCTL data structures */
union drm_psb_extension_arg {
	char extension[DRM_PSB_EXT_NAME_LEN];
	struct drm_psb_extension_rep rep;
};

struct psb_validate_req {
	uint64_t set_flags;
	uint64_t clear_flags;
	uint64_t next;
	uint64_t presumed_gpu_offset;
	uint32_t buffer_handle;
	uint32_t presumed_flags;
	uint32_t group;
	uint32_t pad64;
};

struct psb_validate_rep {
	uint64_t gpu_offset;
	uint32_t placement;
	uint32_t fence_type_mask;
};

#define PSB_USE_PRESUMED     (1 << 0)

struct psb_validate_arg {
	int handled;
	int ret;
	union {
		struct psb_validate_req req;
		struct psb_validate_rep rep;
	} d;
};

#define DRM_PSB_FENCE_NO_USER        (1 << 0)

typedef struct drm_psb_cmdbuf_arg {
	uint64_t buffer_list;	/* List of buffers to validate */
	uint64_t fence_arg;

	uint32_t cmdbuf_handle;	/* 2D Command buffer object or, */
	uint32_t cmdbuf_offset;	/* rasterizer reg-value pairs */
	uint32_t cmdbuf_size;

	uint32_t reloc_handle;	/* Reloc buffer object */
	uint32_t reloc_offset;
	uint32_t num_relocs;

	/* Not implemented yet */
	uint32_t fence_flags;
	uint32_t engine;

} drm_psb_cmdbuf_arg_t;

enum lnc_getparam_key {
	LNC_VIDEO_DEVICE_INFO,
	LNC_VIDEO_GETPARAM_RAR_INFO,
	LNC_VIDEO_GETPARAM_CI_INFO,
	LNC_VIDEO_FRAME_SKIP,
	IMG_VIDEO_DECODE_STATUS,
	IMG_VIDEO_NEW_CONTEXT,
	IMG_VIDEO_RM_CONTEXT,
	IMG_VIDEO_UPDATE_CONTEXT,
	IMG_VIDEO_MB_ERROR,
	IMG_VIDEO_SET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_DISPLAYING_FRAME,
	IMG_VIDEO_GET_HDMI_STATE,
	IMG_VIDEO_SET_HDMI_STATE,
	PNW_VIDEO_QUERY_ENTRY,
	IMG_DISPLAY_SET_WIDI_EXT_STATE,
	IMG_VIDEO_IED_STATE
};

struct drm_lnc_video_getparam_arg {
	enum lnc_getparam_key key;
	uint64_t arg;		/* argument pointer */
	uint64_t value;		/* feed back pointer */
};

struct drm_video_displaying_frameinfo {
	uint32_t buf_handle;
	uint32_t width;
	uint32_t height;
	uint32_t size;		/* buffer size */
	uint32_t format;	/* fourcc */
	uint32_t luma_stride;	/* luma stride */
	uint32_t chroma_u_stride;	/* chroma stride */
	uint32_t chroma_v_stride;
	uint32_t luma_offset;	/* luma offset from the beginning of the memory */
	uint32_t chroma_u_offset;	/* UV offset from the beginning of the memory */
	uint32_t chroma_v_offset;
	uint32_t reserved;
};

#define MAX_SLICES_PER_PICTURE 72
struct  psb_msvdx_mb_region {
	uint32_t start;
	uint32_t end;
};

typedef struct drm_psb_msvdx_decode_status {
	uint32_t num_region;
	struct psb_msvdx_mb_region mb_regions[MAX_SLICES_PER_PICTURE];
} drm_psb_msvdx_decode_status_t;


struct drm_psb_buffer_data {
	void *h_buffer;
};

#endif
