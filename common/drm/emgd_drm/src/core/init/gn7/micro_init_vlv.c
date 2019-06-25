/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: micro_init_vlv.c
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

#define MODULE_NAME hal.init

#include <io.h>
#include <pci.h>
#include <memmap.h>

#include <igd_core_structs.h>

#include <gn7/regs.h>
#include <gn7/context.h>

#include "../cmn/init_dispatch.h"

/*!
 * @addtogroup core_group
 * @{
 */

#ifdef CONFIG_VLV

extern unsigned char io_mapped;
extern unsigned short io_base;

extern int full_config_vlv(igd_context_t *context,
	init_dispatch_t *dispatch);
extern int get_revision_id_vlv(igd_context_t *context, os_pci_dev_t vga_dev);
extern void full_shutdown_vlv(igd_context_t *context);
static int query_vlv(igd_context_t *context,init_dispatch_t *dispatch,
	os_pci_dev_t vga_dev, unsigned int *bus, unsigned int *slot,
	unsigned int *func);
static int config_vlv(igd_context_t *context,
	init_dispatch_t *dispatch);
static int set_param_vlv(igd_context_t *context, unsigned long id,
	unsigned long value);
int get_param_vlv(igd_context_t *context, unsigned long id,
	unsigned long *value);
static void shutdown_vlv(igd_context_t *context);

extern os_pci_dev_t bridge_dev;

static platform_context_vlv_t platform_context_vlv;

init_dispatch_t init_dispatch_vlv = {
	"Intel(R) Atom Processor E3800 Product Family/ Intel(R) Celeron Processor N2920/J1900",
	"VLV_XXX",
	"hdmi",
	query_vlv,
	config_vlv,
	set_param_vlv,
	get_param_vlv,
	shutdown_vlv
};


/*!
 *
 * @param context
 * @param dispatch
 * @param vga_dev
 * @param bus
 * @param slot
 * @param func
 *
 * @return -IGD_ERROR_NODEV on failure
 * @return 0 on success
 */
static int query_vlv(
	igd_context_t *context,
	init_dispatch_t *dispatch,
	os_pci_dev_t vga_dev,
	unsigned int *bus,
	unsigned int *slot,
	unsigned int *func)
{
	platform_context_vlv_t *platform_context = &platform_context_vlv;

	uint32_t tmp;

	EMGD_TRACE_ENTER;

	/* So we don't have to pollute our function tables with multiple
	 * entries for every variant of VLV, update the device ID if one
	 * of the other SKUs is found
	 * FIXME - should we allow this, not yet knowing if
	 * we have codes that will need to differ for each SKU?
	 * --> context->device_context.did = PCI_DEVICE_ID_VGA_VLV;
	 */
	platform_context->igfx_dev.did = context->device_context.did;

	context->platform_context = (void *)&platform_context_vlv;

	/*
	 * Current specs indicate that Atom E6xx has only one PCI function.
	 * If this changes then we need to make sure we have func 0
	 * here as in previous chips.
	 */
	platform_context->igfx_dev.pcidev0 = vga_dev;

	/*
	 * finds the bus, device, func to be returned. Do this for D2:F0 only.
	 * the OS does not need to know the existence of D3:F0
	 */
	OS_PCI_GET_SLOT_ADDRESS(vga_dev, bus, slot, func);

	get_revision_id_vlv(context, vga_dev);
	platform_context->igfx_dev.rid = (unsigned char)context->device_context.rid;

	/*
	 * Lets get GMADR now.
	 * This must be in query so it is available early for the vBIOS.
	 * NOTE: VLV has GMMADR (map direct to dram- i.e. BSM)
	 */
	{

		if(OS_PCI_READ_CONFIG_32(vga_dev,
				VLV_PCI_GMADR_REG0, (void *)&context->device_context.fb_adr)) {
			EMGD_ERROR_EXIT("Reading BSM");
			return -IGD_ERROR_NODEV;
		}
		context->device_context.fb_adr &= 0xf8000000;
		if(OS_PCI_READ_CONFIG_32(vga_dev,
				VLV_PCI_GMADR_REG1, &tmp)) {
			EMGD_ERROR_EXIT("Reading BSM");
			return -IGD_ERROR_NODEV;
		}
		tmp &= 0x0000007f;
		context->device_context.fb_adr |= (((long long)tmp) << 32);
	}

	EMGD_DEBUG("BSM-or-GMADR = 0x%08llx", context->device_context.fb_adr);

	{
		/*
	 	* Read IO Base.
	 	* This must be in query so it is available early for the vBIOS.
	 	*/
		if(OS_PCI_READ_CONFIG_16(vga_dev, VLV_PCI_IOBAR, &io_base)) {
			EMGD_ERROR_EXIT("Reading IO Base");
			return -IGD_ERROR_NODEV;
		}

	}
	/* Base Address is defined in Bits 15:3*/
	io_base &= 0xfff8;
	EMGD_DEBUG("io @: 0x%x", io_base);

	/* Gen is always io_mapped */
	io_mapped = 1;

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 *
 * @param context
 * @param dispatch
 *
 * @return -IGD_ERROR_NODEV on failure
 * @return 0 on success
 */
static int config_vlv(igd_context_t *context,
	init_dispatch_t *dispatch)
{

	EMGD_TRACE_ENTER;

	OPT_MICRO_CALL(full_config_vlv(context, dispatch));

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 *
 * @param context
 * @param id
 * @param value
 *
 * @return -IGD_ERROR_INVAL on failure
 * @return 0 on success
 */
int get_param_vlv(igd_context_t *context, unsigned long id,
	unsigned long *value)
{
	int ret = 0;
	unsigned long control_reg;
	unsigned char * mmio = context->device_context.virt_mmadr;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("ID: 0x%lx", id);

	/* Scratch registers used as below:
	 *
	 * 0x71410:
	 * --------
	 *   Bits 31-16 - EID Firmware identifier 0xE1DF
	 *   Bits 15-00 - Tell what data is present.
	 * Here are bits for what we are using know:
	 *   Bit 0 - Panel id
	 *   Bit 1 - List of ports for which displays are attached
	 *   Bit 2 - Memory reservation
	 * If any of the above bits is set that mean data is followed
	 * in the next registers.
	 *
	 * 0x71414:
	 * --------
	 *   Bits 07-00 - Panel Id
	 *   Bits 11-08 - Port list
	 * Information for Port list: If any of the bit is set means,
	 *   a display is attached to that port as follows:
	 *    Bit 08 - CRT
	 *    Bit 09 - Internal LVDS
	 *    Bit 10 - SDVOB/HDMI-DVI-B/DP-B
	 *    Bit 11 - HDMI-DVI-C/DP-C
	 *
	 * 0x71418:
	 * --------
	 * Bits 15-00 - Reserved Memory value in number of 4k size pages
	 */
	*value = 0;
	control_reg = READ_MMIO_REG_VLV(mmio, 0x71410);
	*value = 0;

	switch(id) {
		case IGD_PARAM_PORT_LIST:
			/*
			 * Verify that the Embedded Firware is present.
			 */
			if ((control_reg>>16) != 0xE1DF) {
				EMGD_DEBUG("Exit No Embedded vBIOS found");
				EMGD_TRACE_EXIT;
				return -IGD_ERROR_INVAL;
			}

			/*
			 * If the port list bit is set in control register,
			 * read the port list
			 */
			if (control_reg & 0x2) {
				unsigned char temp;
				int i = 0;

				temp = (unsigned char)((READ_MMIO_REG_VLV(mmio, 0x71414)>>8) & 0xFF);
				EMGD_DEBUG("Connected Port Bits: 0x%x", temp);

				/*
				 * The meanings of bits in temp were dictated by VBIOS
				 * and should not change due to backward compatibility
				 * with Legacy VBIOS
				 */
				if (temp & 0x02) {
					/* Internal LVDS port */
					value[i++] = 4;
				}
				if (temp & 0x04) {
					/* SDVOB or HDMI-DVI-B Port */
					value[i++] = 2;
				}
				/* FIXME - do we need to differnentiate DP from HDMI?
				 * FIXME - what about MIPI and emb-DP?
				 *       - Dont have numbers assigned for these yet
				 */
			} else {
				EMGD_DEBUG("Port List read failed: Incorrect Operation");
				ret = -IGD_ERROR_INVAL;
			}
			break;

		case IGD_PARAM_PANEL_ID:
			/*
			 * Check for Embedded firmware
			 */
			if ((control_reg>>16) != 0xE1DF) {
				EMGD_DEBUG("No Embedded vBIOS found");
				EMGD_TRACE_EXIT;
				return -IGD_ERROR_INVAL;
			}

			/*
			 * The panel id bit must be set in the control register
			 * to indicate valid panel (config) ID value.
			 */
			if (control_reg & 0x1) {
				*value = READ_MMIO_REG_VLV(mmio, 0x71414) & 0xFF;
				if(!(*value)) {
					/* we cannot allow for config id = 0 */
					ret = -IGD_ERROR_INVAL;
				}
			} else {
				EMGD_DEBUG("Panel ID read failed: Incorrect Operation");
				ret = -IGD_ERROR_INVAL;
			}
			break;
		case IGD_PARAM_MEM_RESERVATION:
			/*
			 * Check for Embedded firmware
			 */
			if ((control_reg>>16) != 0xE1DF) {
				EMGD_DEBUG("No Embedded vBIOS found");
				EMGD_TRACE_EXIT;
				return -IGD_ERROR_INVAL;
			}

			/*
			 * The mem reservation bit must be set in the control register
			 * to indicate valid mem reservation value.
			 */
			if (control_reg & 0x4) {
				*value = (READ_MMIO_REG_VLV(mmio, 0x71418) & 0xFFFF);
			} else {
				EMGD_DEBUG("Mem Reservation read failed: Incorrect Operation");
				ret = -IGD_ERROR_INVAL;
			}
			break;
		default:
			ret = -IGD_ERROR_INVAL;
			break;
	}

	EMGD_TRACE_EXIT;
	return ret;
}

/*!
 *
 * @param context
 * @param id
 * @param value
 *
 * @return -IGD_ERROR_INVAL
 */
static int set_param_vlv(igd_context_t *context, unsigned long id,
	unsigned long value)
{
	EMGD_ERROR("FIXME: set_param_vlv not implemented!!");
	return 0;
}

/*!
 *
 * @param context
 *
 * @return void
 */
static void shutdown_vlv(igd_context_t *context)
{

	OPT_MICRO_VOID_CALL(full_shutdown_vlv(context));
}


/*!
 *
 * @param context
 * @param dispatch
 * @param vga_dev
 *
 * @return -IGD_ERROR_NODEV on failure
 * @return 0 on success
 */
int get_revision_id_vlv(igd_context_t *context,
	os_pci_dev_t vga_dev)
{
	platform_context_vlv_t *platform_context;

	EMGD_TRACE_ENTER;
  	 
	platform_context = (platform_context_vlv_t *)context->platform_context;

	/* Read RID */
	if(OS_PCI_READ_CONFIG_8(vga_dev, PCI_RID,
		(unsigned char *)&context->device_context.rid)) {
		EMGD_ERROR_EXIT("Error occured reading RID");
		return -IGD_ERROR_NODEV;
	}
  	 
	EMGD_DEBUG(" rid = 0x%lx", context->device_context.rid);
	
	EMGD_TRACE_EXIT;
	return 0;
}

#endif
