/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: analog.c
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
 *  This file is contains all necessary functions for Internal
 *  LVDS PORT DRIVER.
 *  This is written according to the port interface defined in pd.h.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.port

#include <linux/kernel.h>

#include <config.h>
#include <igd_pd.h>
#include <pd.h>
#include <pd_print.h>

#include "analog.h"

static pd_version_t analog_version = {9, 0, 0, 0}; /* driver version */
static unsigned long analog_dab_list[] = {0xA0, PD_DAB_LIST_END };

/* dab list */
/* EDID is read from address 0xA0 */

pd_display_status_t analog_run_digital_sense(analog_context_t *pd_context);

static pd_driver_t analog_driver = {
	PD_SDK_VERSION,
	"Analog Port Driver",
	0,
	&analog_version,
	PD_DISPLAY_CRT_INT,
	0,
	analog_dab_list,
	10,
	analog_validate,
	analog_open,
	analog_init_device,
	analog_close,
	analog_set_mode,
	NULL,
	analog_set_attrs,
	analog_get_attrs,
	analog_get_timing_list,
	analog_set_power,
	analog_get_power,
	analog_save,
	analog_restore,
	analog_get_port_status,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0
};

/*----------------------------------------------------------------------
 * Function: analog_init()
 * Description: This is the entry function into analog port driver when
 *              it first loads. This will call pd_register() to register
 *              with Display driver.
 *
 * Parameters:  void *context.
 *
 * Return:     PD_SUCCESS(0)  success
 *             PD_ERR_XXXXXX  otherwise
 *----------------------------------------------------------------------*/
int PD_MODULE_INIT(analog_init, (void *handle))
{
	/* register analog driver with display driver */
	return pd_register(handle, &analog_driver);
} /* end analog_init() */

/*----------------------------------------------------------------------
 * Function: analog_exit()
 * Description: This is the exit function for analog port driver to unload
 *              the driver.
 *
 * Parameters:  void *context.
 *
 * Return:     PD_SUCCESS(0)  success
 *             PD_ERR_XXXXXX  otherwise
 *----------------------------------------------------------------------*/

int PD_MODULE_EXIT(analog_exit, (void))
{
	return (PD_SUCCESS);
}

unsigned long analog_validate(unsigned long cookie)
{
	/* Validate magic cookie */
	return(cookie);
}

int analog_open(pd_callback_t *callback, void **context)
{
	analog_context_t *analog_context;
	pd_port_status_t port_status_tmp;
	unsigned long temp;

	if (!callback) {
		EMGD_ERROR("Received null callback.");
		return (PD_ERR_NULL_PTR);
	}

	/* Allocate any memory required for this device */
	if (!context) {
		EMGD_ERROR("Received null context, cannot allocate a context.");
		return (PD_ERR_NULL_PTR);
	}

	/* Allocate memory for analog_context */
	analog_context = (analog_context_t *)pd_malloc(sizeof(analog_context_t));
	if (!analog_context) {
		EMGD_ERROR("Unable to allocate memory for port driver context.");
		return (PD_ERR_NOMEM);
	}
	pd_memset(analog_context, 0, sizeof(analog_context_t));
	*context = 	analog_context;
	/* Save callback context in analog context */
	analog_context->callback = callback;
	analog_driver.num_devices++;

	analog_context->timing_table = NULL;
	if(!analog_read_reg(analog_context,
			ANALOG_PORT_REG,&temp)){
		/* assume the port is disabled for now */
		analog_context->powerstate = PD_POWER_MODE_D3;
	} else {
		analog_context->powerstate = temp & BIT(31)?
					PD_POWER_MODE_D0: PD_POWER_MODE_D3;
	}

	analog_context->detect_method = 0;
	analog_context->crt_status = analog_get_port_status(
				analog_context, &port_status_tmp);
	analog_context->crt_status = port_status_tmp.connected;


	/* For analog port there is nothing to detect */
	return (PD_SUCCESS);
}

int analog_init_device(void *context)
{
	/* For analog port there is nothing to detect */
	return (PD_SUCCESS);
}

int analog_close(void *context)
{
#ifndef CONFIG_MICRO
	analog_context_t *pd_context = (analog_context_t *)context;
	/* Free any memory allocated for this display */
	if (pd_context) {
		if (pd_context->timing_table) {
			pd_free(pd_context->timing_table);
		}
		pd_free(pd_context);
		analog_driver.num_devices--;
	}
#endif
	return (PD_SUCCESS);
}

int analog_set_mode(void *context, pd_timing_t *mode, unsigned long flags)
{

	/* Nothing to do for CRT port */
	return (PD_SUCCESS);
}

int analog_set_attrs(void *context, unsigned long num, pd_attr_t *list)
{
	/* No attributes to set for analog for now */
	return 0;
}

int analog_get_attrs(void *context, unsigned long *num, pd_attr_t **list)
{
	/* No attributes to get for analog for now */
	*list = NULL;
	*num  = 0;
	return 0;
}

int analog_get_timing_list(void *context, pd_timing_t *in_list,
		pd_timing_t **list)
{
	analog_context_t *pd_context = (analog_context_t *)context;
	pd_dvo_info_t analog_info = {0, 0, 0, 0, 0, 0, 0, 0};
	pd_display_info_t analog_display_info = {0, 0, 0, 0, NULL};
	int ret;

	EMGD_TRACE_ENTER;

	ret = pd_filter_timings(pd_context->callback->callback_context,
		in_list, &pd_context->timing_table, &analog_info, &analog_display_info);
	*list = pd_context->timing_table;

	EMGD_TRACE_EXIT;
	return ret;
}

int analog_set_power(void *context, uint32_t state)
{
	return (PD_SUCCESS);
}

int analog_get_power(void *context, uint32_t *state)
{
	return (PD_SUCCESS);
}

int analog_save(void *context, void **state, unsigned long flags)
{
	/* No save restore process for analog port driver */
	*state = NULL;

	return PD_SUCCESS;
}

int analog_restore(void *context, void *state, unsigned long flags)
{
	return PD_SUCCESS;
}

/*----------------------------------------------------------------------
 * Function: analog_get_port_status()
 *
 * Description:  It is called to get the information about the display
 *
 * Parameters:  context - Port driver's context
 *				port_status - Returns the display type and connection state
 *
 * Return:      PD_SUCCESS(0)  success
 *              PD_ERR_XXXXXX  otherwise
 *----------------------------------------------------------------------*/
int analog_get_port_status(void *context, pd_port_status_t *port_status)
{
	analog_context_t *pd_context = (analog_context_t *)context;

	port_status->display_type = PD_DISPLAY_CRT_INT;
	port_status->connected = PD_DISP_STATUS_UNKNOWN; /* initial value */

	/* Based on the attribute display detect method, run the appropriate
	 * method
	 *     Attribute values:
	 *        1  - DDC method only (Digital Sense method)
	 *        2  - Analog Sense only
	 *        Other value  - All (use both DDC and Analog sense as necessary)
	 */
	if (pd_context->detect_method != 2) { /* if not analog sense */
		port_status->connected = analog_run_digital_sense(pd_context);
	}

	/* Return status of Analog sense */
	if (port_status->connected != PD_DISP_STATUS_ATTACHED) {
		if (pd_context->detect_method != 1) {  /* if not digital method */
			port_status->connected = analog_sense_detect_crt(pd_context);
		}
	}

	return PD_SUCCESS;
}


pd_display_status_t analog_run_digital_sense(analog_context_t *pd_context)
{
	pd_reg_t reg[9];
	int i, attempt;
	ssize_t rc;

	/*  Read EDID header (8 bytes), try atleast twice */
	for (attempt = 0; attempt < 2; attempt++) {
		/* Read first 8 bytes of EDID header */
		for (i = 0; i < 8; i++) {
			reg[i].reg = i;
			/* 0xFD is 253 (>251) invalid data for EDID and DisplayID */
			reg[i].value = 0xFD;
		}

		reg[8].reg = PD_REG_LIST_END;

		rc = pd_context->callback->read_regs(
			pd_context->callback->callback_context,
			reg, PD_REG_DDC);
		if (rc) {
			EMGD_DEBUG("analog_run_digital_sense : read_regs() failed with rc=0x%zx", rc);
			continue;
		}
		break;
	}

	/* Simple check for valid EDID header or valid DisplayID data */
	/*
	if (rc == PD_SUCCESS) {
		if ((reg[0].value == 0x00) && (reg[1].value == 0xFF) &&
			(reg[2].value == 0xFF) && (reg[3].value == 0xFF) &&
			(reg[4].value == 0xFF) && (reg[5].value == 0xFF) &&
			(reg[6].value == 0xFF) && (reg[7].value == 0x00)) {
			return PD_DISP_STATUS_ATTACHED;
		} else if (reg[1].value <= 251) {
			return PD_DISP_STATUS_ATTACHED;
		}
	}
	*/

	/* It is good have a check to see whether the content read on I2C is
	 * actually a valid EDID header or DisplayID data. DisplayID doesn't
	 * have a header. But not having a valid header/data doesn't mean
	 * no display is connected. Also there is no simple way to check whether
	 * valid DisplayID or not. So for now, just checking the i2c read return
	 * value to determine display is detected or not */
	if (rc == 0) {
		/* If data is read successfully, return CONNECTED status */
		return PD_DISP_STATUS_ATTACHED;
	}
	/* If no data read return unknown status */
	return PD_DISP_STATUS_DETACHED;
}


/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: analog.c,v 1.1.2.10 2012/03/16 03:47:24 jsim2 Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/pal/analog/Attic/analog.c,v $
 *----------------------------------------------------------------------------
 */

