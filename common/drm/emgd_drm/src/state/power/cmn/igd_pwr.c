/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_pwr.c
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
 *  
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.power

#include <io.h>

#include <igd_core_structs.h>

#include <dispatch.h>
#include "igd_dispatch_pwr.h"

/*!
 * @addtogroup state_group
 * @{
 */

static dispatch_table_t pwr_dispatch_list[] = {
#ifdef CONFIG_SNB
	{PCI_DEVICE_ID_VGA_SNB, &pwr_dispatch_snb},
#endif
#ifdef CONFIG_IVB
	{PCI_DEVICE_ID_VGA_IVB, &pwr_dispatch_vlv},
#endif
#ifdef CONFIG_VLV
	{PCI_DEVICE_ID_VGA_VLV, &pwr_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV2, &pwr_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV3, &pwr_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV4, &pwr_dispatch_vlv},
#endif
	{0, NULL}
};

typedef struct _power_context {
	void *power_state;
} power_context_t;

static power_context_t power_context;

static int igd_pwr_query(igd_driver_h driver_handle, unsigned int power_state)
{                                                           /* igd_pwr_query */
	igd_context_t *context = (igd_context_t *) driver_handle;
	return context->pwr_dispatch->pwr_query(power_state);
}                                                           /* igd_pwr_query */

/*!
 * This function alters the power state of the graphics device.  This
 * function saves and restores registers as necessary and changes the
 * power states of the mode and overlay module.
 * 
 * This function will convert a request for D2 to a request for D1.
 * 
 * @param driver_handle driver context
 * @param dwPowerState power state to set the device to.
 *
 * @return < 0 on Error
 * @return 0 on Success
 */
static int igd_pwr_alter(igd_driver_h driver_handle, unsigned int dwPowerState)
{                                                           /* igd_pwr_alter */
	unsigned char *mmio;
	int           retval   = 0;
	igd_context_t *context = (igd_context_t *) driver_handle;

	EMGD_DEBUG("in igd_pwr_alter");

	EMGD_DEBUG("Power State requested: 0x%x", dwPowerState);

	if(context == NULL) {
		EMGD_ERROR("In igd_pwr_alter:-Device context is null");
		return -IGD_ERROR_INVAL;
	}

	if(power_context.power_state == NULL) {
		EMGD_DEBUG("In igd_pwr_alter:- Memory not allocated yet."
				" Power Module Not Initialised");
		return -IGD_ERROR_INVAL;
	}

	mmio = context->device_context.virt_mmadr;

	if(context->device_context.power_state == dwPowerState ) {
		EMGD_DEBUG("Already in the present state");
		return -IGD_ERROR_INVAL;
	}

	switch(dwPowerState) {
	case IGD_POWERSTATE_D0:
		/* Do any chipset specific power management */
		retval = context->pwr_dispatch->pwr_set(context, dwPowerState);

		/* restore the registers */
		retval = context->module_dispatch.reg_restore(context,
			power_context.power_state);
		if (retval) {
			return retval;
		}

		/* Officially change the power state after registers are restored */
		context->device_context.power_state = IGD_POWERSTATE_D0;

		break;

	case IGD_POWERSTATE_D1:
	case IGD_POWERSTATE_D2:
		/* Standby - ACPI S1 */

		if (IGD_POWERSTATE_D2 == dwPowerState) {
			dwPowerState = IGD_POWERSTATE_D1;
		}

		/* save registers */
		retval = context->module_dispatch.reg_save(context,
			power_context.power_state);
		if (retval) {
			return retval;
		}

		/* Change the state of the device to Dx.  This is required so the
		 * plane/pipe/port all use this.  This needs to happen after "reg_save"
		 * because igd_sync will timeout if power state is not D0
		 */
		context->device_context.power_state = dwPowerState;

		/* Do any chipset specific power management */
		retval = context->pwr_dispatch->pwr_set(context, dwPowerState);

		break;

	case IGD_POWERSTATE_D3:
		/* Suspend to memory - ACPI S3 */

		/* save registers */
		retval= context->module_dispatch.reg_save(context,
			power_context.power_state);
		if (retval) {
			return retval;
		}

		/* Change the state of the device to Dx.  This is required so the
		 * plane/pipe/port all use this.  This needs to happen after "reg_save"
		 * because igd_sync will timeout if power state is not D0
		 */
		context->device_context.power_state = dwPowerState;

		/* Do any chipset specific power management */
		retval = context->pwr_dispatch->pwr_set(context, dwPowerState);

		break;

	default:
		/* state undefined */
		EMGD_ERROR("In igd_pwr_alter:-Undefined Power State");
		break;

	}

	return retval;
}                                                           /* igd_pwr_alter */

/*!
 * This function should be called from context manager.
 * To delete the memory allocated ealier.
 * 
 * @param context
 *
 * @return void
 */
static void _pwr_shutdown(igd_context_t *context)
{

	if(power_context.power_state) {
		context->module_dispatch.reg_free(context, power_context.power_state);
	}
	power_context.power_state = NULL;
	context->device_context.power_state = IGD_POWERSTATE_UNDEFINED;
	return;
}

/*!
 * This function should only be called only once from context manager .
 * It initializes the power module.
 * 
 * @param context
 *
 * @return < 0 on Error
 * @return 0 on Success
 */
int _pwr_init(igd_context_t *context)
{                                                               /* _pwr_init */
	igd_pwr_dispatch_t **pwr_dispatch;

	EMGD_ASSERT(context, "Null Context", -IGD_ERROR_INVAL);

	pwr_dispatch   = &context->pwr_dispatch;
	/* Hook up Mode's dispatch table function pointers */
	*pwr_dispatch = (igd_pwr_dispatch_t *) dispatch_acquire(
			context, pwr_dispatch_list);
	if (!(*pwr_dispatch)) {
		EMGD_ERROR_EXIT("Unsupported Device");
		return -IGD_ERROR_NODEV;
	}

	/* Hook up the IGD dispatch table entires for power */
	context->drv_dispatch.pwr_alter = igd_pwr_alter;
	context->drv_dispatch.pwr_query = igd_pwr_query;

	/* Inter-module dispatch */
	context->module_dispatch.pwr_shutdown = _pwr_shutdown;


#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT 
	/* Initialize chipset specific default power behavior */
	context->pwr_dispatch->pwr_init(context);

	power_context.power_state = context->module_dispatch.reg_alloc(context,
			IGD_REG_SAVE_VGA |
			IGD_REG_SAVE_DAC | IGD_REG_SAVE_MMIO | IGD_REG_SAVE_RB |
			IGD_REG_SAVE_MODE | IGD_REG_SAVE_3D );

	if(!power_context.power_state) {
		return -IGD_ERROR_NOMEM;
	}
#endif
	return 0;
}                                                               /* _pwr_init */

