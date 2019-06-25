/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_fb.c
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
 *  Framebuffer / kernel mode setting functions.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.kms

#include <linux/version.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <drmP.h>
#include <drm_crtc.h>
#include <drm_edid.h>
#include <drm_crtc_helper.h>
#include <linux/vga_switcheroo.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "i915/intel_drv.h"
#include <igd_core_structs.h>
#include "drm_emgd_private.h"
#include "emgd_drv.h"
#include "emgd_drm.h"
#include "memory.h"


#define RETURN_PROBE_TYPE void
#define RETURN_PROBE
#define PROBE_FUNC output_poll_changed



/*------------------------------------------------------------------------------
 * Global Variables
 *------------------------------------------------------------------------------
 */
extern emgd_drm_config_t     *config_drm;

/* CRTC Dispatch Tables */
extern const struct drm_crtc_funcs emgd_crtc_funcs;
extern const struct drm_crtc_helper_funcs emgd_crtc_helper_funcs;

/* Encoder Dispatch Tables */
extern const struct drm_encoder_funcs emgd_encoder_funcs;
extern const struct drm_encoder_helper_funcs emgd_encoder_helper_funcs;

/* Connector Dispatch Tables */
extern const struct drm_connector_funcs emgd_connector_funcs;
extern const struct drm_connector_helper_funcs emgd_connector_helper_funcs;


/*------------------------------------------------------------------------------
 * Formal Declaration
 *------------------------------------------------------------------------------
 */
int emgd_fbcon_initial_config(emgd_fbdev_t *emgd_fbdev, int alloc_fb);
static void emgd_fbdev_destroy(drm_emgd_private_t *priv);

/* overlay related declarations */
extern int emgd_overlay_planes_init(struct drm_device *dev, emgd_crtc_t *emgd_crtc,
		emgd_ovl_config_t * ovl_config);

void emgd_sysfs_addfb(emgd_framebuffer_t *emgdfb);
void emgd_sysfs_rmfb(emgd_framebuffer_t *emgdfb);
void emgd_sysfs_connector_add(emgd_connector_t *emgd_connector);

void emgd_power_init_hw(struct drm_device *dev);

/*------------------------------------------------------------------------------
 * FB Functions
 *------------------------------------------------------------------------------
 */
int emgd_framebuffer_init(struct drm_device *dev,
			emgd_framebuffer_t *emgd_fb,
			struct DRM_MODE_FB_CMD_TYPE *mode_cmd,
			struct drm_i915_gem_object *bo);

int convert_bpp_depth_to_drm_pixel_formal(unsigned int bpp,
		unsigned int depth);

static struct drm_framebuffer *emgd_user_framebuffer_create(
								struct drm_device *dev,
								struct drm_file *filp,
								struct DRM_MODE_FB_CMD_TYPE *r);

RETURN_PROBE_TYPE  emgd_fb_probe(struct drm_device *dev);
static void emgd_user_framebuffer_destroy (struct drm_framebuffer *fb);
static int  emgd_user_framebuffer_create_handle(struct drm_framebuffer *fb,
				struct drm_file *file_priv, unsigned int *handle);

static const struct drm_mode_config_funcs emgd_mode_funcs = {
	.fb_create  = emgd_user_framebuffer_create,
	.PROBE_FUNC = emgd_fb_probe,
	/*.output_poll_changed:  we don't support hotplug */
};

static const struct drm_framebuffer_funcs emgd_fb_funcs = {
	.destroy       = emgd_user_framebuffer_destroy,
	.create_handle = emgd_user_framebuffer_create_handle,
};


/*
 * enumerators for plane gamma property
 */
const struct drm_prop_enum_list emgd_gamma_enum_list[] = {
	{ 0, "Disabled" },
	{ 1, "Enabled" }
};

/*
 * create_crtcs
 *
 * Creates crtcs. Use dsp_alloc_pipe_planes() to create the igd display
 * context(s) and then create one crtc for each display context.
 *
 * @param dev     (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return None
 */
void create_crtcs(struct drm_device *dev)
{
	emgd_crtc_t *emgd_crtc = NULL;
	drm_emgd_private_t *dev_priv = NULL;
	igd_context_t *context = NULL;
	int i, index = 0;
	unsigned short *r, *g, *b;
	unsigned long *dc;
	int err;

	EMGD_TRACE_ENTER;

	dev_priv = ((drm_emgd_private_t *)dev->dev_private);
	context = dev_priv->context;

	/*
	 * FIXME: this is hard coded to support only 2 pipes (crtc's). To
	 * expand this would require changes to the config data structures and
	 * changes to the dsp module.
	 */
	dev_priv->num_crtc = 2;

	/*
	 * Start with a dsp_alloc_pipe_planes(), but always use an extended DC.
	 * Using an extended DC, forces both pipes to get configured.
	 *
	 * Loop through the display_list array and use the values in the
	 * display list to populate the emgd_crtc.
	 */

	/* Query DC looking for an extended DC */
	err = context->drv_dispatch.query_dc(context,
			IGD_DISPLAY_CONFIG_EXTENDED,
			&dc,
			IGD_QUERY_DC_INIT);
	if (err) {
		/*
		 * Error means no matching DC. Try again but without extended
		 * set.  If this succeeds then only configure one CRTC.
		 */
		err = context->drv_dispatch.query_dc(context,
				IGD_DISPLAY_CONFIG_SINGLE,
				&dc,
				IGD_QUERY_DC_INIT);
		if (err) {
			dev_priv->num_crtc = 0;

			EMGD_ERROR_EXIT("Cannot initialize the display as requested."
					"The query_dc() function returned %d.", err);
			return;
		}

		if (!(config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG))
			dev_priv->num_crtc = 1;
	}

	dev_priv->num_pipe = dev_priv->num_crtc;

	/* Allocate and initialize igd_display_context */
	err = context->module_dispatch.dsp_alloc_pipe_planes(context, *dc, 0);
	if (err) {
		EMGD_ERROR_EXIT("Cannot initialize display context."
				"The dsp_alloc_pipe_planes() function returned %d.", err);
		return;
	}


	/*
	 * for each display context:
	 *   create an emgd_crtc.
	 *   populate it with data from the display context.
	 *   initialize the crtc.
	 *   set up initial gamma values.
	 */
	for (index = 0; index < dev_priv->num_crtc; index++) {

		context->module_dispatch.dsp_get_display_crtc(index, &emgd_crtc);
		if (!emgd_crtc) {
			EMGD_ERROR("create_crtcs: get_display() failed to get display.");
			return;
		}

		emgd_crtc->crtc_id     = 1 << index;
		dev_priv->crtcs[index] = emgd_crtc;

		emgd_crtc->primary_regs = kzalloc(sizeof(struct emgd_plane_regs), GFP_KERNEL);
		if (!emgd_crtc->primary_regs) {
			EMGD_ERROR("Out of memory while creating primary_regs!");
			return;
		}

		EMGD_DEBUG("Creating CRTC with ID: 0x%x", emgd_crtc->crtc_id);

		/* Hook up crtc functions */
		drm_crtc_init(dev, &emgd_crtc->base, &emgd_crtc_funcs);
		EMGD_DEBUG("  Created CRTC [%d]", emgd_crtc->base.base.id);

		INIT_LIST_HEAD(&emgd_crtc->pending_flips);

		/* gamma */
		drm_mode_crtc_set_gamma_size(&emgd_crtc->base, 256);

		/* Set initial gamma values */
		r = emgd_crtc->base.gamma_store;
		g = emgd_crtc->base.gamma_store + 256;
		b = emgd_crtc->base.gamma_store + 512;
		for (i = 0; i < 256; i++) {
			emgd_crtc->lut_r[i] = i;
			emgd_crtc->lut_g[i] = i;
			emgd_crtc->lut_b[i] = i;
			emgd_crtc->lut_a[i] = 0;
			emgd_crtc->default_lut_r[i] = i;
			emgd_crtc->default_lut_g[i] = i;
			emgd_crtc->default_lut_b[i] = i;
			emgd_crtc->default_lut_a[i] = 0;
			r[i] = (i << 8);
			g[i] = (i << 8);
			b[i] = (i << 8);
		}

		emgd_crtc->palette_persistence = 0;
		emgd_crtc->vblank_expected = 0;
		emgd_crtc->flip_pending = NULL;
		emgd_crtc->sprite_flip_pending = NULL;

		emgd_crtc->mode_set.crtc       = &emgd_crtc->base;
		emgd_crtc->mode_set.connectors =
							(struct drm_connector **)(emgd_crtc + 1);
		emgd_crtc->mode_set.num_connectors = 0;

		OS_MEMSET(emgd_crtc->vblank_timestamp, 0, sizeof(struct timeval) * MAX_VBLANK_STORE);
		emgd_crtc->track_index = 0;

		/* Hook up crtc helper functions */
		drm_crtc_helper_add(&emgd_crtc->base, &emgd_crtc_helper_funcs);

		emgd_crtc->pipe = index;
		emgd_crtc->plane = index;
		dev_priv->plane_to_crtc_mapping[emgd_crtc->plane] = &emgd_crtc->base;
		dev_priv->pipe_to_crtc_mapping[emgd_crtc->pipe] = &emgd_crtc->base;

		/* initialize the list of overlay planes for each crtc */
		if (emgd_overlay_planes_init(dev, emgd_crtc, config_drm->ovl_config)){
			EMGD_ERROR("Failure initializing ovl planes for crtc %d",
					emgd_crtc->crtc_id);
		}

		emgd_crtc_attach_properties(&emgd_crtc->base);
	}

	EMGD_TRACE_EXIT;
}



/**
 * create_encoder
 *
 * Creates an encoder for the igd_port in the parameter.
 *
 * @param dev      (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param igd_port (IN) pointer to IGD display port.  (pd_driver must be valid)
 *
 * @return None
 */
static void create_encoder(struct drm_device *dev, igd_display_port_t *igd_port)
{
	emgd_encoder_t  *emgd_encoder;
	unsigned long    drm_encoder_type;


	EMGD_TRACE_ENTER;

	/* Find the corresponding DRM encoder type */
	switch(igd_port->port_type) {
		case IGD_PORT_LVDS:
			drm_encoder_type = DRM_MODE_ENCODER_LVDS;
			break;

		case IGD_PORT_DIGITAL:
			drm_encoder_type = DRM_MODE_ENCODER_TMDS;
			break;

		case IGD_PORT_ANALOG:
			drm_encoder_type = DRM_MODE_ENCODER_DAC;
			break;

		case IGD_PORT_TV:
			drm_encoder_type = DRM_MODE_ENCODER_TVDAC;
			break;

		case IGD_PORT_MIPI_PORT:
		default:
			EMGD_ERROR("Invalid Port Type 0x%lx", igd_port->port_type);
			return;
	}


	emgd_encoder = kzalloc(sizeof(*emgd_encoder), GFP_KERNEL);
	if (!emgd_encoder) {
		EMGD_ERROR("Out of memory!");
		return;
	}


	/* What we call "TWIN" is what KMS calls "CLONE".  None of the
	 * platforms we currently support allow TWIN, so just set
	 * the bits equal to the port type since a port can always
	 * "TWIN" with itself */
	emgd_encoder->clone_mask = igd_port->port_type;
	emgd_encoder->base.possible_clones = emgd_encoder->clone_mask;

	emgd_encoder->igd_port   = igd_port;
	emgd_encoder->state.port = igd_port;

	emgd_encoder->base.possible_crtcs  = KMS_PIPE_ID(igd_port->port_features);


	EMGD_DEBUG("Created encoder=0x%lx port=0x%x, port_type=0x%lx",
		(unsigned long)&(emgd_encoder->base),
		igd_port->port_number, igd_port->port_type);

	drm_encoder_init(dev, &emgd_encoder->base, &emgd_encoder_funcs,
				drm_encoder_type);
	drm_encoder_helper_add(&emgd_encoder->base, &emgd_encoder_helper_funcs);
}


/*
 * Scan a list of attributes and create both a DRM property and a
 * mapping of property to attribute for each attribute.
 */
static unsigned long attr_to_property(struct drm_device *dev,
		emgd_connector_t *emgd_connector,
		igd_attr_t *attributes,
		unsigned long max_attrs)
{
	struct drm_connector *drm_connector = &emgd_connector->base;
	int i, j;
	unsigned long current_value;
	struct drm_property *new_prop;
	emgd_property_map_t *map;
	int drm_flag = 0;
	unsigned long count = 0;
	igd_attr_t *attr_ptr;

	/* Convert port attributes to connector properties and attach them */
	for(i = 0; i < max_attrs && attributes[i].id != PD_ATTR_LIST_END; i++) {

		/* Reset the the flag for next iteration */
		drm_flag = 0;

		/* Attribute PD_ATTR_ID_EXTENSION itself should not be
		 * converted to a DRM Property. This will remove unnecessary
		 * error message "Unsupported PD Attribute type 0x0"
		 */
		if (attributes[i].id == PD_ATTR_ID_EXTENSION) {
			continue;
		}

		/*
		 * Invisible attributes are not settable so don't even report
		 * it was a property
		 *
		 * Or should these be read-only properties?
		 */
		if (attributes[i].flags & PD_ATTR_FLAG_USER_INVISIBLE) {
			drm_flag = DRM_MODE_PROP_IMMUTABLE;
			/* continue; */
		}

		attr_ptr = NULL;
		current_value = 0;

		EMGD_DEBUG("ATTR:%s, ID:%d, Val1:0x%x, Val2:0x%x ",
				attributes[i].name,
				attributes[i].id,
				attributes[i].default_value,
				attributes[i].current_value);

		switch (attributes[i].type) {
			case PD_ATTR_TYPE_RANGE:
			{
				igd_range_attr_t *attr = (igd_range_attr_t *) &attributes[i];

				count++;
				drm_flag |= DRM_MODE_PROP_RANGE;
				new_prop = drm_property_create(dev, drm_flag, attr->name, 2);

				if (NULL == new_prop) {
					EMGD_ERROR("Failed to allocate new property");
					continue;
				}

				new_prop->values[0] = attr->min;
				new_prop->values[1] = attr->max;
				current_value       = attr->current_value;
				attr_ptr = &attributes[i];

				break;
			}

			case PD_ATTR_TYPE_BOOL:
			{
				igd_range_attr_t *attr = (igd_range_attr_t *) &attributes[i];

				count++;
				drm_flag |= DRM_MODE_PROP_RANGE;
				new_prop = drm_property_create(dev, drm_flag, attr->name, 2);

				if (NULL == new_prop) {
					EMGD_ERROR("Failed to allocate new property");
					continue;
				}

				new_prop->values[0] = false;
				new_prop->values[1] = true;
				current_value       = attr->current_value;
				attr_ptr = &attributes[i];

				break;
			}

			case PD_ATTR_TYPE_LIST:
			{
				igd_list_attr_t *attr = (igd_list_attr_t *) &attributes[i];
				igd_list_entry_attr_t *entry;

				count++;
				drm_flag |= DRM_MODE_PROP_ENUM;
				new_prop = drm_property_create(dev, drm_flag, attr->name,
						attr->num_entries);

				if (NULL == new_prop) {
					EMGD_ERROR("Failed to allocate new property");
					continue;
				}

				attr_ptr = &attributes[i];
				current_value = attr->current_index;

				/*
				 * All following PD_ATTR_TYPE_LIST_ENTRY structurs should
				 * be part of this to create an enum property.
				 */
				for (j = 0; j < attr->num_entries; j++) {
					i++;
					count++;
					entry = (igd_list_entry_attr_t *)&attributes[i];

					EMGD_DEBUG("ATTR_TYPE_LIST_ENTRY:%s, ID:%d, Value:0x%x",
							entry->name,
							entry->id,
							entry->value);

					drm_property_add_enum(new_prop, j, entry->value,
							entry->name);
				}
				break;
			}
			case PD_ATTR_TYPE_LIST_ENTRY:
				/* Should never see this by its self */
				continue;
			case PD_ATTR_TYPE_BUFFER:
				{
					/*
					igd_buffer_attr_t *attr =
						(igd_buffer_attr_t *) &attributes[i];
					*/

				count++;
				/* This may be convertable to a property blob */
				continue;
				}
			default:
				EMGD_ERROR("Unsupported PD Attribute type 0x%x",
						attributes[i].type );
				continue;
		}

		if (attr_ptr) {

			drm_object_attach_property(&drm_connector->base,
					new_prop,
					current_value);

			map = kzalloc(sizeof(*map), GFP_KERNEL);
			if (map == NULL) {
				EMGD_ERROR("Failed to map new property");
			} else {
				map->property = new_prop;
				map->attribute = attr_ptr;
				list_add_tail(&map->head, &emgd_connector->property_map);
			}
		}
	}

	return count;

}



/**
 * create_connector_properties
 *
 * Creates properties associated with the input connector.  Connector properties
 * are what EMGD calls "port attributes."  The only difference is EMGD's port
 * attributes are per-encoder, not per-connector.  For this implementation, we
 * are assuming one connector per encoder.  With this assumption, we can draw
 * a direct connection between "port attributes" and "connector properties."
 *
 * @param dev            (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param emgd_connector (IN) Selected connector
 *
 * @return None
 */
static void create_connector_properties(struct drm_device *dev,
				emgd_connector_t *emgd_connector)
{
	igd_display_port_t *igd_port = emgd_connector->encoder->igd_port;
	drm_emgd_private_t *priv     = emgd_connector->priv;
	unsigned long       num_of_attrs = 0;
	unsigned long       attrs = 0;
	int                 i;
	pd_attr_t          *attributes;
	igd_attr_t         *extension = NULL;
	unsigned char *data = 0, *new_data =0;
	igd_display_params_t *display_params = NULL;
	igd_param_t *init_params;
	int ret;

	EMGD_TRACE_ENTER;

	init_params = priv->context->module_dispatch.init_params;

	for (i = 0; i < IGD_MAX_PORTS; i++) {
		if (igd_port->port_number == init_params->display_params[i].port_number) {
			display_params = &init_params->display_params[i];
			break;
		}
	}

	/* Get port attributes from the port driver */
	priv->context->drv_dispatch.get_attrs(priv->context,
								igd_port->port_number,
								&num_of_attrs,
								&attributes);


	/* If the connector has no attributes, then return */
	if (0 >= num_of_attrs) {
		EMGD_TRACE_EXIT;
		return;
	}

	INIT_LIST_HEAD(&emgd_connector->property_map);

	/*
	 * The number of attributes returned from the get_attrs() call above
	 * is misleading for two reasons.
	 *
	 * 1) The list is really a list of HAL attributes. If there are
	 *    port driver attributes, then they are pointed to by special
	 *    attribute on the list.
	 * 2) For list attributes, each list entry shows up as a separate
	 *    attribute.
	 */

	/* First, make a pass through the list looking for an extension attr */
	for (i = 0; attributes[i].id != PD_ATTR_LIST_END; i++) {
		if (attributes[i].id == PD_ATTR_ID_EXTENSION) {
			igd_extension_attr_t *attr = (igd_extension_attr_t *)&attributes[i];

			extension = attr->extension;
			break;
		}
	}

	/* process the main attribute list */
	attrs = attr_to_property(dev, emgd_connector, attributes, num_of_attrs);

	/* process the port attribute list */
	if (extension != NULL) {
		attr_to_property(dev, emgd_connector, extension, num_of_attrs - attrs);
	}


	if(!display_params || (display_params->flags & IGD_DISPLAY_READ_EDID))
	{
	    /* If EDID is available, create EDID property blob */
    	    data = kmalloc(EDID_LENGTH, GFP_KERNEL);
	    if(!data) {
		EMGD_ERROR("Failed to allocate memory to read EDID");
		return;
 	    }

	    ret = priv->context->drv_dispatch.get_EDID_block(priv->context,
								igd_port->port_number,
								data, 0);

	    /* For EDID, read the extension block if present
	     * krealloc will preserve the old data
	     */
	    if(!ret && data[0x7e] > 0) {
		new_data = krealloc(data, (data[0x7e] + 1) * EDID_LENGTH, GFP_KERNEL);

		if(!new_data) {
			kfree(data);
			EMGD_ERROR("Failed to allocate memory to read EDID extension");
			return;
		}

		data = new_data;

		for(i=1; i<= data[0x7e]; i++) {
			ret = priv->context->drv_dispatch.get_EDID_block(priv->context,
										igd_port->port_number,
										data + EDID_LENGTH, i);
		}
	    }

	    if(!ret) {
		/*
		 * If display_params is NOT defined (NULL), always create edid
		 * property blob.
		 *
		 * If display_params is defined, check if user want to use edid.
		 */
		if(!display_params ||
				(display_params->edid_avail & IGD_DISPLAY_USE_EDID)) {
			ret = drm_mode_connector_update_edid_property(&emgd_connector->base,
													(struct edid *)data);
			if(!ret) {
				EMGD_DEBUG("Port=%d, EDID property created successfully",
						igd_port->port_number);
			} else {
				/*if failed set EDID property NULL*/
				drm_mode_connector_update_edid_property(&emgd_connector->base, NULL);
				EMGD_ERROR("Port=%d, failed to create EDID property",
						igd_port->port_number);
			}
		}
	    } else {
		/*if failed set EDID property NULL*/
		drm_mode_connector_update_edid_property(&emgd_connector->base, NULL);
		EMGD_ERROR("Port=%d, failed to read EDID to create property",
				igd_port->port_number);
	    }

	    kfree(data);
	}

	EMGD_TRACE_EXIT;
}





/**
 * create_connector_display_size
 *
 * Creates display size with the input connector.  Connector display size is
 * describes the given display's limitations. the width_mm&height_mm
 * describes the physical size of the display in millimetre
 * @param emgd_connector (IN) Selected connector
 *
 * @return None
 */
static void create_connector_display_size(emgd_connector_t *emgd_connector)
{
	struct edid *edid = 0;

	EMGD_TRACE_ENTER;

	if (emgd_connector->base.edid_blob_ptr == NULL)
		return;

	edid = (struct edid *)emgd_connector->base.edid_blob_ptr->data;

	emgd_connector->base.display_info.width_mm = edid->width_cm * 10;
	emgd_connector->base.display_info.height_mm = edid->height_cm * 10;

	EMGD_TRACE_EXIT;
}



/**
 * create_connectors
 *
 * Creates connectors associated with the encoder.
 *
 * This function currently supports one connector per encoder.  Further
 * development required in the future to support encoders that have more
 * than one connector.
 *
 * @param dev          (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param emgd_encoder (IN) Encoder to be examined.
 *
 * @return None
 */
static void create_connectors(struct drm_device *dev,
				emgd_encoder_t *emgd_encoder,
				igd_param_t *params)
{
	igd_display_port_t    *port = emgd_encoder->igd_port;
	pd_driver_t           *pd   = port->pd_driver;
	unsigned long          connector_type = DRM_MODE_CONNECTOR_LVDS;
	emgd_connector_t      *emgd_connector;


	EMGD_TRACE_ENTER;

	switch (pd->type) {
		case PD_DISPLAY_LVDS_EXT:
		case PD_DISPLAY_LVDS_INT:
			connector_type = DRM_MODE_CONNECTOR_LVDS;
			break;

		case PD_DISPLAY_FP_EXT:
			connector_type = DRM_MODE_CONNECTOR_DVID;
			break;

		case PD_DISPLAY_CRT_EXT:
		case PD_DISPLAY_CRT_INT:
			connector_type = DRM_MODE_CONNECTOR_VGA;
			break;

		case PD_DISPLAY_HDMI_INT:
			connector_type = DRM_MODE_CONNECTOR_HDMIA;
			break;

		case PD_DISPLAY_HDMI_EXT:
			connector_type = DRM_MODE_CONNECTOR_HDMIA;
			break;

		case PD_DISPLAY_DP_INT:
			connector_type = DRM_MODE_CONNECTOR_DisplayPort;
			break;

		case PD_DISPLAY_TVOUT_EXT:
		case PD_DISPLAY_TVFP:
		case PD_DISPLAY_RGBA:
		case PD_DISPLAY_TVOUT_INT:
		case PD_DISPLAY_DRGB:
			EMGD_ERROR("Unsupported connector type");
			return;

		default:
			EMGD_ERROR("Unknown connector type");
			return;
	}

	switch (port->port_number) {
	case IGD_PORT_TYPE_ANALOG:
		emgd_encoder->hpd_pin = HPD_CRT;
		break;
	case IGD_PORT_TYPE_SDVOC:
		emgd_encoder->hpd_pin = HPD_SDVO_C;
		break;
	case IGD_PORT_TYPE_SDVOB:
		emgd_encoder->hpd_pin = HPD_SDVO_B;
		break;
	case IGD_PORT_TYPE_DPB:
		emgd_encoder->hpd_pin = HPD_DP_B;
		break;
	case IGD_PORT_TYPE_DPC:
		emgd_encoder->hpd_pin = HPD_DP_C;
		break;
	default:
		EMGD_ERROR("Unknown port");
		break;
	}

	/* Allocate a new connector */
	emgd_connector = kzalloc(sizeof(*emgd_connector), GFP_KERNEL);
	if (!emgd_connector) {
		EMGD_ERROR("Out of memory!");
		return;
	}

	drm_connector_init(dev, &emgd_connector->base, &emgd_connector_funcs,
											connector_type);

	/*
	 * Possible fix for ever increasing output id's.  However
	 * this could conflict with other graphics drivers in the system
	 *
	emgd_connector->base.connector_type_id = port->port_number;
	*/

	drm_mode_connector_attach_encoder(&emgd_connector->base,
			&emgd_encoder->base);

	drm_connector_helper_add(&emgd_connector->base,
			&emgd_connector_helper_funcs);

	EMGD_DEBUG("Creating connector=0x%lx, encoder=0x%lx, type=0x%lx",
			(unsigned long)&(emgd_connector->base),
			(unsigned long)&(emgd_encoder->base), connector_type);

	emgd_connector->encoder                          = emgd_encoder;
	emgd_connector->priv                             = dev->dev_private;
	emgd_connector->base.display_info.subpixel_order = SubPixelHorizontalRGB;
	emgd_connector->base.interlace_allowed           = false;
	emgd_connector->base.doublescan_allowed          = false;
	emgd_connector->base.encoder                     = &emgd_encoder->base;

	if (!(params->display_flags & IGD_DISPLAY_DETECT)) {
		emgd_connector->base.force  = DRM_FORCE_ON;
	}

	/* Create and attach connector properties */
	create_connector_properties(dev, emgd_connector);
	/* Create connector display size */
	create_connector_display_size(emgd_connector);
	/* Detect connectot port status */
	emgd_connector->base.status =
		emgd_connector->base.funcs->detect(&emgd_connector->base, true);

	drm_sysfs_connector_add(&emgd_connector->base);

	emgd_sysfs_connector_add(emgd_connector);

	EMGD_TRACE_EXIT;
}



/**
 * emgd_setup_outputs
 *
 * This function enumerates all the available outputs (physical connectors) by
 * first initializing all the encoders in the system, and then querying
 * the encoders for the connectors.
 *
 * Because we are adapting from EMGD, the real work behind detecting encoders
 * has already been done by the time we get to this function.  Therefore,
 * all we need to do is using existing EMGD HAL dispatch functions to complete
 * the task.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return
 */
void emgd_setup_outputs(struct drm_device *dev, igd_param_t *params)
{
	drm_emgd_private_t      *priv            = dev->dev_private;
	igd_context_t           *igd_context     = priv->context;
	igd_module_dispatch_t *module_dispatch   = &igd_context->module_dispatch;
	igd_display_port_t      *port            = NULL;
	struct drm_encoder      *encoder;


	EMGD_TRACE_ENTER;

	/* Loop through all available ports.  What KMS calls "encoder" is a
	 * subset of what EMGD calls "port."
	 */
	while ((port = module_dispatch->dsp_get_next_port(igd_context, port, 0))) {

		/* If there is a port driver, then there's an encoder */
		if (port->pd_driver) {
			create_encoder(dev, port);
		}
	}


	/* For each encoder, create the connectors on the encoder */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
			emgd_encoder_t *emgd_encoder;

			emgd_encoder = container_of(encoder, emgd_encoder_t, base);
			create_connectors(dev, emgd_encoder, params);
	}

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_atomic_init(dev);
#endif

	EMGD_TRACE_EXIT;
}

/**
 * emgd_destroy_overlay_properties
 *
 * Destroys the overlay properties created by
 * emgd_create_overlay_properties.
 *
 * @param dev          (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return None
 */

static void emgd_destroy_overlay_properties(struct drm_device *dev)
{

	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Destroy overlay plane properties */
	if (dev_priv->ovl_properties.brightness)
		drm_property_destroy(dev, dev_priv->ovl_properties.brightness);
	if (dev_priv->ovl_properties.contrast)
		drm_property_destroy(dev, dev_priv->ovl_properties.contrast);
	if (dev_priv->ovl_properties.saturation)
		drm_property_destroy(dev, dev_priv->ovl_properties.saturation);
	if (dev_priv->ovl_properties.hue)
		drm_property_destroy(dev, dev_priv->ovl_properties.hue);
	if (dev_priv->ovl_properties.gamma_red)
		drm_property_destroy(dev, dev_priv->ovl_properties.gamma_red);
	if (dev_priv->ovl_properties.gamma_green)
		drm_property_destroy(dev, dev_priv->ovl_properties.gamma_green);
	if (dev_priv->ovl_properties.gamma_blue)
		drm_property_destroy(dev, dev_priv->ovl_properties.gamma_blue);
	if (dev_priv->ovl_properties.gamma_enable)
		drm_property_destroy(dev, dev_priv->ovl_properties.gamma_enable);
	if (dev_priv->ovl_properties.const_alpha)
		drm_property_destroy(dev, dev_priv->ovl_properties.const_alpha);

	return;
}

/**
 * emgd_create_overlay_properties
 *
 * Creates overlay properties to be attached to overlay planes.
 *
 * @param dev          (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return Returns 0 in case of success and -ENOMEM in case of failure
 */

static int  emgd_create_overlay_properties(struct drm_device *dev)
{

	drm_emgd_private_t *dev_priv = (drm_emgd_private_t *)dev->dev_private;
	EMGD_TRACE_ENTER

	/* Create overlay plane properties */
	if (!(dev_priv->ovl_properties.brightness =
		drm_property_create_range(dev, 0, "brightness", 0, 0xFF))) {
		EMGD_ERROR("property brightness could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.contrast =
		drm_property_create_range(dev, 0, "contrast", 0, 0xFF))) {
		EMGD_ERROR("property contrast could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.saturation =
		drm_property_create_range(dev, 0, "saturation", 0, 0x3FF))) {
		EMGD_ERROR("property saturation could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.hue =
		drm_property_create_range(dev, 0, "hue", 0x0,
				0xFFFF))) {
		EMGD_ERROR("property hue could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.gamma_red =
		drm_property_create_range(dev, 0, "gamma_red", 0x0, 0xffff))) {
		EMGD_ERROR("property gamma_red could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.gamma_green =
		drm_property_create_range(dev, 0, "gamma_green", 0x0, 0xffff))) {
		EMGD_ERROR("property gamma_green could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.gamma_blue =
		drm_property_create_range(dev, 0, "gamma_blue", 0x0, 0xffff))) {
		EMGD_ERROR("property gamma_blue could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.gamma_enable =
		drm_property_create_enum(dev, 0, "gamma_enable",
				emgd_gamma_enum_list, ARRAY_SIZE(emgd_gamma_enum_list)))) {
		EMGD_ERROR("enumerator gamma_enable could not be created");
		goto out;
	}
	if (!(dev_priv->ovl_properties.const_alpha =
		drm_property_create_range(dev, 0, "constant_alpha", 0x0, 0xffff))) {
		EMGD_ERROR("property const_alpha could not be created");
		goto out;
	}
	return 0;
out:
	/* In case one of the properties cannot be created we return failure
	 * and destroy all the properties that were created. Even if the
	 * properties are not a critical component, a failure here should be
	 * considered critical.
	 */
	EMGD_ERROR("Failed to create properties");
	emgd_destroy_overlay_properties(dev);
	EMGD_TRACE_EXIT
	return -ENOMEM;
}

/**
 * Cleans up drm properties created in call to emgd_create_drm_properties
 * @param Pointer to drm device structure
 */
void emgd_destroy_drm_properties(struct drm_device *dev)
{
	drm_emgd_private_t  *priv = dev->dev_private;

	drm_property_destroy(dev, priv->crtc_properties.z_order_prop);
	drm_property_destroy(dev, priv->crtc_properties.fb_blend_ovl_prop);
	emgd_destroy_overlay_properties(dev);

}

/**
 * Creates drm properties for planes and crtcs
 * @param dev Pointer to drm device structure
 * @return Returns 0 if properties were created successfully
 */
int emgd_create_drm_properties(struct drm_device *dev)
{
	drm_emgd_private_t  *priv = dev->dev_private;
	EMGD_TRACE_ENTER;

	/* Create crtc properties */
	priv->crtc_properties.z_order_prop = drm_property_create_range(dev, 0,
			"Z_ORDER", 0, UINT_MAX);
	if (!priv->crtc_properties.z_order_prop) {
		goto out;
	}

	priv->crtc_properties.fb_blend_ovl_prop = drm_property_create_range(dev,
			0, "FB_BLEND_OVL", 0, 1);
	if (!priv->crtc_properties.fb_blend_ovl_prop) {
		drm_property_destroy(dev, priv->crtc_properties.z_order_prop);
		goto out;
	}
	/* Create overlay properties*/
	if (emgd_create_overlay_properties(dev))
		goto out;

	EMGD_TRACE_EXIT;
	return 0;

out:
	EMGD_ERROR("Failed to create drm properties");
	EMGD_TRACE_EXIT
	return -ENOMEM;
}



/**
 * emgd_modeset_init
 *
 * This is the main initialization entry point.  Called during driver load
 * and does basic setup.
 *
 * @param dev    (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param params (IN) EMGD HAL parameters
 *
 * @return None
 */
void emgd_modeset_init(struct drm_device *dev, igd_param_t *params)
{
	drm_emgd_private_t *priv = (drm_emgd_private_t *)dev->dev_private;
	struct drm_i915_private *dev_priv = dev->dev_private;
	igd_context_t * context = priv->context;
	int ret;
	struct drm_encoder *encoder;

	EMGD_TRACE_ENTER;

	dev->mode_config.min_width  = 0;
	dev->mode_config.max_width  = 8192;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_height = 8192;
	dev->mode_config.funcs      = (void *)&emgd_mode_funcs;

	dev->mode_config.fb_base = dev_priv->gtt.mappable_base;

	ret = emgd_create_drm_properties(dev);
	if (ret)
		return;

	/* Create the crtcs */
	create_crtcs(dev);

	drm_mode_create_scaling_mode_property(dev);

	emgd_setup_outputs(dev, params);

	emgd_power_init_hw(dev);

	/* Need to make sure outputs are off. Is there someplace else to do
	 * this? */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct drm_encoder_helper_funcs *e_funcs = encoder->helper_private;
		(*e_funcs->dpms)(encoder, DRM_MODE_DPMS_OFF);
	}

	/* Make sure the VGA plane is disabled */
	context->mode_dispatch->kms_disable_vga(priv->context);


	/* Initialize VBLANK handling */
	dev->irq_enabled      = true;
	dev->max_vblank_count = 0xffffff; /* 24-bit frame counter */

	ret = drm_vblank_init(dev, priv->num_crtc);
	if (ret) {
		EMGD_ERROR("Call to drm_vblank_init() failed.  drmWaitVBlank() "
			"will not work.");
		dev->irq_enabled = false;
	}


	/* disable all the crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	/* Initialize the framebuffer device */
	/* emgd_fbdev_init(priv); */

	EMGD_TRACE_EXIT;
}

/**
 * emgd_power_init_hw
 *
 * This is the main initialization entry point for the hw concerning power.
 *
 * @param dev    (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return None
 */
void emgd_power_init_hw(struct drm_device *dev)
{
	drm_emgd_private_t *priv = (drm_emgd_private_t *)dev->dev_private;
	igd_context_t * context = priv->context;

	intel_init_power_well(dev);

	/*
	 * OTC initializes their display driver here.
	 * intel_prepare_ddi(dev);
	 */

	context->pwr_dispatch->im_pwr_init_clock_gating(dev);

	mutex_lock(&dev->struct_mutex);
	intel_enable_gt_powersave(dev);
	mutex_unlock(&dev->struct_mutex);
}


/**
 * emgd_modeset_destroy
 *
 * Clean up resources allocated in emgd_modeset_init.  Called during driver
 * unload
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return None
 */
void emgd_modeset_destroy(struct drm_device *dev)
{

	EMGD_TRACE_ENTER;

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_atomic_fini(dev);
#endif

	emgd_destroy_drm_properties(dev);

	emgd_fbdev_destroy(dev->dev_private);

	intel_disable_fbc(dev);

	intel_disable_gt_powersave(dev);

	flush_scheduled_work();

	drm_mode_config_cleanup(dev);

	EMGD_TRACE_EXIT;
}



/*
 * emgd_fb_probe
 *
 * Do the fb console initialization and modeset for each crtc
 * for hotplug in console mode.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return TBD
 */
RETURN_PROBE_TYPE emgd_fb_probe(struct drm_device *dev)
{
	drm_emgd_private_t *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	struct drm_fb_helper *fb_helper = &dev_priv->emgd_fbdev->helper;
	EMGD_TRACE_ENTER;

	/*
	 * If fbconsole is active, re-initialize it to handle any
	 * possible change is attached display device.
	 */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc && (crtc->fb == fb_helper->fb)) {
			emgd_fbcon_initial_config(dev_priv->emgd_fbdev, 0);
			goto out;
		}
	}
out:

	EMGD_TRACE_EXIT;
	RETURN_PROBE;
}
EXPORT_SYMBOL(emgd_fb_probe);


/**
 * emgd_user_framebuffer_create
 *
 * Handles the DRM_IOCTL_MODE_ADDFB ioctl (which is invoked by userspace
 * drmModeAddFB in libdrm).
 *
 * @param dev      (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param filp     (IN)
 * @param mode_cmd (IN) Ioctl structure containing GEM buffer handle
 *
 * @return pointer to allocated FB instance
 */
static struct drm_framebuffer *emgd_user_framebuffer_create(
								struct drm_device *dev,
								struct drm_file *filp,
								struct DRM_MODE_FB_CMD_TYPE *mode_cmd)
{
	emgd_framebuffer_t *emgd_fb;
	struct drm_i915_gem_object *bo;
	struct drm_i915_gem_object *primary_bo = NULL;
	int i, j;
	__u32 handle;
	int ret;

	EMGD_TRACE_ENTER;

	/* Allocate an EMGD framebuffer structure to wrap around this bo */
	emgd_fb = kzalloc(sizeof(*emgd_fb), GFP_KERNEL);
	if (!emgd_fb) {
		EMGD_ERROR("Failed to allocate DRM framebuffer");
		EMGD_TRACE_EXIT
		return ERR_PTR(-ENOMEM);
	}

	/* Retrieve the GEM buffer object whose handle was passed from userspace */
	handle = mode_cmd->DRMMODE_HANDLE;
	primary_bo = to_intel_bo(drm_gem_object_lookup(dev, filp, handle));
	if (!primary_bo) {
		EMGD_ERROR("Userspace passed invalid bo handle[0]: 0x%x", handle);
		OS_FREE(emgd_fb);
		EMGD_TRACE_EXIT
		return ERR_PTR(-ENOENT);
	}

	/* Process the remaining handles[1-3] if not NULL */
	for (i=0; i<3; i++) {
		if(mode_cmd->handles[i+1]) {
			bo = to_intel_bo(drm_gem_object_lookup(dev, filp, mode_cmd->handles[i+1]));
			if (!bo) {
				EMGD_ERROR("Userspace passed invalid bo handle[%d]: 0x%x", i+1, mode_cmd->handles[i+1]);
				/* Unreference bo and other_bo[] if not NULL */
				drm_gem_object_unreference_unlocked(&primary_bo->base);
				for (j=0; j < 3; j++){
					if(emgd_fb->other_bo[j]) {
						drm_gem_object_unreference_unlocked(&emgd_fb->other_bo[j]->base);
					}
	      		}
				OS_FREE(emgd_fb);
				EMGD_TRACE_EXIT
				return ERR_PTR(-ENOENT);
			}
			emgd_fb->other_bo[i] = bo;
		}
	}

	/* Initialize framebuffer instance */
	ret = emgd_framebuffer_init(dev, emgd_fb, mode_cmd, primary_bo);
	if (ret) {
			drm_gem_object_unreference_unlocked(&primary_bo->base);
			for (i=0; i < 3; i++){
				if (emgd_fb->other_bo[i]) {
					drm_gem_object_unreference_unlocked(&emgd_fb->other_bo[i]->base);
				}
      		}
		OS_FREE(emgd_fb);
		EMGD_TRACE_EXIT
		return ERR_PTR(ret);
	}

	/* Add sysfs node for this framebuffer */
	emgd_sysfs_addfb(emgd_fb);

	EMGD_DEBUG("fb id return is %d", emgd_fb->base.base.id);

	EMGD_TRACE_EXIT;

	return &emgd_fb->base;
}


/**
 * convert_bpp_depth_to_drm_pixel_formal()
 *
 * Converts a bpp and a depth to an DRM pixel format.
 * (Follow the drm_mode_legacy_fb_format function to
 *  figure out which DRM pixel format or directly using
 *  drm_mode_legacy_fb_format??)
 */
int convert_bpp_depth_to_drm_pixel_formal(unsigned int bpp, unsigned int depth)
{
	switch(bpp){
		case 8:
			if(depth == 8)
				return DRM_FORMAT_RGB332;

		case 16:
			if(depth == 15)
				return DRM_FORMAT_XRGB1555;
			else
				return DRM_FORMAT_RGB565;

		case 24:
			return DRM_FORMAT_RGB888;

		case 32:
            if(depth == 24)
                    return DRM_FORMAT_XRGB8888;
            else if (depth == 30)
                    return DRM_FORMAT_XRGB2101010;
            else
                    return DRM_FORMAT_ARGB8888;

		default:
            EMGD_ERROR("bad bpp %d, assuming x8r8g8b8 pixel format", bpp);
            return DRM_FORMAT_XRGB8888;
	}
}

/**
 * emgd_framebuffer_init
 *
 * Initializes a DRM framebuffer with a GEM buffer object.
 *
 * @param dev      (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param emgd_fb  (IN)
 * @param mode_cmd (IN) Input from the DRM_IOCTL_MODE_ADDFB
 * @param bo       (IN) GEM buffer object represented by this framebuffer
 *
 * @return int return value
 */
int emgd_framebuffer_init(struct drm_device *dev,
		emgd_framebuffer_t *emgd_fb,
		struct DRM_MODE_FB_CMD_TYPE *mode_cmd,
		struct drm_i915_gem_object *bo)
{
	int ret;

	EMGD_TRACE_ENTER;

	/*
	 * The following constraints match OTC's code.  These could probably
	 * be moved to the device dependent HAL layer.
	 */
	if (bo->tiling_mode == I915_TILING_Y || mode_cmd->pitches[0] & 0x3f) {
		return -EINVAL;
	}

	emgd_fb->igd_pixel_format = convert_pf_drm_to_emgd(mode_cmd->pixel_format);

	emgd_fb->igd_flags |= IGD_ENABLE_DISPLAY_GAMMA;

	/* Okay, everything looks good.  Call device-independent framebuffer init */
	ret = drm_framebuffer_init(dev, &emgd_fb->base, &emgd_fb_funcs);
	if (ret) {
		EMGD_ERROR("DRM framebuffer initialization failed: %d", ret);
		return ret;
	}
	drm_helper_mode_fill_fb_struct(&emgd_fb->base, mode_cmd);
	emgd_fb->bo = bo;

	EMGD_TRACE_EXIT;
	return 0;
}



/*
 * emgd_user_framebuffer_destroy
 *
 * clean up and remove a framebuffer instance.
 */
static void emgd_user_framebuffer_destroy (struct drm_framebuffer *fb)
{
	emgd_framebuffer_t *emgd_fb = container_of(fb, emgd_framebuffer_t, base);
	int i;

	EMGD_TRACE_ENTER;

	/* Sysfs cleanup */
	emgd_sysfs_rmfb(emgd_fb);

	/* Device independent DRM framebuffer cleanup */
	drm_framebuffer_cleanup(fb);

	/* Release the reference to the underlying GEM buffer */
	drm_gem_object_unreference_unlocked(&emgd_fb->bo->base);

	for (i=0; i < 3; i++){
		if(emgd_fb->other_bo[i] && !emgd_fb->other_bo[i]->pin_count) {
				drm_gem_object_unreference_unlocked(&emgd_fb->other_bo[i]->base);
		}
	}

	/* Release device-specific framebuffer structure */
	kfree(emgd_fb);

	EMGD_TRACE_EXIT;
}



/*
 * emgd_user_framebuffer_create_handle
 *
 * Returns a handle associated with the drm_framebuffer given in the
 * parameter.  This increments the reference count of the underlying
 * GEM object.
 *
 * @param fb (IN)        DRM framebuffer to look up
 * @param file (IN)
 * @param handle (OUT)
 *
 * @return 0 on success
 * @return -EINVAL on failure
 */
static int emgd_user_framebuffer_create_handle(struct drm_framebuffer *fb,
		struct drm_file *file,
		unsigned int *handle)
{
	emgd_framebuffer_t *emgd_fb = container_of(fb, emgd_framebuffer_t, base);
	int ret;

	ret = drm_gem_handle_create(file, &emgd_fb->bo->base, handle);
	EMGD_DEBUG("Created handle %d for framebuffer", *handle);

	return ret;
}

/**
 * emgd_fbdev_init
 *
 * Allocates and initializes a framebuffer device.  Whereas normal DRM
 * drivers use drm_fb_helper_initial_config() to setup a sane mode and
 * allocate a framebuffer surface to go with the framebuffer device,
 * we call a custom function to configure the initial config based
 * on the settings in default_config.c.
 *
 * @param priv (IN) EMGD private DRM data structure
 *
 * @return 0 on success
 */

int emgd_fbdev_init(drm_emgd_private_t *priv)
{
	emgd_fbdev_t *emgd_fbdev;
	int ret;

	EMGD_TRACE_ENTER;

	emgd_fbdev = OS_ALLOC(sizeof(emgd_fbdev_t));
	if (!emgd_fbdev) {
		return -ENOMEM;
	}

	emgd_fbdev->priv = priv;
	priv->emgd_fbdev = emgd_fbdev;

	ret = drm_fb_helper_init(priv->dev, &emgd_fbdev->helper,
		priv->num_pipe, INTELFB_CONN_LIMIT);
	if (ret) {
		OS_FREE(emgd_fbdev);
		EMGD_ERROR("emgd_fbdev failed to allocate!!!");
		priv->emgd_fbdev = NULL;
		return ret;
	}

	/* Register all of the connectors with the fb helper */
	drm_fb_helper_single_add_all_connectors(&emgd_fbdev->helper);

	/*
	 * Setup our initial configuration based on default_config.c rather than
	 * using the standard helper.
	 */
	emgd_fbcon_initial_config(emgd_fbdev, 1);

	EMGD_TRACE_EXIT;
	return 0;
}



/**
 * emgd_fbdev_destroy
 *
 * Cleans up resources allocated during emgd_fbdev_init.
 * It is unclear if this clean up is necessary because KMS APIs maybe smart
 * enough to call the corresponding destory function.
 *
 * @param priv (IN) EMGD private DRM data structure
 *
 * @return None
 */
static void emgd_fbdev_destroy(drm_emgd_private_t *priv)
{
	emgd_fbdev_t    * emgd_fbdev = NULL;
	emgd_framebuffer_t * fb      = NULL;
	struct drm_crtc * crtc = NULL;
	emgd_crtc_t     * emgd_crtc = NULL;
	struct drm_mode_set modeset;

	EMGD_TRACE_ENTER;

	/* The "OTHER" FB first!
	 * 
	 * Since this function basically does the opposite of the fb_initial_config, 
	 * then we should check for 2nd framebuffer on the 2nd crtc and clean that.
	 * (as how fb_initial_config set that up for us in the beginning).
	 * Also, take note that emgd_last_close only was catering to the primary fbdev
	 * which only effected the primary crtc (and secondary in clone?). 
	 * But in dual display FB mode, that 2nd crtc is probably still be on 
	 * so lets' unset that 2nd mode here and ensure we free that 2nd framebuffer
	 */
	if ( (config_drm->crtcs[1].framebuffer_id !=
		config_drm->crtcs[0].framebuffer_id) &&
		(priv->initfb_info2.bo != NULL) ) {
		EMGD_DEBUG("Found the other FB");
		/* so we have 2 framebuffers, if the 2nd one is actually tied to the
		 * other crtc, then lets unset the mode first before freeing that fb.
		 * Theoratically, based on the initial_config code, its not just
		 * the 2nd framebuffer, this is for every non-first crtc 
		 * so we'll loop it the same way here for cleanup.
		 */
		list_for_each_entry(crtc, &priv->dev->mode_config.crtc_list, head) {
			EMGD_DEBUG("Check other CRTC mode'd to other FB");
			if (crtc != NULL) {
				emgd_crtc = container_of(crtc, emgd_crtc_t, base);
				/* all together! - in reverse order ! */
				/* Step 1 --> unset the CRTC mode first*/
				if ((crtc->fb != NULL) && 
				    (crtc->fb == &priv->initfb_info2.base)) {
					EMGD_DEBUG("Found a CRTC with other FB - unset mode");
					/* only unset the mode this CRTC is in fact
					 * pointing to our secondary fb */
					/* all together! - in reverse order ! */
					/* Step 1 --> unset the CRTC mode */
					memset(&modeset, 0, sizeof(modeset));
					modeset.crtc = crtc;
					/* from kernel 3.5 thru 3.10-Rc1, this is way to disable CRTC */
					mutex_unlock(&priv->dev->struct_mutex);
					drm_crtc_helper_set_config(&modeset);
					mutex_lock(&priv->dev->struct_mutex);
				}
			}
		}
		/* Step 2 --> free up the drm_framebuffer */
		EMGD_DEBUG("Free up the other FB from DRM and GEM");
		drm_framebuffer_cleanup(&priv->initfb_info2.base);
		/* Step 3 --> free up the gem object */
		mutex_unlock(&priv->dev->struct_mutex);
		drm_gem_object_unreference_unlocked(&priv->initfb_info2.bo->base);
		mutex_lock(&priv->dev->struct_mutex);
		/* Step 4 --> free emgd_framebuffer_t - we alloc'd this */
		memset(&priv->initfb_info2, 0, sizeof(emgd_framebuffer_t));
	}
	
	/* The "MAIN" FB next!
	 *
	 * This is the only FB that actually registers with Fbdev and console
	 * So the cleanup is slightly different here - no need to worry about
	 * "unsetting" the mode of a CRTC because fbdev / console would have
	 * dont the proper cleanup during driver_lastclose.
	 * But of course we need to do things like unregister_framebuffer 
	 * and iounmap (Take note we purposely double pin this particular FB
	 * during initial_fb_config, but we dont explicitly unpin it because
	 * the "drm_object_unreference_unlocked" should handle that (big loop
	 * back up DRM and then down into GEM again.
	 */
	if (priv->fbdev) {
		/* NOTE: the connection between priv->fbdev and priv->emgd_fbdev:
		 * 1. priv->fbdev->par   = emgd_fbdev
		 * 2. priv->fbdev->screen_base = emgd_fbdev->emgd_fb->bo
		 */
		emgd_fbdev = priv->emgd_fbdev;
		fb         = emgd_fbdev->emgd_fb;
		/* NOTE: priv->emgd_fbdev->emgd_fb = &priv->initfb_info */
		/* thus, this local "fb->bo" = "priv->initfb_info.bo"*/
		
		unregister_framebuffer(priv->fbdev);

		if (priv->fbdev->cmap.len) {
			fb_dealloc_cmap(&priv->fbdev->cmap);
		}

		iounmap(priv->fbdev->screen_base);

		framebuffer_release(priv->fbdev);
		/* we alloc'd this- but DONT free it - unregister or release will free it*/
		/*kfree(priv->fbdev);*/
		priv->fbdev = NULL;

		drm_framebuffer_cleanup(&fb->base);

		if (fb->bo) {
			/*intel_unpin_fb_obj(fb->bo);*/
			mutex_unlock(&priv->dev->struct_mutex);
			drm_gem_object_unreference_unlocked(&fb->bo->base);
			mutex_lock(&priv->dev->struct_mutex);
			fb->bo = NULL;
		}
		
		/* dont free this - a pointer drm_emgd_private_t member*/
		emgd_fbdev->emgd_fb = NULL;

		/* free this - we allocated it during fbdev init */
		OS_FREE(priv->emgd_fbdev);
		priv->emgd_fbdev = NULL;

	}
	
	EMGD_TRACE_EXIT;
}


void intel_mark_busy(struct drm_device *dev)
{
     intel_sanitize_pm(dev);
     i915_update_gfx_val(dev->dev_private);
}

void intel_mark_idle(struct drm_device *dev)
{
	    intel_sanitize_pm(dev);
}



struct intel_display_error_state {
	struct intel_cursor_error_state {
		u32 control;
		u32 position;
		u32 base;
		u32 size;
	} cursor[2];

	struct intel_pipe_error_state {
		u32 conf;
		u32 source;

		u32 htotal;
		u32 hblank;
		u32 hsync;
		u32 vtotal;
		u32 vblank;
		u32 vsync;
	} pipe[2];

	struct intel_plane_error_state {
		u32 control;
		u32 stride;
		u32 size;
		u32 pos;
		u32 addr;
		u32 surface;
		u32 tile_offset;
	} plane[2];
};

struct intel_display_error_state *
intel_display_capture_error_state(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_display_error_state *error;
	int i;

	error = kmalloc(sizeof(*error), GFP_ATOMIC);
	if (error == NULL)
		return NULL;

	for (i = 0; i < 2; i++) {
		error->cursor[i].control = I915_READ(CURCNTR(i));
		error->cursor[i].position = I915_READ(CURPOS(i));
		error->cursor[i].base = I915_READ(CURBASE(i));

		error->plane[i].control = I915_READ(DSPCNTR(i));
		error->plane[i].stride = I915_READ(DSPSTRIDE(i));
		error->plane[i].size = I915_READ(DSPSIZE(i));
		error->plane[i].pos = I915_READ(DSPPOS(i));
		error->plane[i].addr = I915_READ(DSPADDR(i));
		if (INTEL_INFO(dev)->gen >= 4) {
			error->plane[i].surface = I915_READ(DSPSURF(i));
			error->plane[i].tile_offset = I915_READ(DSPTILEOFF(i));
		}

		error->pipe[i].conf = I915_READ(PIPECONF(i));
		error->pipe[i].source = I915_READ(PIPESRC(i));
		error->pipe[i].htotal = I915_READ(HTOTAL(i));
		error->pipe[i].hblank = I915_READ(HBLANK(i));
		error->pipe[i].hsync = I915_READ(HSYNC(i));
		error->pipe[i].vtotal = I915_READ(VTOTAL(i));
		error->pipe[i].vblank = I915_READ(VBLANK(i));
		error->pipe[i].vsync = I915_READ(VSYNC(i));
	}

	return error;
}

#define err_printf(e, ...) i915_error_printf(e, __VA_ARGS__)

void
intel_display_print_error_state(struct drm_i915_error_state_buf *m,
				struct drm_device *dev,
				struct intel_display_error_state *error)
{
	int i;

	for (i = 0; i < 2; i++) {
		err_printf(m, "Pipe [%d]:\n", i);
		err_printf(m, "  CONF: %08x\n", error->pipe[i].conf);
		err_printf(m, "  SRC: %08x\n", error->pipe[i].source);
		err_printf(m, "  HTOTAL: %08x\n", error->pipe[i].htotal);
		err_printf(m, "  HBLANK: %08x\n", error->pipe[i].hblank);
		err_printf(m, "  HSYNC: %08x\n", error->pipe[i].hsync);
		err_printf(m, "  VTOTAL: %08x\n", error->pipe[i].vtotal);
		err_printf(m, "  VBLANK: %08x\n", error->pipe[i].vblank);
		err_printf(m, "  VSYNC: %08x\n", error->pipe[i].vsync);

		err_printf(m, "Plane [%d]:\n", i);
		err_printf(m, "  CNTR: %08x\n", error->plane[i].control);
		err_printf(m, "  STRIDE: %08x\n", error->plane[i].stride);
		err_printf(m, "  SIZE: %08x\n", error->plane[i].size);
		err_printf(m, "  POS: %08x\n", error->plane[i].pos);
		err_printf(m, "  ADDR: %08x\n", error->plane[i].addr);
		if (INTEL_INFO(dev)->gen >= 4) {
			err_printf(m, "  SURF: %08x\n", error->plane[i].surface);
			err_printf(m, "  TILEOFF: %08x\n", error->plane[i].tile_offset);
		}

		err_printf(m, "Cursor [%d]:\n", i);
		err_printf(m, "  CNTR: %08x\n", error->cursor[i].control);
		err_printf(m, "  POS: %08x\n", error->cursor[i].position);
		err_printf(m, "  BASE: %08x\n", error->cursor[i].base);
	}
}
