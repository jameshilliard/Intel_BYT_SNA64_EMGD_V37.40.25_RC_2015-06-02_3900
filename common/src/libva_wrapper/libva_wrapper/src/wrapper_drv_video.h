/*
 * Copyright � 2009 Intel Corporation
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
 *    Boon Wooi Tay<boon.wooi.tay@intel.com>
 *    Kalaiyappan, Periyakaruppan Kumaran<periyakaruppan.kumaran.kalaiyappan@intel.com>
 *
 */

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_dec_vp8.h>

#include "list.h"

#define DLL_EXPORT __attribute__((visibility("default")))

#define MAX_NUM_DRV	2

#define I965_DRV	0
#define PSB_DRV		1




#define RESTORE_I965DATA(ctx, vawr) \
	do { \
		ctx->pDriverData = vawr->drv_data[I965_DRV]; \
		ctx->drm_state = vawr->drm_state[I965_DRV]; \
	} while(0)
#define RESTORE_PSBDATA(ctx, vawr)	\
	do { \
		ctx->pDriverData = vawr->drv_data[PSB_DRV]; \
		ctx->drm_state = vawr->drm_state[PSB_DRV]; \
	} while(0)
#define RESTORE_DRVDATA(ctx, vawr)	\
	do { \
		ctx->pDriverData = vawr->profile == VAProfileVP8Version0_3 ? vawr->drv_data[PSB_DRV] : vawr->drv_data[I965_DRV]; \
		ctx->drm_state = vawr->profile == VAProfileVP8Version0_3 ? vawr->drm_state[PSB_DRV] : vawr->drm_state[I965_DRV]; \
	} while(0)

#define CALL_DRVVTABLE(vawr, status, param) status = ((vawr->profile == VAProfileVP8Version0_3) ? vawr->drv_vtable[PSB_DRV]->param : vawr->drv_vtable[I965_DRV]->param)
#define CHECK_INVALID_PARAM(param) \
    do { \
        if (param) { \
            vaStatus = VA_STATUS_ERROR_INVALID_PARAMETER; \
            DEBUG_FAILURE; \
            return vaStatus; \
        } \
    } while (0)
#define GET_SURFACEID(vawr, surface_lookup, surface, surface_out)	\
	if (vawr->profile == VAProfileVP8Version0_3) {	\
		LIST_FOR_EACH_ENTRY(surface_lookup, &vawr->surfaces, link) 	\
				if (surface_lookup->pvr_surface == surface) 	\
					surface_out = surface_lookup->pvr_surface;	\
	} else 	\
		surface_out = surface

#define GET_SURFACEID_I965(vawr, surface_lookup, surface, surface_out)	\
	if (vawr->profile == VAProfileVP8Version0_3) {	\
		LIST_FOR_EACH_ENTRY(surface_lookup, &vawr->surfaces, link) 	\
				if (surface_lookup->pvr_surface == surface) 	\
					surface_out = surface_lookup->i965_surface;	\
	} else 	\
		surface_out = surface

/* Linked list macro to list.h */
#define LIST list
#define LIST_INIT list_init
#define LIST_IS_EMPTY list_is_empty
#define LIST_ADD list_add
#define LIST_DEL list_del
#define LIST_FIRST_ENTRY list_first_entry
#define LIST_FOR_EACH_ENTRY list_for_each_entry
#define LIST_FOR_EACH_ENTRY_SAFE list_for_each_entry_safe


struct vawr_driver_data
{
	void *drv_data[MAX_NUM_DRV];
	struct VADriverVTable *drv_vtable[MAX_NUM_DRV];
	void * drm_state[MAX_NUM_DRV];
	struct LIST surfaces;	/* surface_id lookup table */
	struct LIST images; /* image id lookup table */
	struct LIST handles; /* va driver lib handle */
	VAProfile profile;
	VABufferID pic_param_buf_id;
	pthread_mutex_t vaapi_mutex;
};

typedef struct vawr_surface_lookup
{
	VASurfaceID	i965_surface;
	VASurfaceID pvr_surface;
	struct LIST link;
}vawr_surface_lookup_t;

typedef struct vawr_image_lookup
{
	VAImageID	image;
	int owner; /* I965 or PSB */
	struct LIST link;
}vawr_image_lookup_t;

typedef struct vawr_handle_lookup
{
	void * handle;
	struct LIST link;
}vawr_handle_lookup_t;

#define GET_VAWRDATA(ctx)    ctx->pDriverData
#define RESTORE_VAWRDATA(ctx, vawr)	ctx->pDriverData = vawr
