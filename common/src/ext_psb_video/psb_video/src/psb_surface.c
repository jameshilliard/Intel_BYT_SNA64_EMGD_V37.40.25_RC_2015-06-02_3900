/*
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
 * Copyright (c) Imagination Technologies Limited, UK
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Waldo Bastian <waldo.bastian@intel.com>
 *
 */

#include "psb_def.h"
#include "psb_surface.h"
#include "psb_drv_debug.h"
#include <ipvr_drm.h>

/*
 * Create surface
 */
VAStatus psb_surface_create(psb_driver_data_p driver_data,
                            int width, int height, int fourcc, unsigned int flags,
                            psb_surface_p psb_surface /* out */
                           )
{
    int tiling = GET_SURFACE_INFO_tiling(psb_surface);

    if (fourcc == VA_FOURCC_NV12) {
        if ((width <= 0) || (width * height > 5120 * 5120) || (height <= 0)) {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        if (512 >= width) {
            psb_surface->stride_mode = STRIDE_512;
            psb_surface->stride = 512;
        } else if (1024 >= width) {
            psb_surface->stride_mode = STRIDE_1024;
            psb_surface->stride = 1024;
        } else if (1280 >= width) {
            if (tiling) {
                psb_surface->stride_mode = STRIDE_2048;
                psb_surface->stride = 2048;
            }
            else {
            psb_surface->stride_mode = STRIDE_1280;
            psb_surface->stride = 1280;
            }
        } else if (2048 >= width) {
            psb_surface->stride_mode = STRIDE_2048;
            psb_surface->stride = 2048;
        } else if (4096 >= width) {
            psb_surface->stride_mode = STRIDE_4096;
            psb_surface->stride = 4096;
        } else {
            psb_surface->stride_mode = STRIDE_NA;
            psb_surface->stride = (width + 0x3f) & ~0x3f;
        }

        psb_surface->luma_offset = 0;
        psb_surface->chroma_offset = psb_surface->stride * height;
        psb_surface->size = (psb_surface->stride * height * 3) / 2;
        psb_surface->fourcc = VA_FOURCC_NV12;
    }
    else {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s::%d unsupported fourcc \"%c%c%c%c\"\n",
            __FILE__, __LINE__,
            fourcc & 0xf, (fourcc >> 8) & 0xf, (fourcc >> 16) & 0xf, (fourcc >> 24) & 0xf);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

    psb_surface->buf = drm_ipvr_gem_bo_alloc(driver_data->bufmgr, NULL,
        "VASurface", psb_surface->size, tiling, IPVR_CACHE_UNCACHED);

    return psb_surface->buf ? VA_STATUS_SUCCESS: VA_STATUS_ERROR_ALLOCATION_FAILED;
}

/*
 * Create surface by importing PRIME BO
 */
VAStatus psb_surface_create_from_prime(
    psb_driver_data_p driver_data,
    int width, int height, int fourcc, int tiling,
    unsigned int *pitches, unsigned int *offsets,
    unsigned int size,
    psb_surface_p psb_surface, /* out */
    int prime_fd,
    unsigned int flags)
{
    drv_debug_msg(VIDEO_DEBUG_ERROR, "%s::%d, psb_surface=%p\n", __func__, __LINE__, psb_surface);
    ASSERT (!tiling);
    if (fourcc == VA_FOURCC_NV12) {
        psb_surface->stride = pitches[0];
        if (512 == pitches[0]) {
            psb_surface->stride_mode = STRIDE_512;
        } else if (1024 == pitches[0]) {
            psb_surface->stride_mode = STRIDE_1024;
        } else if (1280 == pitches[0]) {
            psb_surface->stride_mode = STRIDE_1280;
            if (tiling) {
                psb_surface->stride_mode = STRIDE_2048;
                psb_surface->stride = 2048;
            }
        } else if (2048 == pitches[0]) {
            psb_surface->stride_mode = STRIDE_2048;
        } else if (4096 == pitches[0]) {
            psb_surface->stride_mode = STRIDE_4096;
        } else {
            psb_surface->stride_mode = STRIDE_NA;
        }
        if (psb_surface->stride != pitches[0]) {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        psb_surface->luma_offset = offsets[0];
        psb_surface->chroma_offset = offsets[1];

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s surface stride %d height %d, width %d, "
            "luma_off %d, chroma off %d\n", __func__, psb_surface->stride,
            height, width, psb_surface->luma_offset, psb_surface->chroma_offset);
        psb_surface->size = size;
        psb_surface->fourcc = VA_FOURCC_NV12;
    } else {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s unknown fourcc %c%c%c%c\n",
            __func__,
            fourcc & 0xff, (fourcc >> 8) & 0xff,
            (fourcc >> 16) & 0xff, (fourcc >> 24) & 0xff);
        return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    }
    ASSERT (psb_surface->size > 0);
    drv_debug_msg(VIDEO_DEBUG_ERROR, "%s create_from_prime with fd %d and size 0x%x\n",
        __func__, prime_fd, psb_surface->size);
    psb_surface->buf = drm_ipvr_gem_bo_create_from_prime(driver_data->bufmgr, NULL,
        "imported_surface", prime_fd, psb_surface->size);
    return psb_surface->buf ? VA_STATUS_SUCCESS: VA_STATUS_ERROR_ALLOCATION_FAILED;
}

VAStatus psb_surface_create_for_userptr(
    psb_driver_data_p driver_data,
    int width, int height,
    unsigned size, /* total buffer size need to be allocated */
    unsigned int fourcc, /* expected fourcc */
    unsigned int luma_stride, /* luma stride, could be width aligned with a special value */
    unsigned int chroma_u_stride, /* chroma stride */
    unsigned int chroma_v_stride,
    unsigned int luma_offset, /* could be 0 */
    unsigned int chroma_u_offset, /* UV offset from the beginning of the memory */
    unsigned int chroma_v_offset,
    psb_surface_p psb_surface /* out */
)
{
    // not support now
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

/*
 * Temporarily map surface and set all chroma values of surface to 'chroma'
 */
VAStatus psb_surface_set_chroma(psb_surface_p psb_surface, int chroma)
{
    unsigned char *surface_data;
    int  ret = drm_ipvr_gem_bo_map(psb_surface->buf, 1);

    if (ret) return VA_STATUS_ERROR_UNKNOWN;
   surface_data = psb_surface->buf->virt;
    memset(surface_data + psb_surface->chroma_offset, chroma, psb_surface->size - psb_surface->chroma_offset);

    drm_ipvr_gem_bo_unmap(psb_surface->buf);
    return VA_STATUS_SUCCESS;
}

/*
 * Destroy surface
 */
void psb_surface_destroy(psb_surface_p psb_surface)
{
    drm_ipvr_gem_bo_unreference(psb_surface->buf);
}

VAStatus psb_surface_sync(psb_surface_p psb_surface)
{
    drm_ipvr_gem_bo_wait(psb_surface->buf);
    return VA_STATUS_SUCCESS;
}

VAStatus psb_surface_query_status(psb_surface_p psb_surface, VASurfaceStatus *status)
{
    // query decode status in VED (not in I915 rendering side)
    if (drm_ipvr_gem_bo_busy(psb_surface->buf))
        *status = VASurfaceRendering;
    else
        *status = VASurfaceReady;

    return VA_STATUS_SUCCESS;
}
