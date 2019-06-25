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

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <pthread.h>

#include "wrapper_drv_video.h"

#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <fcntl.h>

#define ALIGN(i, n)    (((i) + (n) - 1) & ~((n) - 1))

#define DRIVER_EXTENSION	"_drv_video.so"

static struct vawr_driver_data *vawr = NULL;

#define WRAPPER_LOCK(mutex) \
do { \
    pthread_mutex_lock(mutex); \
} while (0)

#define WRAPPER_UNLOCK(mutex) \
do { \
    pthread_mutex_unlock(mutex); \
} while (0)

void vawr_errorMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva-wrapper error: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

void vawr_infoMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva-wrapper: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

static inline int
va_getDriverInitName(char *name, int namelen, int major, int minor)
{
    int ret = snprintf(name, namelen, "__vaDriverInit_%d_%d", major, minor);
    return ret > 0 && ret < namelen;
}

static VAStatus vawr_openDriver(VADriverContextP ctx, char *driver_name)
{
    VAStatus vaStatus = VA_STATUS_ERROR_UNKNOWN;
    char *search_path = NULL;
    char *saveptr = NULL;
    char *driver_dir = NULL;

    if (geteuid() == getuid())
        /* don't allow setuid apps to use LIBVA_DRIVERS_PATH */
        search_path = getenv("LIBVA_DRIVERS_PATH");
    if (!search_path)
        search_path = VA_DRIVERS_PATH;

    search_path = strdup((const char *)search_path);
    driver_dir = strtok_r(search_path, ":", &saveptr);

    while (driver_dir) {
        void *handle = NULL;
        char *driver_path = (char *) calloc(1, strlen(driver_dir) +
                                             strlen(driver_name) +
                                             strlen(DRIVER_EXTENSION) + 2 );

        if (!driver_path) {
            vawr_errorMessage("%s L%d Out of memory!n",
                                __FUNCTION__, __LINE__);
            free(search_path);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        strncpy( driver_path, driver_dir, strlen(driver_dir) + 1);
        strncat( driver_path, "/", strlen("/") );
        strncat( driver_path, driver_name, strlen(driver_name) );
        strncat( driver_path, DRIVER_EXTENSION, strlen(DRIVER_EXTENSION) );

        handle = dlopen( driver_path, RTLD_NOW| RTLD_GLOBAL);
        if (!handle) {
            /* Manage error if the file exists or not we need to know what's going
            * on */
            vawr_errorMessage("dlopen of %s failed: %s\n", driver_path, dlerror());
        } else {
            VADriverInit init_func = NULL;
            char init_func_s[256];
            int i;

            static const struct {
                int major;
                int minor;
            } compatible_versions[] = {
                { VA_MAJOR_VERSION, VA_MINOR_VERSION },
                { 0, 37 },
                { -1, }
            };

            for (i = 0; compatible_versions[i].major >= 0; i++) {
                if (va_getDriverInitName(init_func_s, sizeof(init_func_s),
                                         compatible_versions[i].major,
                                         compatible_versions[i].minor)) {
                    init_func = (VADriverInit)dlsym(handle, init_func_s);
                    if (init_func) {
                        vawr_infoMessage("Found init function %s\n", init_func_s);
                        break;
                    }
                }
            }

            if (compatible_versions[i].major < 0) {
                vawr_errorMessage("%s has no function %s\n",
                                driver_path, init_func_s);
                dlclose(handle);
            } else {
                if (init_func)
                    vaStatus = (*init_func)(ctx);
                if (VA_STATUS_SUCCESS != vaStatus) {
                    vawr_errorMessage("%s init failed\n", driver_path);
                    dlclose(handle);
                } else {
                    free(driver_path);

                    /* Store the handle to a private structure */
                    vawr_handle_lookup_t * handle_lookup = calloc(1, sizeof (*handle_lookup));
                    if (!handle_lookup) {
                        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
                        dlclose(handle);
                        break;
                    }
                    handle_lookup->handle = handle;
                    LIST_ADD(&handle_lookup->link, &vawr->handles);

                    break;
                }
            }
        }
        free(driver_path);
        driver_dir = strtok_r(NULL, ":", &saveptr);
    }

    free(search_path);

    return vaStatus;
}

VAStatus
vawr_Terminate(VADriverContextP ctx)
{
    VAStatus vaStatus;
    vawr_handle_lookup_t *handle_lookup = NULL, *temp = NULL;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    /*
    * assume surface list and image list all free by destroy surface and destroy image
    */
    if (vawr->profile == VAProfileVP8Version0_3) {
        RESTORE_PSBDATA(ctx, vawr);
        vaStatus = vawr->drv_vtable[PSB_DRV]->vaTerminate(ctx);

        RESTORE_I965DATA(ctx, vawr);
        vaStatus = vawr->drv_vtable[I965_DRV]->vaTerminate(ctx);

        free(vawr->drm_state[PSB_DRV]);
    } else {
        RESTORE_I965DATA(ctx, vawr);
        vaStatus = vawr->drv_vtable[I965_DRV]->vaTerminate(ctx);
    }

    free(vawr->drv_vtable[PSB_DRV]);
    free(vawr->drv_vtable[I965_DRV]);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    pthread_mutex_destroy(&vawr->vaapi_mutex);

    /* handle is Stored when open driver */
    LIST_FOR_EACH_ENTRY_SAFE(handle_lookup, temp, &vawr->handles, link) {
        if (handle_lookup->handle) {
            dlclose(handle_lookup->handle);;
        }
        LIST_DEL(&handle_lookup->link);
        free(handle_lookup);
    }

    /*
    * for va_DisplayContextDestroy will called in vaTerminater after vawr_Terminate
    * restore the drm_state
    * vaTerminate do more thing than other vaapi, need to look out
    */
    ctx->drm_state = vawr->drm_state[I965_DRV];

    /* free private driver data */
    free(vawr);
    ctx->pDriverData = NULL;

    return vaStatus;
}

VAStatus
vawr_QueryConfigProfiles(VADriverContextP ctx,
                         VAProfile *profile_list,       /* out */
                         int *num_profiles)             /* out */
{
    VAStatus vaStatus;

    WRAPPER_LOCK(&vawr->vaapi_mutex);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigProfiles(ctx, profile_list, num_profiles));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    /* PSB_DRV has not published VP8 profile in QueryConfigProfiles yet,
     * that means we have to do it here.
     */
    profile_list[(*num_profiles)++] = VAProfileVP8Version0_3;

    return vaStatus;
}

VAStatus
vawr_QueryConfigEntrypoints(VADriverContextP ctx,
                            VAProfile profile,
                            VAEntrypoint *entrypoint_list,      /* out */
                            int *num_entrypoints)               /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    /* PSB_DRV currently only supports VLD entry point for VP8 */
    if (profile == VAProfileVP8Version0_3) {
	*entrypoint_list = VAEntrypointVLD;
	*num_entrypoints = 1;
	WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return VA_STATUS_SUCCESS;
    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigEntrypoints(ctx, profile, entrypoint_list, num_entrypoints));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_GetConfigAttributes(VADriverContextP ctx,
                         VAProfile profile,
                         VAEntrypoint entrypoint,
                         VAConfigAttrib *attrib_list,  /* in/out */
                         int num_attribs)
{
    VAStatus vaStatus;
    int i;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    /* PSB_DRV currently only supports VAConfigAttribRTFormat attribute type */
    if (profile == VAProfileVP8Version0_3) {
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
        WRAPPER_UNLOCK(&vawr->vaapi_mutex);
        return VA_STATUS_SUCCESS;
    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetConfigAttributes(ctx, profile, entrypoint, attrib_list, num_attribs));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_CreateConfig(VADriverContextP ctx,
                  VAProfile profile,
                  VAEntrypoint entrypoint,
                  VAConfigAttrib *attrib_list,
                  int num_attribs,
                  VAConfigID *config_id)		/* out */
{
    VAStatus vaStatus;
    struct VADriverVTable *psb_vtable = NULL;
    struct VADriverVTable * const vtable = ctx->vtable;
    char *driver_name = "pvr";
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    if (profile == VAProfileVP8Version0_3) {
	    vawr->profile = VAProfileVP8Version0_3;

    	/* This is the first time we learn about the config profile,
        * and we have to load psb_drv_video here if
        * a VP8 config is to be created.
        * Is this a good place to load psb_drv_video? We can
        * load it during vaInitialize together with i965_drv_video,
        * but it could slow down init for process that doesn't
        * require VP8.
        */
        psb_vtable = calloc(1, sizeof(*psb_vtable));
        if (!psb_vtable) {
            WRAPPER_UNLOCK(&vawr->vaapi_mutex);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        ctx->vtable = psb_vtable;

        /*  drm_state will be freed at vawr_vaTerminate */
        ctx->drm_state = malloc(sizeof(struct drm_state));
        if (!(ctx->drm_state)) {
		free(psb_vtable);
		WRAPPER_UNLOCK(&vawr->vaapi_mutex);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
        memset(ctx->drm_state, 0, sizeof(struct drm_state));

        vaStatus = vawr_openDriver(ctx, driver_name);
        if (VA_STATUS_SUCCESS == vaStatus) {
            /* We have successfully initialized i965 video driver,
             * Let's store i965's private driver data and vtable.
             */
            vawr->drv_data[PSB_DRV] = (void *)ctx->pDriverData;
            vawr->drv_vtable[PSB_DRV] = psb_vtable;
            vawr->drm_state[PSB_DRV] = ctx->drm_state;

            /* Also restore the va's vtable */
            ctx->vtable = vtable;

            /* TODO: Shall we backup ctx structure? Some members like versions, max_num_profiles
             * will be overwritten but are they still important? Most likely not.
             * Let's not do it for now.
             */
        } else {
            /*
            * if open driver failed, context is invaild, cannot continue,
            * restore ctx and return error
            */
            ctx->pDriverData = vawr->drv_data[I965_DRV];
            ctx->vtable = vawr->drv_vtable[I965_DRV];
            ctx->drm_state = vawr->drm_state[I965_DRV];
            WRAPPER_UNLOCK(&vawr->vaapi_mutex);
            return VA_STATUS_ERROR_UNKNOWN;
        }

    }

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateConfig(ctx, profile, entrypoint, attrib_list, num_attribs, config_id));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DestroyConfig(VADriverContextP ctx, VAConfigID config_id)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyConfig(ctx, config_id));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus vawr_QueryConfigAttributes(VADriverContextP ctx,
                                    VAConfigID config_id,
                                    VAProfile *profile,                 /* out */
                                    VAEntrypoint *entrypoint,           /* out */
                                    VAConfigAttrib *attrib_list,        /* out */
                                    int *num_attribs)                   /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryConfigAttributes(ctx, config_id, profile, entrypoint, attrib_list, num_attribs));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_CreateSurfaces(VADriverContextP ctx,
                    int width,
                    int height,
                    int format,
                    int num_surfaces,
                    VASurfaceID *surfaces)
                    //VASurfaceAttrib    *attrib_list,
                    //unsigned int        num_attribs)      /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    // XXX, assume we have already got correct profile (vaCreateConfig is called before vaCreateSurface)
    if (vawr->profile == VAProfileVP8Version0_3) {
        RESTORE_PSBDATA(ctx, vawr);
        vaStatus = vawr->drv_vtable[PSB_DRV]->vaCreateSurfaces2(ctx, format, width, height, surfaces, num_surfaces, NULL, 0);
    } else {
    RESTORE_I965DATA(ctx, vawr);
    vaStatus = vawr->drv_vtable[I965_DRV]->vaCreateSurfaces(ctx, width, height, format, num_surfaces, surfaces);
    }

    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DestroySurfaces(VADriverContextP ctx,
                     VASurfaceID *surface_list,
                     int num_surfaces)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vawr_surface_lookup_t *surface = NULL, *temp = NULL;
    int i;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    if (vawr->profile == VAProfileVP8Version0_3) {
        /* First destroy the PVR surfaces */
        for (i=0; i<num_surfaces; i++) {
            LIST_FOR_EACH_ENTRY_SAFE(surface, temp, &vawr->surfaces, link) {
                if (surface->pvr_surface == surface_list[i]) {
                    /* Restore the PVR's context for DestroySurfaces purpose */
                    ctx->pDriverData = vawr->drv_data[PSB_DRV];
                    vaStatus = vawr->drv_vtable[PSB_DRV]->vaDestroySurfaces(ctx, &surface->pvr_surface, 1);
                    ctx->pDriverData = vawr->drv_data[I965_DRV];
                    vaStatus = vawr->drv_vtable[I965_DRV]->vaDestroySurfaces(ctx, &surface->i965_surface, 1);
                    surface->i965_surface = 0;
                    surface->pvr_surface = 0;
                    LIST_DEL(&surface->link);
                    free(surface); /* surface will not be used, should be freed */
                }
            }
        }
        RESTORE_VAWRDATA(ctx, vawr);
    } else {
        /* destroy the i965 surfaces */
        RESTORE_I965DATA(ctx, vawr);
        vaStatus = vawr->drv_vtable[0]->vaDestroySurfaces(ctx, surface_list, num_surfaces);
        RESTORE_VAWRDATA(ctx, vawr);
    }
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}

VAImage * m_export_img = NULL;
VABufferInfo * m_export_buf_info = NULL;
VASurfaceID	surface_id = 0;
VASurfaceID*  m_Surfaces_965 = NULL;

VAStatus
vawr_CreateContext(VADriverContextP ctx,
                   VAConfigID config_id,
                   int picture_width,
                   int picture_height,
                   int flag,
                   VASurfaceID *render_targets,
                   int num_render_targets,
                   VAContextID *context)                /* out */
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_targets[num_render_targets];
    int i;


	//int rendertarget_i;
	VASurfaceAttrib surf_attr[2];
	VASurfaceAttribExternalBuffers ext_buf;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    /* If config profile is VP8, we have to map the render targets into pvr driver's TTM
     * Since config_id is opague to wrapper (at least for now), we can only check vawr->profile
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
		vawr_surface_lookup_t *surface;
		LIST_INIT(&vawr->surfaces);

		ext_buf.buffers = malloc((num_render_targets) * sizeof(unsigned long));
		m_export_img = malloc((num_render_targets) * sizeof(VAImage));
		m_export_buf_info = malloc((num_render_targets) * sizeof(VABufferInfo));
		if (ext_buf.buffers == NULL || m_export_img == NULL || m_export_buf_info == NULL) {
			if (ext_buf.buffers) free(ext_buf.buffers);
			if (m_export_img) free(m_export_img);
			if (m_export_buf_info) free(m_export_buf_info);
			RESTORE_VAWRDATA(ctx, vawr);
			WRAPPER_UNLOCK(&vawr->vaapi_mutex);
			return VA_STATUS_ERROR_ALLOCATION_FAILED;
		}

		RESTORE_PSBDATA(ctx, vawr);
		for (i=0; i<num_render_targets; i++)
		{
			    vaStatus = vawr->drv_vtable[PSB_DRV]->vaDeriveImage(ctx, render_targets[i], &m_export_img[i]);
			    if (vaStatus != VA_STATUS_SUCCESS) {
				free(ext_buf.buffers);
				free(m_export_img);
				free(m_export_buf_info);
				RESTORE_VAWRDATA(ctx, vawr);
				WRAPPER_UNLOCK(&vawr->vaapi_mutex);

					return vaStatus;
			    }
			    m_export_buf_info[i].mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
			    vaStatus = vawr->drv_vtable[PSB_DRV]->vaAcquireBufferHandle(ctx, m_export_img[i].buf, &m_export_buf_info[i]);
			    if (vaStatus != VA_STATUS_SUCCESS) {
				free(ext_buf.buffers);
				free(m_export_img);
				free(m_export_buf_info);
				RESTORE_VAWRDATA(ctx, vawr);
				WRAPPER_UNLOCK(&vawr->vaapi_mutex);

					return vaStatus;
				}
			    ext_buf.buffers[i] = m_export_buf_info[i].handle;

		}

		ext_buf.data_size = m_export_img[0].data_size;
		printf("exported data size is 0x%x\n", ext_buf.data_size);
		ext_buf.height = m_export_img[0].height;
		ext_buf.width = m_export_img[0].width;
		ext_buf.num_buffers = num_render_targets;
		ext_buf.num_planes = m_export_img[0].num_planes;
		ext_buf.pitches[0] = m_export_img[0].pitches[0];
		ext_buf.pitches[1] = m_export_img[0].pitches[1];
		ext_buf.pitches[2] = m_export_img[0].pitches[2];
		ext_buf.pitches[3] = 0;
		ext_buf.offsets[0] = m_export_img[0].offsets[0];
		ext_buf.offsets[1] = m_export_img[0].offsets[1];
		ext_buf.offsets[2] = m_export_img[0].offsets[2];
		ext_buf.offsets[3] = 0;
		ext_buf.pixel_format = m_export_img[0].format.fourcc;
		surf_attr[0].type = VASurfaceAttribMemoryType;
		surf_attr[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
		surf_attr[0].value.type = VAGenericValueTypeInteger;
		surf_attr[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
		surf_attr[1].type = VASurfaceAttribExternalBufferDescriptor;
		surf_attr[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
		surf_attr[1].value.type = VAGenericValueTypePointer;
		surf_attr[1].value.value.p = (void*)&ext_buf;

		/* import 965 fd and create ipvr surface */
		RESTORE_I965DATA(ctx, vawr);

		m_Surfaces_965 = malloc((num_render_targets) * sizeof(VASurfaceID));
		if (m_Surfaces_965 == NULL) {
			free(ext_buf.buffers);
			free(m_export_img);
			free(m_export_buf_info);
			RESTORE_VAWRDATA(ctx, vawr);
			WRAPPER_UNLOCK(&vawr->vaapi_mutex);
			return VA_STATUS_ERROR_ALLOCATION_FAILED;
		}

		vaStatus = vawr->drv_vtable[I965_DRV]->vaCreateSurfaces2(
		    ctx,
		    VA_RT_FORMAT_YUV420, m_export_img[0].width, ((m_export_img[0].height + 32 - 1) & ~(32 - 1)),
		    //&surface_id, 1,
		    m_Surfaces_965, num_render_targets,
		    surf_attr, 2
		);

		free(ext_buf.buffers);
		free(m_export_img);
		free(m_export_buf_info);

		if (vaStatus == VA_STATUS_SUCCESS)
		{
			for (i=0; i<num_render_targets; i++)
			{

				/* Find a way to store the returned surface_id with correct mapping of i965's surface_id,
				 * since all future surface_id communicated between application and wrappre is i965's
				 * while one communicated between wrapper and pvr driver is pvr's.
				 */
				surface = calloc(1, sizeof *surface);
				if (!surface) {
				    RESTORE_VAWRDATA(ctx, vawr);
					    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
					return VA_STATUS_ERROR_ALLOCATION_FAILED;
				}

				surface->i965_surface = m_Surfaces_965[i];
				surface->pvr_surface =  render_targets[i];
				LIST_ADD(&surface->link,&vawr->surfaces);

				/* render_targets is pvr's surface_id from here on */
				vawr_render_targets[i] = render_targets[i];
			}

			free(m_Surfaces_965);
		}
		else
		{
		    free(m_Surfaces_965);
		    RESTORE_VAWRDATA(ctx, vawr);
		    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
			return vaStatus;
		}
	}

	RESTORE_DRVDATA(ctx, vawr);

	if (vawr->profile == VAProfileVP8Version0_3) {
	    CALL_DRVVTABLE(vawr, vaStatus, vaCreateContext(ctx, config_id, picture_width, picture_height, flag, vawr_render_targets, num_render_targets, context));
	} else {
	    CALL_DRVVTABLE(vawr, vaStatus, vaCreateContext(ctx, config_id, picture_width, picture_height, flag, render_targets, num_render_targets, context));
	}

	RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DestroyContext(VADriverContextP ctx, VAContextID context)
{
    VAStatus vaStatus;

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyContext(ctx, context));
    RESTORE_VAWRDATA(ctx, vawr);

	return vaStatus;
}

VAStatus
vawr_CreateBuffer(VADriverContextP ctx,
                  VAContextID context,          /* in */
                  VABufferType type,            /* in */
                  unsigned int size,            /* in */
                  unsigned int num_elements,    /* in */
                  void *data,                   /* in */
                  VABufferID *buf_id)           /* out */
{
    VAStatus vaStatus;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateBuffer(ctx, context, type, size, num_elements, data, buf_id));
    RESTORE_VAWRDATA(ctx, vawr);

    /* For now, let's track the VP8's VAPictureParameterBufferType buf_id so that we can overwrite the
     * i965's VASurfaceID embedded in the picture parameter with pvr's. This is a dirty hack until we
     * find a better way to deal with surface_id.
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
		if (type == VAPictureParameterBufferType) {
			vawr->pic_param_buf_id = *buf_id;
		}
	}
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_BufferSetNumElements(VADriverContextP ctx,
                          VABufferID buf_id,           /* in */
                          unsigned int num_elements)   /* in */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBufferSetNumElements(ctx, buf_id, num_elements));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_BufferInfo(VADriverContextP ctx,
    VABufferID buf_id,          /* in */
    VABufferType *type,         /* out */
    unsigned int *size,         /* out */
    unsigned int *num_elements) /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBufferInfo(ctx, buf_id, type, size, num_elements));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}

VAStatus
vawr_MapBuffer(VADriverContextP ctx,
               VABufferID buf_id,       /* in */
               void **pbuf)             /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaMapBuffer(ctx, buf_id, pbuf));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_UnmapBuffer(VADriverContextP ctx, VABufferID buf_id)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaUnmapBuffer(ctx, buf_id));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DestroyBuffer(VADriverContextP ctx, VABufferID buffer_id)
{
    VAStatus vaStatus;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroyBuffer(ctx, buffer_id));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_BeginPicture(VADriverContextP ctx,
                  VAContextID context,
                  VASurfaceID render_target)
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    //vawr_infoMessage("vawr_BeginPicture: render_target %d\n", render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaBeginPicture(ctx, context, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_RenderPicture(VADriverContextP ctx,
                   VAContextID context,
                   VABufferID *buffers,
                   int num_buffers)
{
    VAStatus vaStatus;
    VAPictureParameterBufferVP8 * const pic_param;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);

    /* For now, let's track the VP8's VAPictureParameterBufferType buf_id so that we can overwrite the
     * i965's VASurfaceID embedded in the picture parameter with pvr's. This is a dirty hack until we
     * find a better way to deal with surface_id.
     */
	if (vawr->profile == VAProfileVP8Version0_3) {
	    if (vawr->pic_param_buf_id) {
	        vawr_surface_lookup_t *surface_lookup = NULL;

		CALL_DRVVTABLE(vawr, vaStatus, vaMapBuffer(ctx, vawr->pic_param_buf_id, (void **)&pic_param));
		/* Ok we have the picture parameter, let's translate parameters with surface_id */
			LIST_FOR_EACH_ENTRY(surface_lookup, &vawr->surfaces, link) {
					if (surface_lookup->i965_surface == pic_param->last_ref_frame)
						pic_param->last_ref_frame = surface_lookup->pvr_surface;
					if (surface_lookup->i965_surface == pic_param->golden_ref_frame)
						pic_param->golden_ref_frame = surface_lookup->pvr_surface;
					if (surface_lookup->i965_surface == pic_param->alt_ref_frame)
						pic_param->alt_ref_frame = surface_lookup->pvr_surface;
			}
		CALL_DRVVTABLE(vawr, vaStatus, vaUnmapBuffer(ctx, vawr->pic_param_buf_id));
		vawr->pic_param_buf_id = 0;
	    }
	}
    CALL_DRVVTABLE(vawr, vaStatus, vaRenderPicture(ctx, context, buffers, num_buffers));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_EndPicture(VADriverContextP ctx, VAContextID context)
{
    VAStatus vaStatus;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaEndPicture(ctx, context));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_SyncSurface(VADriverContextP ctx,
                 VASurfaceID render_target)
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;

    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSyncSurface(ctx, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_QuerySurfaceStatus(VADriverContextP ctx,
                        VASurfaceID render_target,
                        VASurfaceStatus *status)        /* out */
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, render_target, vawr_render_target);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQuerySurfaceStatus(ctx, vawr_render_target, status));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_PutSurface(VADriverContextP ctx,
                VASurfaceID render_target,
                void *draw, /* X Drawable */
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
                unsigned int flags) /* de-interlacing flags */
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID_I965(vawr, surface_lookup, render_target, vawr_render_target);

	/* Rendering should always be done via i965 *//* change to psb surface  */
	ctx->pDriverData = vawr->drv_data[I965_DRV];
	ctx->drm_state = vawr->drm_state[I965_DRV];
	vaStatus = vawr->drv_vtable[0]->vaPutSurface(ctx, vawr_render_target, draw, srcx, srcy, srcw, srch, destx, desty, destw, desth, cliprects, number_cliprects, flags);
    RESTORE_VAWRDATA(ctx, vawr);

    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_QueryImageFormats(VADriverContextP ctx,
                       VAImageFormat *format_list,      /* out */
                       int *num_formats)                /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryImageFormats(ctx, format_list, num_formats));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_CreateImage(VADriverContextP ctx,
                 VAImageFormat *format,
                 int width,
                 int height,
                 VAImage *out_image)        /* out */
{
    VAStatus vaStatus;
    vawr_image_lookup_t *image;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateImage(ctx, format, width, height, out_image));

    /* Find a way to store the returned image with who create it,
     * the image should be destroied by who create it
     * avoid that image created by i965 but destroied by psb
     */
    image = calloc(1, sizeof *image);
    if (!image) {
        RESTORE_VAWRDATA(ctx, vawr);
        WRAPPER_UNLOCK(&vawr->vaapi_mutex);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    image->image = out_image->image_id;
    image->owner =  ((vawr->profile == VAProfileVP8Version0_3) ? PSB_DRV : I965_DRV);
    LIST_ADD(&image->link,&vawr->images);

    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus vawr_DeriveImage(VADriverContextP ctx,
                          VASurfaceID surface,
                          VAImage *out_image)        /* out */
{
    VAStatus vaStatus;
    VASurfaceID vawr_surface = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    vawr_image_lookup_t *image;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDeriveImage(ctx, vawr_surface, out_image));

    image = calloc(1, sizeof *image);
    if (!image) {
        RESTORE_VAWRDATA(ctx, vawr);
        WRAPPER_UNLOCK(&vawr->vaapi_mutex);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    image->image = out_image->image_id;
    image->owner =  ((vawr->profile == VAProfileVP8Version0_3) ? PSB_DRV : I965_DRV);
    LIST_ADD(&image->link,&vawr->images);

    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}

VAStatus
vawr_DestroyImage(VADriverContextP ctx, VAImageID image)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    vawr_image_lookup_t *image_lookup = NULL, *temp = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    LIST_FOR_EACH_ENTRY_SAFE(image_lookup, temp, &vawr->images, link) {
        if (image_lookup->image == image) {
            /* Restore the PVR's context for DestroyImage purpose */
            ctx->pDriverData = vawr->drv_data[image_lookup->owner];
            vaStatus = vawr->drv_vtable[image_lookup->owner]->vaDestroyImage(ctx, image);
            LIST_DEL(&image_lookup->link);
            free(image_lookup);
        }
    }

    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}

VAStatus
vawr_SetImagePalette(VADriverContextP ctx,
                     VAImageID image,
                     unsigned char *palette)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetImagePalette(ctx, image, palette));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_GetImage(VADriverContextP ctx,
              VASurfaceID surface,
              int x,   /* coordinates of the upper left source pixel */
              int y,
              unsigned int width,      /* width and height of the region */
              unsigned int height,
              VAImageID image)
{
    VAStatus vaStatus;
    VASurfaceID vawr_surface = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetImage(ctx, vawr_surface, x, y, width, height, image));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_PutImage(VADriverContextP ctx,
              VASurfaceID surface,
              VAImageID image,
              int src_x,
              int src_y,
              unsigned int src_width,
              unsigned int src_height,
              int dest_x,
              int dest_y,
              unsigned int dest_width,
              unsigned int dest_height)
{
    VAStatus vaStatus;
    VASurfaceID vawr_surface = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, surface, vawr_surface);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaPutImage(ctx, vawr_surface, image, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_QuerySubpictureFormats(VADriverContextP ctx,
                            VAImageFormat *format_list,         /* out */
                            unsigned int *flags,                /* out */
                            unsigned int *num_formats)          /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQuerySubpictureFormats(ctx, format_list, flags, num_formats));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_CreateSubpicture(VADriverContextP ctx,
                      VAImageID image,
                      VASubpictureID *subpicture)         /* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaCreateSubpicture(ctx, image, subpicture));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DestroySubpicture(VADriverContextP ctx,
                       VASubpictureID subpicture)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDestroySubpicture(ctx, subpicture));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_SetSubpictureImage(VADriverContextP ctx,
                        VASubpictureID subpicture,
                        VAImageID image)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureImage(ctx, subpicture, image));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_SetSubpictureChromakey(VADriverContextP ctx,
                            VASubpictureID subpicture,
                            unsigned int chromakey_min,
                            unsigned int chromakey_max,
                            unsigned int chromakey_mask)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureChromakey(ctx, subpicture, chromakey_min, chromakey_max, chromakey_mask));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_SetSubpictureGlobalAlpha(VADriverContextP ctx,
                              VASubpictureID subpicture,
                              float global_alpha)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetSubpictureGlobalAlpha(ctx, subpicture, global_alpha));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_AssociateSubpicture(VADriverContextP ctx,
                         VASubpictureID subpicture,
                         VASurfaceID *target_surfaces,
                         int num_surfaces,
                         short src_x, /* upper left offset in subpicture */
                         short src_y,
                         unsigned short src_width,
                         unsigned short src_height,
                         short dest_x, /* upper left offset in surface */
                         short dest_y,
                         unsigned short dest_width,
                         unsigned short dest_height,
                         /*
                          * whether to enable chroma-keying or global-alpha
                          * see VA_SUBPICTURE_XXX values
                          */
                         unsigned int flags)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaAssociateSubpicture(ctx, subpicture, target_surfaces, num_surfaces, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height, flags));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_DeassociateSubpicture(VADriverContextP ctx,
                           VASubpictureID subpicture,
                           VASurfaceID *target_surfaces,
                           int num_surfaces)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaDeassociateSubpicture(ctx, subpicture, target_surfaces, num_surfaces));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;
}

VAStatus
vawr_QueryDisplayAttributes(VADriverContextP ctx,
                            VADisplayAttribute *attr_list,	/* out */
                            int *num_attributes)		/* out */
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaQueryDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;

}

VAStatus
vawr_GetDisplayAttributes(VADriverContextP ctx,
                          VADisplayAttribute *attr_list,	/* in/out */
                          int num_attributes)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaGetDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;

}

VAStatus
vawr_SetDisplayAttributes(VADriverContextP ctx,
                          VADisplayAttribute *attr_list,
                          int num_attributes)
{
    VAStatus vaStatus;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaSetDisplayAttributes(ctx, attr_list, num_attributes));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
	return vaStatus;

}

VAStatus vawr_LockSurface(VADriverContextP ctx,
    VASurfaceID surface,
    unsigned int *fourcc, /* following are output argument */
    unsigned int *luma_stride,
    unsigned int *chroma_u_stride,
    unsigned int *chroma_v_stride,
    unsigned int *luma_offset,
    unsigned int *chroma_u_offset,
    unsigned int *chroma_v_offset,
    unsigned int *buffer_name,
    void **buffer)
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, surface, vawr_render_target);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaLockSurface(ctx, vawr_render_target, fourcc, luma_stride, chroma_u_stride, chroma_v_stride, luma_offset, chroma_u_offset, chroma_v_offset, buffer_name, buffer));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}


VAStatus vawr_UnlockSurface(VADriverContextP ctx,
    VASurfaceID surface)
{
    VAStatus vaStatus;
    VASurfaceID vawr_render_target = 0;
    vawr_surface_lookup_t *surface_lookup = NULL;
    WRAPPER_LOCK(&vawr->vaapi_mutex);
    GET_SURFACEID(vawr, surface_lookup, surface, vawr_render_target);

    RESTORE_DRVDATA(ctx, vawr);
    CALL_DRVVTABLE(vawr, vaStatus, vaUnlockSurface( ctx, vawr_render_target));
    RESTORE_VAWRDATA(ctx, vawr);
    WRAPPER_UNLOCK(&vawr->vaapi_mutex);
    return vaStatus;
}

VAStatus DLL_EXPORT
__vaDriverInit_0_37(VADriverContextP ctx);


VAStatus
__vaDriverInit_0_37(  VADriverContextP ctx )
{
    VAStatus vaStatus;
    struct VADriverVTable *i965_vtable = NULL;
    struct VADriverVTable * const vtable = ctx->vtable;
    char *driver_name = "i965";

    if (!vawr)
        vawr = calloc(1, sizeof(*vawr));
    if (!vawr)
	return VA_STATUS_ERROR_ALLOCATION_FAILED;

    LIST_INIT(&vawr->handles);

    i965_vtable = calloc(1, sizeof(*i965_vtable));
    if (!i965_vtable)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    /* Store i965_ctx into wrapper's private driver data
     * It will later be used when i965 VA API is called
     */

    /* By default we should load and initialize OTC's i965 video driver */

    /* First, hook i965_vtable to ctx so that i965_drv_video uses it. */
    ctx->vtable = i965_vtable;

    vawr->drm_state[I965_DRV] = ctx->drm_state;

    /* Then, load the i965 driver */
    vaStatus = vawr_openDriver(ctx, driver_name);

    if (VA_STATUS_SUCCESS == vaStatus) {
	/* We have successfully initialized i965 video driver,
	 * Let's store i965's private driver data and vtable.
	 */
        vawr->drv_data[I965_DRV] = (void *)ctx->pDriverData;
        vawr->drv_vtable[I965_DRV] = i965_vtable;
        vawr->profile = VAProfileNone;

        /* Also restore the va's vtable */
        ctx->vtable = vtable;

        /* Increase max num of profiles and entrypoints for PSB's VP8 profile */
        ctx->max_profiles = ctx->max_profiles + 1;
        ctx->max_entrypoints = ctx->max_entrypoints + 1;

        /* Populate va wrapper's vtable */
        vtable->vaTerminate = vawr_Terminate;
        vtable->vaQueryConfigEntrypoints = vawr_QueryConfigEntrypoints;
        vtable->vaQueryConfigProfiles = vawr_QueryConfigProfiles;
        vtable->vaQueryConfigEntrypoints = vawr_QueryConfigEntrypoints;
        vtable->vaQueryConfigAttributes = vawr_QueryConfigAttributes;
        vtable->vaCreateConfig = vawr_CreateConfig;
        vtable->vaDestroyConfig = vawr_DestroyConfig;
        vtable->vaGetConfigAttributes = vawr_GetConfigAttributes;
        vtable->vaCreateSurfaces = vawr_CreateSurfaces;
        vtable->vaDestroySurfaces = vawr_DestroySurfaces;
        vtable->vaCreateContext = vawr_CreateContext;
        vtable->vaDestroyContext = vawr_DestroyContext;
        vtable->vaCreateBuffer = vawr_CreateBuffer;
        vtable->vaBufferSetNumElements = vawr_BufferSetNumElements;
        vtable->vaBufferInfo = vawr_BufferInfo;
        vtable->vaMapBuffer = vawr_MapBuffer;
        vtable->vaUnmapBuffer = vawr_UnmapBuffer;
        vtable->vaDestroyBuffer = vawr_DestroyBuffer;
        vtable->vaBeginPicture = vawr_BeginPicture;
        vtable->vaRenderPicture = vawr_RenderPicture;
        vtable->vaEndPicture = vawr_EndPicture;
        vtable->vaSyncSurface = vawr_SyncSurface;
        vtable->vaQuerySurfaceStatus = vawr_QuerySurfaceStatus;
        vtable->vaPutSurface = vawr_PutSurface;
        vtable->vaQueryImageFormats = vawr_QueryImageFormats;
        vtable->vaCreateImage = vawr_CreateImage;
        vtable->vaDeriveImage = vawr_DeriveImage;
        vtable->vaDestroyImage = vawr_DestroyImage;
        vtable->vaSetImagePalette = vawr_SetImagePalette;
        vtable->vaGetImage = vawr_GetImage;
        vtable->vaPutImage = vawr_PutImage;
        vtable->vaQuerySubpictureFormats = vawr_QuerySubpictureFormats;
        vtable->vaCreateSubpicture = vawr_CreateSubpicture;
        vtable->vaDestroySubpicture = vawr_DestroySubpicture;
        vtable->vaSetSubpictureImage = vawr_SetSubpictureImage;
        vtable->vaSetSubpictureChromakey = vawr_SetSubpictureChromakey;
        vtable->vaSetSubpictureGlobalAlpha = vawr_SetSubpictureGlobalAlpha;
        vtable->vaAssociateSubpicture = vawr_AssociateSubpicture;
        vtable->vaDeassociateSubpicture = vawr_DeassociateSubpicture;
        vtable->vaQueryDisplayAttributes = vawr_QueryDisplayAttributes;
        vtable->vaGetDisplayAttributes = vawr_GetDisplayAttributes;
        vtable->vaSetDisplayAttributes = vawr_SetDisplayAttributes;
        vtable->vaLockSurface = vawr_LockSurface;
        vtable->vaUnlockSurface= vawr_UnlockSurface;
    }

    LIST_INIT(&vawr->images);

    pthread_mutex_init(&vawr->vaapi_mutex, NULL);

    /* Store wrapper's private driver data*/
    ctx->pDriverData = (void *)vawr;

    return vaStatus;
}
