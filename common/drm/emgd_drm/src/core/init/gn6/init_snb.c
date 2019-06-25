/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: init_snb.c
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
#include <memory.h>

#include <igd_core_structs.h>

#include <asm/cacheflush.h>
#include <gn6/regs.h>
#include <gn6/context.h>

#include "../cmn/init_dispatch.h"

/*!
 * @addtogroup core_group
 * @{
 */

static int snb_bus_master_enable(platform_context_snb_t *platform_context);
static int full_config_vga_snb(igd_context_t *context,
	init_dispatch_t *dispatch);
extern int get_param_snb(igd_context_t *context, unsigned long id,
	unsigned long *value);

/*!
 * 
 * @param context
 * @param dispatch
 * @param platform_context
 * 
 * @return -IGD_ERROR_NODEV on failure
 * @return 0 on success
 */
int full_query_bridge_snb(
	igd_context_t *context,
	init_dispatch_t *dispatch,
	platform_context_snb_t *platform_context)
{
	os_pci_dev_t pdev = (os_pci_dev_t)0;

	EMGD_TRACE_ENTER;

	pdev = OS_PCI_FIND_DEVICE(
			PCI_VENDOR_ID_INTEL,
			PCI_DEVICE_ID_BRIDGE_SNB,
			0xFFFF, /* Scan the whole PCI bus */
			0,
			0,
			(os_pci_dev_t)0);

	if(!pdev) {
		EMGD_ERROR_EXIT("Bridge device NOT found.");
		return -IGD_ERROR_NODEV;
	}

	EMGD_DEBUG("Bridge device found: 0x%x", PCI_DEVICE_ID_BRIDGE_SNB);
	platform_context->dramc_dev.pcidev = pdev;
	platform_context->dramc_dev.did = PCI_DEVICE_ID_BRIDGE_SNB;
	context->device_context.bid = PCI_DEVICE_ID_BRIDGE_SNB;

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * 
 * @param context
 * @param dispatch
 * @param platform_context
 * 
 * @return 0 on success
 * @return 1 on failure
 */
int full_config_snb(igd_context_t *context,
	init_dispatch_t *dispatch)
{
	platform_context_snb_t *platform_context;
	int ret;

	EMGD_TRACE_ENTER;

	platform_context = (platform_context_snb_t *)context->platform_context;

	/* Bus master enabling for Gen4 platform */
	ret = snb_bus_master_enable(platform_context);
	if(ret) {
		EMGD_ERROR("Enabling bus master");
	}

	/* Config VGA */
	ret = full_config_vga_snb(context, dispatch);
	if(ret) {
		EMGD_ERROR_EXIT("Config VGA Failed");
		return ret;
	}

	/* Set the chipset revision id */
	context->device_context.rid = platform_context->igfx_dev.rid;

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * 
 * @param platform_context
 * 
 * @return -1 on failure
 * @return 0 on success
 */
static int snb_bus_master_enable(platform_context_snb_t *platform_context){
	int ret;
	unsigned char tmp;

	EMGD_TRACE_ENTER;

	ret = OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0, 0x4, &tmp);
	if(ret) {
		EMGD_ERROR_EXIT("PCI read of bus master");
		return -1;
	}

	/* Get Bit 2, 1, and 0 and see if it is == 1
	 * all 3 bits has to be enabled. This is to enable register read/write
	 * in the case of a PCI card being added */
	if((tmp & 0x7) != 0x7 ){

		tmp |= 0x7;

		ret = OS_PCI_WRITE_CONFIG_8(platform_context->igfx_dev.pcidev0, 0x4, tmp);
		if(ret) {
			EMGD_ERROR_EXIT("Error: PCI write of bus master");
			return -1;
		}
	}

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
static int full_config_vga_snb(igd_context_t *context,
	init_dispatch_t *dispatch)
{
	uint32_t tmp32;
	unsigned long gttmmadr_size;
	unsigned char tmp = 0;
	int size  = 0;
	int ret = 0;

	platform_context_snb_t *platform_context =
		(platform_context_snb_t *)context->platform_context;

	EMGD_TRACE_ENTER;

	/* Read SNB MSAC */
	ret=OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0,
		INTEL_OFFSET_VGA_MSAC_SNB, (void*)&tmp);

	/* SNB GTTMMADR is 4MB */
	gttmmadr_size = 4*1024*1024;

	if(!ret){
		switch (tmp & 0x6) {
		case 0x2:
			size = 256*1024*1024;
			context->device_context.fb_adr &= 0x0000007ff0000000;
			break;
		case 0x6:
			size = 512*1024*1024;
			context->device_context.fb_adr &= 0x0000007fe0000000;
			break;
		case 0x0:
			size = 128*1024*1024;
			context->device_context.fb_adr &= 0x0000007ff8000000;
			break;
		default:
			EMGD_ERROR_EXIT("Invalid Aperture Size");
			return -IGD_ERROR_NODEV;
			break;
		}
	}

	EMGD_DEBUG("MSAC=0x%x, GMADR Aperture Size=%dMB, fb_adr=0x%llx",tmp, size/(1024*1024), context->device_context.fb_adr);

	/* In SNB MMIO and GTT are together
	 * MMIO base = GTTMMADR
	 * GTTADR base = GTTMMADR + 2MB
	 */
	if(OS_PCI_READ_CONFIG_32(platform_context->igfx_dev.pcidev0,
			INTEL_OFFSET_VGA_MMADR,
			(void*)&context->device_context.mmadr)) {
		EMGD_ERROR_EXIT("Reading MMADR");
		return -IGD_ERROR_NODEV;
	}
	context->device_context.mmadr &= 0x00000000ffc00000;

	OS_PCI_READ_CONFIG_32(platform_context->igfx_dev.pcidev0,
		INTEL_OFFSET_VGA_MMADR+4,(void*)&tmp32);
	tmp &= 0x0000007f;

	context->device_context.mmadr |= ((unsigned long long)tmp32 << 32);

	context->device_context.virt_mmadr =
		OS_MAP_IO_TO_MEM_NOCACHE(
			context->device_context.mmadr,
			gttmmadr_size);
	EMGD_DEBUG("MMADR mapped %luKB @ (phys):0x%llx  (virt):%p",
		gttmmadr_size/2048,
		context->device_context.mmadr,
		context->device_context.virt_mmadr);

	/* GTT is directly after MMIO Space. */
	/* In Koheo, virt_gttadr is declared as "unsigned long *",
	 * not "unsigned char *" as in IEGD. Have to becareful when
	 * access GTT with virt_gttadr pointer, each increment of the pointer
	 * is equal to 4 bytes in address
	 */
	context->device_context.gttadr =
		(context->device_context.mmadr + 2*1024*1024);
	context->device_context.virt_gttadr =
		(unsigned long *)(context->device_context.virt_mmadr + 2*1024*1024);

	/* Number of Entries in the GTT (or size of the GTT in DWORDS).
	 * One Entry per page in the Aperture. */

	/* BIOS can allocate GTT size of 2MB, but not all the GTT are used
	 * For aperture size of 512MB, GTT size used is (512M/4k)*4 = 512k
	 * For aperture size of 256MB, GTT size used is (256M/4k)*4 = 256k
	 * gatt_pages = aperture_size/4k (=GTT entries)
	 */
	context->device_context.gatt_pages = (size/4096);

	EMGD_DEBUG("GTTADR mapped %luKB @ (virt):%p",
		gttmmadr_size/2048,
		context->device_context.virt_gttadr);
	EMGD_DEBUG("GTT actual size %ldKB entries",
		context->device_context.gatt_pages/1024);

	/* PCI Interrupt Line */
	if(OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0,
			PCI_INTERRUPT_LINE,
			(void*)&platform_context->igfx_dev.irq)) {
		platform_context->igfx_dev.irq = 0;
	}

	EMGD_TRACE_EXIT;
	return 0;
}


