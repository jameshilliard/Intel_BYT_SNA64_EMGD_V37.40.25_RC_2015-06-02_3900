/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: hdmi_port.c
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
 *  Port driver interface functions
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.port

#include <igd_core_structs.h>
#include "hdmi_port.h"
#include "emgd_drm.h"
#include "default_config.h"

extern emgd_drm_config_t *config_drm;

/* Include supported chipset here */
static unsigned long supported_chipset[] =
{
	PCI_DEVICE_ID_VGA_SNB,
	PCI_DEVICE_ID_VGA_IVB,
	PCI_DEVICE_ID_VGA_VLV,
	PCI_DEVICE_ID_VGA_VLV2,
	PCI_DEVICE_ID_VGA_VLV3,
	PCI_DEVICE_ID_VGA_VLV4,
};

/* .......................................................................... */
int hdmi_open(pd_callback_t *p_callback, void **p_context);
int hdmi_init_device(void *p_context);
int hdmi_get_timing_list(void *p_context, pd_timing_t *p_in_list,
	pd_timing_t **pp_out_list);
int hdmi_set_mode(void *p_context, pd_timing_t *p_mode, unsigned long flags);
int hdmi_post_set_mode(void *p_context, pd_timing_t *p_mode,
	unsigned long status);
int hdmi_get_attributes(void *p_context, unsigned long *p_num_attr,
	pd_attr_t **pp_list);
int hdmi_set_attributes(void *p_context, unsigned long num_attr,
	pd_attr_t *p_list);
unsigned long hdmi_validate(unsigned long cookie);
int hdmi_close(void *p_context);
int hdmi_set_power(void *p_context, uint32_t state);
int hdmi_get_power(void *p_context, uint32_t *p_state);
int hdmi_save(void *p_context, void **pp_state, unsigned long flags);
int hdmi_restore(void *p_context, void *p_state, unsigned long flags);
int hdmi_get_port_status(void *p_context, pd_port_status_t *port_status);
int hdmi_init_attribute_table(hdmi_device_context_t *p_ctx);

static pd_version_t  g_hdmi_version = {1, 0, 0, 0};
/* May include dab list for SDVO here if we would like to do SDVO card detection
here instead of HAL */
static unsigned long g_hdmi_dab_list[] = {0x70, 0x72, PD_DAB_LIST_END};

static pd_driver_t	 g_hdmi_drv = {
	PD_SDK_VERSION,
	"HDMI Port Driver",
	0,
	&g_hdmi_version,
	PD_DISPLAY_HDMI_INT,
	PD_FLAG_CLOCK_MASTER,
	g_hdmi_dab_list,
	400, /* Need to confirm 400 or 1000 */
	hdmi_validate,
	hdmi_open,
	hdmi_init_device,
	hdmi_close,
	hdmi_set_mode,
	hdmi_post_set_mode,
	hdmi_set_attributes,
	hdmi_get_attributes,
	hdmi_get_timing_list,
	hdmi_set_power,
	hdmi_get_power,
	hdmi_save,
	hdmi_restore,
	hdmi_get_port_status,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0
};


/* ............................ S3D Attributes ............................ */
static pd_attr_t g_s3d_set_attrs[] =
/*	ID,	Type, */
/*Name,	Flag, default_value, current_value,pad0/min, pad1/max, pad2/step */
{
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST,
		MAKE_NAME("S3D Set"),
		0, 0, PD_S3D_MODE_OFF, 12, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("Off"),
		0, PD_S3D_MODE_OFF, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("Frame Packing"),
		0, PD_S3D_MODE_FRAME_PACKING, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 0"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_HOR_SUB_SAMPLING_OO, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 1"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_HOR_SUB_SAMPLING_OE, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 2"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_HOR_SUB_SAMPLING_EO, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 3"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_HOR_SUB_SAMPLING_EE, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 4"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_QM_SUB_SAMPLING_OO, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 5"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_QM_SUB_SAMPLING_OE, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 6"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_QM_SUB_SAMPLING_EO, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Half 7"),
		0, PD_S3D_MODE_SBS_HALF | PD_S3D_FORMAT_QM_SUB_SAMPLING_EE, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("Top_Bottom"),
		0, PD_S3D_MODE_TOP_BOTTOM, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_SET, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("SBS Full"),
		0, PD_S3D_MODE_SBS_FULL, 0, 0, 0, 0),
};

static pd_attr_t g_s3d_expose_mode_attrs[] =
/*	ID,	Type, */
/*Name,	Flag, default_value, current_value,pad0/min, pad1/max, pad2/step */
{
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_EXPOSE, PD_ATTR_TYPE_LIST,
		MAKE_NAME("expose 3D modes"),
		0, 0, PD_S3D_MODE_EXPOSE_OFF, 2, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_EXPOSE, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("Off"),
		0, PD_S3D_MODE_EXPOSE_OFF, 0, 0, 0, 0),
	PD_MAKE_ATTR(PD_ATTR_ID_S3D_MODE_EXPOSE, PD_ATTR_TYPE_LIST_ENTRY,
		MAKE_NAME("On"),
		0, PD_S3D_MODE_EXPOSE_ON, 0, 0, 0, 0),
};

/* .......................................................................... */
/* .......................................................................... */
/*============================================================================
	Function	:	pd_init is the first function that is invoked by IEG driver.

	Parameters	:	handle : not used

	Remarks     :   pd_init initializes pd_driver_t structure and registers the
					port driver with IEG driver by calling pd_register function

	Returns     :	Status returned by pd_register function.
	------------------------------------------------------------------------- */
int PD_MODULE_INIT(hdmi_init, (void *handle))
{
	int status;

	EMGD_DEBUG("hdmi: pd_init()");

	status = pd_register(handle, &g_hdmi_drv);
	if (status != PD_SUCCESS) {
		EMGD_DEBUG("hdmi: Error ! pd_init: pd_register() failed with "
			"status=%#x", status);
	}
	return status;
}


/*----------------------------------------------------------------------
 * Function: hdmi_exit()
 *
 * Description: This is the exit function for hdmi port driver to unload
 *              the driver.
 *
 * Parameters:  None.
 *
 * Return:      PD_SUCCESS(0)  success
 *              PD_ERR_XXXXXX  otherwise
 *----------------------------------------------------------------------*/
int PD_MODULE_EXIT(hdmi_exit, (void))
{
	return (PD_SUCCESS);
} /* end hdmi_exit() */


/*	============================================================================
	Function	:	hdmi_open is called for each combination of port and dab
					registers to detect the hdmi device.

	Parameters	:	p_callback : Contains pointers to read_regs/write_regs
								functions to access I2C registes.

					pp_context	  : Pointer to port driver allocated context
								structure is returned in this argument

	Remarks 	:	hdmi_open detects the presence of hdmi device for specified
					port.

	Returns 	:	PD_SUCCESS If hdmi device is detected
					PD_ERR_xxx On Failure
	------------------------------------------------------------------------- */
int hdmi_open(pd_callback_t *p_callback, void **pp_context)
{
	hdmi_device_context_t *p_ctx;
	pd_reg_t reg_list[2];
	unsigned short chipset;
	int ret,i,valid_chipset = 0;

	if (!p_callback || !pp_context) {
		EMGD_ERROR("Invalid parameter");
		return (PD_ERR_NULL_PTR);
	}

	/* Verify that this is an GMCH with Internal HDMI available */
	reg_list[0].reg = 2;
	reg_list[1].reg = PD_REG_LIST_END;
	ret = p_callback->read_regs(p_callback->callback_context, reg_list, PD_REG_PCI);
	if(ret != PD_SUCCESS) {
		return ret;
	}
	chipset = (unsigned short)(reg_list[0].value & 0xffff);
	/* Loop through supported chipset */
	for (i = 0; i < ARRAY_SIZE(supported_chipset); i++) {
		if (chipset == supported_chipset[i]){
			valid_chipset = 1;
			break;
		}
	}
	if(!valid_chipset){
		return PD_ERR_NODEV;
	}

	/* Allocate HDMI context */
	p_ctx = pd_malloc(sizeof(hdmi_device_context_t));
	if (p_ctx == NULL) {
		EMGD_ERROR("hdmi: Error ! hdmi_open: pd_malloc() failed");
		return PD_ERR_NOMEM;
	}
	pd_memset(p_ctx, 0, sizeof(hdmi_device_context_t));
	/* Before setting this pointer, there are cases where we call the port
	 * driver's open function twice, see pi_pd_register for this.  In that case,
	 * in order to avoid a memory leak, we need to free a previously allocated
	 * context.
	 */
	if (*pp_context) {
		pd_free(*pp_context);
	}
	*pp_context = p_ctx;
	p_ctx->p_callback = p_callback;
	p_ctx->chipset = chipset;

	if(chipset == PCI_DEVICE_ID_VGA_SNB) {
		if(p_ctx->p_callback->port_num == PORT_B_NUMBER){
			p_ctx->control_reg = HDMI_CONTROL_REG_B;
		}else if(p_ctx->p_callback->port_num == PORT_C_NUMBER){
			p_ctx->control_reg = HDMI_CONTROL_REG_C;
		}else if(p_ctx->p_callback->port_num == PORT_D_NUMBER){
			p_ctx->control_reg = HDMI_CONTROL_REG_D;
		}else{
			EMGD_ERROR("Unsupported port number for internal HDMI");
			return PD_ERR_INTERNAL;
		}
	} else {
		if(p_ctx->p_callback->port_num == PORT_B_NUMBER){
			p_ctx->control_reg = VLV_HDMI_CONTROL_REG_B;
		}else if(p_ctx->p_callback->port_num == PORT_C_NUMBER){
			p_ctx->control_reg = VLV_HDMI_CONTROL_REG_C;
		}else{
			EMGD_ERROR("Unsupported port number for internal HDMI");
			return PD_ERR_INTERNAL;
		}
	}
	g_hdmi_drv.type = PD_DISPLAY_HDMI_INT;

	p_ctx->power_state = PD_POWER_MODE_D0;  /* default state is ON */

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_init_device is called to initialize a hdmi device

	Parameters	:	p_context : Pointer to port driver allocated context
					structure

	Remarks 	:

	Returns 	:	PD_SUCCESS	If initialization is successful
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_init_device(void *p_context)
{
	EMGD_DEBUG("hdmi: hdmi_init_device()");

	g_hdmi_drv.num_devices++;
	/* Should we continue even no panel is available? */
	/* status = hdmi_audio_characteristic(p_context); */
	hdmi_audio_characteristic(p_context);

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_get_timing_list is called to get the list of display
					modes supported by the hdmi device and the display.

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_in_list: List of display modes supported by the IEG driver
					pp_out_list: List of modes supported by the hdmi device

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_get_timing_list(void *p_context, pd_timing_t *p_in_list,
	pd_timing_t **pp_out_list)
{
	int ret = 0;
	hdmi_device_context_t *pd_context = (hdmi_device_context_t *)p_context;
	pd_dvo_info_t hdmi_info = {0, 0, 0, 0, 0, 0, 0, 0};
	pd_display_info_t hdmi_display_info = {0, 0, 0, 0, NULL};

	EMGD_DEBUG("hdmi: hdmi_get_timing_list()");

	ret = pd_filter_timings(pd_context->p_callback->callback_context,
		p_in_list, &pd_context->timing_table, &hdmi_info, &hdmi_display_info);
	*pp_out_list = pd_context->timing_table;

	return ret;
}

/*	============================================================================
	Function	:	hdmi_set_mode is called to test if specified mode can be
					supported or to set it.

	Parameters	:	p_context: Pointer to port driver allocated context
					p_mode	: New mode
					flags	: In test mode it is set to PD_SET_MODE_FLAG_TEST

	Remarks 	:	hdmi_set_mode first verifies that the new mode is
					supportable.
					If not it returns an error status
					If the flags is not set to PD_SET_MODE_FLAG_TEST it sets the
					new mode.

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_set_mode(void *p_context, pd_timing_t *p_mode, unsigned long flags)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	/* mmio_reg_t	ctrl_reg=0; */
	uint32_t aud_config_reg=0;
	uint32_t audio_pixel_clk;

	EMGD_DEBUG("hdmi: hdmi_set_mode()");

	/* HDMI port can be connected to different Pipe.
	 * Map the audio and video_dip registers to pipe in use.
	 * The registers will be used during post_set_mode
	 */
	if(hdmi_map_audio_video_dip_regs(p_ctx) != PD_SUCCESS) {
		EMGD_ERROR("Failed to map audio and video_dip registers to pipe in use");
		return PD_ERR_UNSUCCESSFUL;
	}

	/* default value */
	audio_pixel_clk = 1;

	/* Program audio config register */
	if(p_mode->dclk == 25175) {
		audio_pixel_clk = 0;
	} else if(p_mode->dclk == 25200) {
		audio_pixel_clk = 1;
	} else if(p_mode->dclk == 27000) {
		audio_pixel_clk = 2;
	} else if(p_mode->dclk == 27027) {
		audio_pixel_clk = 3;
	} else if(p_mode->dclk == 54000) {
		audio_pixel_clk = 4;
	} else if(p_mode->dclk == 54054) {
		audio_pixel_clk = 5;
	} else if(p_mode->dclk == 74175) {
		audio_pixel_clk = 6;
	} else if(p_mode->dclk == 74250) {
		audio_pixel_clk = 7;
	} else if(p_mode->dclk == 14835) {
		audio_pixel_clk = 8;
	} else if(p_mode->dclk == 14850) {
		audio_pixel_clk = 9;
	}

	hdmi_read_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_config, &aud_config_reg);
	aud_config_reg &= ~(BIT(16)|BIT(17)|BIT(18)|BIT(19));
	aud_config_reg |= (audio_pixel_clk & 0xF) << 16;
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_config, aud_config_reg);

	/* set mode flag test */
	if (flags & PD_SET_MODE_FLAG_TEST) {
		return PD_SUCCESS;
	}

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_post_set_mode

	Parameters	:	p_context: Pointer to port driver allocated context
					p_mode	:
					flags	:

	Remarks 	:

	Returns 	:
	------------------------------------------------------------------------- */
int hdmi_post_set_mode(void *p_context, pd_timing_t *p_mode,
	unsigned long status)
{
	int pd_status = PD_SUCCESS;

	mmio_reg_t	ctrl_reg = 0;
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	EMGD_DEBUG("hdmi: hdmi_post_set_mode()");
	if(((p_ctx->p_callback->eld) != NULL) && (*(p_ctx->p_callback->eld) != NULL)){
		if((*p_ctx->p_callback->eld)->audio_support){
			pd_status = hdmi_configure(p_context);
		}else{
			/* Disable audio. This looks like a HDMI-DVI connection */
			hdmi_read_mmio_reg(p_ctx, p_ctx->control_reg,
							&ctrl_reg);
			ctrl_reg &= ~BIT(6);
			ctrl_reg &= ~BIT(9);
			hdmi_write_mmio_reg(p_ctx, p_ctx->control_reg ,
							ctrl_reg);
			hdmi_read_mmio_reg(p_ctx, p_ctx->control_reg,
							&ctrl_reg);
		}
	}
	return pd_status;
}


/*	============================================================================
	Function	:	hdmi_get_attributes is called to get the list of all the
					available attributes

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_Num	: Return the total number of attributes
					pp_list	: Return the list of port driver attributes

	Remarks 	:	hdmi_get_attributes calls hdmi interface functions to get all
					available range,bool and list attributes supported by the
					hdmi device

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_get_attributes(void *p_context, unsigned long *p_num_attr,
	pd_attr_t **pp_list)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	int status = PD_SUCCESS;

	/* Create attribute table if not already created */
	if (p_ctx->p_attr_table == NULL) {
		hdmi_init_attribute_table(p_ctx);
	}


	*pp_list	= p_ctx->p_attr_table;
	*p_num_attr	= p_ctx->num_attrs;

	EMGD_DEBUG("hdmi: hdmi_get_attributes()");
	return status;
}


/*	============================================================================
	Function	:	hdmi_set_attributes is called to modify one or more display
					attributes

	Parameters	:	p_context: Pointer to port driver allocated context structure
					num 	: Number of attributes
					p_list	: List of attributes

	Remarks 	:	hdmi_set_attributes scans the attribute list to find the ones
					that are to be modified by checking flags field in each
					attribute for PD_ATTR_FLAG_VALUE_CHANGED bit. If this bit is
					set it will call hdmi interface functions to set the new
					value for the attribute.

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_set_attributes(void *p_context, unsigned long num_attrs,
	pd_attr_t *p_list)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	unsigned long i = 0;

	/*	Create attribute table if not already created                         */
	if (p_ctx->p_attr_table == NULL) {
		hdmi_init_attribute_table(p_ctx);
	}


	for (i = 0; i < num_attrs; i++) {
		pd_attr_t *p_attr;
		uint32_t new_value;

		if ((p_list[i].flags & PD_ATTR_FLAG_VALUE_CHANGED) == 0) {
			continue;
		}

		/*	Clear attribute changed flag */
		p_list[i].flags &= ~PD_ATTR_FLAG_VALUE_CHANGED;
		new_value = p_list[i].current_value;

		p_attr = pd_get_attr(p_ctx->p_attr_table, p_ctx->num_attrs, p_list[i].id, 0);
		if (p_attr == NULL) {
			EMGD_DEBUG("hdmi: Error ! pd_get_attr() failed for "
					"attr='%s', id=%ld", HDMI_GET_ATTR_NAME((&p_list[i])),
					(unsigned long)p_list[i].id);
			continue;
		}

		p_attr->current_value = new_value;
		EMGD_DEBUG("hdmi: Success ! hdmi_set_attributes: "
			"attr='%s', id=%ld, current_value=%ld",
			HDMI_GET_ATTR_NAME(p_attr), (unsigned long)p_attr->id,
			(unsigned long)p_attr->current_value);
	}
	EMGD_DEBUG("hdmi: hdmi_set_attributes()");

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	hdmi_set_power is called to change the power state of the
					device

	Parameters	:	p_context: Pointer to port driver allocated context structure
					state	: New power state

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_set_power(void *p_context, uint32_t state)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	mmio_reg_t	ctrl_reg = 0;

	EMGD_DEBUG("hdmi: hdmi_set_power()");

	/* program port */
	hdmi_read_mmio_reg(p_ctx, p_ctx->control_reg,
						&ctrl_reg);

	switch(state) {
	case PD_POWER_MODE_D0:

		EMGD_DEBUG("hdmi: hdmi_set_power: Power state: PD_POWER_MODE_D0");
		ctrl_reg |= BIT(31);
		/* Select TMDS encoding */
		ctrl_reg |= BIT(11);
		ctrl_reg &= ~BIT(10);
		/* Select HDMI mode */
		ctrl_reg |= BIT(9);
		/* update power state */
		p_ctx->power_state = PD_POWER_MODE_D0;
		break;

	case PD_POWER_MODE_D1:
	case PD_POWER_MODE_D2:
	case PD_POWER_MODE_D3:

		EMGD_DEBUG("hdmi: hdmi_set_power: Power state: PD_POWER_MODE_D1/D2/D3");
		ctrl_reg &= ~BIT(31);
		/* Disable audio */
		ctrl_reg &= ~BIT(6);
		/* Disable NULL Infoframes */
		ctrl_reg &= ~BIT(9);
		/* update power state */
		p_ctx->power_state = state;
		break;

	default:
		EMGD_ERROR("hdmi: Error, Invalid Power state received!");
		return (PD_ERR_INVALID_POWER);
	}
	/* Now write the new power state */
	hdmi_write_mmio_reg(p_ctx, p_ctx->control_reg, ctrl_reg);

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	hdmi_get_power is called to get the current power state

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_state	: Returns the current power state

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int hdmi_get_power(void *p_context, uint32_t *p_state)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;

	EMGD_DEBUG("hdmi: hdmi_get_power()");

	if(p_ctx == NULL || p_state == NULL) {
		EMGD_ERROR("HDMI:hdmi_get_power()=> Error: Null Pointer passed in");
		return PD_ERR_NULL_PTR;
	}

	/* Return the power state saved in the context */
	*p_state = p_ctx->power_state;

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	hdmi_save is called to save the default state of registers

	Parameters	:	p_context: Pointer to port driver allocated context structure
					pp_state : Returs a pointer to list of hdmi registers
					terminated with PD_REG_LIST_END.
					flags	: Not used

	Remarks     :	hdmi_save does not save any registers.

	Returns		:	PD_SUCCESS
	------------------------------------------------------------------------- */
int hdmi_save(void *p_context, void **pp_state, unsigned long flags)
{

	EMGD_DEBUG("hdmi: hdmi_save()");

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	hdmi_restore is called to restore the registers which were
					save previously via a call to hdmi_save

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_state	: List of hdmi registers
					flags	: Not used

	Remarks     :

	Returns     :	PD_SUCCESS
	------------------------------------------------------------------------- */
int hdmi_restore(void *p_context, void *p_state, unsigned long flags)
{

	EMGD_DEBUG("hdmi: hdmi_restore()");

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	hdmi_validate

	Parameters	:	cookie

	Remarks 	:	hdmi_Valite returns the cookie it received as an argument

	Returns 	:	cookie
	------------------------------------------------------------------------- */
unsigned long hdmi_validate(unsigned long cookie)
{
	EMGD_DEBUG("hdmi: hdmi_validate()");
	return cookie;
}


/*	============================================================================
	Function	:	hdmi_close is the last function to be called in the port
					driver

	Parameters	:	p_context: Pointer to port driver allocated context structure

	Remarks 	:

	Returns 	:	PD_SUCCESS
	------------------------------------------------------------------------- */
int hdmi_close(void *p_context)
{
#ifndef CONFIG_MICRO
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;

	EMGD_DEBUG("hdmi: hdmi_close()");

	if (p_ctx) {
		if (p_ctx->p_attr_table != NULL) {
			pd_free(p_ctx->p_attr_table);
			p_ctx->p_attr_table = NULL;
			p_ctx->num_attrs	 = 0;
		}
		if (p_ctx->timing_table) {
			pd_free(p_ctx->timing_table);
			p_ctx->timing_table = NULL;
		}
		pd_free(p_ctx);
		g_hdmi_drv.num_devices--;
	}
#endif
	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_get_port_status is called to get the status of the
					display

	Parameters	:	p_context: Pointer to port driver allocated context structure
					port_status : Returns display type and connection state

	Returns 	:	PD_SUCCESS or PD_ERR_XXX
	------------------------------------------------------------------------- */
int hdmi_get_port_status(void *p_context, pd_port_status_t *port_status)
{
	int status = 0, retries = 5;
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;

	EMGD_DEBUG("hdmi: hdmi_get_port_status()");

	port_status->display_type = PD_DISPLAY_HDMI_INT;
	port_status->connected    = PD_DISP_STATUS_DETACHED;

	/* We need to detect if the display is attched here.
	   Using read edid to check this */
	status = hdmi_read_ddc_edid_header(p_ctx, 0x0);
	if (!status &&
		(config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)) {
		do {
			/*
			 *Putting driver to sleep before next read
			 *enable the hdmi panel to be more stable.
			 */
			pd_usleep(2000);
			status = hdmi_read_ddc_edid_header(p_ctx, 0x0);
		} while (!status && --retries);
	}

	if (status)
		port_status->connected = PD_DISP_STATUS_ATTACHED;

	return PD_SUCCESS;
}

unsigned long hdmi_get_s3d_attrs(hdmi_device_context_t *p_ctx,   pd_attr_t *p_attr)
{
	int i,num_attrs = 0;

	/* Return number of attributes if list is NULL */
	if (p_attr == NULL) {
		return (ARRAY_SIZE(g_s3d_set_attrs) +
				ARRAY_SIZE(g_s3d_expose_mode_attrs));
	}

	for (i = 0; i < ARRAY_SIZE(g_s3d_set_attrs); i++) {
		pd_attr_t *p_attr_cur = &p_attr[num_attrs];

		pd_memcpy(p_attr_cur, &g_s3d_set_attrs[i], sizeof(g_s3d_set_attrs[i]));
		num_attrs++;
	}

	for (i = 0; i <  ARRAY_SIZE(g_s3d_expose_mode_attrs); i++) {
		pd_attr_t *p_attr_cur = &p_attr[num_attrs];

		pd_memcpy(p_attr_cur, &g_s3d_expose_mode_attrs[i],
				sizeof(g_s3d_expose_mode_attrs[i]));
		num_attrs++;
	}

	return num_attrs;
}


/*	============================================================================
	Function	:	hdmi_init_attribute_table

	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */
int hdmi_init_attribute_table(hdmi_device_context_t *p_ctx)
{
	unsigned long num_s3d_attrs;
	unsigned char *p_table;

	num_s3d_attrs = hdmi_get_s3d_attrs(p_ctx, NULL);
	p_ctx->num_attrs  = num_s3d_attrs;

	p_ctx->p_attr_table = pd_malloc((p_ctx->num_attrs + 1) * sizeof(pd_attr_t));
	if (p_ctx->p_attr_table == NULL) {

		EMGD_ERROR("hdmi: Error ! sdvo_init_attribute_table: "
			"pd_malloc(p_attr_table) failed");

		p_ctx->num_attrs = 0;
		return PD_ERR_NOMEM;
	}

	pd_memset(p_ctx->p_attr_table, 0, (p_ctx->num_attrs + 1) * sizeof(pd_attr_t));
	p_table = (unsigned char *)p_ctx->p_attr_table;

	hdmi_get_s3d_attrs(p_ctx, (pd_attr_t *)p_table);

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_read_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char hdmi_read_mmio_reg(hdmi_device_context_t *p_ctx, uint32_t reg,
						mmio_reg_t *p_Value)
{
	pd_reg_t reg_list[2];
	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
			reg_list, PD_REG_MIO)) {
		return FALSE;
	} else {
		*p_Value = (mmio_reg_t)reg_list[0].value;
		EMGD_DEBUG("hdmi : R : 0x%02lx, 0x%02lx", (unsigned long)reg,
			(unsigned long)*p_Value);
		return TRUE;
	}
}

/*	============================================================================
	Function	:	hdmi_write_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char hdmi_write_mmio_reg(hdmi_device_context_t *p_ctx, uint32_t reg,
						mmio_reg_t p_Value)
{
	pd_reg_t reg_list[2];
	reg_list[0].reg = reg;
	reg_list[0].value = p_Value;

	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->write_regs(p_ctx->p_callback->callback_context, reg_list,
			PD_REG_MIO)) {

		return FALSE;

	}else {

		return TRUE;
	}
}

/*	============================================================================
	Function	:	hdmi_write_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver based on bit and bit number
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char hdmi_write_mmio_reg_bit(hdmi_device_context_t *p_ctx,
	uint32_t reg, unsigned char input, unsigned char bit_loc)
{
	pd_reg_t reg_list[2];
	mmio_reg_t p_Value;

	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
		reg_list, PD_REG_MIO)) {
		return FALSE;
	} else {
		p_Value = (mmio_reg_t)reg_list[0].value;

		if(input){
			p_Value |= (bit_loc);
		}else{
			p_Value &= (~bit_loc);
		}

		reg_list[0].reg		= reg;
		reg_list[0].value	= p_Value;
		reg_list[1].reg		= PD_REG_LIST_END;

		if (p_ctx->p_callback->write_regs(p_ctx->p_callback->callback_context, reg_list,
				PD_REG_MIO)) {
			return FALSE;
		}else {
			return TRUE;
		}
	}
}

/*	============================================================================
	Function	:	hdmi_read_ddc_reg is called to for i2c read operation for ddc
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

int hdmi_read_ddc_reg(hdmi_device_context_t *p_ctx, unsigned char offset,
						i2c_reg_t *p_Value)
{
	pd_reg_t reg_list[2];
	reg_list[0].reg = offset;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
		reg_list, PD_REG_DDC)) {
		return FALSE;
	} else {
		*p_Value = (i2c_reg_t)reg_list[0].value;
		EMGD_DEBUG("hdmi : R : 0x%02x, 0x%02x", offset, *p_Value);
		return TRUE;
	}
}

/*  ============================================================================
    Function    :   hdmi_read_ddc_edid_header is called to try get EDID header

    Parameters  :   p_ctx: Pointer to port driver allocated context structure
                    offset: register offset address

    Returns     :   TRUE on got a valid EDID header
                    FALSE on not got a valid EDID header
    ------------------------------------------------------------------------- */
int hdmi_read_ddc_edid_header(hdmi_device_context_t *p_ctx, unsigned char offset)
{
    pd_reg_t reg_list[9] = {{0}};
    reg_list[0].reg = offset;
    reg_list[1].reg = offset+1;
    reg_list[2].reg = offset+2;
    reg_list[3].reg = offset+3;
    reg_list[4].reg = offset+4;
    reg_list[5].reg = offset+5;
    reg_list[6].reg = offset+6;
    reg_list[7].reg = offset+7;
    reg_list[8].reg = PD_REG_LIST_END;

    if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
                                     reg_list, PD_REG_DDC)) {
        EMGD_DEBUG("Can not get EDID header");
        return FALSE;
    } else {
        if (reg_list[0].value != 0x00 ||
            reg_list[1].value != 0xff ||
            reg_list[2].value != 0xff ||
            reg_list[3].value != 0xff ||
            reg_list[4].value != 0xff ||
            reg_list[5].value != 0xff ||
            reg_list[6].value != 0xff ||
            reg_list[7].value != 0x00)
        {
            EMGD_DEBUG("Got data, but not EDID header");
            return FALSE;
        }

        EMGD_DEBUG("Got EDID header");
        return TRUE;
    }
}

/*	============================================================================
	Function	:	hdmi_audio_characteristic

	Parameters	:

	Remarks 	:	Set audio characteristic to be used in ELD calcilation.
					TODO: Make sure this is recalled if hotplug is detected

	Returns 	:
	------------------------------------------------------------------------- */
int hdmi_audio_characteristic(hdmi_device_context_t *p_ctx)
{
	EMGD_DEBUG("hdmi: hdmi_audio_characteristic()");

	if(!(p_ctx)){
		EMGD_DEBUG("hdmi: hdmi_audio_characteristic()->context is null");
		return PD_ERR_INTERNAL;
	}
	if((!p_ctx->p_callback) || (!p_ctx->p_callback->eld)){
		EMGD_DEBUG("hdmi: hdmi_audio_characteristic()->callback or eld is null");
		return PD_ERR_INTERNAL;
	}
	if(*(p_ctx->p_callback->eld) == NULL){
		return PD_ERR_NULL_PTR;
	}

	(*p_ctx->p_callback->eld)->audio_flag |= PD_AUDIO_CHAR_AVAIL;
	/* Source DPG codes */
	(*p_ctx->p_callback->eld)->NPL = 13;
	(*p_ctx->p_callback->eld)->K0 = 30;
	(*p_ctx->p_callback->eld)->K1 = 74;

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_configure is called to program AVI infoframes, SPD
					infoframes, ELD data for audio, colorimetry and pixel replication.

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_mode: Point to current video output display mode

	Remarks 	:

	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int hdmi_configure(hdmi_device_context_t *p_ctx)
{
	uint32_t aud_cntl_st_reg=0, udi_if_ctrl_reg=0;
	pd_attr_t *p_attr;
	igd_display_port_t *port = (igd_display_port_t *)p_ctx->p_callback->callback_context;

	EMGD_DEBUG("hdmi: hdmi_configure()");

	/* Just return success since set mode should still work */
	if((p_ctx->p_callback->eld) == NULL){
		EMGD_ERROR("ELD not available");
		return PD_SUCCESS;
	}
	if(*(p_ctx->p_callback->eld) == NULL){
		EMGD_ERROR("ELD not available");
		return PD_SUCCESS;
	}
	if(hdmi_audio_characteristic(p_ctx) != PD_SUCCESS){
		EMGD_ERROR("Audio not available");
		return PD_SUCCESS;
	}

	/* Build and send ELD Info Frames */
	if(hdmi_send_eld(p_ctx) != PD_SUCCESS){
		EMGD_ERROR("Fail to write ELD to transmitter");
	}

	if(CHECK_NAME(port->edid->vendor, "CMN") && (port->edid->product_code == 0x1123)) {
		/* This monitor from CHIMEI with product code 0x1123 (model N116HSE-EB1) will not
		 * be able to work with HDMI if AVI infoframe is programmed into the HDMI output.
		 * The best solution/hack for now is to skip programming AVI by product identity in EDID.
		 * If EDID is disable, hdmi_configure() will not be called and no infoframe will be
		 * programmed. So this monitor will work with EDID disable as well
		 */
		EMGD_DEBUG("Monitor N116HSE-EB1 detected, skip programming AVI");
	}
	else {
		/* Build and send AVI Info Frames */
		if (hdmi_avi_info_frame(p_ctx) != PD_SUCCESS) {
			EMGD_ERROR("Fail to write AVI infoframes to transmitter");
		}
	}

	/* Build and send Vendor Specific Info Frames */
	p_attr = pd_get_attr(p_ctx->p_attr_table, p_ctx->num_attrs,
										PD_ATTR_ID_S3D_MODE_SET, 0);
	if (p_attr != NULL) {
		switch (p_attr->current_value & 0xF0)
		{
			case PD_S3D_MODE_OFF:
				p_ctx->s3d_enable = 0;
				p_ctx->s3d_structure = 0x0;
				p_ctx->s3d_ext_data = 0x0;
				break;

			case PD_S3D_MODE_SBS_HALF:
				/* S3D SBS Half */
				p_ctx->s3d_enable = 1;
				p_ctx->s3d_structure = PD_S3D_FORMAT_SBS_HALF << 4;	/* 0x80 */
				if ((p_attr->current_value & 0x0F) < PD_S3D_FORMAT_EXT_DATA_RESERVED) {
					p_ctx->s3d_ext_data = (p_attr->current_value & 0x0F) << 4;
				}
				else {
					p_ctx->s3d_ext_data = 0x0;	/* default to horizontal sub-sampling odd/odd */
				}
				break;
			case PD_S3D_MODE_TOP_BOTTOM:
				/* S3D Top-Bottom */
				p_ctx->s3d_enable = 1;
				p_ctx->s3d_structure = PD_S3D_FORMAT_TOP_BOTTOM << 4;	/* 0x60 */
				p_ctx->s3d_ext_data = 0x0;
				break;
			case PD_S3D_MODE_FRAME_PACKING:
				/* Frame-packing */
				p_ctx->s3d_enable = 1;
				p_ctx->s3d_structure = PD_S3D_FORMAT_FRAME_PACKING << 4;	/* 0x00 */
				p_ctx->s3d_ext_data = 0x0;
				break;
			case PD_S3D_MODE_SBS_FULL:
				/* S3D SBS Full */
				p_ctx->s3d_enable = 1;
				p_ctx->s3d_structure = PD_S3D_FORMAT_SBS_FULL << 4;	/* 0x30 */
				p_ctx->s3d_ext_data = 0x0;
				break;
			default:
				p_ctx->s3d_enable = 0;
				p_ctx->s3d_structure = 0x0;
				p_ctx->s3d_ext_data = 0x0;
				break;
		}

		if (p_ctx->s3d_enable) {
			EMGD_DEBUG("Program VSD, S3D_MODE_SET value: 0x%x, s3d_structure: 0x%x, s3d_ext_data: 0x%x",
					p_attr->current_value,
					p_ctx->s3d_structure,
					p_ctx->s3d_ext_data);
			if (hdmi_vendor_info_frame(p_ctx) != PD_SUCCESS) {
				EMGD_ERROR("Fail to write Vendor Specific infoframes to transmitter");
			}
		}
	} else {
		EMGD_ERROR("S3D_MODE_SET attribute not found in HDMI port!");
	}

#ifndef CONFIG_MICRO
	/* Build and send SPD Info Frames */
	if (hdmi_spd_info_frame(p_ctx) != PD_SUCCESS) {
		EMGD_ERROR("Fail to write SPD Infoframes to transmitter");
	}
#endif

	/* Enable DIP */
	/* Read the UDI Infoframe control register */
	hdmi_read_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_ctl, &udi_if_ctrl_reg);
	/* Enable graphics island data packet */
	udi_if_ctrl_reg |= BIT(31);
	/* Enable DIP type */
	udi_if_ctrl_reg |= (BIT(21)|BIT(24));

	if (p_ctx->s3d_enable) {
		/* Enable Vendor-specific infoframes */
		udi_if_ctrl_reg |= BIT(22);
	} else {
		/* Disable Vendor-specific infoframes */
		udi_if_ctrl_reg &= ~BIT(22);
	}

	if( p_ctx->chipset == PCI_DEVICE_ID_VGA_VLV  || 
		p_ctx->chipset == PCI_DEVICE_ID_VGA_VLV2 ||
		p_ctx->chipset == PCI_DEVICE_ID_VGA_VLV3 ||
		p_ctx->chipset == PCI_DEVICE_ID_VGA_VLV4) {
		/* Select port */
		if(p_ctx->p_callback->port_num == PORT_B_NUMBER){
			udi_if_ctrl_reg &= ~BIT(30);
			udi_if_ctrl_reg |= BIT(29);
		}else if(p_ctx->p_callback->port_num == PORT_C_NUMBER){
			udi_if_ctrl_reg |= BIT(30);
			udi_if_ctrl_reg &= ~BIT(29);
		}else{
			EMGD_ERROR("Unsupported port number for internal HDMI");
			return PD_ERR_INTERNAL;
		}
	}
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);
	EMGD_DEBUG("DIP reg: 0x%x = 0x%x", p_ctx->hdmi_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

	/* Enable ELD */
	/* Read the audio control state register */
	hdmi_read_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_cntl_st, &aud_cntl_st_reg);
	/* Set ELD valid to valid */
	aud_cntl_st_reg |= BIT(14);
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_send_eld

	Parameters	:

	Remarks 	:	Builds eld structure and write it into SDVO ELD buffers

	Returns 	:
	------------------------------------------------------------------------- */
int hdmi_send_eld(hdmi_device_context_t *p_ctx)
{
	uint32_t aud_cntl_st_reg=0, aud_hdmiw_hdmiedid_reg=0;
	uint32_t data_to_write=0, offset=0;
	unsigned char *eld_data = NULL;
	uint32_t i;

	EMGD_DEBUG("hdmi: hdmi_send_eld()");

	if(!((*p_ctx->p_callback->eld)->audio_flag & ELD_AVAIL)){
		EMGD_DEBUG("Eld not available");
		return PD_ERR_UNSUCCESSFUL;
	}

	/* ELD 1.0, CEA version retrieve from callback */
	(*p_ctx->p_callback->eld)->cea_ver = 0;
	(*p_ctx->p_callback->eld)->eld_ver = 1;

	/* Capability Flags */
	(*p_ctx->p_callback->eld)->repeater = 0;
	(*p_ctx->p_callback->eld)->_44ms = 0;

	/* We do not provide Monitor length*/
	(*p_ctx->p_callback->eld)->mnl = 0;

#ifdef CONFIG_MICRO
	(*p_ctx->p_callback->eld)->sadc = 0;
#endif

	/* Read the audio control state register */
	hdmi_read_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_cntl_st, &aud_cntl_st_reg);


    /* This register is not being used in Sandy Bridge & the data to write is fixed at 84 bytes
       Check for VLV */
	/* Set ELD valid to invalid */
//	aud_cntl_st_reg &= ~BIT(14);
//	hdmi_write_mmio_reg(p_ctx, p_ctx-hdmi_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	/* Reset ELD access address for HW */
	aud_cntl_st_reg &= ~(BIT(5)|BIT(6)|BIT(7)|BIT(8));
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	/* Data to write is max possible HW ELD buffer size */
	data_to_write = (aud_cntl_st_reg >> 9) & 0x1F;
	offset = 0;
	eld_data = (*p_ctx->p_callback->eld)->eld_ptr;

	/* Write ELD to register AUD_HDMIW_HDMIEDID */
	while (data_to_write > 0) {
		if (data_to_write >= 4) {
			/* Note: it is assumed that the reciever of the buffer is also
			 * following little-endian format and the buffer is arranged in
			 * little endian format */
			aud_hdmiw_hdmiedid_reg = MAKE_DWORD(eld_data[offset],
				eld_data[offset+1], eld_data[offset+2], eld_data[offset+3]);

			offset +=4;
			data_to_write -= 4;
			hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_hdmiw_hdmiedid, aud_hdmiw_hdmiedid_reg);
		} else {
			unsigned char buffer[4];
			pd_memset(&buffer[0],0,4*sizeof(unsigned char));
			for(i=0;i<data_to_write;i++) {
				buffer[i] = eld_data[offset++];
			}
			aud_hdmiw_hdmiedid_reg = MAKE_DWORD(buffer[0],
				buffer[1], buffer[2], buffer[3]);
			hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->aud_hdmiw_hdmiedid, aud_hdmiw_hdmiedid_reg);
			data_to_write = 0;
		}
	}

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_avi_info_frame is called to program AVI infoframes

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_mode: Point to current video output display mode

	Remarks 	:

	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int hdmi_avi_info_frame(hdmi_device_context_t *p_context)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	int status = PD_SUCCESS;
	unsigned long sum = 0;
	hdmi_avi_info_t	avi;
	int i;

	EMGD_DEBUG("hdmi: hdmi_avi_info_frame()");

	/* Initialize AVI InfoFrame to no scan info, no bar info, no active format */
	/* mode rgb, active format same as picture format */
	/* no aspect ratio, no colorimetry, no scaling */
	pd_memset(&avi, 0, sizeof(hdmi_avi_info_t));
	avi.header.type = HDMI_AVI_INFO_TYPE;
	avi.header.version = 2;
	avi.header.length = HDMI_AVI_BUFFER_SIZE - sizeof(hdmi_info_header_t);

	/* Set Video ID Code */
	avi.video_id_code = (unsigned char)(*p_ctx->p_callback->eld)->video_code;

	/* Set aspect ratio */
	avi.pic_aspect_ratio = (unsigned char)(*p_ctx->p_callback->eld)->aspect_ratio;

	/* Set Colorimetry */
	avi.colorimetry = (unsigned char)(*p_ctx->p_callback->eld)->colorimetry;

	/* Calc checksum */
	for (i= 0; i < HDMI_AVI_BUFFER_SIZE; i++) {
		sum += avi.data[i];
	}
	avi.header.chksum = (unsigned char)(0x100 - (sum & 0xFF));

	/* Update register */
	status = hdmi_write_dip(p_ctx, avi.data, HDMI_AVI_BUFFER_SIZE, DIP_AVI);
	return status;
}

/*	============================================================================
	Function	:	hdmi_spd_info_frame is called to program SPD infoframes

	Parameters	:	p_context: Pointer to port driver allocated context
								structure

	Remarks     :

	Returns     :	int : Status of command execution
	------------------------------------------------------------------------- */
int hdmi_spd_info_frame(hdmi_device_context_t *p_context)
{
	hdmi_device_context_t *p_ctx = (hdmi_device_context_t *)p_context;
	int status = PD_SUCCESS;
	hdmi_spd_info_t	spd;
	unsigned long sum = 0;
	int i;

	EMGD_DEBUG("hdmi: hdmi_spd_info_frame()");

	/* Initialize SPD InfoFrame to zero */
	pd_memset(&spd, 0, sizeof(hdmi_spd_info_t));
	spd.header.type = HDMI_SPD_INFO_TYPE;
	spd.header.version = 1;
	spd.header.length = HDMI_SPD_BUFFER_SIZE - sizeof(hdmi_info_header_t);

	pd_memcpy(spd.vendor_name, HDMI_VENDOR_NAME, HDMI_INTEL_VENDOR_NAME_SIZE);
	pd_memcpy(spd.product_desc, HDMI_VENDOR_DESCRIPTION, HDMI_IEGD_VENDOR_DESCRIPTION_SIZE);
	spd.source_device = HDMI_SPD_SOURCE_PC;

	/* Calc checksum */
	for (i= 0; i < HDMI_SPD_BUFFER_SIZE; i++) {
		sum += spd.data[i];
	}
	spd.header.chksum = (unsigned char)(0x100 - (sum & 0xFF));

	status = hdmi_write_dip(p_ctx, spd.data, HDMI_SPD_BUFFER_SIZE, DIP_SPD);
	return status;
}

#define HDMI_REG_ID_1 0x03
#define HDMI_REG_ID_2 0x0C
#define HDMI_REG_ID_3 0x00
#define HDMI_3D_VIDEO_FORMAT 0x40

/**
 * intel_hdmi_set_vendor_infoframe
 * Sets the vendor info frame for a particular 3D format  */
int hdmi_vendor_info_frame(hdmi_device_context_t *p_context)
{
	int status = PD_SUCCESS;
	hdmi_vendor_info_t hdmi_vendor_if;
	unsigned long sum = 0;
	int i;

	memset(&hdmi_vendor_if, 0, sizeof(hdmi_vendor_if));
	hdmi_vendor_if.header.type = HDMI_VENDOR_INFO_TYPE;
	hdmi_vendor_if.header.version = HDMI_VENDOR_INFO_VERSION;
	hdmi_vendor_if.header.length = HDMI_VENDOR_INFO_BUFFER_SIZE
										- sizeof(hdmi_info_header_t);
	hdmi_vendor_if.ieee[0] = HDMI_REG_ID_1;
	hdmi_vendor_if.ieee[1] = HDMI_REG_ID_2;
	hdmi_vendor_if.ieee[2] = HDMI_REG_ID_3;
	hdmi_vendor_if.hdmi_video_format = HDMI_3D_VIDEO_FORMAT;
	hdmi_vendor_if.s3d_struct = p_context->s3d_structure;
	hdmi_vendor_if.s3d_ext_data = p_context->s3d_ext_data;

	/* Calc checksum */
	for (i= 0; i < HDMI_VENDOR_INFO_BUFFER_SIZE; i++) {
		sum += hdmi_vendor_if.data[i];
	}
	hdmi_vendor_if.header.chksum = (unsigned char)(0x100 - (sum & 0xFF));

	status = hdmi_write_dip(p_context, hdmi_vendor_if.data, HDMI_VENDOR_INFO_BUFFER_SIZE, DIP_VSD);

	return status;

}

/*	============================================================================
	Function	:	hdmi_write_dip

	Parameters	:

	Remarks 	:	Write DIP

	Returns 	:
	------------------------------------------------------------------------- */
int hdmi_write_dip(hdmi_device_context_t *p_ctx, unsigned char *input,
	unsigned long dip_len, dip_type_t dip_type)
{
	unsigned char data_to_write = 0;
	uint32_t udi_if_ctrl_reg = 0, udi_vidpac_data_reg = 0;
	uint32_t i = 0;
	int count = 0;
	unsigned long temp;

	EMGD_DEBUG("hdmi: hdmi_write_dip()");

	hdmi_read_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_ctl, &udi_if_ctrl_reg);
	/* To update the DIP buffer, UDI_IF_CTL[31] needs to be set */
	udi_if_ctrl_reg |= BIT(31);

	/* Set enable mask for dip type to disable */
	switch (dip_type) {
	case DIP_AVI:
		udi_if_ctrl_reg &= ~BIT(21);
		break;
	case DIP_VSD:
		udi_if_ctrl_reg &= ~BIT(22);
		break;
	case DIP_GMP:
		udi_if_ctrl_reg &= ~BIT(23);
		break;
	case DIP_SPD:
		udi_if_ctrl_reg &= ~BIT(24);
		break;
	default:
		return PD_ERR_UNSUCCESSFUL;
	}

	/* Make sure input DIP size is acceptable */
	temp = (((udi_if_ctrl_reg >> 8) & 0xF)*4);
	EMGD_DEBUG("DIP_LEN=0x%lx, DIP buffer size=0x%lx", dip_len, temp);
	if (dip_len > temp) {
		EMGD_ERROR("Warning: DIP_LEN is larger than DIP buffer size, truncated to buffer size!");
		dip_len = ((udi_if_ctrl_reg >> 8) & 0xF)*4;
	}

	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

	if(p_ctx->chipset != PCI_DEVICE_ID_VGA_SNB) {
		/* This is not needed by SNB */
		/* Select port */
		if(p_ctx->p_callback->port_num == PORT_B_NUMBER){
			udi_if_ctrl_reg &= ~BIT(30);
			udi_if_ctrl_reg |= BIT(29);
		} else if(p_ctx->p_callback->port_num == PORT_C_NUMBER){
			udi_if_ctrl_reg |= BIT(30);
			udi_if_ctrl_reg &= ~BIT(29);
		} else{
			EMGD_ERROR("Unsupported port number for internal HDMI");
			return PD_ERR_INTERNAL;
		}
	}

	/* Select buffer index for this specific DIP */
	udi_if_ctrl_reg &= ~(BIT(19)|BIT(20));
	switch(dip_type) {
	case DIP_AVI:
		break;
	case DIP_VSD:
		udi_if_ctrl_reg |= BIT(19);
		break;
	case DIP_GMP:
		udi_if_ctrl_reg |= BIT(20);
		break;
	case DIP_SPD:
		udi_if_ctrl_reg |= (BIT(19)|BIT(20));
		break;
	default:
		return PD_ERR_UNSUCCESSFUL;
	}

	/* AVI, SPD, VSD - enable DIP transmission every vsync */
	udi_if_ctrl_reg &= ~BIT(17);
	udi_if_ctrl_reg |= BIT(16);

	/* set the DIP access address */
	udi_if_ctrl_reg &= ~(BIT(0)|BIT(1)|BIT(2)|BIT(3));

	/* update buffer index, port, frequency and RAM access address */
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

	/* Write DIP data */
	count = 0;
	data_to_write = (unsigned char)dip_len;

	/*
		Write the first three bytes
		|      |ECC for header|             |             |             |
		| DW 0 |(Read Only)   |Header Byte 2|Header Byte 1|Header Byte 0|
		|      |              |             |             |             |
	*/


	udi_vidpac_data_reg = MAKE_DWORD(input[count], input[count+1],
		input[count+2], (unsigned char)0);
	/* Update the buffer */
	hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_data, udi_vidpac_data_reg);

	/* Increment the buffer pointer to start adding the payload data */
	count += 3;
	data_to_write -= 3;
	while (data_to_write > 0) {
		/* TBD: Do we need to wait for previous write completion? */
		if (data_to_write >= 4) {
			udi_vidpac_data_reg = MAKE_DWORD(input[count], input[count+1],
				input[count+2], input[count+3]);

			data_to_write -= 4;
			count +=4;
			hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_data,udi_vidpac_data_reg);
		} else if(data_to_write > 0) {
			unsigned char buffer[4];
			pd_memset(&buffer[0],0,4*sizeof(unsigned char));
			for(i=0;i<data_to_write;i++) {
				buffer[i] = input[count++];
			}
			udi_vidpac_data_reg = MAKE_DWORD(buffer[0],
				buffer[1], buffer[2], buffer[3]);
			data_to_write = 0;
			hdmi_write_mmio_reg(p_ctx, p_ctx->hdmi_audio_port_regs->video_dip_data,udi_vidpac_data_reg);
		}
	}

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	hdmi_map_audio_video_dip_regs is called to get the current
					pipe in use for a port.

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
	Remarks 	:

	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int hdmi_map_audio_video_dip_regs(hdmi_device_context_t *p_ctx)
{
	int current_pipe = p_ctx->p_callback->current_pipe;

	if(p_ctx->chipset == PCI_DEVICE_ID_VGA_SNB) {
		EMGD_DEBUG("Current pipe: %d", current_pipe);

		if (current_pipe == PIPE_A) {
			p_ctx->hdmi_audio_port_regs = &g_hdmi_audio_video_dip_regs_a;
		}
		else if (current_pipe == PIPE_B) {
			p_ctx->hdmi_audio_port_regs = &g_hdmi_audio_video_dip_regs_b;
		}
		else if (current_pipe == PIPE_C) {
			p_ctx->hdmi_audio_port_regs = &g_hdmi_audio_video_dip_regs_c;
		} else {
			EMGD_ERROR("Unsupported pipe number!");
			return PD_ERR_UNSUCCESSFUL;
		}
	} else {
		EMGD_DEBUG("Current pipe: %d", current_pipe);

		/* It is either PIPE_A or PIPE_B for VLV */
		if (current_pipe == PIPE_A) {
			p_ctx->hdmi_audio_port_regs = &g_vlv_hdmi_audio_video_dip_regs_a;
		}
		else if (current_pipe == PIPE_B) {
			p_ctx->hdmi_audio_port_regs = &g_vlv_hdmi_audio_video_dip_regs_b;
		} else {
			EMGD_ERROR("Unsupported pipe number!");
			return PD_ERR_UNSUCCESSFUL;
		}
	}

	return PD_SUCCESS;
}
