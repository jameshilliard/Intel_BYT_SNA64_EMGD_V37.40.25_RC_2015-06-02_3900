/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: kms_mode.c
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
 *  Contains client interface support functions for display allocations
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.mode

#include <io.h>
#include <memory.h>
#include <ossched.h>
#include <math_fix.h>

#include <igd_core_structs.h>

#include <pi.h>
#include <dispatch.h>
#include <module_init.h>
#include "emgd_drm.h"


/*!
 * @addtogroup display_group
 * @{
 */

#define TIMING_CHANGED(a,b,c,d,e,f,g) timing_changed(a,b,c,d,e,f,g)
#define CALCULATE_ELD_INFOFRAMES

#define MODE_MIN(x, y) (x<y)?x:y

/*
 *  Device-Independant Kernel Mode Setting (KMS) dispatch functions
 */
static int igd_get_EDID_block(igd_driver_h driver_handle,
		unsigned short port_number,
		unsigned char *edid_ptr,
		unsigned char block_number);
static int igd_set_attrs( igd_driver_h  driver_handle,
		emgd_crtc_t *emgd_crtc,
		unsigned short port_number,
		unsigned long num_attrs,
		igd_attr_t *attr_list);
static int igd_get_attrs( igd_driver_h  driver_handle,
		unsigned short port_number,
		unsigned long *num_attrs,
		igd_attr_t **attr_list);

/*
 *  Device-Dependant Kernel Mode Setting (KMS) dispatch table
 */
static dispatch_table_t mode_kms_dispatch[] = {
	DISPATCH_SNB( &mode_kms_dispatch_snb )
	DISPATCH_IVB( &mode_kms_dispatch_vlv )
	DISPATCH_VLV( &mode_kms_dispatch_vlv )
	DISPATCH_VLV2( &mode_kms_dispatch_vlv )
	DISPATCH_VLV3( &mode_kms_dispatch_vlv )
	DISPATCH_VLV4( &mode_kms_dispatch_vlv )
	DISPATCH_END
};


/*
 * Do not malloc the context for two reasons.
 *  1) vBIOS needs to minimize mallocs
 *  2) Mode context needs to stay around until after all modules are
 *    shut down for the register restore functionality.
*/
//mode_context_t mode_context[1];

/* This symbol has to be in this file as it is part of
 *  driver ONLY.
 */
static fw_info_t global_fw_info;

/* Mandatory 3D modes used to patch the drm_mode flag */
struct s3d_modes {
	int width, height, freq;
	unsigned int select, value;
	unsigned int formats;
};

struct s3d_modes s3d_mandatory_modes[] = {
	{ 1920, 1080, 24, DRM_MODE_FLAG_INTERLACE, 0,
			DRM_MODE_FLAG_3D_TOP_AND_BOTTOM | DRM_MODE_FLAG_3D_FRAME_PACKING },
	{ 1920, 1080, 50, DRM_MODE_FLAG_INTERLACE, DRM_MODE_FLAG_INTERLACE,
			DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF },
	{ 1920, 1080, 60, DRM_MODE_FLAG_INTERLACE, DRM_MODE_FLAG_INTERLACE,
			DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF },
	{ 1280, 720,  50, DRM_MODE_FLAG_INTERLACE, 0,
			DRM_MODE_FLAG_3D_TOP_AND_BOTTOM | DRM_MODE_FLAG_3D_FRAME_PACKING },
	{ 1280, 720,  60, DRM_MODE_FLAG_INTERLACE, 0,
			DRM_MODE_FLAG_3D_TOP_AND_BOTTOM | DRM_MODE_FLAG_3D_FRAME_PACKING }
};


#ifdef CALCULATE_ELD_INFOFRAMES

/*!
 * Calculates infoframes information top be used by HDMI port drivers
 *
 * @param port
 * @param timing_info
 * @param temp_cea
 *
 * @return 0
 */
static int calculate_infoframes(
	igd_display_port_t *port,
	igd_timing_info_t *timing_info,
	cea_extension_t *temp_cea)
{

	pd_timing_t     *cea_timings = NULL, *cea_timing_temp = NULL;

	EMGD_TRACE_ENTER;

	/* VBIOS has no access to CEA timing tables and this is not supported
	   there as well */
	if(timing_info->flags & PD_MODE_CEA){
		if(timing_info->width != 640 && timing_info->height != 480){
			port->edid->cea->quantization	= HDMI_QUANTIZATION_RGB_220;
		}

		/* Based on DPG algorithm. If monitors support more than 2 channels
		   for 192Khz or/and 92Khz then set two pixel repeat one.
		   KIV: Add pruning for pixel PIX_REPLICATION_3 if required  */
		if(temp_cea->audio_cap[CAP_192_KHZ].max_channels>2 ||
			temp_cea->audio_cap[CAP_96_KHZ].max_channels>2){
			port->edid->cea->pixel_rep = PIX_REPLICATION_1;
		}


		/* Based on HDMI spec 6.7.1 & 6.7.2 */
		if ((timing_info->width == 720) && ((timing_info->height == 480) ||
			 (timing_info->height== 576))){
			port->edid->cea->colorimetry	= HDMI_COLORIMETRY_ITU601;
		} else if(((timing_info->width==1280) && (timing_info->height==720)) ||
			((timing_info->width == 1920) && (timing_info->height == 1080))){
			port->edid->cea->colorimetry	= HDMI_COLORIMETRY_ITU709;
		}

		cea_timings = (igd_timing_info_t *) OS_ALLOC(cea_timing_table_size);
		OS_MEMCPY(cea_timings, cea_timing_table, cea_timing_table_size);
		cea_timing_temp = cea_timings;

		while (cea_timings->width != IGD_TIMING_TABLE_END){
			if(cea_timings->width == timing_info->width &&
			   cea_timings->height == timing_info->height &&
			   cea_timings->refresh == timing_info->refresh &&
			   cea_timings->dclk == timing_info->dclk &&
			   (cea_timings->flags &
			   (PD_ASPECT_16_9| IGD_SCAN_INTERLACE)) ==
			   (timing_info->flags &
			   (PD_ASPECT_16_9| IGD_SCAN_INTERLACE))){
					port->edid->cea->video_code		= cea_timings->mode_number;
					break;
				}
			cea_timings++;
		}

		OS_FREE(cea_timing_temp);

	}

	EMGD_TRACE_EXIT;
	return 0;
}
#endif /* CALCULATE_ELD_INFOFRAMES */

/*!
 * Calculates the Edid like data (ELD) if port supports audio transmission
 *
 * @param port
 * @param timing_info
 *
 * @return 0
 */
int calculate_eld(
	igd_display_port_t *port,
	igd_timing_info_t *timing_info)
{
	/* Calculate for non-content protected & content protected(Array of 2) */
#ifdef CALCULATE_ELD_INFOFRAMES
	unsigned long cal_NPL[2]; /* Number of packer per line calculate*/
	unsigned long poss_NPL[2]; /* Number of packer per line possible*/
	unsigned long max_bitRate_2[2],max_bitRate_8[2];
	unsigned long h_refresh, audio_freq;
	unsigned char input;
	int i,j,pix_rep;
#endif
	cea_extension_t *temp_cea = NULL ;

	EMGD_TRACE_ENTER;
	/* Only calculate eld for HDMI port */
	if ((port->pd_driver->type !=  PD_DISPLAY_HDMI_EXT) &&
		(port->pd_driver->type !=  PD_DISPLAY_HDMI_INT) &&
		(port->pd_driver->type !=  PD_DISPLAY_DP_INT)) {
		return 0;
	}

	if(port->firmware_type == PI_FIRMWARE_EDID){
		temp_cea = (cea_extension_t*)port->edid->cea;
	}
	/* Displayid unsupported for now. Uncomment this code when audio
	information is available for Display ID
	temp_cea = (&cea_extension_t)port->displayid->cea;*/

	if(temp_cea == NULL){
		/* CEA data unavailable, display does not have audio capability? */
		/* We would allocate dummy edid structure and here and we should ony
		   used canned ELD */
		if(port->edid == NULL) {
			port->edid = (edid_t *) OS_ALLOC(sizeof(edid_t));
			OS_MEMSET(port->edid, 0 , (sizeof(edid_t)));
		}

		port->edid->cea = (cea_extension_t *) OS_ALLOC(sizeof(cea_extension_t));
		OS_MEMSET(port->edid->cea, 0 , (sizeof(cea_extension_t)));
		port->edid->cea->canned_eld = 1;
		temp_cea = (cea_extension_t*)port->edid->cea;
		port->callback->eld = &(port->edid->cea);
	}

	/* Default to canned ELD data */
	temp_cea->LPCM_CAD[0] = 0x9;
	temp_cea->speaker_alloc_block[0] = 0x1;
	/* Default 0 Pixel replication */
	port->edid->cea->pixel_rep = PIX_REPLICATION_0;
	/* Default */
	port->edid->cea->colorimetry = HDMI_COLORIMETRY_NODATA;
	/* Default RGB 256 wuantization full range */
	port->edid->cea->quantization = HDMI_QUANTIZATION_RGB_256;
	/* Default Unknown video code */
	port->edid->cea->video_code = 0;
	port->edid->cea->aspect_ratio = (timing_info->flags & PD_ASPECT_16_9)
	             ? PD_ASPECT_RATIO_16_9 : PD_ASPECT_RATIO_4_3;

#ifdef CALCULATE_ELD_INFOFRAMES
	calculate_infoframes(port,timing_info,temp_cea);
	/* If canned eld is not set and audio info from transmitter is available */
	if(temp_cea->canned_eld != 1 && (temp_cea->audio_flag & PD_AUDIO_CHAR_AVAIL)){
		pix_rep = port->edid->cea->pixel_rep;
		/*h_refresh = timing_info->dclk/timing_info->htotal;*/
		h_refresh = timing_info->refresh;
		cal_NPL[0] = (pix_rep*(timing_info->hsync_end - timing_info->hsync_start) -
					port->edid->cea->K0) /32;
		cal_NPL[1] = (pix_rep*(timing_info->hsync_end - timing_info->hsync_start) -
					port->edid->cea->K1) /32;

		poss_NPL[0] = MODE_MIN(cal_NPL[0],port->edid->cea->NPL);
		poss_NPL[1] = MODE_MIN(cal_NPL[1],port->edid->cea->NPL);

		max_bitRate_2[0] = h_refresh * poss_NPL[0] - 1500;
		max_bitRate_2[1] = h_refresh * poss_NPL[1] - 1500;

		max_bitRate_8[0] = h_refresh * poss_NPL[0] * 4 - 1500;
		max_bitRate_8[1] = h_refresh * poss_NPL[1] * 4 - 1500;

		/* Loop trough Content Protection disabled then enabled */
		for(i=0 ; i<2; i++){
			for(j=0 ; j<3; j++){
				input = 0;
				audio_freq = 48000 * (1<<j); /* 48Khz->96Khz->192Khz */
				if(max_bitRate_8[i] >= audio_freq){
					input = 7;
				}else if(max_bitRate_2[i] >= audio_freq){
					input = 1;
				}
				/* take the minimum value min(transmitter, receiver) */
				input = MODE_MIN(input,temp_cea->audio_cap[j].max_channels);
				temp_cea->LPCM_CAD[j] |= input<<((1-i)*3);
				if(temp_cea->audio_cap[j]._24bit){
					temp_cea->LPCM_CAD[j] |= BIT(7);
				}
				if(temp_cea->audio_cap[j]._20bit){
					temp_cea->LPCM_CAD[j] |= BIT(6);
				}
			}
		}

		/* TODO: Further construction of ELD from Monitor Name String begins here
		   for now we only support VSDB */
		/* By default we don send any vendor specific block unless latency value
		   use for audio sync feature is available */
		temp_cea->vsdbl = 0;
		/* This means the latecy field is available VSBD_LATENCY_FIELD = 8*/
		if(temp_cea->vendor_block.vendor_block_size > VSBD_LATENCY_FIELD){
			OS_MEMCPY(temp_cea->misc_data, temp_cea->vendor_data_block,
				temp_cea->vendor_block.vendor_block_size);
			temp_cea->vsdbl = temp_cea->vendor_block.vendor_block_size;
			/* If the VSBD has latency fields */
			if(*(temp_cea->vendor_data_block + VSBD_LATENCY_FIELD - 1) & 0x80){
				if(timing_info->flags & IGD_SCAN_INTERLACE){
					if(*(temp_cea->vendor_data_block + VSBD_LATENCY_FIELD - 1) & 0x40){
						temp_cea->vendor_block.p_latency = 1;
						temp_cea->vendor_block.i_latency = 1;
					}else{
						/* No latency available: Since it is an interlace mode but no
						   vsbd_intlc_fld_present is available */
						temp_cea->vendor_block.p_latency = 0;
						temp_cea->vendor_block.i_latency = 0;
					}
				}else{
					temp_cea->vendor_block.p_latency = 1;
					temp_cea->vendor_block.i_latency = 0;
				}
			}
		}
	}
#endif /* CALCULATE_ELD_INFOFRAMES */
	temp_cea->audio_flag |= ELD_AVAIL;
	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 *
 * @param driver_handle from igd_init().
 * @param port_number
 * @param edid_ptr
 * @param block_number
 *
 * @return 0 on success
 * @return -IGD_ERROR_INVAL or -IGD_ERROR_EDID on failure
 */
/* FIXME: Should we move this to PI??? */
static int igd_get_EDID_block(igd_driver_h driver_handle,
		unsigned short port_number,
		unsigned char *edid_ptr,
		unsigned char block_number)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	igd_display_port_t *port;
	int                   ret;

	EMGD_TRACE_ENTER;

	EMGD_ASSERT(driver_handle, "Null driver_handle", -IGD_ERROR_INVAL);
	EMGD_ASSERT(edid_ptr, "Null edid_ptr", -IGD_ERROR_INVAL);

	port = context->module_dispatch.dsp_port_list[port_number];
	if(!port) {
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	/* If firmware type is Display ID, convert Display ID to EDID */
	if (port->firmware_type == PI_FIRMWARE_DISPLAYID) {
		context->module_dispatch.convert_displayid_to_edid(
			port, edid_ptr);
		return 0;
	}

	/* Read EDID */
	ret = context->module_dispatch.pi_i2c_read_regs(
		context,
		port,
		port->ddc_reg,
		port->ddc_speed,
		port->ddc_dab,
		128*block_number,
		edid_ptr,
		128);

	if (ret) {
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_EDID;
	}

    /* Check if got a valid EDID header */ 
    if ((block_number == 0) && (*(uint64_t *)edid_ptr != 0x00FFFFFFFFFFFF00))
    {
        EMGD_TRACE_EXIT;
        return -IGD_ERROR_EDID;
    }

	EMGD_TRACE_EXIT;
	return 0;
} /* end igd_get_EDID_block() */


/*!
 * This function get attributes for a port. SS will
 * allocate the memory required to return the *attr_list.
 * This is a live copy of attributes used by both IAL and SS.
 * Don't deallocate this memory. This will be freed by SS.
 *
 * The list returned will contain a list of HAL attributes
 * followed by a pointer to the PD attributes.
 *
 * @param driver_handle pointer to an IGD context pointer returned
 * 	from a successful call to igd_init().
 * @param port_number port number of port to get pd attributes.
 * @param num_attrs pointer to return the number of attributes
 * 	returned in attr_list.
 * @param attr_list - pointer to return the attributes.
 *
 * @return -IGD_INVAL on failure
 * @return 0 on success
 */
static int igd_get_attrs(
	igd_driver_h  driver_handle,
	unsigned short port_number,
	unsigned long *num_attrs,
	igd_attr_t **attr_list)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	int                   ret;
	unsigned long         hal_attrib_num, pd_attr_num;
	igd_display_port_t    *port;
	igd_attr_t            *pd_attr_list = NULL;


	EMGD_TRACE_ENTER;

	/* basic parameter check */
	EMGD_ASSERT(driver_handle, "Null Driver Handle", -IGD_ERROR_INVAL);

	/* Get the display context that is currently using this port */
	context->module_dispatch.dsp_get_display_port(port_number, &port, 0);

	if(!port) {
		/*
		 * No display is using this port,
		 * should we abort at this point?
		 */
		EMGD_TRACE_EXIT;
		EMGD_ERROR("No display is using port %d.", port_number);
		return -IGD_ERROR_INVAL;
	}

	/* Get PD attributes */
	pd_attr_num = *num_attrs;


	ret = port->pd_driver->get_attrs( port->pd_context,
									&pd_attr_num,
									&pd_attr_list );

	if (ret) {
		pd_attr_num  = 0;
		pd_attr_list = NULL;
	}

	/* find the extension attribute and attach the pd_attr_list */
	for (hal_attrib_num = 0;
			PD_ATTR_LIST_END != port->attributes[hal_attrib_num].id;
			hal_attrib_num++ ) {

		if (PD_ATTR_ID_EXTENSION == port->attributes[hal_attrib_num].id ) {
			((igd_extension_attr_t *)(&port->attributes[hal_attrib_num]))->extension = pd_attr_list;
		}
	}

	/* If attr_list is NULL then the caller is only interested in
	 * the number of attributes
	 */
	if( NULL != attr_list ) {
		*attr_list = port->attributes;
	}

	if (0 == pd_attr_num ) {
		/* if there are no PD attributes, then subtract 1 from hal_attrib_num
		 * so that the "extension" attribute is not counted*/
		*num_attrs = hal_attrib_num - 1;
	} else {
		/* subtract 1 because we should not count the "extension" attribute */
		*num_attrs = hal_attrib_num + pd_attr_num - 1;
	}

	EMGD_DEBUG("Port: %s, num_pd_attr = %ld, num_hal_attr = %ld", port->port_name,
													pd_attr_num, hal_attrib_num - 1);

	EMGD_TRACE_EXIT;
	return 0;
} /* end igd_get_attrs() */


/*!
 * This function set attributes on a port.
 *
 * @param driver_handle pointer to an IGD context pointer returned
 * 	from a successful call to igd_init().
 * @param emgd_crtc crtc that port is attached to (may be null)
 * @param port_number port number of port to get pd attributes.
 * @param num_attrs pointer to return the number of attributes
 * 	returned in attr_list.
 * @param attr_list - pointer to return the attributes.
 *
 * @return -IGD_INVAL on failure
 * @return 0 on success
 */
static int igd_set_attrs(
	igd_driver_h  driver_handle,
	emgd_crtc_t *emgd_crtc,
	unsigned short port_number,
	unsigned long num_attrs,
	igd_attr_t *attr_list)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	igd_display_port_t    *port;
	igd_attr_t            *attr;
	unsigned int          i;

	unsigned long         num_attrs_set = 0;
	int                   ret = 0, setmode = 0;
	igd_timing_info_t     *pd_timing_table = NULL;

	EMGD_TRACE_ENTER;

	/* basic parameter check */
	EMGD_ASSERT(driver_handle, "Null Driver Handle", -IGD_ERROR_INVAL);

	/* Get the display context that is currently using this port */
	context->module_dispatch.dsp_get_display_port(port_number, &port, 0);

	if(!port) {
		/* No display is using this port, should we abort at this point? */
		return -IGD_ERROR_INVAL;
	}

	if(num_attrs == 0) {
		return 0;
	}

	/*
	 * Take care of HAL attributes.  Here we keep track of the number of
	 * attributes set.  If the number set is equal to num_attrs, then we
	 * don't need to call port driver's set_attr when this loop is done
	 */
	for( i = 0; i < num_attrs; i ++ ) {
		switch (attr_list[i].id) {
			case PD_ATTR_ID_FB_GAMMA:
			case PD_ATTR_ID_FB_BRIGHTNESS:
			case PD_ATTR_ID_FB_CONTRAST:
			case PD_ATTR_ID_DP_USER_BPC:
				/* Update the actual HAL port attribute */
				attr = port->attributes;
				while (attr->id != PD_ATTR_LIST_END) {
					if (attr->id == attr_list[i].id) {
						igd_range_attr_t *a = (igd_range_attr_t *)attr;

						/* Validate that value is within range */
						if ((attr_list[i].current_value <= a->max) &&
							(attr_list[i].current_value >= a->min)) {
							attr->current_value =
								attr_list[i].current_value;
							break;
						}
					}
					attr++;
				}
				//set_color_correct(display, (igd_range_attr_t *) &attr_list[i]);
				num_attrs_set++;
				break;

			default:
				/* ignore all non HAL-related attributes */
				break;
		}
	}

	if ((num_attrs_set > 0) && (emgd_crtc != NULL)) {
		context->mode_dispatch->kms_set_color_correct(emgd_crtc);
	}

	/* Pass the attribute list down to the port driver for futher processing
	 * if necessary */
	if (num_attrs > num_attrs_set) {
		ret = port->pd_driver->set_attrs(port->pd_context, num_attrs, attr_list);

		if (ret) {
			return -IGD_INVAL;
		}
	}

	attr = attr_list;
	i = 0;
	while (i++ < num_attrs) {
		if (attr->flags & PD_ATTR_FLAG_SETMODE) {
			setmode = 1;
			break;
		}
		attr++;
	}

	ret = 0;
	if (setmode) {
		/* Update internal timings */
		ret = port->pd_driver->get_timing_list(port->pd_context,
				(pd_timing_t *)port->timing_table,
				(pd_timing_t **)&pd_timing_table);

		if (ret) {
			EMGD_ERROR_EXIT("get_timing_list failed.");
			return -IGD_ERROR_INVAL;
		}

		port->timing_table = pd_timing_table;
		port->num_timing = get_native_dtd(pd_timing_table,
				PI_SUPPORTED_TIMINGS, &port->fp_native_dtd,
				PD_MODE_DTD_FP_NATIVE);
		ret = IGD_DO_QRY_SETMODE;
	}

	EMGD_TRACE_EXIT;
	return ret;
} /* end igd_set_attrs() */

/*!
 * mode_save
 *
 * This function is used to save mode module register state.
 *
 * @param context SS level igd_context
 * @param state pointer to module_state handle, where module_state_h is
 * pointer to actual state
 *@param flags should have IGD_REG_SAVE_MODE bit set for save
 *
 * @return 0 on success
 * @return -IGD_INVAL on failure
 */
static int mode_save(igd_context_t *context, module_state_h *state,
	unsigned long *flags)
{
	mode_state_t       *mstate;
	int                i, ret;
	igd_display_port_t *port = NULL;

	EMGD_TRACE_ENTER;

	if (!state || !(*flags & IGD_REG_SAVE_MODE)) {
		EMGD_ERROR_EXIT("NULL pointer to save mode state or"
			" flags don't have IGD_REG_SAVE_MODE bit set.");
		return -IGD_ERROR_INVAL;
	}

	/* First allocate memory for mode state which includes pd states */
	mstate = OS_ALLOC(sizeof(mode_state_t));
	if (!mstate) {
		EMGD_ERROR_EXIT("memory allocation failed.");
		return -IGD_ERROR_NOMEM;
	}
	OS_MEMSET(mstate, 0, sizeof(mode_state_t));

	/* Call pd_save */
	port = NULL;
	i = 0;
	while ((port = context->module_dispatch.dsp_get_next_port(context, port,0))
			!= NULL) {
		if (port->pd_driver) {
			EMGD_DEBUG("saving %s", port->pd_driver->name);
			ret = port->pd_driver->pd_save(port->pd_context,
				&(mstate->pd_state[i].state), 0);
			if (ret) {
				EMGD_DEBUG("pd_save failed for %s", port->pd_driver->name);
			}

			mstate->pd_state[i].port = port;
			i++;
		}
	}

	/* Update mode state */
	*state = (module_state_h) mstate;

	EMGD_DEBUG("mode_save: saved %d port driver states.", i);

	EMGD_TRACE_EXIT;
	return 0;
} /* end mode_save() */

/*!
 * mode_restore
 *
 * This function is used to save mode module register state.
 *
 * @param context SS level igd_context
 * @param state pointer to module_state handle, where module_state_h is
 * pointer to actual state
 * *@param flags should have IGD_REG_SAVE_MODE bit set for restore
 *
 * @return 0 on success
 * @return -IGD_INVAL on failure
 */
static int mode_restore(igd_context_t *context, module_state_h *state,
	unsigned long *flags)
{
	int                i, ret;
	igd_display_port_t *port = NULL;
	mode_state_t       *mstate;

	EMGD_TRACE_ENTER;

	if (!state || !(*flags & IGD_REG_SAVE_MODE)) {
		EMGD_ERROR_EXIT("Null mode state.or trying to restore without a save");
		return 0;
	}

	mstate = (mode_state_t *)(*state);

	if (!mstate) {
		EMGD_DEBUG("mode_restore: mstate = NULL");
		EMGD_TRACE_EXIT;
		return 0;
	}

	/* Restore all PD drivers */
	i = 0;
	while (mstate->pd_state[i].port) {
		port = mstate->pd_state[i].port;

		EMGD_DEBUG("restoring %s", port->pd_driver->name);
		ret = port->pd_driver->pd_restore(port->pd_context,
				mstate->pd_state[i].state, 0);
		if (ret) {
			/* What can be done if restore fails */
			EMGD_DEBUG("pd_restore failed for %s", port->pd_driver->name);
		}

		i++;
	}

	/* Free the memory allocated */
	OS_FREE(mstate);
	*state = NULL;

	EMGD_TRACE_EXIT;
	return 0;
} /* end mode_restore() */


/*!
 * This function is used to shutdown any module/dsp
 * module specific structures or tables etc.
 *
 * @param context SS level igd_context.
 *
 * @return -IGD_INVAL on failure
 * @return 0 on success
 */
static void mode_shutdown(igd_context_t *context)
{
	module_state_h *mode_state = NULL;
	unsigned long *flags = NULL;

	EMGD_DEBUG("mode_shutdown Entry");

	/* Reset all planes, pipe, ports to a known "off" state */
	context->mode_dispatch->im_reset_plane_pipe_ports(context);

	/* Shutdown dsp module */
	context->module_dispatch.dsp_shutdown(context);

	/* Restore mode state */
	context->module_dispatch.reg_get_mod_state(
			REG_MODE_STATE, &mode_state, &flags);

	mode_restore(context, mode_state, flags);

	/* Shutdown PI module */
	context->module_dispatch.pi_shutdown(context);

	EMGD_DEBUG("Return");
	return;
} /* end mode_shutdown() */



/*!
 * This function is used to initialize any module/dsp
 * module specific structures or tables etc.
 *
 * @param context SS level igd_context.
 *
 * @return 0 on success.
 * @return -IGD_INVAL or -IGD_ERROR_NODEV on failure
 */
int mode_init(igd_context_t *context)
{
	igd_dispatch_t *drv_dispatch;
	igd_module_dispatch_t *module_dispatch;
	igd_mode_dispatch_t **mode_dispatch;
	mode_context_t * mode_context;
	EMGD_TRACE_ENTER;

	EMGD_DEBUG("Allocating a mode context...");

	drv_dispatch    = &context->drv_dispatch;
	module_dispatch = &context->module_dispatch;
	mode_dispatch   = &context->mode_dispatch;
	mode_context    = &context->mode_context;


	/* Hook up the IGD dispatch table entries for mode */
	drv_dispatch->get_attrs = igd_get_attrs;
	drv_dispatch->set_attrs = igd_set_attrs;
	drv_dispatch->get_EDID_block = igd_get_EDID_block;
	//drv_dispatch->query_mode_list = igd_query_mode_list;

	/* Clear the allocated memory for mode context */
	OS_MEMSET(mode_context, 0, sizeof(mode_context_t));

	/* Set the pointer to igd level context */
	mode_context->context = context;
	mode_context->first_alter = TRUE;
	mode_context->ref_freq =
		context->module_dispatch.init_params->ref_freq;
	/*To give option for validation*/
	mode_context->en_reg_override =
		context->module_dispatch.init_params->en_reg_override;
	mode_context->disp_arb =
		context->module_dispatch.init_params->disp_arb;
	mode_context->fifo_watermark1 =
		context->module_dispatch.init_params->fifo_watermark1;
	mode_context->fifo_watermark2 =
		context->module_dispatch.init_params->fifo_watermark2;
	mode_context->fifo_watermark3 =
		context->module_dispatch.init_params->fifo_watermark3;
	mode_context->fifo_watermark4 =
		context->module_dispatch.init_params->fifo_watermark4;
	mode_context->fifo_watermark5 =
		context->module_dispatch.init_params->fifo_watermark5;
	mode_context->fifo_watermark6 =
		context->module_dispatch.init_params->fifo_watermark6;
	mode_context->gvd_hp_control =
		context->module_dispatch.init_params->gvd_hp_control;
	mode_context->bunit_chicken_bits =
		context->module_dispatch.init_params->bunit_chicken_bits;
	mode_context->bunit_write_flush =
		context->module_dispatch.init_params->bunit_write_flush;
	mode_context->disp_chicken_bits =
		context->module_dispatch.init_params->disp_chicken_bits;
	/* Assign the fw_info structure and Zero-out the contents */
	mode_context->fw_info = &global_fw_info;
	OS_MEMSET(mode_context->fw_info, 0, sizeof(fw_info_t));

	/* Hook up Mode's dispatch table function pointers */
	*mode_dispatch = (igd_mode_dispatch_t *) dispatch_acquire(
				context, mode_kms_dispatch);
	if (!(*mode_dispatch)) {
		EMGD_ERROR_EXIT("Unsupported Device");
		return -IGD_ERROR_NODEV;
	}

	/* Hook up optional inter-module functions */
	module_dispatch                = &context->module_dispatch;
	module_dispatch->mode_filter_modes  =
			(*mode_dispatch)->im_filter_modes;
	module_dispatch->mode_reset_plane_pipe_ports =
			(*mode_dispatch)->im_reset_plane_pipe_ports;
	module_dispatch->mode_save      = mode_save;
	module_dispatch->mode_restore   = mode_restore;
	module_dispatch->mode_shutdown  = mode_shutdown;

	/* Initialize dsp module */
	if (dsp_init(context)) {
		EMGD_ERROR("dsp_init() failed.");
		return -IGD_INVAL;
	}

	/* Initialze port interface (pi) module */
	if (pi_init(context)) {
		EMGD_ERROR_EXIT("pi_init() failed.");
		if(module_dispatch->dsp_shutdown) {
			module_dispatch->dsp_shutdown(context);
		}
		return -IGD_ERROR_INVAL;
	}

	(*mode_dispatch)->im_reset_plane_pipe_ports(context);

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 * This function is used to adjust the timing required
 * by S3D format Framing Packing and Side-by-Side Full
 *
 * @param port	igd_display_port
 * @param current_timings timing before fix up
 * @param new_timings timing after fix up
 *
 * @return 0 on success.
 * @return -IGD_ERROR_INVAL on failure
 */
int s3d_timings_fixup(igd_display_port_t *port,
		igd_timing_info_t *current_timings,
		igd_timing_info_t *new_timings)
{
	unsigned long	s3d_mode_sel = 0;
	int status = -IGD_ERROR_INVAL;
	EMGD_TRACE_ENTER;

	if(pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_S3D_MODE_SET,
			0,		/*no PD_FLAG for S3D attr*/
			NULL,	/* don't need the attr ptr*/
			&s3d_mode_sel)) {
		EMGD_DEBUG("S3D_MODE_SET attributes not found, port doesn't support S3D");
		EMGD_TRACE_EXIT;
		return status;
	}

	if (s3d_mode_sel == PD_S3D_MODE_FRAME_PACKING) {
		memcpy(new_timings, current_timings, sizeof(igd_timing_info_t));
		new_timings->height += new_timings->vtotal;
		new_timings->vblank_start += new_timings->vtotal;
		new_timings->vblank_end += new_timings->vtotal;
		new_timings->vsync_start += new_timings->vtotal;
		new_timings->vsync_end += new_timings->vtotal;
		new_timings->vtotal *= 2;
		new_timings->dclk = (new_timings->vtotal * new_timings->htotal * new_timings->refresh)/1000;

		EMGD_DEBUG("S3D Framepacking format selected, new timings: %dx%d@%d",
						new_timings->width, new_timings->height, new_timings->refresh );
		/* Return success to caller to indicate new_timings is valid */
		status = IGD_SUCCESS;
	}
	else if (s3d_mode_sel == PD_S3D_MODE_SBS_FULL) {
		memcpy(new_timings, current_timings, sizeof(igd_timing_info_t));
		new_timings->hblank_start += new_timings->width;
		new_timings->hblank_end += new_timings->width;
		new_timings->hsync_start += new_timings->width;
		new_timings->hsync_end += new_timings->width;
		new_timings->htotal += new_timings->width;
		new_timings->width *= 2;
		new_timings->dclk = (new_timings->vtotal * new_timings->htotal * new_timings->refresh)/1000;

		EMGD_DEBUG("S3D Side-by-Side Full format selected, new timings: %dx%d@%d",
						new_timings->width, new_timings->height, new_timings->refresh );
		/* Return success to caller to indicate new_timings is valid */
		status = IGD_SUCCESS;
	}
	else {
		EMGD_DEBUG("S3D format selected = 0x%lx, no timings fix required", s3d_mode_sel);
	}

	EMGD_TRACE_EXIT;
	return status;
}


/*!
 * This function is used to set the S3D port attributes
 * based on the S3D flag from drm_display_mode
 *
 * @param port	igd_display_port
 * @param drm_mode	display mode from drm
 *
 * @return 0 on success.
 * @return -IGD_ERROR_INVAL on failure
 */
int s3d_mode_set(emgd_crtc_t *emgd_crtc,
		struct drm_display_mode *drm_mode)
{
	igd_attr_t		*attr;
	igd_attr_t		attr2;
	igd_display_port_t *port = NULL;
	emgd_framebuffer_t *emgd_fb;
	unsigned long	s3d_expose_mode = 0;
	unsigned long	s3d_mode_sel = 0;

	EMGD_TRACE_ENTER;

	port = PORT(emgd_crtc);

	if(pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_S3D_MODE_EXPOSE,
			0,		/* no PD_FLAG for S3D attr*/
			NULL,	/* don't need the attr ptr*/
			&s3d_expose_mode)) {
		EMGD_DEBUG("S3D_EXPOSE_MODE attribute not found, port doesn't support S3D");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	EMGD_DEBUG("S3D: Current Port=%s, s3d_supported=0x%x, s3d_expose_mode=0x%lx",
								port->port_name, port->s3d_supported, s3d_expose_mode);

	if(s3d_expose_mode) {

		EMGD_DEBUG("S3D: expose 3D modes ENABLE, try to map "
					"DRM 3D flags: 0x%x to S3D_MODE_SET attribute",
					drm_mode->flags & DRM_MODE_FLAG_3D_MASK);

		if(pi_pd_find_attr_and_value(port,
				PD_ATTR_ID_S3D_MODE_SET,
				0,		/* no PD_FLAG for S3D attr*/
				&attr,
				NULL)) {
			EMGD_DEBUG("S3D_MODE_SET attribute not found, port doesn't support S3D");
			EMGD_TRACE_EXIT;
			return -IGD_ERROR_INVAL;
		}
		memcpy(&attr2, attr, sizeof(igd_attr_t));

		switch (drm_mode->flags & DRM_MODE_FLAG_3D_MASK)
		{
			case DRM_MODE_FLAG_3D_TOP_AND_BOTTOM:
				attr2.current_value = PD_S3D_MODE_TOP_BOTTOM;
				break;
			case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF:
				/* For Side-by-side half, default to Horizontal sub-sampling
				 * even/left picture and odd/right picture */
				attr2.current_value = PD_S3D_MODE_SBS_HALF |
										PD_S3D_FORMAT_HOR_SUB_SAMPLING_EO;
				break;
			case DRM_MODE_FLAG_3D_FRAME_PACKING:
				attr2.current_value = PD_S3D_MODE_FRAME_PACKING;
				break;
			default:
				attr2.current_value = PD_S3D_MODE_OFF;
				EMGD_DEBUG("DRM 3D flags: 0x%x is Zero or Unknown, disable S3D",
							drm_mode->flags & DRM_MODE_FLAG_3D_MASK);
		}

		attr2.flags |= PD_ATTR_FLAG_VALUE_CHANGED;
		port->pd_driver->set_attrs(port->pd_context, 1, &attr2);
	}

	/* Get the latest S3D_MODE_SET attribute value */
	if(pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_S3D_MODE_SET,
			0,		/*no PD_FLAG for S3D attr*/
			NULL,	/* don't need the attr ptr*/
			&s3d_mode_sel)) {
		EMGD_DEBUG("S3D_MODE_SET attributes not found, port doesn't support S3D");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	emgd_fb = container_of(emgd_crtc->base.fb, emgd_framebuffer_t, base);
	/* If 3D mode was set in connector property and user have added additional
	 * 2 buffers in emgd_fb, then user would like to use sprites for the left
	 * and right images */
	if(s3d_mode_sel != PD_S3D_MODE_OFF &&
		emgd_fb->other_bo[0] && emgd_fb->other_bo[1]) {
		emgd_fb->igd_flags |= IGD_HDMI_STEREO_3D_MODE;
	} else {
		emgd_fb->igd_flags &= ~IGD_HDMI_STEREO_3D_MODE;
	}

	/* Instead of every time reading current s3d mode from
	 * port attributes, save it in crtc for faster access */
	emgd_crtc->current_s3d_mode = s3d_mode_sel;

	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}


/*!
 * This function is used to patch the drm_display_mode with S3D flag
 * if S3D display is attached and PD_ATTR_ID_S3D_MODE_EXPOSE
 * port attribute is set to enable
 *
 * @param port	igd_display_port
 * @param drm_mode	display mode from drm
 *
 * @return 0 on success.
 * @return -IGD_ERROR_INVAL on failure
 */
int s3d_mode_patch(igd_display_port_t *port,
		struct drm_display_mode *drm_mode)
{
	unsigned long	s3d_expose_mode = 0;
	int i;

	EMGD_TRACE_ENTER;

	/* Check if S3D_EXPOSE_MODE attribute is set to expose
	 * DRM 3D flags. For now, only mandatory 3D modes are exposed.
	 */
	if(pi_pd_find_attr_and_value(port,
			PD_ATTR_ID_S3D_MODE_EXPOSE,
			0,		/* no PD_FLAG for S3D attr*/
			NULL,	/* don't need the attr ptr*/
			&s3d_expose_mode)) {
		EMGD_DEBUG("S3D_EXPOSE_MODE attribute not found, port doesn't support S3D");
		EMGD_TRACE_EXIT;
		return -IGD_ERROR_INVAL;
	}

	if(s3d_expose_mode) {
		drm_mode->flags &= ~DRM_MODE_FLAG_3D_MASK;

		for(i = 0; i < sizeof(s3d_mandatory_modes)/sizeof(struct s3d_modes); i++) {
			if(drm_mode->hdisplay == s3d_mandatory_modes[i].width &&
				drm_mode->vdisplay == s3d_mandatory_modes[i].height &&
				(drm_mode->flags & s3d_mandatory_modes[i].select) ==
										s3d_mandatory_modes[i].value &&
				drm_mode->vrefresh == s3d_mandatory_modes[i].freq)
			{
					drm_mode->flags |= s3d_mandatory_modes[i].formats;
					EMGD_DEBUG("Patch DRM mode %dx%d@%d with S3D flags",
							drm_mode->hdisplay, drm_mode->vdisplay, drm_mode->vrefresh);
			}
		}
	}
	EMGD_TRACE_EXIT;
	return IGD_SUCCESS;
}
