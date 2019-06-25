/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_connector.c
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
 *  Connector / kernel mode setting functions.
 *-----------------------------------------------------------------------------
 */
#define MODULE_NAME hal.kms


#include <linux/version.h>
#include <drmP.h>
#include <drm_crtc_helper.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "drm_emgd_private.h"

void emgd_sysfs_connector_remove(emgd_connector_t *emgd_connector);
extern int s3d_mode_patch(igd_display_port_t *port,
			  struct drm_display_mode *drm_mode);

static enum drm_connector_status
emgd_connector_detect(struct drm_connector *connector, bool force);

static int  emgd_connector_set_property(struct drm_connector *connector,
					struct drm_property *property,
					uint64_t value);
static void emgd_connector_destroy(struct drm_connector *connector);
static int  emgd_connector_get_modes(struct drm_connector *connector);
static int  emgd_connector_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode);
static struct drm_encoder *
emgd_connector_best_encoder(struct drm_connector *connector);


const struct drm_connector_funcs emgd_connector_funcs = {
	.dpms         = drm_helper_connector_dpms,
	.detect       = emgd_connector_detect,
	.fill_modes   = drm_helper_probe_single_connector_modes,
	.set_property = emgd_connector_set_property,
	.destroy      = emgd_connector_destroy,
};

const struct drm_connector_helper_funcs emgd_connector_helper_funcs = {
	.get_modes    = emgd_connector_get_modes,
	.mode_valid   = emgd_connector_mode_valid,
	.best_encoder = emgd_connector_best_encoder,
};



/**
 * emgd_mode_to_kms
 *
 * Converts an EMGD mode to a DRM KMS mode
 *
 * @param emgd_mode (IN)  emgd_mode timing information
 * @param drm_mode  (OUT) DRM mode
 */
static void emgd_mode_to_kms(igd_display_info_t *emgd_mode,
				struct drm_display_mode *drm_mode)
{
	if (emgd_mode->flags & IGD_MODE_SUPPORTED) {
		drm_mode->status = MODE_OK;
	} else {
		drm_mode->status = MODE_BAD;
	}

	drm_mode->type        = DRM_MODE_TYPE_DRIVER;
	drm_mode->clock       = emgd_mode->dclk;
	drm_mode->hdisplay    = emgd_mode->width;
	drm_mode->hsync_start = emgd_mode->hsync_start;
	drm_mode->hsync_end   = emgd_mode->hsync_end;
	drm_mode->htotal      = emgd_mode->htotal;
	drm_mode->vdisplay    = emgd_mode->height;
	drm_mode->vsync_start = emgd_mode->vsync_start;
	drm_mode->vsync_end   = emgd_mode->vsync_end;
	drm_mode->vtotal      = emgd_mode->vtotal;
	drm_mode->flags       = 0;
	drm_mode->vrefresh    = emgd_mode->refresh;

	drm_mode_set_name(drm_mode);

	/* Make native modes, preferred modes */
	if (emgd_mode->flags & IGD_MODE_DTD_FP_NATIVE) {
		drm_mode->type = DRM_MODE_TYPE_PREFERRED;
	}
}



/**
 * emgd_connector_detect
 *
 * Checks to see if a display device is attached to the connector.  EMGD
 * does not currently support hot-plug.
 *
 * The prototype for this function seemed to change sometime
 * around the 2.6.35 timeframe however, different distributions
 * cherrypicked it earlier.
 *
 * Fedora 14's 2.6.35.11 kernel has the patch (needs bool force)
 * MeeGo's 2.6.35.10 kernel doesn't.
 *
 * @param encoder (IN) Encoder
 * @param mode    (IN) power mode
 *
 * @return None
 */

static enum drm_connector_status emgd_connector_detect(
		struct drm_connector *connector, bool force) {
	emgd_connector_t          *emgd_connector;
	enum drm_connector_status  connector_status;
	igd_display_port_t        *igd_port;
	pd_port_status_t           port_status;
	int                        ret;

	EMGD_TRACE_ENTER;

	emgd_connector = container_of(connector, emgd_connector_t, base);
	igd_port       = emgd_connector->encoder->igd_port;

	memset(&port_status, 0, sizeof(pd_port_status_t));

	/* Get current status from the port driver */
	ret = igd_port->pd_driver->pd_get_port_status(igd_port->pd_context, &port_status);

	if (ret != PD_SUCCESS) {
		connector_status = connector_status_disconnected;
		return connector_status;
	}

	switch (port_status.connected) {
		case PD_DISP_STATUS_ATTACHED:
			connector_status = connector_status_connected;
			break;

		case PD_DISP_STATUS_DETACHED:
			connector_status = connector_status_disconnected;
			break;

		case PD_DISP_STATUS_UNKNOWN:
		default:
			/*
			 * Technically "unknown" is correct here, but that isn't actually
			 * what we want to pass back to userspace via KMS.  LVDS panels
			 * always have unknown connection status, so they'll always be
			 * ignored by userspace apps that only operate on connected outputs.
			 * If the driver is configured to use a port, then we should just
			 * assume that its actually connected when we report back to
			 * userspace.
			 */

			connector_status = connector_status_connected;
			break;
	}

	EMGD_TRACE_EXIT;

	return connector_status;
}



/**
 * emgd_connector_set_property
 *
 * Sets a port attribute.
 *
 * @param connector (IN) connector
 * @param property  (IN)
 * @param value     (IN)
 *
 * @return TBD
 */
static int emgd_connector_set_property(struct drm_connector *connector,
			struct drm_property *property,
			uint64_t value)
{
	emgd_connector_t   *emgd_connector;
	emgd_encoder_t     *emgd_encoder;
	emgd_crtc_t        *emgd_crtc;
	drm_emgd_private_t *priv;
	int                 ret = 0;
	igd_attr_t          selected_attr;
	unsigned short      port_number;
	emgd_property_map_t *prop_map;
	igd_display_port_t  *port;


	EMGD_TRACE_ENTER;

	/*
	 * Set the property value to the new one.  This doesn't actually change
	 * anything on the HW.
	 */

	ret = drm_object_property_set_value(&connector->base, property, value);
	if (ret) {
		return ret;
	}

	/* Take care of the HW changes associated with the value change */
	emgd_connector = container_of(connector, emgd_connector_t, base);
	emgd_encoder   = emgd_connector->encoder;
	port_number    = emgd_encoder->igd_port->port_number;
	priv           = emgd_connector->priv;
	port           = emgd_encoder->igd_port;
	emgd_crtc = container_of(emgd_encoder->base.crtc, emgd_crtc_t, base);

	list_for_each_entry(prop_map, &emgd_connector->property_map, head) {
		if (prop_map->property == property) {
			memcpy(&selected_attr, prop_map->attribute, sizeof(igd_attr_t));
			selected_attr.current_value = (unsigned long) value;
			selected_attr.flags |= PD_ATTR_FLAG_VALUE_CHANGED;

			EMGD_DEBUG("DRM Property ID: 0x%x, EMGD Attr ID: 0x%x, new value=%d",
				   property->base.id, selected_attr.id, (int)value);

			ret = priv->context->drv_dispatch.set_attrs(priv->context,
					emgd_crtc,
					port_number,
					1, /* Setting 1 attribute */
					&selected_attr);
			if(ret) {
				EMGD_ERROR("Failed to set EMGD Attr!");
			}
			return ret;
		}
	}

	EMGD_TRACE_EXIT;

	return ret;
}



/**
 * emgd_connector_destroy
 *
 * Cleans up the emgd_connector object.
 *
 * @param connector (IN) connector to clean up
 *
 * @return None
 */
static void emgd_connector_destroy(struct drm_connector *connector)
{
	emgd_connector_t  *emgd_connector;
	emgd_property_map_t *prop, *pm;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("connector=0x%lx",(unsigned long)connector);

	emgd_connector = container_of(connector, emgd_connector_t, base);

	drm_sysfs_connector_remove(connector);
	emgd_sysfs_connector_remove(emgd_connector);

	drm_connector_cleanup(connector);

	/* Remove the property map */

	list_for_each_entry_safe(prop, pm, &emgd_connector->property_map, head){
		list_del(&prop->head);
		kfree(prop);
	}

	kfree(emgd_connector);

	EMGD_TRACE_EXIT;
}



/**
 * emgd_connector_get_modes
 *
 * Get the list of supported modes for the given connector
 *
 * @param connector (IN) connector to query.
 *
 * @return None
 */
static int emgd_connector_get_modes(struct drm_connector *connector)
{
	emgd_connector_t        *emgd_connector;
	igd_display_port_t      *igd_port;
	unsigned long            i = 0, k = 0;
	struct drm_display_mode *drm_mode;
	int ret;

	EMGD_TRACE_ENTER;

	emgd_connector = container_of(connector, emgd_connector_t, base);
	igd_port       = emgd_connector->encoder->igd_port;

	EMGD_DEBUG("[EMGD] emgd_connector_get_modes for port %d",
		igd_port->port_number);

	ret  = emgd_connector->priv->context->module_dispatch.update_timing_table(igd_port);
	if (ret){
		EMGD_ERROR("port driver: get timing list error.");
		return 0;
	}

	while ((i < igd_port->num_timing) &&
		(igd_port->timing_table[k].width != IGD_TIMING_TABLE_END)){
		drm_mode = drm_mode_create(emgd_connector->priv->dev);

		emgd_mode_to_kms((igd_display_info_t *)&igd_port->timing_table[k],
			drm_mode);
		k++;

		if (drm_mode->status == MODE_OK )
		{
			/* Add current mode to the connector */
			drm_mode_probed_add(connector, drm_mode);
			i++;

			/* Patch drm_mode with S3D flags if supported */
			if(igd_port->s3d_supported) {
				s3d_mode_patch(igd_port, drm_mode);
			}
		}
	}

	EMGD_TRACE_EXIT;

	return igd_port->num_timing;
}



/**
 * emgd_connector_mode_valid
 *
 * Examines the mode given and see if the connector can support it.
 * Note:  the ModeStatus enum is defined in xorg/hw/xfree86/common/xf86str.h
 *
 * @param connector (IN) the connector to be analyzed.
 * @param mode      (IN) mode to check
 *
 * @return MODE_OK if supported, other ModeStatus enum if not
 */
static int emgd_connector_mode_valid(struct drm_connector *connector,
			struct drm_display_mode *mode)
{
	emgd_connector_t        *emgd_connector;
	igd_display_port_t      *igd_port;

	EMGD_TRACE_ENTER;

	/* We don't need to do error checking here because the mode list given
	 * to KMS kernel mode code is one that we are already okay with.  */

	/* In the event that we end up here because we return an empty list
	 * at emgd_connector_get_modes() time, then we really don't want KMS
	 * to forcefully insert the DMT modes.  So in this case, remote fail.
	 */
	emgd_connector = container_of(connector, emgd_connector_t, base);
	igd_port       = emgd_connector->encoder->igd_port;

	if (0 == igd_port->num_timing) {
		return MODE_ERROR;
	}

	EMGD_TRACE_EXIT;

	return mode->status;
}



/**
 * emgd_connector_best_encoder
 *
 * Returns the best encoder for the given connector.  In EMGD the connector is
 * fixed to the encoder.
 *
 * @param connector (IN) the connector to be analyzed.
 *
 * @return Encoder onto which the connector is fixed on.
 */
struct drm_encoder *
emgd_connector_best_encoder(struct drm_connector *connector)
{
	emgd_connector_t *emgd_connector;

	EMGD_TRACE_ENTER;

	emgd_connector = container_of(connector, emgd_connector_t, base);

	EMGD_TRACE_EXIT;

	return &emgd_connector->encoder->base;
}
