/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: micro_init_snb.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files \(the "Software"\), to deal
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
 * 
 *-----------------------------------------------------------------------------
 * Description:
 *  
 *-----------------------------------------------------------------------------
 */

#include <io.h>
#include <pci.h>
#include <memmap.h>

#include <igd_core_structs.h>

#include <gn6/regs.h>
#include <gn6/context.h>

#include "../cmn/init_dispatch.h"

/*!
 * @addtogroup core_group
 * @{
 */

#ifdef CONFIG_SNB

extern unsigned char io_mapped;
extern unsigned short io_base;

static platform_context_snb_t platform_context_snb;

static int query_snb(igd_context_t *context,init_dispatch_t *dispatch,
	os_pci_dev_t vga_dev, unsigned int *bus, unsigned int *slot,
	unsigned int *func);
extern int full_query_bridge_snb(
	igd_context_t *context,
	init_dispatch_t *dispatch,
	platform_context_snb_t *platform_context);
static int config_snb(igd_context_t *context,
	init_dispatch_t *dispatch);
extern int full_config_snb(igd_context_t *context,
	init_dispatch_t *dispatch);
static int set_param_snb(igd_context_t *context, unsigned long id,
	unsigned long value);
int get_param_snb(igd_context_t *context, unsigned long id,
	unsigned long *value);
static void shutdown_snb(igd_context_t *context);

/*
 * Gn4 class chips store sku configuration information in the bridge
 * device so add that to the init dispatch.
 */
init_dispatch_t init_dispatch_snb = {
	"Intel Sandybridge",
	"SNB",
	"analog",
	query_snb,
	config_snb,
	set_param_snb,
	get_param_snb,
	shutdown_snb
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
static int query_snb(
	igd_context_t *context,
	init_dispatch_t *dispatch,
	os_pci_dev_t vga_dev,
	unsigned int *bus,
	unsigned int *slot,
	unsigned int *func)
{
	platform_context_snb_t *platform_context = &platform_context_snb;
	os_pci_dev_t vga_dev_old;
	uint32_t tmp32;

	EMGD_TRACE_ENTER;
	
	/* Use to keep original PCI Device ID.  Should not be overwrite its  */
	platform_context->igfx_dev.did = context->device_context.did;

	context->platform_context = (void *)&platform_context_snb;

	/***** Query Bridge *****/
	OPT_MICRO_CALL(full_query_bridge_snb(context, dispatch,
			&platform_context_snb));

	/***** Query VGA *****/
	OS_PCI_GET_SLOT_ADDRESS(vga_dev, bus, slot, func);
	if(*func != 0) {
		/* The VGA device found is func1, so continue looking for func0. */
		vga_dev_old = vga_dev;

		vga_dev = OS_PCI_FIND_DEVICE(
					PCI_VENDOR_ID_INTEL,
					context->device_context.did,
					0xFFFF,
					0,
					0,
					vga_dev);

		if(vga_dev) {
			OS_PCI_GET_SLOT_ADDRESS(vga_dev, bus, slot, func);
			if(*func != 0) {
				EMGD_ERROR_EXIT("Error: VGA device not found");
				OS_PCI_FREE_DEVICE(vga_dev);
				return -IGD_ERROR_NODEV;
			}
		}

		OS_PCI_FREE_DEVICE(vga_dev_old);
	}
	platform_context->igfx_dev.pcidev0 = vga_dev;
	EMGD_DEBUG("VGA device found: 0x%x", context->device_context.did);

#ifndef CONFIG_MICRO
	/* Read RID */
	if(OS_PCI_READ_CONFIG_8(vga_dev, INTEL_OFFSET_VGA_RID,
				(void*) &platform_context->igfx_dev.rid)) {
		EMGD_ERROR_EXIT("Error occured reading RID");
		return -IGD_ERROR_NODEV;
	}

	EMGD_DEBUG("rid = 0x%x",
		(unsigned int)platform_context->igfx_dev.rid);
#endif /* !CONFIG_MICRO */

	/* Read GMADR */
	/* This must be in query so it is available for the vBIOS. */
	if(OS_PCI_READ_CONFIG_32(vga_dev, INTEL_OFFSET_VGA_GMADR,
			(void*)&context->device_context.fb_adr)) {
		EMGD_ERROR_EXIT("Error occured reading GMADR");
		return -IGD_ERROR_NODEV;
	}
	context->device_context.fb_adr &= 0x00000000f8000000;

	OS_PCI_READ_CONFIG_32(vga_dev,INTEL_OFFSET_VGA_GMADR+4,(void*)&tmp32);
	tmp32 &= 0x7f;

	context->device_context.fb_adr |= ((unsigned long long)tmp32 << 32);

	EMGD_DEBUG("PCI_VGA_GMADR_REG = 0x%llx", context->device_context.fb_adr);

	/* Read IO Base.
	 * This must be in query so it is available early for the vBIOS. */
	if(OS_PCI_READ_CONFIG_16(vga_dev, INTEL_OFFSET_VGA_IOBAR, &io_base)) {
		EMGD_ERROR_EXIT("Reading IO Base");
		return -IGD_ERROR_NODEV;
	}
	io_base &= 0xfffe;
	EMGD_DEBUG("io @: 0x%x", io_base);

	/* Gen4 is always io_mapped */
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
static int config_snb(igd_context_t *context,
	init_dispatch_t *dispatch)
{
/*	unsigned long coreclk;
	platform_context_snb_t *platform_context =
		(platform_context_snb_t *)context->platform_context;
*/
	EMGD_TRACE_ENTER;

	OPT_MICRO_CALL(full_config_snb(context, dispatch));

	/* SNB does not have INTEL_OFFSET_VGA_CORECLK,
	 * for safety purpose set max_dclk to the lowest
	 */
	context->device_context.max_dclk = 350000;

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
int get_param_snb(igd_context_t *context, unsigned long id,
	unsigned long *value)
{
	int ret = 0;
	unsigned char *mmio;
	uint32_t control_reg;

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
	 *    Bit 09 - DVOA/Internal LVDS
	 *    Bit 10 - DVOB/RGBA
	 *    Bit 11 - DVOC
	 *
	 * 0x71418:
	 * --------
	 *	 Bits 15-00 - Reserved Memory value in number of 4k size pages
	 */
	mmio = context->device_context.virt_mmadr;
	control_reg = EMGD_READ32(mmio + 0x71410);
	*value = 0;

	switch(id) {
#ifndef CONFIG_MICRO
	case IGD_PARAM_PANEL_ID:
		/* If the panel id bit is set in control register, read the panel id */
		/* Check for EID identifier */
		if ((control_reg>>16) != 0xE1DF) {
			EMGD_DEBUG("No Embedded vBIOS found");
			EMGD_TRACE_EXIT;
			return -IGD_ERROR_INVAL;
		}

		if (control_reg & 0x1) {
			*value = EMGD_READ32(mmio + 0x71414) & 0xFF;
			if(!(*value)) {
				/* we cannot allow for config id = 0*/
				ret = -IGD_ERROR_INVAL;
			}
		} else {
			EMGD_DEBUG("Panel ID read failed: Incorrect Operation");
			ret = -IGD_ERROR_INVAL;
		}
		break;
#endif
	case IGD_PARAM_PORT_LIST:
		/* If the port list bit is set in control register,
		 * read the port list */
		if ((control_reg>>16) != 0xE1DF) {
			EMGD_DEBUG("No Embedded vBIOS found");
			EMGD_TRACE_EXIT;
			return -IGD_ERROR_INVAL;
		}

		if (control_reg & 0x2) {
			unsigned long temp, i = 0;
			temp = (EMGD_READ32(mmio + 0x71414)>>8) & 0xFF;
			EMGD_DEBUG("Connected Port Bits: 0x%lx", temp);
			/* the meanings of bits in temp were dictated by VBIOS
			   and should not change due to backward compatibility
			   with Legacy VBIOS
			 */
			if (temp & 0x01) {
				value[i++] = 5;            /* Analog port */
			}
			if (temp & 0x02) {
			}
			if (temp & 0x04) {
				value[i++] = 2;            /* DVOB Port */
			}
			if (temp & 0x08) {
				value[i++] = 3;            /* DVOC Port */
			}
			if (temp & 0x10) {
			}
		} else {
			EMGD_DEBUG("Port List read failed: Incorrect Operation");
			ret = -IGD_ERROR_INVAL;
		}
		break;

#ifndef CONFIG_MICRO
	case IGD_PARAM_MEM_RESERVATION:
		/* If the mem reserv bit is set in control register,
		 * read the memory value */
		if ((control_reg>>16) != 0xE1DF) {
			EMGD_DEBUG("No Embedded vBIOS found");
			EMGD_TRACE_EXIT;
			return -IGD_ERROR_INVAL;
		}

		if (control_reg & 0x4) {
			*value = (EMGD_READ32(mmio + 0x71418) & 0xFFFF);
		} else {
			EMGD_DEBUG("Mem Reservation read failed: Incorrect Operation");
			ret = -IGD_ERROR_INVAL;
		}
		break;
	case IGD_PARAM_HW_CONFIG:
		/* Return HW config bits */
		*value = context->device_context.hw_config;
		break;
#endif
	default:
		ret = -IGD_ERROR_INVAL;
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
 * @return -IGD_ERROR_INVAL on failure
 * @return 0 on success
 */
static int set_param_snb(igd_context_t *context, unsigned long id,
	unsigned long value)
{

	EMGD_TRACE_ENTER;

	switch(id) {
	case IGD_PARAM_HW_CONFIG:
		context->device_context.hw_config = value;
		break;
	default:
		return -IGD_ERROR_INVAL;
		break;
	}

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * 
 * @param context
 * 
 * @return void
 */
static void shutdown_snb(igd_context_t *context)
{
	platform_context_snb_t *platform_context =
		(platform_context_snb_t *)context->platform_context;

	EMGD_TRACE_ENTER;

	/* unmap registers */
	if(context->device_context.virt_mmadr) {
		EMGD_DEBUG("Unmapping Gfx registers and GTT Table...");
		OS_UNMAP_IO_FROM_MEM((void *)context->device_context.virt_mmadr,
			DEVICE_MMIO_SIZE);
	}

	if (platform_context) {
		OS_PCI_FREE_DEVICE(platform_context->dramc_dev.pcidev);
		OS_PCI_FREE_DEVICE(platform_context->igfx_dev.pcidev0);
	}

	EMGD_TRACE_EXIT;
}

#endif
