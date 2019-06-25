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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <errno.h>

#include <va/va_backend.h>
#include <va/va_backend_tpi.h>
#include <va/va_backend_egl.h>
#include <va/va_drmcommon.h>

#include "psb_drv_video.h"
#include "psb_surface.h"
#include "tng_VP8.h"
#include "psb_output.h"
#include "psb_def.h"
#include "psb_drv_debug.h"
#include "pnw_rotate.h"

#include <ipvr_drm.h>
#include <ipvr_bufmgr.h>

#ifndef PSB_PACKAGE_VERSION
#define PSB_PACKAGE_VERSION "Undefined"
#endif

#define PSB_DRV_VERSION  PSB_PACKAGE_VERSION
#define PSB_CHG_REVISION "(0X00000071)"

#define PSB_STR_VENDOR_MRST     "Intel GMA500-MRST-" PSB_DRV_VERSION " " PSB_CHG_REVISION
#define PSB_STR_VENDOR_MFLD     "Intel GMA500-MFLD-" PSB_DRV_VERSION " " PSB_CHG_REVISION
#define PSB_STR_VENDOR_MRFL     "Intel GMA500-MRFL-" PSB_DRV_VERSION " " PSB_CHG_REVISION
#define PSB_STR_VENDOR_BAYTRAIL     "New Intel GMA500-BAYTRAIL-" PSB_DRV_VERSION " " PSB_CHG_REVISION
#define PSB_STR_VENDOR_LEXINGTON     "Intel GMA500-LEXINGTON-" PSB_DRV_VERSION " " PSB_CHG_REVISION

#define MAX_UNUSED_BUFFERS      16

#define PSB_MAX_FLIP_DELAY (1000/30/10)

#include <signal.h>

#define EXPORT __attribute__ ((visibility("default")))

#define INIT_DRIVER_DATA    psb_driver_data_p driver_data = (psb_driver_data_p) ctx->pDriverData;

#define INIT_FORMAT_VTABLE format_vtable_p format_vtable = ((profile < PSB_MAX_PROFILES) && (entrypoint < PSB_MAX_ENTRYPOINTS)) ? (profile == VAProfileNone? driver_data->vpp_profile : driver_data->profile2Format[profile][entrypoint]) : NULL;

#define CONFIG(id)  ((object_config_p) object_heap_lookup( &driver_data->config_heap, id ))
#define CONTEXT(id) ((object_context_p) object_heap_lookup( &driver_data->context_heap, id ))
#define SURFACE(id)    ((object_surface_p) object_heap_lookup( &driver_data->surface_heap, id ))
#define BUFFER(id)  ((object_buffer_p) object_heap_lookup( &driver_data->buffer_heap, id ))

#define CONFIG_ID_OFFSET        0x01000000
#define CONTEXT_ID_OFFSET       0x02000000
#define SURFACE_ID_OFFSET       0x03000000
#define BUFFER_ID_OFFSET        0x04000000
#define IMAGE_ID_OFFSET         0x05000000
#define SUBPIC_ID_OFFSET        0x06000000

static int psb_get_device_info(VADriverContextP ctx);

unsigned long read_addr(unsigned long *virt_addr)
{
    return *virt_addr;
}

void write_addr(unsigned long *virt_addr, unsigned long value)
{
    *virt_addr = value;
}

void psb_init_surface_pvr2dbuf(psb_driver_data_p driver_data);
void psb_free_surface_pvr2dbuf(psb_driver_data_p driver_data);

VAStatus psb_QueryConfigProfiles(
    VADriverContextP ctx,
    VAProfile *profile_list,    /* out */
    int *num_profiles            /* out */
)
{
    DEBUG_FUNC_ENTER
    (void) ctx; /* unused */
    int i = 0;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    INIT_DRIVER_DATA

    CHECK_INVALID_PARAM(profile_list == NULL);
    CHECK_INVALID_PARAM(num_profiles == NULL);



    if (IS_MRFL(driver_data) || IS_BAYTRAIL(driver_data))
	{
        profile_list[i++] = VAProfileVP8Version0_3;
    }

    /* If the assert fails then PSB_MAX_PROFILES needs to be bigger */
    ASSERT(i <= PSB_MAX_PROFILES);
    *num_profiles = i;
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

VAStatus psb_QueryConfigEntrypoints(
    VADriverContextP ctx,
    VAProfile profile,
    VAEntrypoint  *entrypoint_list,    /* out */
    int *num_entrypoints        /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int entrypoints = 0;
    int i;

    CHECK_INVALID_PARAM(entrypoint_list == NULL);
    CHECK_INVALID_PARAM((num_entrypoints == NULL) || (profile >= PSB_MAX_PROFILES));

    for (i = 0; i < PSB_MAX_ENTRYPOINTS; i++) {
	if (profile != VAProfileNone && driver_data->profile2Format[profile][i]) {
                entrypoints++;
                *entrypoint_list++ = i;
        }
    }

    /* If the assert fails then PSB_MAX_ENTRYPOINTS needs to be bigger */
    ASSERT(entrypoints <= PSB_MAX_ENTRYPOINTS);

    if (0 == entrypoints) {
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
    }

    *num_entrypoints = entrypoints;
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

/*
 * Figure out if we should return VA_STATUS_ERROR_UNSUPPORTED_PROFILE
 * or VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT
 */
static VAStatus psb__error_unsupported_profile_entrypoint(psb_driver_data_p driver_data, VAProfile profile, VAEntrypoint entrypoint)
{
    /* Does the driver support _any_ entrypoint for this profile? */
    if (profile < PSB_MAX_PROFILES) {
        int i;

        /* Do the parameter check for MFLD and MRFLD */
        if (profile == VAProfileNone)
            return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;

        for (i = 0; i < PSB_MAX_ENTRYPOINTS; i++) {
            if (driver_data->profile2Format[profile][i]) {
                /* There is an entrypoint, so the profile is supported */
                return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
            }
        }
    }
    return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

VAStatus psb_GetConfigAttributes(
    VADriverContextP ctx,
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,    /* in/out */
    int num_attribs
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA

    INIT_FORMAT_VTABLE

    int i;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    if (NULL == format_vtable) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to NULL format_table\n", __func__);
        return psb__error_unsupported_profile_entrypoint(driver_data, profile, entrypoint);
    }

    CHECK_INVALID_PARAM(attrib_list == NULL);
    CHECK_INVALID_PARAM(num_attribs <= 0);

    /* Generic attributes */
    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            attrib_list[i].value = VA_RT_FORMAT_YUV420;
            break;

        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }
    /* format specific attributes */
    format_vtable->queryConfigAttributes(profile, entrypoint, attrib_list, num_attribs);
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

static VAStatus psb__update_attribute(object_config_p obj_config, VAConfigAttrib *attrib)
{
    int i;
    /* Check existing attributes */
    for (i = 0; i < obj_config->attrib_count; i++) {
        if (obj_config->attrib_list[i].type == attrib->type) {
            /* Update existing attribute */
            obj_config->attrib_list[i].value = attrib->value;
            return VA_STATUS_SUCCESS;
        }
    }
    if (obj_config->attrib_count < PSB_MAX_CONFIG_ATTRIBUTES) {
        i = obj_config->attrib_count;
        obj_config->attrib_list[i].type = attrib->type;
        obj_config->attrib_list[i].value = attrib->value;
        obj_config->attrib_count++;
        return VA_STATUS_SUCCESS;
    }
    return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
}

static VAStatus psb__validate_config(object_config_p obj_config)
{
    int i;
    /* Check all attributes */
    for (i = 0; i < obj_config->attrib_count; i++) {
        switch (obj_config->attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            if (obj_config->attrib_list[i].value != VA_RT_FORMAT_YUV420) {
                return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
            }
            break;

        default:
            /*
             * Ignore unknown attributes here, it
             * may be format specific.
             */
            break;
        }
    }
    return VA_STATUS_SUCCESS;
}

VAStatus psb_CreateConfig(
    VADriverContextP ctx,
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list,
    int num_attribs,
    VAConfigID *config_id        /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    INIT_FORMAT_VTABLE

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int configID;
    object_config_p obj_config;
    int i;

    drv_debug_msg(VIDEO_DEBUG_INIT, "CreateConfig profile:%d, entrypoint:%d, num_attribs:%d.\n",
        profile, entrypoint, num_attribs);
    /*echo 8 > /sys/module/pvrsrvkm/parameters/no_ec will disable error concealment*/

    CHECK_INVALID_PARAM(config_id == NULL);
    CHECK_INVALID_PARAM(num_attribs < 0);
    CHECK_INVALID_PARAM(attrib_list == NULL);

    if (NULL == format_vtable) {
        vaStatus = psb__error_unsupported_profile_entrypoint(driver_data, profile, entrypoint);
    }

    CHECK_VASTATUS();

    configID = object_heap_allocate(&driver_data->config_heap);
    obj_config = CONFIG(configID);
    CHECK_ALLOCATION(obj_config);

    MEMSET_OBJECT(obj_config, struct object_config_s);

    obj_config->profile = profile;
    obj_config->format_vtable = format_vtable;
    obj_config->entrypoint = entrypoint;
    obj_config->attrib_list[0].type = VAConfigAttribRTFormat;
    obj_config->attrib_list[0].value = VA_RT_FORMAT_YUV420;
    obj_config->attrib_count = 1;

    for (i = 0; i < num_attribs; i++) {
        if (attrib_list[i].type > VAConfigAttribTypeMax) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to unsupported ATTR\n", __func__);
            DEBUG_FUNC_EXIT
            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
        }

        vaStatus = psb__update_attribute(obj_config, &(attrib_list[i]));
        if (VA_STATUS_SUCCESS != vaStatus) {
            break;
        }
    }

    if (VA_STATUS_SUCCESS == vaStatus) {
        vaStatus = psb__validate_config(obj_config);
    }

    if (VA_STATUS_SUCCESS == vaStatus) {
        vaStatus = format_vtable->validateConfig(obj_config);
    }

    /* Error recovery */
    if (VA_STATUS_SUCCESS != vaStatus) {
        object_heap_free(&driver_data->config_heap, (object_base_p) obj_config);
    } else {
        *config_id = configID;
    }

#ifdef PSBVIDEO_MSVDX_EC
    if (IS_BAYTRAIL(driver_data))
     driver_data->ec_enabled = 0;

    if (profile == VAProfileVP8Version0_3)
	driver_data->ec_enabled = 1;

#else
    driver_data->ec_enabled = 0;
#endif
    /* TODO: enable it after enough test */
    if (IS_BAYTRAIL(driver_data))
        driver_data->ec_enabled = 0;

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_DestroyConfig(
    VADriverContextP ctx,
    VAConfigID config_id
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_config_p obj_config;

    obj_config = CONFIG(config_id);
    CHECK_CONFIG(obj_config);

    object_heap_free(&driver_data->config_heap, (object_base_p) obj_config);
    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_QueryConfigAttributes(
    VADriverContextP ctx,
    VAConfigID config_id,
    VAProfile *profile,        /* out */
    VAEntrypoint *entrypoint,     /* out */
    VAConfigAttrib *attrib_list,    /* out */
    int *num_attribs        /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_config_p obj_config;
    int i;

    CHECK_INVALID_PARAM(profile == NULL);
    CHECK_INVALID_PARAM(entrypoint == NULL);
    CHECK_INVALID_PARAM(attrib_list == NULL);
    CHECK_INVALID_PARAM(num_attribs == NULL);

    obj_config = CONFIG(config_id);
    CHECK_CONFIG(obj_config);

    *profile = obj_config->profile;
    *entrypoint = obj_config->entrypoint;
    *num_attribs =  obj_config->attrib_count;
    for (i = 0; i < obj_config->attrib_count; i++) {
        attrib_list[i] = obj_config->attrib_list[i];
    }

    DEBUG_FUNC_EXIT
    return vaStatus;
}

void psb__destroy_surface(psb_driver_data_p driver_data, object_surface_p obj_surface)
{
    if (NULL != obj_surface) {
	obj_surface->is_ref_surface = 0;

        /* fixme: this is to work-around the case that destroying hw context
         * with pending cmds, which might lead to incorrect driver state */
	psb_surface_sync(obj_surface->psb_surface);
        psb_surface_destroy(obj_surface->psb_surface);


        free(obj_surface->psb_surface);
        object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
    }
}

VAStatus psb__checkSurfaceDimensions(psb_driver_data_p driver_data, int width, int height)
{
    if ((width <= 0) || (width > 4096) || (height <= 0) || (height > 4096)) {
        return VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus psb_GetSurfaceAttributes(
    VADriverContextP ctx,
        VAConfigID config,
        VASurfaceAttrib *attrib_list,
        unsigned int num_attribs
        )
{
    DEBUG_FUNC_ENTER
    unsigned int i;
    VAStatus vaStatus;
    CHECK_INVALID_PARAM(attrib_list == NULL);
    CHECK_INVALID_PARAM(num_attribs <= 0);

    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VASurfaceAttribPixelFormat:
            attrib_list[i].value.type = VAGenericValueTypeInteger;
            attrib_list[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
            attrib_list[i].value.value.i = VA_FOURCC_NV12;
            break;
        case VASurfaceAttribMinWidth:
        case VASurfaceAttribMinHeight:
            attrib_list[i].flags = VA_SURFACE_ATTRIB_NOT_SUPPORTED;
            break;
        case VASurfaceAttribMaxWidth:
        case VASurfaceAttribMaxHeight:
            attrib_list[i].value.type = VAGenericValueTypeInteger;
            attrib_list[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
            attrib_list[i].value.value.i = 4096;
            break;
        case VASurfaceAttribMemoryType:
            attrib_list[i].flags = VA_SURFACE_ATTRIB_SETTABLE | VA_SURFACE_ATTRIB_GETTABLE;
            attrib_list[i].value.type = VAGenericValueTypeInteger;
            attrib_list[i].value.value.i =
                VA_SURFACE_ATTRIB_MEM_TYPE_VA |
                VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
            break;
        case VASurfaceAttribExternalBufferDescriptor:
            attrib_list[i].flags = VA_SURFACE_ATTRIB_SETTABLE;
            attrib_list[i].value.type = VAGenericValueTypePointer;
            break;
        default:
            attrib_list[i].flags = VA_SURFACE_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }

    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;

}

VAStatus psb_CreateSurfaces(
        VADriverContextP ctx,
        int width,
        int height,
        int format,
        int num_surfaces,
        VASurfaceID *surface_list        /* out */
)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus psb_CreateSurfaces2(
    VADriverContextP ctx,
    unsigned int format,
    unsigned int width,
    unsigned int height,
    VASurfaceID *surface_list,        /* out */
    unsigned int num_surfaces,
    VASurfaceAttrib *attrib_list,
    unsigned int num_attribs
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    unsigned int i, height_origin;
    driver_data->is_protected = (VA_RT_FORMAT_PROTECTED & format);
    unsigned long fourcc = VA_FOURCC_NV12;
    unsigned int flags = 0;
    enum {
        IPVR_MEM_TYPE_NATIVE,
        IPVR_MEM_TYPE_DRM_PRIME,
        IPVR_MEM_TYPE_MAX,
    } memory_type = IPVR_MEM_TYPE_NATIVE;
    VASurfaceAttribExternalBuffers  *pExternalBufDesc = NULL;

    CHECK_INVALID_PARAM(num_surfaces <= 0);
    CHECK_SURFACE(surface_list);

    for (i = 0; i < num_attribs && attrib_list; i++) {
        if ((attrib_list[i].type == VASurfaceAttribExternalBufferDescriptor) &&
            (attrib_list[i].flags == VA_SURFACE_ATTRIB_SETTABLE)) {
            CHECK_INVALID_PARAM(attrib_list[i].value.type != VAGenericValueTypePointer);
            pExternalBufDesc = (VASurfaceAttribExternalBuffers *)attrib_list[i].value.value.p;
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s::%d: extBufDesc=%p.\n",
                __func__, __LINE__, pExternalBufDesc);
        }
        if ((attrib_list[i].type == VASurfaceAttribMemoryType) &&
            (attrib_list[i].flags & VA_SURFACE_ATTRIB_SETTABLE)) {
            CHECK_INVALID_PARAM(attrib_list[i].value.type != VAGenericValueTypeInteger);
            switch (attrib_list[i].value.value.i) {
            case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME:
                memory_type = IPVR_MEM_TYPE_DRM_PRIME;
                break;
            case VA_SURFACE_ATTRIB_MEM_TYPE_VA:
                memory_type = IPVR_MEM_TYPE_NATIVE;
                break;
            default:
                drv_debug_msg(VIDEO_DEBUG_ERROR, "Unsupported memory type: %d.\n",
                    attrib_list[i].value.value.i);
                DEBUG_FUNC_EXIT
                return VA_STATUS_ERROR_INVALID_PARAMETER;
            }
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s::%d: memtype=%d.\n",
                __func__, __LINE__, memory_type);
        }
        if ((attrib_list[i].type == VASurfaceAttribPixelFormat) &&
            (attrib_list[i].flags & VA_SURFACE_ATTRIB_SETTABLE)) {
            CHECK_INVALID_PARAM(attrib_list[i].value.type != VAGenericValueTypeInteger);
            fourcc = attrib_list[i].value.value.i;
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s::%d: fourcc=0x%08x.\n",
                __func__, __LINE__, fourcc);
        }
    }

    format = format & (~VA_RT_FORMAT_PROTECTED);

    /* We only support one format */
    if (VA_RT_FORMAT_YUV420 != format) {
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
        DEBUG_FAILURE;
        return vaStatus;
    }

    vaStatus = psb__checkSurfaceDimensions(driver_data, width, height);
    CHECK_VASTATUS();

    if (memory_type == IPVR_MEM_TYPE_DRM_PRIME) {
        CHECK_INVALID_PARAM(pExternalBufDesc == NULL);
        for (i = 0; i < num_surfaces; i++) {
            int surfaceID;
            object_surface_p obj_surface;
	     psb_surface_p psb_surface;

            drv_debug_msg(VIDEO_DEBUG_INIT, "%s :[%d] pExternalBufDesc=%p.\n",
                               __func__, i, pExternalBufDesc);

            surfaceID = object_heap_allocate(&driver_data->surface_heap);
            obj_surface = SURFACE(surfaceID);
            if (NULL == obj_surface) {
                vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
                DEBUG_FAILURE;
                break;
            }
            MEMSET_OBJECT(obj_surface, struct object_surface_s);

            obj_surface->surface_id = surfaceID;
            surface_list[i] = surfaceID;
            obj_surface->context_id = -1;
            obj_surface->width = pExternalBufDesc->width;
            obj_surface->height = pExternalBufDesc->height;

            psb_surface = (psb_surface_p) calloc(1, sizeof(struct psb_surface_s));
            if (NULL == psb_surface) {
                object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
                obj_surface->surface_id = VA_INVALID_SURFACE;
                vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
                DEBUG_FAILURE;
                break;
            }

            fourcc = VA_FOURCC_NV12;
            drv_debug_msg(VIDEO_DEBUG_INIT, "%s :[%d] buffers[i]=%d.\n",
                               __func__, i, (intptr_t)(pExternalBufDesc->buffers[i]));

            flags |= driver_data->is_protected ? IS_PROTECTED : 0;
            vaStatus = psb_surface_create_from_prime(driver_data, width, height,
                            fourcc, 0, pExternalBufDesc->pitches, pExternalBufDesc->offsets,
                            pExternalBufDesc->data_size,
                            psb_surface, (intptr_t)(pExternalBufDesc->buffers[i]), flags);
            drv_debug_msg(VIDEO_DEBUG_INIT, "%s :psb_surface_create_from_prime returns %d.\n",
                               __func__, vaStatus);

            if (VA_STATUS_SUCCESS != vaStatus) {
                free(psb_surface);
                object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
                obj_surface->surface_id = VA_INVALID_SURFACE;
                DEBUG_FAILURE;
                break;
            }
            /* by default, surface fourcc is NV12 */
            psb_surface->fourcc = fourcc;
            obj_surface->psb_surface = psb_surface;
        }

        /* Error recovery */
        if (VA_STATUS_SUCCESS != vaStatus) {
            /* surface_list[i-1] was the last successful allocation */
            for (; i--;) {
                object_surface_p obj_surface = SURFACE(surface_list[i]);
                psb__destroy_surface(driver_data, obj_surface);
                surface_list[i] = VA_INVALID_SURFACE;
            }
            drv_debug_msg(VIDEO_DEBUG_ERROR, "CreateSurfaces failed\n");
            DEBUG_FUNC_EXIT
            return vaStatus;
        }
    }
    else {
    /* Adjust height to be a multiple of 32 (height of macroblock in interlaced mode) */
    height_origin = height;
    height = (height + 0x1f) & ~0x1f;

    
    for (i = 0; i < num_surfaces; i++) {
        int surfaceID;
        object_surface_p obj_surface;
        psb_surface_p psb_surface;

        surfaceID = object_heap_allocate(&driver_data->surface_heap);
        obj_surface = SURFACE(surfaceID);
        if (NULL == obj_surface) {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            DEBUG_FAILURE;
            break;
        }
        MEMSET_OBJECT(obj_surface, struct object_surface_s);

        obj_surface->surface_id = surfaceID;
        surface_list[i] = surfaceID;
        obj_surface->context_id = -1;
        obj_surface->width = width;
        obj_surface->height = height;
        obj_surface->height_origin = height_origin;

        psb_surface = (psb_surface_p) calloc(1, sizeof(struct psb_surface_s));
        if (NULL == psb_surface) {
            object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
            obj_surface->surface_id = VA_INVALID_SURFACE;
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            DEBUG_FAILURE;
            break;
        }

            fourcc = VA_FOURCC_NV12;

        flags |= driver_data->is_protected ? IS_PROTECTED : 0;
        vaStatus = psb_surface_create(driver_data, width, height, fourcc,
                                      flags, psb_surface);
            drv_debug_msg(VIDEO_DEBUG_INIT, "%s :ipvr_surface_create returns %d.\n",
                               __FUNCTION__, vaStatus);

        if (VA_STATUS_SUCCESS != vaStatus) {
            free(psb_surface);
            object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
            obj_surface->surface_id = VA_INVALID_SURFACE;
            DEBUG_FAILURE;
            break;
        }
        /* by default, surface fourcc is NV12 */
            psb_surface->fourcc = fourcc;
        obj_surface->psb_surface = psb_surface;
    }

    /* Error recovery */
    if (VA_STATUS_SUCCESS != vaStatus) {
        /* surface_list[i-1] was the last successful allocation */
        for (; i--;) {
            object_surface_p obj_surface = SURFACE(surface_list[i]);
            psb__destroy_surface(driver_data, obj_surface);
            surface_list[i] = VA_INVALID_SURFACE;
        }
        drv_debug_msg(VIDEO_DEBUG_ERROR, "CreateSurfaces failed\n");
        return vaStatus;
    }
    DEBUG_FUNC_EXIT
        return vaStatus;
    }
    return vaStatus;
}

VAStatus psb_DestroySurfaces(
    VADriverContextP ctx,
    VASurfaceID *surface_list,
    int num_surfaces
)
{
    INIT_DRIVER_DATA
    int i;
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    if (num_surfaces <= 0) {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    CHECK_SURFACE(surface_list);

    /* Make validation happy */
    for (i = 0; i < num_surfaces; i++) {
        object_surface_p obj_surface = SURFACE(surface_list[i]);
        if (obj_surface == NULL) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to NULL surface\n", __func__);
            return VA_STATUS_ERROR_INVALID_SURFACE;
        }
        if (obj_surface->derived_imgcnt > 0) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "Some surface is deriving by images\n");
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }

    for (i = 0; i < num_surfaces; i++) {
        object_surface_p obj_surface = SURFACE(surface_list[i]);
        if (obj_surface == NULL) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to NULL surface\n", __func__);
            return VA_STATUS_ERROR_INVALID_SURFACE;
        }
        drv_debug_msg(VIDEO_DEBUG_INIT, "%s : obj_surface->surface_id = 0x%x\n",__FUNCTION__, obj_surface->surface_id);
        psb__destroy_surface(driver_data, obj_surface);
        surface_list[i] = VA_INVALID_SURFACE;
    }

    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

int psb_new_context(psb_driver_data_p driver_data, int ctx_type)
{
    /* do nothing
    * this ioctl is for ttm, and now ipvr used instead
    */
    return 0;
}

#ifdef PSBVIDEO_MSVDX_DEC_TILING
int psb_update_context(psb_driver_data_p driver_data, unsigned long ctx_type)
{
    /* do nothing
    * this ioctl is for ttm, and now ipvr used instead
    */
    return 0;
}
#endif

int psb_rm_context(psb_driver_data_p driver_data)
{
    /* do nothing
    * this ioctl is for ttm, and now ipvr used instead
    */
    return 0;
}

#ifdef PSBVIDEO_MSVDX_DEC_TILING
unsigned long psb__tile_stride_log2_256(int w)
{
    int stride_mode = 0;

    if (512 >= w)
        stride_mode = 1;
    else if (1024 >= w)
        stride_mode = 2;
    else if (2048 >= w)
        stride_mode = 3;
    else if (4096 >= w)
        stride_mode = 4;

    return stride_mode;
}

unsigned long psb__tile_stride_log2_512(int w)
{
    int stride_mode = 0;

    if (1024 >= w)
        stride_mode = 0;
    else if (2048 >= w)
        stride_mode = 1;
    else if (4096 >= w)
        stride_mode = 2;

    return stride_mode;
}
#endif

VAStatus psb_CreateContext(
    VADriverContextP ctx,
    VAConfigID config_id,
    int picture_width,
    int picture_height,
    int flag,
    VASurfaceID *render_targets,
    int num_render_targets,
    VAContextID *context        /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_config_p obj_config;
    int i;
    uint32_t tiling_scheme;

    if (IS_BAYTRAIL(driver_data))
        tiling_scheme = 1;
    else
        tiling_scheme = 0;
    drv_debug_msg(VIDEO_DEBUG_INIT, "CreateContext config_id:%d, pic_w:%d, pic_h:%d, flag:%d, num_render_targets:%d.\n",
        config_id, picture_width, picture_height, flag, num_render_targets);

    CHECK_INVALID_PARAM(num_render_targets <= 0);

    CHECK_SURFACE(render_targets);
    CHECK_CONTEXT(context);

    vaStatus = psb__checkSurfaceDimensions(driver_data, picture_width, picture_height);
    CHECK_VASTATUS();

    obj_config = CONFIG(config_id);
    CHECK_CONFIG(obj_config);


    int contextID = object_heap_allocate(&driver_data->context_heap);
    object_context_p obj_context = CONTEXT(contextID);
    CHECK_ALLOCATION(obj_context);

    *context = contextID;

    MEMSET_OBJECT(obj_context, struct object_context_s);

    obj_context->driver_data = driver_data;
    obj_context->current_render_target = NULL;
    obj_context->ec_target = NULL;
    obj_context->ec_candidate = NULL;
    obj_context->context_id = contextID;
    obj_context->config_id = config_id;
    obj_context->picture_width = picture_width;
    obj_context->picture_height = picture_height;
    obj_context->num_render_targets = num_render_targets;
    obj_context->video_crop.x = 0;
    obj_context->video_crop.y = 0;
    obj_context->video_crop.width = picture_width;
    obj_context->video_crop.height = picture_height;
    obj_context->msvdx_scaling = 0;
#ifdef SLICE_HEADER_PARSING
    obj_context->msvdx_frame_end = 0;
#endif
    obj_context->scaling_width = 0;
    obj_context->scaling_height = 0;
    obj_context->render_targets = (VASurfaceID *) calloc(1, num_render_targets * sizeof(VASurfaceID));
    if (obj_context->render_targets == NULL) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;

        object_heap_free(&driver_data->context_heap, (object_base_p) obj_context);

        return vaStatus;
    }

    /* allocate buffer points for vaRenderPicture */
    obj_context->num_buffers = 10;
    obj_context->buffer_list = (object_buffer_p *) calloc(1, sizeof(object_buffer_p) * obj_context->num_buffers);
    if (obj_context->buffer_list == NULL) {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        DEBUG_FAILURE;

        free(obj_context->render_targets);
        object_heap_free(&driver_data->context_heap, (object_base_p) obj_context);
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to allocation failure\n", __func__);

        return vaStatus;
    }

    memset(obj_context->buffers_unused, 0, sizeof(obj_context->buffers_unused));
    memset(obj_context->buffers_unused_count, 0, sizeof(obj_context->buffers_unused_count));
    memset(obj_context->buffers_unused_tail, 0, sizeof(obj_context->buffers_unused_tail));
    memset(obj_context->buffers_active, 0, sizeof(obj_context->buffers_active));





    for (i = 0; i < num_render_targets; i++) {
        object_surface_p obj_surface = SURFACE(render_targets[i]);
        psb_surface_p psb_surface;

        if (NULL == obj_surface) {
            vaStatus = VA_STATUS_ERROR_INVALID_SURFACE;
            DEBUG_FAILURE;
            break;
        }

        psb_surface = obj_surface->psb_surface;

        /* Clear format specific surface info */
        obj_context->render_targets[i] = render_targets[i];
        obj_surface->context_id = contextID; /* Claim ownership of surface */

        if (GET_SURFACE_INFO_tiling(psb_surface)) {
            if (IS_BAYTRAIL(driver_data)) {
            obj_context->msvdx_tile = psb__tile_stride_log2_512(obj_surface->width);
    }
            else {
                if (obj_config->entrypoint == VAEntrypointVideoProc
                    && obj_config->profile == VAProfileNone)
                    /* It's for two pass rotation case
                     * Need the source surface width for tile stride setting
                     */
                    obj_context->msvdx_tile = psb__tile_stride_log2_256(obj_context->picture_width);
                else
                    obj_context->msvdx_tile = psb__tile_stride_log2_256(obj_surface->width);
            }
        }
    }

    obj_context->va_flags = flag;
    obj_context->format_vtable = obj_config->format_vtable;
    obj_context->format_data = NULL;

    obj_context->ctx_type = IPVR_CONTEXT_TYPE_VED;

    obj_context->ipvr_ctx = drm_ipvr_gem_context_create(driver_data->bufmgr,
        obj_context->ctx_type,
        obj_context->msvdx_tile, tiling_scheme);

    /* TODO: validate ctx_id */
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s created drm context 0x%08x. VADrvContext %p, driver_data %p\n",
        __func__, obj_context->ipvr_ctx->ctx_id, ctx, driver_data);

    vaStatus = obj_context->format_vtable->createContext(obj_context, obj_config);

    /* Error recovery */
    if (VA_STATUS_SUCCESS != vaStatus) {
        obj_context->context_id = -1;
        obj_context->config_id = -1;
        obj_context->picture_width = 0;
        obj_context->picture_height = 0;
        free(obj_context->render_targets);
        free(obj_context->buffer_list);
        obj_context->num_buffers = 0;
        obj_context->render_targets = NULL;
        obj_context->num_render_targets = 0;
        obj_context->va_flags = 0;
        object_heap_free(&driver_data->context_heap, (object_base_p) obj_context);
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed at %d\n", __func__, __LINE__);
        DEBUG_FUNC_EXIT

        return vaStatus;
    }

    obj_context->frame_count = 0;
    obj_context->slice_count = 0;
    obj_context->profile = obj_config->profile;
    obj_context->entry_point = obj_config->entrypoint;


    DEBUG_FUNC_EXIT
    return vaStatus;
}

static VAStatus psb__allocate_malloc_buffer(object_buffer_p obj_buffer, int size)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    obj_buffer->buffer_data = realloc(obj_buffer->buffer_data, size);
    CHECK_ALLOCATION(obj_buffer->buffer_data);

    return vaStatus;
}

static VAStatus psb__unmap_buffer(object_buffer_p obj_buffer);

static VAStatus psb__allocate_BO_buffer(psb_driver_data_p driver_data, object_context_p obj_context, object_buffer_p obj_buffer, int size, unsigned char *data, VABufferType type)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    ASSERT(NULL == obj_buffer->buffer_data);

    /* TODO: implement it */
    if (type == VAProtectedSliceDataBufferType) {
        if (obj_buffer->ipvr_bo) {
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "RAR: force old RAR buffer destroy and new buffer re-allocation by set size=0\n");
            obj_buffer->alloc_size = 0;
        }
    }

    uint32_t cache_level = IPVR_CACHE_WRITEBACK;
    switch (obj_buffer->type) {
    case VAImageBufferType: /* Xserver side PutSurface, Image/subpicture buffer
        * should be shared between two process
        */
        cache_level = IPVR_CACHE_UNCACHED;
        break;
    default:
        cache_level = IPVR_CACHE_WRITECOMBINE;
        break;
    }

    /**
     * call libdrm_ipvr to allocate from its internal cache or get new pages
     */
    if (!obj_buffer->ipvr_bo) {
        size = (size + 0x7fff) & ~0x7fff;
		if (!obj_context)
        obj_buffer->ipvr_bo = drm_ipvr_gem_bo_alloc(driver_data->bufmgr, NULL,
            buffer_type_to_string(obj_buffer->type), size, 0, cache_level);
		else
        obj_buffer->ipvr_bo = drm_ipvr_gem_bo_alloc(driver_data->bufmgr, obj_context->ipvr_ctx,
            buffer_type_to_string(obj_buffer->type), size, 0, cache_level);
        if (obj_buffer->ipvr_bo) {
            obj_buffer->alloc_size = obj_buffer->ipvr_bo->size;
        }
        else {
            obj_buffer->alloc_size = 0;
            obj_buffer->size = 0;
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
    }

    return vaStatus;
}

static VAStatus psb__map_buffer(object_buffer_p obj_buffer)
{
    int ret;
    if (obj_buffer->ipvr_bo) {
        ret = drm_ipvr_gem_bo_map(obj_buffer->ipvr_bo, 1);
        if (ret) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "Mapping buffer %08x (off 0x%lx) failed: %s (%d)\n",
                obj_buffer->ipvr_bo->handle, obj_buffer->ipvr_bo->offset,
                strerror(ret), ret);
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }
        obj_buffer->buffer_data = obj_buffer->ipvr_bo->virt;
        return VA_STATUS_SUCCESS;
    }
    assert(obj_buffer->buffer_data);
    return VA_STATUS_SUCCESS;
}

static VAStatus psb__unmap_buffer(object_buffer_p obj_buffer)
{
    int ret;
    if (obj_buffer->ipvr_bo) {
        obj_buffer->buffer_data = NULL;
        ret = drm_ipvr_gem_bo_unmap(obj_buffer->ipvr_bo);
        if (ret == 0) {
            return VA_STATUS_SUCCESS;
        }
        else {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "Unmapping buffer %08x (off 0x%lx) failed: %s (%d)\n",
                obj_buffer->ipvr_bo->handle, obj_buffer->ipvr_bo->offset,
                strerror(ret), ret);
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }
    }
    return VA_STATUS_SUCCESS;
}

static void psb__destroy_buffer(psb_driver_data_p driver_data, object_buffer_p obj_buffer)
{
    if (obj_buffer->ipvr_bo) {
        if (obj_buffer->buffer_data) {
            psb__unmap_buffer(obj_buffer);
        }
        drm_ipvr_gem_bo_unreference(obj_buffer->ipvr_bo);
        obj_buffer->ipvr_bo = NULL;
    }

    if (NULL != obj_buffer->buffer_data) {
        free(obj_buffer->buffer_data);
        obj_buffer->buffer_data = NULL;
        obj_buffer->size = 0;
    }

    object_heap_free(&driver_data->buffer_heap, (object_base_p) obj_buffer);
}

void psb__suspend_buffer(psb_driver_data_p driver_data, object_buffer_p obj_buffer)
{

    if (obj_buffer->ipvr_bo) {
        /**
         * unreference the bo
         * libdrm_ipvr will manage the life cycle/LRU of it: free or cache it.
         */
        drm_ipvr_gem_bo_unreference(obj_buffer->ipvr_bo);
        obj_buffer->ipvr_bo = NULL;
    }

    if (obj_buffer->context) {
        VABufferType type = obj_buffer->type;
        object_context_p obj_context = obj_buffer->context;

        /* Remove buffer from active list */
        *obj_buffer->pptr_prev_next = obj_buffer->ptr_next;

        /* Add buffer to tail of unused list */
        obj_buffer->ptr_next = NULL;
        obj_buffer->last_used = obj_context->frame_count;
        if (obj_context->buffers_unused_tail[type]) {
            obj_buffer->pptr_prev_next = &(obj_context->buffers_unused_tail[type]->ptr_next);
        } else {
            obj_buffer->pptr_prev_next = &(obj_context->buffers_unused[type]);
        }
        *obj_buffer->pptr_prev_next = obj_buffer;
        obj_context->buffers_unused_tail[type] = obj_buffer;
        obj_context->buffers_unused_count[type]++;

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Adding buffer %08x type %s to unused list. unused count = %d\n", obj_buffer->base.id,
                                 buffer_type_to_string(obj_buffer->type), obj_context->buffers_unused_count[type]);

        object_heap_suspend_object((object_base_p) obj_buffer, 1); /* suspend */
    }
    else
        psb__destroy_buffer(driver_data, obj_buffer);
}

static void psb__destroy_context(psb_driver_data_p driver_data, object_context_p obj_context)
{
    int i;

    obj_context->format_vtable->destroyContext(obj_context);

    for (i = 0; i < PSB_MAX_BUFFERTYPES; i++) {
        object_buffer_p obj_buffer;
        obj_buffer = obj_context->buffers_active[i];
        for (; obj_buffer; obj_buffer = obj_buffer->ptr_next) {
            drv_debug_msg(VIDEO_DEBUG_INIT, "%s: destroying active buffer %08x\n", __FUNCTION__, obj_buffer->base.id);
            psb__destroy_buffer(driver_data, obj_buffer);
        }
        obj_buffer = obj_context->buffers_unused[i];
        for (; obj_buffer; obj_buffer = obj_buffer->ptr_next) {
            drv_debug_msg(VIDEO_DEBUG_INIT, "%s: destroying unused buffer %08x\n", __FUNCTION__, obj_buffer->base.id);
            psb__destroy_buffer(driver_data, obj_buffer);
        }
        obj_context->buffers_unused_count[i] = 0;
    }

    obj_context->context_id = -1;
    obj_context->config_id = -1;
    obj_context->picture_width = 0;
    obj_context->picture_height = 0;
    if (obj_context->render_targets)
        free(obj_context->render_targets);
    obj_context->render_targets = NULL;
    obj_context->num_render_targets = 0;
    obj_context->va_flags = 0;

    obj_context->current_render_target = NULL;
    obj_context->ec_target = NULL;
    obj_context->ec_candidate = NULL;
    if (obj_context->buffer_list)
        free(obj_context->buffer_list);
    obj_context->num_buffers = 0;

    object_heap_free(&driver_data->context_heap, (object_base_p) obj_context);

    psb_rm_context(driver_data);
    drm_ipvr_gem_context_destroy(obj_context->ipvr_ctx);
    obj_context->ipvr_ctx = NULL;
}

VAStatus psb_DestroyContext(
    VADriverContextP ctx,
    VAContextID context
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_context_p obj_context = CONTEXT(context);
    CHECK_CONTEXT(obj_context);

    psb__destroy_context(driver_data, obj_context);

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb__CreateBuffer(
    psb_driver_data_p driver_data,
    object_context_p obj_context,       /* in */
    VABufferType type,  /* in */
    unsigned int size,          /* in */
    unsigned int num_elements, /* in */
    unsigned char *data,         /* in */
    VABufferID *buf_desc    /* out */
)
{
    DEBUG_FUNC_ENTER
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int bufferID;
    object_buffer_p obj_buffer = obj_context ? obj_context->buffers_unused[type] : NULL;
    int unused_count = obj_context ? obj_context->buffers_unused_count[type] : 0;


    /*
     * Buffer Management
     * For each buffer type, maintain
     *   - a LRU sorted list of unused buffers
     *   - a list of active buffers
     * We only create a new buffer when
     *   - no unused buffers are available
     *   - the last unused buffer is still queued
     *   - the last unused buffer was used very recently and may still be fenced
     *      - used recently is defined as within the current frame_count (subject to tweaks)
     *
     * The buffer that is returned will be moved to the list of active buffers
     *   - vaDestroyBuffer and vaRenderPicture will move the active buffer back to the list of unused buffers
    */
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "Requesting buffer creation, size=%d,elements=%d,type=%s\n", size, num_elements,
                             buffer_type_to_string(type));

    if (obj_buffer) {
        bufferID = obj_buffer->base.id;
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Reusing buffer %08x type %s from unused list. Unused = %d\n", bufferID,
                                 buffer_type_to_string(type), unused_count);

        /* Remove from unused list */
        obj_context->buffers_unused[type] = obj_buffer->ptr_next;
        if (obj_context->buffers_unused[type]) {
            obj_context->buffers_unused[type]->pptr_prev_next = &(obj_context->buffers_unused[type]);
            ASSERT(obj_context->buffers_unused_tail[type] != obj_buffer);
        } else {
            ASSERT(obj_context->buffers_unused_tail[type] == obj_buffer);
            obj_context->buffers_unused_tail[type] = 0;
        }
        obj_context->buffers_unused_count[type]--;

        object_heap_suspend_object((object_base_p)obj_buffer, 0); /* Make BufferID valid again */
        ASSERT(type == obj_buffer->type);
        ASSERT(obj_context == obj_buffer->context);
    } else {
        bufferID = object_heap_allocate(&driver_data->buffer_heap);
        obj_buffer = BUFFER(bufferID);
        CHECK_ALLOCATION(obj_buffer);

        MEMSET_OBJECT(obj_buffer, struct object_buffer_s);

        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Allocating new buffer %08x type %s.\n", bufferID, buffer_type_to_string(type));
        obj_buffer->type = type;
        obj_buffer->buffer_data = NULL;
        obj_buffer->ipvr_bo = NULL;
        obj_buffer->size = 0;
        obj_buffer->max_num_elements = 0;
        obj_buffer->alloc_size = 0;
        obj_buffer->context = obj_context;
    }
    if (obj_context) {
        /* Add to front of active list */
        obj_buffer->ptr_next = obj_context->buffers_active[type];
        if (obj_buffer->ptr_next) {
            obj_buffer->ptr_next->pptr_prev_next = &(obj_buffer->ptr_next);
        }
        obj_buffer->pptr_prev_next = &(obj_context->buffers_active[type]);
        *obj_buffer->pptr_prev_next = obj_buffer;
    }

    switch (obj_buffer->type) {
    case VABitPlaneBufferType:
    case VASliceDataBufferType:
    case VAResidualDataBufferType:
    case VAImageBufferType:
    case VASliceGroupMapBufferType:
    case VAEncCodedBufferType:
    case VAProtectedSliceDataBufferType:
#ifdef SLICE_HEADER_PARSING
    case VAParseSliceHeaderGroupBufferType:
#endif
        vaStatus = psb__allocate_BO_buffer(driver_data, obj_context,obj_buffer, size * num_elements, data, obj_buffer->type);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "succeeded with %p hnd %x offset 0x%lx.\n",
            obj_buffer->ipvr_bo, obj_buffer->ipvr_bo->handle, obj_buffer->ipvr_bo->offset);
        DEBUG_FAILURE;
        break;
    case VAPictureParameterBufferType:
    case VAIQMatrixBufferType:
    case VASliceParameterBufferType:
    case VAMacroblockParameterBufferType:
    case VADeblockingParameterBufferType:
    case VAEncPackedHeaderParameterBufferType:
    case VAEncPackedHeaderDataBufferType:
    case VAEncSequenceParameterBufferType:
    case VAEncPictureParameterBufferType:
    case VAEncSliceParameterBufferType:
    case VAQMatrixBufferType:
    case VAEncMiscParameterBufferType:
    case VAProbabilityBufferType:
    case VAHuffmanTableBufferType:
    case VAProcPipelineParameterBufferType:
    case VAProcFilterParameterBufferType:
#ifdef SLICE_HEADER_PARSING
    case VAParsePictureParameterBufferType:
#endif
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "Allocate new malloc buffers for vaCreateBuffer:type=%s,size=%d, buffer_data=%p.\n",
                                 buffer_type_to_string(type), size, obj_buffer->buffer_data);
        vaStatus = psb__allocate_malloc_buffer(obj_buffer, size * num_elements);
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "succeeded with %p.\n", obj_buffer->buffer_data);
        DEBUG_FAILURE;
        break;

    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;
        DEBUG_FAILURE;
        break;;
    }

    if (VA_STATUS_SUCCESS == vaStatus) {
        obj_buffer->size = size;
        obj_buffer->max_num_elements = num_elements;
        obj_buffer->num_elements = num_elements;
        if (data && (obj_buffer->type != VAProtectedSliceDataBufferType)) {
            if (obj_buffer->ipvr_bo) {
                assert(obj_buffer->ipvr_bo->size >= size);
            }
            vaStatus = psb__map_buffer(obj_buffer);
            if (VA_STATUS_SUCCESS == vaStatus) {
                drv_debug_msg(VIDEO_DEBUG_GENERAL, "memcpy for %ux%u bytes.\n", size, num_elements);
                memcpy(obj_buffer->buffer_data, data, size * num_elements);

                psb__unmap_buffer(obj_buffer);
            }
        }
    }
    if (VA_STATUS_SUCCESS == vaStatus) {
        *buf_desc = bufferID;
        drv_debug_msg(VIDEO_DEBUG_GENERAL, "buffer ID %u.\n", bufferID);
    } else {
        psb__destroy_buffer(driver_data, obj_buffer);
    }

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_CreateBuffer(
    VADriverContextP ctx,
    VAContextID context,        /* in */
    VABufferType type,  /* in */
    unsigned int size,          /* in */
    unsigned int num_elements, /* in */
    void *data,         /* in */
    VABufferID *buf_desc    /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: VADrvContext %p driver_data %p.\n", __func__, ctx, driver_data);

    CHECK_INVALID_PARAM(num_elements <= 0);

    switch (type) {
    case VABitPlaneBufferType:
    case VASliceDataBufferType:
    case VAProtectedSliceDataBufferType:
    case VAResidualDataBufferType:
    case VASliceGroupMapBufferType:
    case VAPictureParameterBufferType:
    case VAIQMatrixBufferType:
    case VASliceParameterBufferType:
    case VAMacroblockParameterBufferType:
    case VADeblockingParameterBufferType:
    case VAEncCodedBufferType:
    case VAEncSequenceParameterBufferType:
    case VAEncPictureParameterBufferType:
    case VAEncSliceParameterBufferType:
    case VAEncPackedHeaderParameterBufferType:
    case VAEncPackedHeaderDataBufferType:
    case VAQMatrixBufferType:
    case VAEncMiscParameterBufferType:
    case VAProbabilityBufferType:
    case VAHuffmanTableBufferType:
    case VAProcPipelineParameterBufferType:
    case VAProcFilterParameterBufferType:
#ifdef SLICE_HEADER_PARSING
    case VAParsePictureParameterBufferType:
    case VAParseSliceHeaderGroupBufferType:
#endif
        break;

    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;
        DEBUG_FAILURE;
        return vaStatus;
    }

    object_context_p obj_context = CONTEXT(context);
    CHECK_CONTEXT(obj_context);
    CHECK_INVALID_PARAM(buf_desc == NULL);

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "%s: before createBuffer VADrvContext %p driver_data %p ctx %p data %p bufDesc %p.\n", __func__, ctx, driver_data, obj_context, data, buf_desc);
    vaStatus = psb__CreateBuffer(driver_data, obj_context, type, size, num_elements, data, buf_desc);

    DEBUG_FUNC_EXIT
    return vaStatus;
}


VAStatus psb_BufferInfo(
    VADriverContextP ctx,
    VABufferID buf_id,  /* in */
    VABufferType *type, /* out */
    unsigned int *size,         /* out */
    unsigned int *num_elements /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    object_buffer_p obj_buffer = BUFFER(buf_id);
    CHECK_BUFFER(obj_buffer);

    *type = obj_buffer->type;
    *size = obj_buffer->size;
    *num_elements = obj_buffer->num_elements;
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}


VAStatus psb_BufferSetNumElements(
    VADriverContextP ctx,
    VABufferID buf_id,    /* in */
    unsigned int num_elements    /* in */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_buffer_p obj_buffer = BUFFER(buf_id);
    CHECK_BUFFER(obj_buffer);

    if ((num_elements <= 0) || (num_elements > obj_buffer->max_num_elements)) {
        vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    if (VA_STATUS_SUCCESS == vaStatus) {
        obj_buffer->num_elements = num_elements;
    }

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_MapBuffer(
    VADriverContextP ctx,
    VABufferID buf_id,    /* in */
    void **pbuf         /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "mapping buffer %u\n", buf_id);
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_buffer_p obj_buffer = BUFFER(buf_id);
    CHECK_BUFFER(obj_buffer);

    CHECK_INVALID_PARAM(pbuf == NULL);

    vaStatus = psb__map_buffer(obj_buffer);
    CHECK_VASTATUS();

    if (NULL != obj_buffer->buffer_data) {
        *pbuf = obj_buffer->buffer_data;

        /* *(IMG_UINT32 *)((unsigned char *)obj_buffer->buffer_data + 4) = 16; */
    } else {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_UnmapBuffer(
    VADriverContextP ctx,
    VABufferID buf_id    /* in */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_buffer_p obj_buffer = BUFFER(buf_id);
    CHECK_BUFFER(obj_buffer);

    vaStatus = psb__unmap_buffer(obj_buffer);
    DEBUG_FUNC_EXIT
    return vaStatus;
}


VAStatus psb_DestroyBuffer(
    VADriverContextP ctx,
    VABufferID buffer_id
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_buffer_p obj_buffer = BUFFER(buffer_id);
    if (NULL == obj_buffer) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to NULL buffer\n", __func__);
        return vaStatus;
    }
    psb__suspend_buffer(driver_data, obj_buffer);
    DEBUG_FUNC_EXIT
    return vaStatus;
}


VAStatus psb_BeginPicture(
    VADriverContextP ctx,
    VAContextID context,
    VASurfaceID render_target
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_context_p obj_context;
    object_surface_p obj_surface;
    object_config_p obj_config;

    obj_context = CONTEXT(context);
    CHECK_CONTEXT(obj_context);
    
    /* Must not be within BeginPicture / EndPicture already */
    ASSERT(obj_context->current_render_target == NULL);

    obj_surface = SURFACE(render_target);
    CHECK_SURFACE(obj_surface);

    obj_context->current_render_surface_id = render_target;
    obj_context->current_render_target = obj_surface;
    obj_context->slice_count = 0;

    obj_config = CONFIG(obj_context->config_id);
    if (obj_config == NULL) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to NULL config\n", __func__);
        return VA_STATUS_ERROR_INVALID_CONFIG;
    }

    vaStatus = obj_context->format_vtable->beginPicture(obj_context);

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "---BeginPicture 0x%08x for frame %d --\n",
                             render_target, obj_context->frame_count);
    psb__trace_message("------Trace frame %d------\n", obj_context->frame_count);

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_RenderPicture(
    VADriverContextP ctx,
    VAContextID context,
    VABufferID *buffers,
    int num_buffers
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_context_p obj_context;
    object_buffer_p *buffer_list;
    int i;

    obj_context = CONTEXT(context);
    CHECK_CONTEXT(obj_context);

    CHECK_INVALID_PARAM(num_buffers <= 0);
    /* Don't crash on NULL pointers */
    CHECK_BUFFER(buffers);
    /* Must be within BeginPicture / EndPicture */
    ASSERT(obj_context->current_render_target != NULL);

    if (num_buffers > obj_context->num_buffers) {
        free(obj_context->buffer_list);

        obj_context->buffer_list = (object_buffer_p *) calloc(1, sizeof(object_buffer_p) * num_buffers);
        if (obj_context->buffer_list == NULL) {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            obj_context->num_buffers = 0;
        }

        obj_context->num_buffers = num_buffers;
    }
    buffer_list = obj_context->buffer_list;

    if (VA_STATUS_SUCCESS == vaStatus) {
        /* Lookup buffer references */
        for (i = 0; i < num_buffers; i++) {
            object_buffer_p obj_buffer = BUFFER(buffers[i]);
            CHECK_BUFFER(obj_buffer);

            buffer_list[i] = obj_buffer;
            drv_debug_msg(VIDEO_DEBUG_GENERAL, "Render buffer %08x type %s\n", obj_buffer->base.id,
                                     buffer_type_to_string(obj_buffer->type));
        }
    }

    if (VA_STATUS_SUCCESS == vaStatus) {
        vaStatus = obj_context->format_vtable->renderPicture(obj_context, buffer_list, num_buffers);
    }

    if (buffer_list) {
        /* Release buffers */
        for (i = 0; i < num_buffers; i++) {
            if (buffer_list[i]) {
                psb__suspend_buffer(driver_data, buffer_list[i]);
            }
        }
    }

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_EndPicture(
    VADriverContextP ctx,
    VAContextID context
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus;
    object_context_p obj_context;

    obj_context = CONTEXT(context);
    CHECK_CONTEXT(obj_context);

    vaStatus = obj_context->format_vtable->endPicture(obj_context);

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "---EndPicture for frame %d --\n", obj_context->frame_count);

    obj_context->current_render_target = NULL;
    obj_context->frame_count++;

    psb__trace_message("FrameCount = %03d\n", obj_context->frame_count);
    drv_debug_msg(VIDEO_DEBUG_GENERAL, "FrameCount = %03d\n", obj_context->frame_count);
    psb__trace_message(NULL);


    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_SyncSurface(
    VADriverContextP ctx,
    VASurfaceID render_target
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_surface_p obj_surface;

    drv_debug_msg(VIDEO_DEBUG_GENERAL, "psb_SyncSurface: 0x%08x\n", render_target);

    obj_surface = SURFACE(render_target);
    CHECK_SURFACE(obj_surface);

    vaStatus = psb_surface_sync(obj_surface->psb_surface);

    DEBUG_FAILURE;
    DEBUG_FUNC_EXIT
    return vaStatus;
}


VAStatus psb_QuerySurfaceStatus(
    VADriverContextP ctx,
    VASurfaceID render_target,
    VASurfaceStatus *status    /* out */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_surface_p obj_surface;
    VASurfaceStatus surface_status;

    obj_surface = SURFACE(render_target);
    CHECK_SURFACE(obj_surface);

    CHECK_INVALID_PARAM(status == NULL);
    vaStatus = psb_surface_query_status(obj_surface->psb_surface, &surface_status);

    *status = surface_status;
    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_QuerySurfaceError(
    VADriverContextP ctx,
    VASurfaceID render_target,
    VAStatus error_status,
    void **error_info /*out*/
)
{
    /* do nothing
    * this ioctl is for ttm, and now ipvr used instead
    */
    return VA_STATUS_SUCCESS;
}

#define PSB_MAX_SURFACE_ATTRIBUTES 16

VAStatus psb_QuerySurfaceAttributes(VADriverContextP ctx,
                            VAConfigID config,
                            VASurfaceAttrib *attrib_list,
                            unsigned int *num_attribs)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA

    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_config_p obj_config;
    unsigned int i = 0;

    CHECK_INVALID_PARAM(num_attribs == NULL);

    if (attrib_list == NULL) {
        *num_attribs = PSB_MAX_SURFACE_ATTRIBUTES;
        return VA_STATUS_SUCCESS;
    }

    obj_config = CONFIG(config);
    CHECK_CONFIG(obj_config);

    VASurfaceAttrib *attribs = NULL;
    attribs = malloc(PSB_MAX_SURFACE_ATTRIBUTES *sizeof(VASurfaceAttrib));
    if (attribs == NULL)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    attribs[i].type = VASurfaceAttribPixelFormat;
    attribs[i].value.type = VAGenericValueTypeInteger;
    attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
    attribs[i].value.value.i = VA_FOURCC('N', 'V', '1', '2');
    i++;

    attribs[i].type = VASurfaceAttribMemoryType;
    attribs[i].value.type = VAGenericValueTypeInteger;
    attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
    if (obj_config->entrypoint == VAEntrypointEncSlice && obj_config->profile == VAProfileVP8Version0_3) {
        attribs[i].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA
            | VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM
#ifdef ANDROID
            | VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC
            | VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_ION
#endif
            ;
    } else {
        attribs[i].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA
            | VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM
            | VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR
#ifdef ANDROID
            | VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC
            | VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_ION
#endif
            ;
    }
    i++;

    attribs[i].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[i].value.type = VAGenericValueTypePointer;
    attribs[i].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[i].value.value.p = NULL;
    i++;

    //modules have speical formats to support
    if (obj_config->entrypoint == VAEntrypointVLD) { /* decode */
    } else if (obj_config->entrypoint == VAEntrypointEncSlice ||  /* encode */
                   obj_config->entrypoint == VAEntrypointEncPicture) {
    }
    else if (obj_config->entrypoint == VAEntrypointVideoProc) { /* vpp */

    }

    if (i > *num_attribs) {
        *num_attribs = i;
        drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: failed due to num_attribs\n", __func__);
        free(attribs);
        return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
    }

    *num_attribs = i;
    memcpy(attrib_list, attribs, i * sizeof(*attribs));
    free(attribs);

    DEBUG_FUNC_EXIT
    return vaStatus;
}

VAStatus psb_LockSurface(
    VADriverContextP ctx,
    VASurfaceID surface,
    unsigned int *fourcc, /* following are output argument */
    unsigned int *luma_stride,
    unsigned int *chroma_u_stride,
    unsigned int *chroma_v_stride,
    unsigned int *luma_offset,
    unsigned int *chroma_u_offset,
    unsigned int *chroma_v_offset,
    unsigned int *buffer_name,
    void **buffer
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    unsigned char *surface_data;

    object_surface_p obj_surface = SURFACE(surface);
    psb_surface_p psb_surface;
    CHECK_SURFACE(obj_surface);

    psb_surface = obj_surface->psb_surface;
    if (buffer_name)
	 drm_ipvr_gem_bo_flink(psb_surface->buf, buffer_name);

    if (buffer) { /* map the surface buffer */
        if (drm_ipvr_gem_bo_map(psb_surface->buf, 1)) {

            *buffer = NULL;
            vaStatus = VA_STATUS_ERROR_UNKNOWN;
            DEBUG_FAILURE;
            drv_debug_msg(VIDEO_DEBUG_ERROR, "%s: BO mapping failed\n", __func__);
            return vaStatus;
        }
	 surface_data = psb_surface->buf->virt;
        *buffer = surface_data;
    }

    *fourcc = VA_FOURCC_NV12;
    *luma_stride = psb_surface->stride;
    *chroma_u_stride = psb_surface->stride;
    *chroma_v_stride = psb_surface->stride;
    *luma_offset = 0;
    *chroma_u_offset = obj_surface->height * psb_surface->stride;
    *chroma_v_offset = obj_surface->height * psb_surface->stride + 1;
    DEBUG_FUNC_EXIT
    return vaStatus;
}


VAStatus psb_UnlockSurface(
    VADriverContextP ctx,
    VASurfaceID surface
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    VAStatus vaStatus = VA_STATUS_SUCCESS;

    object_surface_p obj_surface = SURFACE(surface);
    CHECK_SURFACE(obj_surface);

    psb_surface_p psb_surface = obj_surface->psb_surface;

    drm_ipvr_gem_bo_unmap(psb_surface->buf);
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

VAStatus psb_PutSurfaceBuf(
    VADriverContextP ctx,
    VASurfaceID surface,
    unsigned char* data,
    int* data_len,
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    VARectangle *cliprects, /* client supplied clip list */
    unsigned int number_cliprects, /* number of clip rects in the clip list */
    unsigned int flags /* de-interlacing flags */
)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA;
    object_surface_p obj_surface = SURFACE(surface);

    obj_surface = SURFACE(surface);
    if (obj_surface == NULL)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

VAStatus psb_SetTimestampForSurface(
    VADriverContextP ctx,
    VASurfaceID surface,
    long long timestamp
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_surface_p obj_surface = SURFACE(surface);

    obj_surface = SURFACE(surface);
    CHECK_SURFACE(obj_surface);

    if (obj_surface->share_info) {
        obj_surface->share_info->timestamp = timestamp;
        return VA_STATUS_SUCCESS;
    } else {
        return VA_STATUS_ERROR_UNKNOWN;
    }
}

VAStatus psb_SetVideoCrops(
    VADriverContextP ctx,
    VAContextID context,
    VARectangle *crop
)
{
    INIT_DRIVER_DATA;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    object_context_p obj_context = CONTEXT(context);

    CHECK_CONTEXT(obj_context);

    if (!crop)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (crop->x < 0 || crop->y < 0 || crop->width > 4096 || crop->height > 4096)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    memcpy(&obj_context->video_crop, crop, sizeof(*crop));

    return vaStatus;
}


int  LOCK_HARDWARE(psb_driver_data_p driver_data)
{
    char ret = 0;

    if (driver_data->dri2 || driver_data->dri_dummy)
        return 0;

    pthread_mutex_lock(&driver_data->drm_mutex);
    DRM_CAS(driver_data->drm_lock, driver_data->drm_context,
            (DRM_LOCK_HELD | driver_data->drm_context), ret);
    if (ret) {
        ret = drmGetLock(driver_data->drm_fd, driver_data->drm_context, 0);
        /* driver_data->contended_lock=1; */
    }

    return ret;
}

int UNLOCK_HARDWARE(psb_driver_data_p driver_data)
{
    /* driver_data->contended_lock=0; */
    if (driver_data->dri2 || driver_data->dri_dummy)
        return 0;

    DRM_UNLOCK(driver_data->drm_fd, driver_data->drm_lock, driver_data->drm_context);
    pthread_mutex_unlock(&driver_data->drm_mutex);

    return 0;
}

static void psb__deinitDRM(VADriverContextP ctx)
{
    INIT_DRIVER_DATA

    drm_ipvr_gem_bufmgr_destroy(driver_data->bufmgr);
    driver_data->bufmgr = NULL;
    driver_data->drm_fd = -1;
}


static VAStatus psb__initDRI(VADriverContextP ctx)
{
    INIT_DRIVER_DATA
    struct drm_state *drm_state = (struct drm_state *)ctx->drm_state;

#ifdef BAYTRAIL
    if (drm_state->fd >= 0)
        close(drm_state->fd);
    drm_state->fd = open("/dev/dri/card1", O_RDWR);
    if (drm_state->fd < 0) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to open device node /dev/dri/card1\n");
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    }
#endif

    driver_data->drm_fd = drm_state->fd;
    driver_data->dri_dummy = 1;
    driver_data->dri2 = 0;
    driver_data->bus_id = NULL;

    return VA_STATUS_SUCCESS;
}

static VAStatus psb__initDRM(VADriverContextP ctx)
{
    INIT_DRIVER_DATA
    VAStatus vaStatus;

    vaStatus = psb__initDRI(ctx);

       if (vaStatus == VA_STATUS_SUCCESS) {
        driver_data->bufmgr = drm_ipvr_gem_bufmgr_init(driver_data->drm_fd);
        if (driver_data->bufmgr == NULL) {
            drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to get GEM pool\n");
            return VA_STATUS_ERROR_UNKNOWN;
        }
        return VA_STATUS_SUCCESS;
    }
    else
        return vaStatus;
}

VAStatus psb_Terminate(VADriverContextP ctx)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    object_subpic_p obj_subpic;
    object_image_p obj_image;
    object_buffer_p obj_buffer;
    object_surface_p obj_surface;
    object_context_p obj_context;
    object_config_p obj_config;
    object_heap_iterator iter;

    drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: begin to tear down\n");

    /* Clean up left over contexts */
    obj_context = (object_context_p) object_heap_first(&driver_data->context_heap, &iter);
    while (obj_context) {
        drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: contextID %08x still allocated, destroying\n", obj_context->base.id);
        psb__destroy_context(driver_data, obj_context);
        obj_context = (object_context_p) object_heap_next(&driver_data->context_heap, &iter);
    }
    object_heap_destroy(&driver_data->context_heap);

    /* Clean up SubpicIDs */
    obj_subpic = (object_subpic_p) object_heap_first(&driver_data->subpic_heap, &iter);
    while (obj_subpic) {
        drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: subpictureID %08x still allocated, destroying\n", obj_subpic->base.id);
        psb__destroy_subpicture(driver_data, obj_subpic);
        obj_subpic = (object_subpic_p) object_heap_next(&driver_data->subpic_heap, &iter);
    }
    object_heap_destroy(&driver_data->subpic_heap);

    /* Clean up ImageIDs */
    obj_image = (object_image_p) object_heap_first(&driver_data->image_heap, &iter);
    while (obj_image) {
        drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: imageID %08x still allocated, destroying\n", obj_image->base.id);
        psb__destroy_image(driver_data, obj_image);
        obj_image = (object_image_p) object_heap_next(&driver_data->image_heap, &iter);
    }
    object_heap_destroy(&driver_data->image_heap);

    /* Clean up left over buffers */
    obj_buffer = (object_buffer_p) object_heap_first(&driver_data->buffer_heap, &iter);
    while (obj_buffer) {
        drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: bufferID %08x still allocated, destroying\n", obj_buffer->base.id);
        psb__destroy_buffer(driver_data, obj_buffer);
        obj_buffer = (object_buffer_p) object_heap_next(&driver_data->buffer_heap, &iter);
    }
    object_heap_destroy(&driver_data->buffer_heap);

    /* Clean up left over surfaces */
    obj_surface = (object_surface_p) object_heap_first(&driver_data->surface_heap, &iter);
    while (obj_surface) {
        drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: surfaceID %08x still allocated, destroying\n", obj_surface->base.id);
        psb__destroy_surface(driver_data, obj_surface);
        obj_surface = (object_surface_p) object_heap_next(&driver_data->surface_heap, &iter);
    }
    object_heap_destroy(&driver_data->surface_heap);

    /* Clean up configIDs */
    obj_config = (object_config_p) object_heap_first(&driver_data->config_heap, &iter);
    while (obj_config) {
        object_heap_free(&driver_data->config_heap, (object_base_p) obj_config);
        obj_config = (object_config_p) object_heap_next(&driver_data->config_heap, &iter);
    }
    object_heap_destroy(&driver_data->config_heap);

    drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: de-initialized DRM\n");

    psb__deinitDRM(ctx);

    pthread_mutex_destroy(&driver_data->drm_mutex);
    free(ctx->pDriverData);
    free(ctx->vtable_egl);
    free(ctx->vtable_tpi);

    ctx->pDriverData = NULL;
    drv_debug_msg(VIDEO_DEBUG_INIT, "vaTerminate: goodbye\n\n");

    psb__close_log();
    DEBUG_FUNC_EXIT
    return VA_STATUS_SUCCESS;
}

static VAStatus
psb_AcquireBufferHandle(VADriverContextP ctx, VABufferID buf_id,
    VABufferInfo *buf_info);

static VAStatus
psb_ReleaseBufferHandle(VADriverContextP ctx, VABufferID buf_id);

EXPORT VAStatus __vaDriverInit_0_37(VADriverContextP ctx)
{
    psb_driver_data_p driver_data;
    VAStatus va_status = VA_STATUS_SUCCESS;
    int result;
    if (psb_video_trace_fp) {
        /* make gdb always stop here */
        signal(SIGUSR1, SIG_IGN);
        kill(getpid(), SIGUSR1);
    }

    psb__open_log();

    drv_debug_msg(VIDEO_DEBUG_INIT, "vaInitilize: start the journey\n");

    ctx->version_major = 0;
    ctx->version_minor = 37;

    ctx->max_profiles = PSB_MAX_PROFILES;
    ctx->max_entrypoints = PSB_MAX_ENTRYPOINTS;
    ctx->max_attributes = PSB_MAX_CONFIG_ATTRIBUTES;
    ctx->max_image_formats = PSB_MAX_IMAGE_FORMATS;
    ctx->max_subpic_formats = PSB_MAX_SUBPIC_FORMATS;
    ctx->max_display_attributes = PSB_MAX_DISPLAY_ATTRIBUTES;

    ctx->vtable->vaTerminate = psb_Terminate;
    ctx->vtable->vaQueryConfigEntrypoints = psb_QueryConfigEntrypoints;
    ctx->vtable->vaTerminate = psb_Terminate;
    ctx->vtable->vaQueryConfigProfiles = psb_QueryConfigProfiles;
    ctx->vtable->vaQueryConfigEntrypoints = psb_QueryConfigEntrypoints;
    ctx->vtable->vaQueryConfigAttributes = psb_QueryConfigAttributes;
    ctx->vtable->vaCreateConfig = psb_CreateConfig;
    ctx->vtable->vaDestroyConfig = psb_DestroyConfig;
    ctx->vtable->vaGetConfigAttributes = psb_GetConfigAttributes;
    ctx->vtable->vaCreateSurfaces2 = psb_CreateSurfaces2; 
    ctx->vtable->vaCreateSurfaces = psb_CreateSurfaces;
    ctx->vtable->vaGetSurfaceAttributes = psb_GetSurfaceAttributes;
    ctx->vtable->vaDestroySurfaces = psb_DestroySurfaces;
    ctx->vtable->vaCreateContext = psb_CreateContext;
    ctx->vtable->vaDestroyContext = psb_DestroyContext;
    ctx->vtable->vaCreateBuffer = psb_CreateBuffer;
    ctx->vtable->vaBufferSetNumElements = psb_BufferSetNumElements;
    ctx->vtable->vaMapBuffer = psb_MapBuffer;
    ctx->vtable->vaUnmapBuffer = psb_UnmapBuffer;
    ctx->vtable->vaDestroyBuffer = psb_DestroyBuffer;
    ctx->vtable->vaBeginPicture = psb_BeginPicture;
    ctx->vtable->vaRenderPicture = psb_RenderPicture;
    ctx->vtable->vaEndPicture = psb_EndPicture;
    ctx->vtable->vaSyncSurface = psb_SyncSurface;
    ctx->vtable->vaQuerySurfaceStatus = psb_QuerySurfaceStatus;
    ctx->vtable->vaQuerySurfaceError = psb_QuerySurfaceError;
    // psb doesn't support PutSurface on ipvr currently
    // ctx->vtable->vaPutSurface = psb_PutSurface;
    ctx->vtable->vaQueryImageFormats = psb_QueryImageFormats;
    ctx->vtable->vaCreateImage = psb_CreateImage;
    ctx->vtable->vaDeriveImage = psb_DeriveImage;
    ctx->vtable->vaDestroyImage = psb_DestroyImage;
    ctx->vtable->vaSetImagePalette = psb_SetImagePalette;
    ctx->vtable->vaGetImage = psb_GetImage;
    ctx->vtable->vaPutImage = psb_PutImage;
    ctx->vtable->vaQuerySubpictureFormats = psb_QuerySubpictureFormats;
    ctx->vtable->vaCreateSubpicture = psb_CreateSubpicture;
    ctx->vtable->vaDestroySubpicture = psb_DestroySubpicture;
    ctx->vtable->vaSetSubpictureImage = psb_SetSubpictureImage;
    ctx->vtable->vaSetSubpictureChromakey = psb_SetSubpictureChromakey;
    ctx->vtable->vaSetSubpictureGlobalAlpha = psb_SetSubpictureGlobalAlpha;
    ctx->vtable->vaAssociateSubpicture = psb_AssociateSubpicture;
    ctx->vtable->vaDeassociateSubpicture = psb_DeassociateSubpicture;
    ctx->vtable->vaQueryDisplayAttributes = psb_QueryDisplayAttributes;
    ctx->vtable->vaGetDisplayAttributes = psb_GetDisplayAttributes;
    ctx->vtable->vaSetDisplayAttributes = psb_SetDisplayAttributes;
    ctx->vtable->vaQuerySurfaceAttributes = psb_QuerySurfaceAttributes;
    ctx->vtable->vaBufferInfo = psb_BufferInfo;
    ctx->vtable->vaLockSurface = psb_LockSurface;
    ctx->vtable->vaUnlockSurface = psb_UnlockSurface;

    ctx->vtable->vaAcquireBufferHandle = psb_AcquireBufferHandle;
    ctx->vtable->vaReleaseBufferHandle = psb_ReleaseBufferHandle;

    ctx->vtable_tpi = calloc(1, sizeof(struct VADriverVTableTPI));
    if (NULL == ctx->vtable_tpi)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;


    ctx->vtable_egl = calloc(1, sizeof(struct VADriverVTableEGL));
    if (NULL == ctx->vtable_egl) {
        free(ctx->vtable_tpi);
        drv_debug_msg(VIDEO_DEBUG_ERROR, "ERROR: failed to allocate EGL table\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    driver_data = (psb_driver_data_p) calloc(1, sizeof(*driver_data));
    ctx->pDriverData = (unsigned char *) driver_data;
    if (NULL == driver_data) {
            free(ctx->vtable_tpi);
            free(ctx->vtable_egl);
        drv_debug_msg(VIDEO_DEBUG_ERROR, "ERROR: failed to allocate driverData\n");
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    if (VA_STATUS_SUCCESS != psb__initDRM(ctx)) {
        free(ctx->vtable_tpi);
        free(ctx->vtable_egl);
        free(ctx->pDriverData);
        ctx->pDriverData = NULL;
        drv_debug_msg(VIDEO_DEBUG_ERROR, "ERROR: failed to init DRI\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    pthread_mutex_init(&driver_data->drm_mutex, NULL);

    if (psb_get_device_info(ctx)) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "ERROR: failed to get video device info\n");
    }

    if (IS_BAYTRAIL(driver_data))
        ctx->str_vendor = PSB_STR_VENDOR_BAYTRAIL;
    else {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "Unsupported video device, abort\n");
        pthread_mutex_destroy(&driver_data->drm_mutex);
        psb__deinitDRM(ctx);
        free(ctx->vtable_tpi);
        free(ctx->vtable_egl);
        free(ctx->pDriverData);
        ctx->pDriverData = NULL;
        return VA_STATUS_ERROR_UNKNOWN;
    }

    if (IS_BAYTRAIL(driver_data)) {
	drv_debug_msg(VIDEO_DEBUG_GENERAL, "merrifield VXD392 decoder\n");
	driver_data->profile2Format[VAProfileVP8Version0_3][VAEntrypointVLD] = &tng_VP8_vtable;
    }

    result = object_heap_init(&driver_data->config_heap, sizeof(struct object_config_s), CONFIG_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }

    result = object_heap_init(&driver_data->context_heap, sizeof(struct object_context_s), CONTEXT_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }

    result = object_heap_init(&driver_data->surface_heap, sizeof(struct object_surface_s), SURFACE_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }

    result = object_heap_init(&driver_data->buffer_heap, sizeof(struct object_buffer_s), BUFFER_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }

    result = object_heap_init(&driver_data->image_heap, sizeof(struct object_image_s), IMAGE_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }
    result = object_heap_init(&driver_data->subpic_heap, sizeof(struct object_subpic_s), SUBPIC_ID_OFFSET);
    if (result) {
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        goto out_err;
    }

    drv_debug_msg(VIDEO_DEBUG_INIT, "vaInitilize: succeeded!\n\n");

    return VA_STATUS_SUCCESS;
out_err:
    psb_Terminate(ctx);
    return va_status;
}



static VAStatus
psb_AcquireBufferHandle(VADriverContextP ctx, VABufferID buf_id,
    VABufferInfo *buf_info)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    object_buffer_p obj_buffer = BUFFER(buf_id);

    if (!obj_buffer)
        return VA_STATUS_ERROR_INVALID_BUFFER;
    /* XXX: only VA surface|image like buffers are supported for now */
    if (obj_buffer->type != VAImageBufferType)
        return VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;

    if (!buf_info)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if ((buf_info->mem_type != VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME)
        && (buf_info->mem_type != VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM))
        return VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;

    if (!obj_buffer->ipvr_bo)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    drm_ipvr_gem_bo_wait(obj_buffer->ipvr_bo);

    if (obj_buffer->export_refcount > 0) {
        if (obj_buffer->export_state.mem_type != buf_info->mem_type)
            return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        VABufferInfo * const local_buf_info = &obj_buffer->export_state;

        switch (buf_info->mem_type) {
        case VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM: {
            uint32_t name;
            if (drm_ipvr_gem_bo_flink(obj_buffer->ipvr_bo, &name) != 0)
                return VA_STATUS_ERROR_INVALID_BUFFER;
            local_buf_info->handle = name;
            break;
        }
        case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME: {
            int fd;
            if (drm_ipvr_gem_bo_export_to_prime(obj_buffer->ipvr_bo, &fd) != 0)
                return VA_STATUS_ERROR_INVALID_BUFFER;
            local_buf_info->handle = (intptr_t)fd;
            break;
        }
        }

        local_buf_info->type = obj_buffer->type;
        local_buf_info->mem_type = buf_info->mem_type;
        local_buf_info->mem_size =
            obj_buffer->num_elements * obj_buffer->size;
    }

    obj_buffer->export_refcount++;
    *buf_info = obj_buffer->export_state;
    return VA_STATUS_SUCCESS;
}

static VAStatus
psb_ReleaseBufferHandle(VADriverContextP ctx, VABufferID buf_id)
{
    DEBUG_FUNC_ENTER
    INIT_DRIVER_DATA
    object_buffer_p obj_buffer = BUFFER(buf_id);
    if (!obj_buffer)
        return VA_STATUS_ERROR_INVALID_BUFFER;
    /* XXX: only VA surface|image like buffers are supported for now */
    if (obj_buffer->type != VAImageBufferType)
        return VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;

    if (obj_buffer->export_refcount == 0)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    if (--obj_buffer->export_refcount == 0) {
        VABufferInfo * const buf_info = &obj_buffer->export_state;

        switch (buf_info->mem_type) {
        case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME: {
            close((intptr_t)buf_info->handle);
            break;
        }
        }
        buf_info->mem_type = 0;
    }
    return VA_STATUS_SUCCESS;

}

static int psb_get_device_info(VADriverContextP ctx)
{
 INIT_DRIVER_DATA;

    uint16_t dev_id, caps;
    int ret;

    driver_data->dev_id = 0xFFFF; /* by default unknown */
    ret = drm_ipvr_gem_bufmgr_get_device_info(driver_data->bufmgr, &dev_id, &caps);
    if (ret) {
        drv_debug_msg(VIDEO_DEBUG_ERROR, "failed to get video device info\n");
        return ret;
    }

    driver_data->dev_id = dev_id;
    drv_debug_msg(VIDEO_DEBUG_INIT, "Retrieve Device ID 0x%08x\n", driver_data->dev_id);

    return 0;
}
