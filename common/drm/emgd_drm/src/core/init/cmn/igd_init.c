/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_init.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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
 *  This file contains the implementation of the core init module. It
 *  is responsible from initializing the HAL and the device and gathering
 *  all necessary general device information. This module is partially
 *  optional such that portions may be disabled to save code space.
 *  When CONFIG_MICRO is enabled:
 *  Firmware parameters are NOT read.
 *  Only the first device instance is queried and configured.
 *  Revision ID is NOT queried, it is assumed to be 0.
 *  MMIO regions are not discovered or mapped.
 *  FB memory is not discovered or mapped.
 *  Shutdown is a no-op.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.init

#include <io.h>
#include <pci.h>
#include <memory.h>
#include <memmap.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <reset.h>
#include <dsp.h>
#include <module_init.h>

#include "init_dispatch.h"

/* OAL header */

/*!
 * @addtogroup core_group
 * @{
 */


unsigned long _sgx_base, _msvdx_base, _topaz_base;

/* Notes: If the bus is of value 0xFFFF, then the particular
 * device is searched for in the whole PCI topology
 */
typedef struct _iegd_pci {
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned short bus;
	unsigned short dev;
	unsigned short func;
} iegd_pci_t;

static iegd_pci_t intel_pci_device_table[] = {
#ifdef CONFIG_SNB
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_SNB, 0, 2, 0},
#endif
#ifdef CONFIG_IVB
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_IVB, 0, 2, 0},
#endif
#ifdef CONFIG_VLV
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_VLV, 0, 2, 0},
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_VLV2, 0, 2, 0},
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_VLV3, 0, 2, 0},
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_VGA_VLV4, 0, 2, 0},
#endif

};


#define MAX_PCI_DEVICE_SUPPORTED \
	(sizeof(intel_pci_device_table)/sizeof(intel_pci_device_table[0]))

static dispatch_table_t init_dispatch_table[] = {
	DISPATCH_SNB(&init_dispatch_snb)
	DISPATCH_IVB(&init_dispatch_vlv)
	DISPATCH_VLV(&init_dispatch_vlv)
	DISPATCH_VLV2(&init_dispatch_vlv)
	DISPATCH_VLV3(&init_dispatch_vlv)
	DISPATCH_VLV4(&init_dispatch_vlv)
	DISPATCH_END
};

static init_dispatch_t *init_dispatch;


/*---------------------------------------------------------------------------
 * Optional Init Module Components
 *--------------------------------------------------------------------------*/
#ifndef CONFIG_MICRO

/*!
 * This function should never be called directly. It comprises the optional
 * portion of the igd_set_param function which should be used instead.
 *
 * @param context
 * @param id
 * @param value
 *
 * @return set_param()
 * @return 0
 */
static int _init_set_param(igd_context_t *context,
	unsigned long id,
	unsigned long value)
{

	switch(id) {
	case IGD_PARAM_DEBUG_MASK:
		*((unsigned long *)emgd_debug) = value;
		break;
	default:
		return init_dispatch->set_param(context, id, value);
	}

	return 0;
}

/*!
 * This function should never be called directly. It comprises the optional
 * portion of the igd_get_param function which should be used instead.
 *
 * @param display_handle
 * @param id
 * @param value
 *
 * @return set_param()
 * @return 0
 */
static int _init_get_param(igd_display_h display_handle,
	unsigned long id,
	unsigned long *value)
{
	switch(id) {
	case IGD_PARAM_DEBUG_MASK:
		*value = *((unsigned long *)emgd_debug);
		break;
	default:
		break;
	}
	return 0;
}

/*!
 * Hook up the dispatch pointers. These are only available when the
 * full init module is compiled in.
 *
 * @param context
 *
 * @return void
 */
static void _init_dispatch(igd_context_t *context)
{
	/* Hook up top level dispatch table functions owner by init */
	context->drv_dispatch.get_param           = igd_get_param;
	context->drv_dispatch.set_param           = igd_set_param;
	return;
}

/*!
 * Shutdown all modules in the required order. Optional modules must
 * only be called if they exist.
 *
 * @param context
 *
 * @return void
 */
static void shutdown_modules(igd_context_t *context)
{
	EMGD_TRACE_ENTER;

	if(context->module_dispatch.mode_shutdown) {
		context->module_dispatch.mode_shutdown(context);
	}
	if(context->module_dispatch.pwr_shutdown) {
		context->module_dispatch.pwr_shutdown(context);
	}
	if(context->module_dispatch.overlay_shutdown) {
		context->module_dispatch.overlay_shutdown(context);
	}

	/*
	 * Reg module must be last to restore the state of the device to the
	 * way it was before the driver started.
	 */
	EMGD_DEBUG("post reg_shutdown: %p", context->module_dispatch.reg_shutdown);
	if(context->module_dispatch.reg_shutdown) {
		context->module_dispatch.reg_shutdown(context);
	}

	EMGD_TRACE_EXIT;
}

#endif


/*!
 * Non-Optional Init Module Components
 *
 * @param found_device
 * @param pdev
 *
 * @return 0 on success
 * @return -IGD_ERROR_NODEV on failure
 */
static int detect_device(iegd_pci_t **found_device,
	os_pci_dev_t *pdev)
{
	int i;

	/* Scan the PCI bus for supported device */
	for(i = 0; i < MAX_PCI_DEVICE_SUPPORTED; i++) {

		*pdev = OS_PCI_FIND_DEVICE(intel_pci_device_table[i].vendor_id,
			intel_pci_device_table[i].device_id,
			intel_pci_device_table[i].bus,
			intel_pci_device_table[i].dev,
			intel_pci_device_table[i].func,
			(os_pci_dev_t)0);

		if(*pdev) {
			*found_device = &intel_pci_device_table[i];
			break;
		}
	}
	if(!*pdev) {
		EMGD_ERROR("No supported VGA devices found.");
		return -IGD_ERROR_NODEV;
	}

	EMGD_DEBUG("VGA device found: 0x%x", (*found_device)->device_id);

	return 0;
}

/* By declaring io_mapped and io_base as globals, we no longer need to
 * include context.h in the vbios common io.c */

/* Device 0:2:0 io_base */
unsigned char io_mapped;      /* True for io mapped MMIO space */
unsigned short io_base;

/*!
 * This function is directly exported.
 *
 * @param found_device
 * @param pdev
 *
 * @return igd_driver_h
 * @return NULL on failure
 */
igd_driver_h igd_driver_init( igd_init_info_t *init_info )
{
	igd_context_t *context;
	os_pci_dev_t pdev = (os_pci_dev_t)NULL;
	iegd_pci_t *found_device;
	int ret;

	EMGD_TRACE_ENTER;

	/* Allocate a context */
	context = (void *) OS_ALLOC(sizeof(igd_context_t));
	if(!context) {
		EMGD_ERROR_EXIT("igd_driver_init failed to create context");
		return NULL;
	}
	OS_MEMSET(context, 0, sizeof(igd_context_t));

	/* Search VGA devices for a supported one */
	ret = detect_device(&found_device, &pdev);
	if(ret) {
		OS_FREE(context);
		return NULL;
	}

	context->device_context.did = found_device->device_id;
	init_dispatch = (init_dispatch_t *)dispatch_acquire(context,
		init_dispatch_table);

	if(!init_dispatch) {
		EMGD_ERROR_EXIT("No dispatch found for listed device");
		OS_FREE(context);
		return NULL;
	}

	ret = init_dispatch->query(context, init_dispatch, pdev, &init_info->bus,
		&init_info->slot, &init_info->func);
	if(ret) {
		OS_FREE(context);
		EMGD_ERROR_EXIT("Device Dependent Query Failed");
		return NULL;
	}

	/* init info */
	init_info->vendor_id = found_device->vendor_id;
	init_info->device_id = found_device->device_id;
	init_info->name = init_dispatch->name;
	init_info->chipset = init_dispatch->chipset;
	init_info->default_pd_list = init_dispatch->default_pd_list;

	EMGD_TRACE_EXIT;

	return (igd_driver_h)context;
}

/*!
 * This function is directly exported.
 *
 * @param driver_handle
 *
 * @return 0 on success
 * @return 1 on failure
 */
int igd_driver_config(igd_driver_h driver_handle)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	int ret;

	EMGD_TRACE_ENTER;

	EMGD_ASSERT(context, "Null context!", -IGD_ERROR_INVAL);

	ret = init_dispatch->config(context, init_dispatch);
	if(ret) {
		EMGD_ERROR_EXIT("Device Dependent Config Failed");
		return ret;
	}


	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * Initialize modules in the required order. Optional modules must be called
 * with their initalization macro to ensure that they are not called when
 * their option is not enabled.
 *
 * @param params
 * @param context
 *
 * @return 0 on success
 * @return 1 on failure
 */
/* FIXME: All modules should get params from mod_dispatch */
static int init_modules(igd_param_t *params, igd_context_t *context)
{
	unsigned int ret;

	EMGD_TRACE_ENTER;

	/*
	 * Reg module must be first so that the state of the device can be
	 * saved before anything else is touched.
	 */
	ret = REG_INIT(context, (params->preserve_regs)?IGD_DRIVER_SAVE_RESTORE:0);
	if (ret) {
		EMGD_DEBUG("Error initializing register module");
	}

	/*
	 *  Mode is not optional. Its init function must exist.
	 */
	ret = mode_init(context);
	if (ret) {
		EMGD_ERROR_EXIT("Mode Module Init Failed");
		return ret;
	}

	ret = OVERLAY_INIT(context, params);
	if(ret) {
		EMGD_ERROR_EXIT("Overlay Module Init Failed");
		return ret;
	}

	ret = PWR_INIT(context);
	if(ret) {
		EMGD_DEBUG("Error initializing power module");
	}

	ret = RESET_INIT(context);
	if(ret) {
		EMGD_DEBUG("Error initializing reset module");
	}

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * This function is directly exported.
 *
 * @param driver_handle
 * @param dsp
 * @param params
 *
 * @return 0 on success
 * @return 1 on failure
 */
int igd_module_init(igd_driver_h driver_handle,
	igd_dispatch_t **dispatch,
	igd_param_t *params)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	device_context_t *device;
	int ret = 0;

	EMGD_TRACE_ENTER;

	device = &context->device_context;
	context->device_context.power_state = IGD_POWERSTATE_D0;
	context->module_dispatch.init_params = params;

	/* Intialize IGD Modules */
	ret = init_modules(params, context);
	if (ret) {
		EMGD_ERROR_EXIT("Init Modules Failed");
		return ret;
	}

	OPT_MICRO_VOID_CALL(_init_dispatch(context));

	*dispatch = &context->drv_dispatch;

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * This function is directly exported.
 *
 * @param driver_handle
 * @param info
 *
 * @return 0
 */
int igd_get_config_info(igd_driver_h driver_handle,
	igd_config_info_t *config_info)
{
	igd_context_t *context = (igd_context_t *)driver_handle;

	EMGD_TRACE_ENTER;

	EMGD_ASSERT(context, "Null context!", -IGD_ERROR_INVAL);
	EMGD_ASSERT(config_info, "Null config_info!", -IGD_ERROR_INVAL);

	OS_MEMSET(config_info, 0, sizeof(igd_config_info_t));

	/* Config information already obtained from driver_config() */
	config_info->mmio_base_phys = context->device_context.mmadr;
	config_info->mmio_base_virt = context->device_context.virt_mmadr;
	config_info->gtt_memory_base_phys = context->device_context.fb_adr;
	config_info->revision_id = context->device_context.rid;
	config_info->hw_status_offset = context->device_context.hw_status_offset;
	config_info->stolen_memory_base_virt = 0; /* FIXME: remove this */

	/* get the portions held in the dsp module */
	if(context->module_dispatch.dsp_get_config_info) {
		context->module_dispatch.dsp_get_config_info(context, config_info);
	}
	/* get the portions held in the pi module */
	if(context->module_dispatch.pi_get_config_info) {
		context->module_dispatch.pi_get_config_info(context, config_info);
	}

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * This function is directly exported. When compiled without the init
 * module the function calls the DD layer get_param function. When the
 * full init module is included this function calls _init_get_param to
 * first get the DI parameters then calls the DD layer.
 *
 * @param driver_handle
 * @param id
 * @param value
 *
 * @return get_param()
 */
int igd_get_param(igd_driver_h driver_handle,
	unsigned long id,
	unsigned long *value)
{
	igd_context_t *context = (igd_context_t *)driver_handle;

	EMGD_ASSERT(context, "Null Driver Handle", -IGD_ERROR_INVAL);
	EMGD_ASSERT(value, "Null Value", -IGD_ERROR_INVAL);

	OPT_MICRO_CALL(_init_get_param(driver_handle, id, value));

	return init_dispatch->get_param(context, id, value);
}

/*!
 * This function is directly exported. When compiled without the init
 * module the function does nothing and returns 0. When the full init
 * module is included this function calls _init_set_param and returns
 * the result.
 *
 * @param driver_handle
 * @param id
 * @param value
 *
 * @return 0
 */
int igd_set_param(igd_driver_h driver_handle,
	unsigned long id,
	unsigned long value)
{
	igd_context_t *context = (igd_context_t *)driver_handle;

	EMGD_ASSERT(context, "Null Driver Handle", -IGD_ERROR_INVAL);

	OPT_MICRO_CALL(_init_set_param(context, id, value));
	return 0;
}

/*!
 * This function is exported directly. It will shutdown an instance
 * of the HAL that was initialized with igd_driver_init.
 *
 * Since the symbol is exported as part of the documented API it must
 * always exist, however it becomes an empty function when the init
 * module is not fully included.
 *
 * @param driver_handle
 *
 * @return void
 */
void igd_driver_shutdown(igd_driver_h driver_handle)
{
	igd_context_t *context = (igd_context_t *)driver_handle;

	EMGD_TRACE_ENTER;

	EMGD_ASSERT(context, "Null Driver Handle", );


	/* Shutdown the device context */
	init_dispatch->shutdown(context);

	/* release the driver's context */
	if(context) {
		EMGD_DEBUG("Freeing context");
		OS_FREE(context);
	}

	EMGD_TRACE_EXIT;
	return;
}

/*!
 * This function is exported. It will shutdown most of the display
 * functions.
 *
 * @param driver_handle
 *
 * @return void
 */
void igd_driver_shutdown_hal(igd_driver_h driver_handle)
{
	igd_context_t *context = (igd_context_t *)driver_handle;

	EMGD_TRACE_ENTER;

	EMGD_ASSERT(context, "Null Driver Handle", );

	if(context->device_context.power_state != IGD_POWERSTATE_D0) {
		return;
	}

	/* Shutdown Modules */
	shutdown_modules(context);

	EMGD_TRACE_EXIT;
	return;
}

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: igd_init.c,v 1.21.16.13 2012/02/20 06:22:53 jsim2 Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/core/init/cmn/igd_init.c,v $
 *----------------------------------------------------------------------------
 */
