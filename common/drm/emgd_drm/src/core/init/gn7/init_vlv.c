/*
 *-----------------------------------------------------------------------------
 * Filename: init_vlv.c
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
#include <memory.h>

#include <igd_core_structs.h>

#include <asm/cacheflush.h>
#include <gn7/regs.h>
#include <gn7/context.h>

#include "../cmn/init_dispatch.h"

#define MMADR_OFFSET	0x180000

/*!
 * @addtogroup core_group
 * @{
 */

static int bus_master_enable_vlv(platform_context_vlv_t *platform_context);
static int full_config_vga_vlv(igd_context_t *context,
	init_dispatch_t *dispatch);
extern int get_param_vlv(igd_context_t *context, unsigned long id,
	unsigned long *value);
static int freq_init_vlv(igd_context_t *context);
static int gtfifoctl_init_vlv(igd_context_t *context);


/*!
 *
 * @param context
 * @param dispatch
 *
 * @return 1 on failure
 * @return 0 on success
 */
int full_config_vlv(igd_context_t *context,
	init_dispatch_t *dispatch)
{
	platform_context_vlv_t *platform_context;
	int ret;

	EMGD_TRACE_ENTER;

	platform_context = (platform_context_vlv_t *)context->platform_context;

	/*
	 * Enable bus mastering for platforms whose BIOS did not perform this
	 * task for us.
	 */
	ret = bus_master_enable_vlv(platform_context);
	if(ret) {
		EMGD_ERROR("Error: Enabling bus master");
	}

	/* Config VGA */
	ret = full_config_vga_vlv(context, dispatch);
	if(ret) {
		EMGD_ERROR_EXIT("Config VGA Failed");
		return ret;
	}

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
/*
 */
static int bus_master_enable_vlv(platform_context_vlv_t *platform_context){
	int ret;
	unsigned char tmp;

	EMGD_TRACE_ENTER;

	ret = OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0, PCI_COMMAND_MASTER, &tmp);

	if(ret) {
		EMGD_ERROR_EXIT("PCI read of bus master");
		return -1;
	}

	/*
	 * Get Bit 2, 1, and 0 and see if it is == 1
	 * all 3 bits has to be enabled. This is to enable register read/write
	 * in the case of a PCI card being added
	 */
	if((tmp & 0x7) != 0x7 ) {

		tmp |= 0x7;
		ret = OS_PCI_WRITE_CONFIG_8(platform_context->igfx_dev.pcidev0,PCI_COMMAND_MASTER, tmp);

		if(ret) {
			EMGD_ERROR_EXIT("PCI write of bus master");
			return -1;
		}
	}

	EMGD_TRACE_EXIT;
	return 0;
}

/*!
 * Helper function to read IOSF sideband register
 * @param mmio
 * @param reg
 * @param request
 *
 * @return value from sideband register
 */
unsigned long sideband_register_read(unsigned char *mmio,
		unsigned long reg, unsigned long request)
{
	unsigned long temp, value;
	os_alarm_t timeout;

	/* Polls SB_PACKET_REG and wait for Busy bit (bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Unable to read sideband register, timeout waiting for Busy bit to be cleared!");
		return -1;
	}

	/* Write SB_ADDRESS_REG with destination offset */
	WRITE_MMIO_REG_VLV(reg, mmio, SB_ADDRESS_REG);

	/* Write SB_PACKET_REG to trigger the transaction */
	WRITE_MMIO_REG_VLV(request, mmio, SB_PACKET_REG);

	/* Polls SB_PACKET_REG and wait for Busy bit (bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Failed to read sideband register, timeout waiting for Busy bit to be cleared!");
		return -1;
	}

	/* Read SB_DATA_REG to obtain desired register data */
	value = READ_MMIO_REG_VLV(mmio, SB_DATA_REG);

	return value;

}

/*!
 * Helper function to write IOSF sideband register
 * @param mmio
 * @param reg
 * @param value
 * @param request
 *
 * @return 0 on success
 */
int sideband_register_write(unsigned char *mmio,
		unsigned long reg, unsigned long value, unsigned long request)
{
	unsigned long temp;
	os_alarm_t timeout;

	/* Polls SB_PACKET_REG and wait for Busy bit(bit 0) to be cleared */
	timeout = OS_SET_ALARM(100);
	do {
		temp = READ_MMIO_REG_VLV(mmio, SB_PACKET_REG);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	if(temp & 0x1){
		EMGD_ERROR_EXIT("Unable to write sideband register, timeout waiting for Busy bit to be cleared!");
		return -1;
	}

	/* Write SB_ADDRESS_REG with destination offset */
	WRITE_MMIO_REG_VLV(reg, mmio, SB_ADDRESS_REG);
	/* Write the value to be written to SB_DATA_REG */
	WRITE_MMIO_REG_VLV(value, mmio, SB_DATA_REG);

	/* Write SB_PACKET_REG to trigger the transaction */
	WRITE_MMIO_REG_VLV(request, mmio, SB_PACKET_REG);

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
static int full_config_vga_vlv(igd_context_t *context,
	init_dispatch_t *dispatch)
{
	int ret = 0;
	int size  = 0;
	unsigned long gttmmadr_size;
	uint32_t tmp32;
	unsigned long tmp;
	platform_context_vlv_t *platform_context =
		(platform_context_vlv_t *)context->platform_context;
	sku_ctrl_reg_t					sku_ctrl_reg;

	EMGD_TRACE_ENTER;

	/* Read VLV MSAC */
	ret = OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0,
			VLV_PCI_MSAC_REG /*same pci config as cdv*/, (void*)&tmp);
	{
			/* nothing really - we did this already in micro_init*/
			/* VLV GTTMMADR is 4MB */
			gttmmadr_size = VLV_GTT_GTTMMADR_SIZE;
			if(!ret){
				switch (tmp & 0x6) {
				case 0x0:
					size = 128*1024*1024;
					context->device_context.fb_adr &= 0x0000007FF8000000;
					break;
				case 0x2:
					size = 256*1024*1024;
					context->device_context.fb_adr &= 0x0000007FF0000000;
					break;
				case 0x6:
					size = 512*1024*1024;
					context->device_context.fb_adr &= 0x0000007FE0000000;
					break;
				default:
					EMGD_ERROR_EXIT("Invalid Aperture Size");
					return -IGD_ERROR_NODEV;
					break;
				}
		}
	}

	EMGD_DEBUG("MSAC=0x%lx, GMADR Aperture Size=%dMB, fb_adr=0x%llx",
			tmp, size/(1024*1024), context->device_context.fb_adr);

	/* BIOS can allocate GTT size of 2MB, but not all the GTT is used
	 * For aperture size of 512MB, GTT size used is (512M/4k)*4 = 512k
	 * For aperture size of 256MB, GTT size used is (256M/4k)*4 = 256k
	 * gatt_pages = aperture_size/4k (=GTT entries)
	 */
	context->device_context.gatt_pages = (size/4096);

	EMGD_DEBUG("Number of GTT entries = 0x%lx",
			context->device_context.gatt_pages);

	{
		/* In VLV MMIO and GTT are together
		 * MMIO base = GTTMMADR GTTADR base = GTTMMADR + 2MB
		 * Map the Device 2 MMIO register. These registers are similar to other GenX
		 */
		if(OS_PCI_READ_CONFIG_32(platform_context->igfx_dev.pcidev0,
				INTEL_OFFSET_VGA_GTTMMADR0, (void*)&context->device_context.mmadr)) {
			EMGD_ERROR_EXIT("Reading MMADR");
			return -IGD_ERROR_NODEV;
		}
		context->device_context.mmadr &= 0x00000000ffc00000;
		/* take note this register is now 64 bit so we need to read
		 * upper and lower 32 bits */
		if(context->device_context.mmadr & 0x4){
			EMGD_ERROR("GTTMMADR read looks like 64-bit addressing!!!!");
			if(OS_PCI_READ_CONFIG_32(platform_context->igfx_dev.pcidev0,
					INTEL_OFFSET_VGA_GTTMMADR1, (void*)&tmp32)) {
				EMGD_ERROR_EXIT("Reading MMADR");
				return -IGD_ERROR_NODEV;
			}
			tmp32 &= 0x0000007f;
			context->device_context.mmadr |= (((long long)tmp32) << 32);
		}
	}


	context->device_context.virt_mmadr =
		OS_MAP_IO_TO_MEM_NOCACHE(context->device_context.mmadr,
				gttmmadr_size);
	if (!context->device_context.virt_mmadr) {
		EMGD_ERROR_EXIT("Failed to map MMADR");
		return -IGD_ERROR_NODEV;
	}
	EMGD_DEBUG("mmadr mapped %ldKB @ (phys):0x%llx  (virt):%p",
		gttmmadr_size/1024,
		context->device_context.mmadr,
		context->device_context.virt_mmadr);

	/* GTT is directly after MMIO Space. */
	/* In koheo, virt_gttadr is declared as "unsigned long *",
	 * not "unsigned char *" as in IEGD. Have to becareful when
	 * access GTT with virt_gttadr pointer, each increment of the pointer
	 * is equal to 4 bytes in address
	 */
	{
		/* for VLV, like Gen6, the GTT_ADR is actually MM_ADR + 2MB */
		context->device_context.gttadr =
			(context->device_context.mmadr + 2*1024*1024);
		context->device_context.virt_gttadr =
			(unsigned long *)(context->device_context.virt_mmadr + 2*1024*1024);
		/* For VLV - Display Register offsets need an additional base-shift by + 0x180000*/
		//context->device_context.virt_mmadr += MMADR_OFFSET;
	}

	/* Number of Entries in the GTT (or size of the GTT in DWORDS).
	 * One Entry per page in the Aperture. */
	EMGD_DEBUG("GTTADR mapped %luKB @ (virt):%p",
		gttmmadr_size/1024,
		context->device_context.virt_gttadr);
	EMGD_DEBUG("GTT actual size %ldKB entries",
		context->device_context.gatt_pages/1024);


	/* PCI Interrupt Line */
	if(OS_PCI_READ_CONFIG_8(platform_context->igfx_dev.pcidev0,
			PCI_INTERRUPT_LINE, (void*)&platform_context->igfx_dev.irq)) {
		platform_context->igfx_dev.irq = 0;
	}

	/* Power Gate Status: This register indicates the status of the voltage islands for
	 * the various blocks that the PUNIT controls. This register is updated by the firmware
	 * after servicing request from driver to power up/down of the various islands.
	 *
	 * Each island is controlled by a 2-bit field.
	 * 00 -- no clock or power gating
	 * 01 -- clock gating
	 * 10 -- reset
	 * 11 -- power gating
	 */

	/* Read PUNIT Power Gate Status */
	tmp = sideband_register_read(context->device_context.virt_mmadr, 0x61, 0x100604f0);
	EMGD_DEBUG("PUNIT Power Gate Status (0x61) = 0x%lx", tmp);

	/* Check Subsystem 3: Display and DPIO status */
	if((tmp & 0xc0) != 0) {
		/* Init code to ungate Gunit, this is required for
		   firmware boot-loader without VBIOS/GOP */
		sideband_register_write(context->device_context.virt_mmadr, 0x60, 0xc0, 0x100704f0);
		mdelay(2);
		sideband_register_write(context->device_context.virt_mmadr, 0x60, 0x00, 0x100704f0);
		mdelay(2);
		tmp = sideband_register_read(context->device_context.virt_mmadr, 0x61, 0x100604f0);
		EMGD_DEBUG("PUNIT Power Gate Status (0x61) after ungate = 0x%lx", tmp);
	}

	/* Before we start anything on Display pipeline, lets ask the CCU to
	 * give us Spread Spectrum + Bend for the display clocks. Honestly, we
	 * should be programming this in "clocks_vlv" but since its a 1-time
	 * thing is "always required", we decided to just roll it under "init".
	 */
	tmp = sideband_register_read(context->device_context.virt_mmadr, 0x114, 0x1006a9f1);
	sideband_register_write(context->device_context.virt_mmadr, 0x114, (tmp | 0x30003), 0x1007a9f1);

	/* Assuming we're fused for RC6 support, lets first wake up render and media
	 * for HAL reg init save/restore and later GEM will properly manage RC6
	 */
	WRITE_MMIO_REG_VLV(1, context->device_context.virt_mmadr, GTLC_WAKE_CTRL);
	WRITE_MMIO_REG_VLV(_MASKED_BIT_ENABLE(0xffff), context->device_context.virt_mmadr, FORCEWAKE_REQ_RENDR_VLV);
	WRITE_MMIO_REG_VLV(_MASKED_BIT_ENABLE(0xffff), context->device_context.virt_mmadr, FORCEWAKE_REQ_MEDIA_VLV);
	mdelay(1);

	/* Obtain the SKU and VCO freq */
	sku_ctrl_reg.reg = sideband_register_read(context->device_context.virt_mmadr, 0x08, 0x100614f1);
	platform_context->igfx_dev.soc_sku = sku_ctrl_reg.soc_sku;
	platform_context->igfx_dev.vco_clk = sku_ctrl_reg.vco_clk;

	/* Program GMBUSFREQ register corresponding to SKU/VCO and CDCLK.
	 * This is a 1-time thing and is required in BIOS-less system.
	 */
	if (!freq_init_vlv(context))
	{
		EMGD_ERROR("GMBUSFREQ initialization");
	}

	/* GT FIFO Control initialization
	 * To enable SEC to access G-unit registers.
	 */
	gtfifoctl_init_vlv(context);

	EMGD_TRACE_EXIT;
	return 0;
}


/*!
 *
 * @param context
 *
 * @return void
 */
void full_shutdown_vlv(igd_context_t *context)
{
	platform_context_vlv_t *platform_context =
		(platform_context_vlv_t *)context->platform_context;

	EMGD_TRACE_ENTER;

	/* unmap registers */

	if(context->device_context.virt_mmadr) {
		EMGD_DEBUG("Unmapping Gfx registers and GTT Table...");
		OS_UNMAP_IO_FROM_MEM((void *)context->device_context.virt_mmadr,
				VLV_GTT_GTTMMADR_SIZE);
	}

	context->device_context.virt_mmadr = NULL;
	context->device_context.virt_gttadr = NULL;
	if (platform_context) {
		OS_PCI_FREE_DEVICE(platform_context->igfx_dev.pcidev0);
	}
	EMGD_TRACE_EXIT;
}

#define CDCLK_NOT_SUPPORTED	-1
#define CDCLK_INVALID		-2

static  int	cdclk[3][32] =
{
		/* SKU200 VCO 800 MHz */
		{	/*5'b00000*/	CDCLK_INVALID,
			/*5'b00001*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*5'b00010*/	/*533*/	CDCLK_NOT_SUPPORTED,
			/*5'b00011*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*5'b00100*/	320,
			/*5'b00101*/	267,
			/*5'b00110*/	CDCLK_INVALID,
			/*5'b00111*/	200,
			/*5'b01000*/	/*178*/	CDCLK_NOT_SUPPORTED,
			/*5'b01001*/	/*160*/	CDCLK_NOT_SUPPORTED,
			/*5'b01010*/	CDCLK_INVALID,
			/*5'b01011*/	/*133*/	CDCLK_NOT_SUPPORTED,
			/*5'b01100*/	CDCLK_INVALID,
			/*5'b01101*/	CDCLK_INVALID,
			/*5'b01110*/	/*107*/	CDCLK_NOT_SUPPORTED,
			/*5'b01111*/	/*100*/	CDCLK_NOT_SUPPORTED,
			/*5'b10000*/	CDCLK_INVALID,
			/*5'b10001*/	/*89*/	CDCLK_NOT_SUPPORTED,
			/*5'b10010*/	CDCLK_INVALID,
			/*5'b10011*/	/*80*/	CDCLK_NOT_SUPPORTED,
			/*5'b10100*/	CDCLK_INVALID,
			/*5'b10101*/	CDCLK_INVALID,
			/*5'b10110*/	CDCLK_INVALID,
			/*5'b10111*/	/*67*/	CDCLK_NOT_SUPPORTED,
			/*5'b11000*/	CDCLK_INVALID,
			/*5'b11001*/	CDCLK_INVALID,
			/*5'b11010*/	CDCLK_INVALID,
			/*5'b11011*/	CDCLK_INVALID,
			/*5'b11100*/	CDCLK_INVALID,
			/*5'b11101*/	/*53*/	CDCLK_NOT_SUPPORTED,
			/*5'b11110*/	CDCLK_INVALID,
			/*5'b11111*/	/*50*/	CDCLK_NOT_SUPPORTED,
		},
		/* SKU266 VCO 1.6 GHz */
		{	/*5'b00000*/	CDCLK_INVALID,
			/*5'b00001*/	/*1600*/	CDCLK_NOT_SUPPORTED,
			/*5'b00010*/	/*1067*/	CDCLK_NOT_SUPPORTED,
			/*5'b00011*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*5'b00100*/	/*640*/	CDCLK_NOT_SUPPORTED,
			/*5'b00101*/	/*533*/	CDCLK_NOT_SUPPORTED,
			/*5'b00110*/	CDCLK_INVALID,
			/*5'b00111*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*5'b01000*/	/*356*/	CDCLK_NOT_SUPPORTED,
			/*5'b01001*/	320,
			/*5'b01010*/	CDCLK_INVALID,
			/*5'b01011*/	267,
			/*5'b01100*/	CDCLK_INVALID,
			/*5'b01101*/	CDCLK_INVALID,
			/*5'b01110*/	/*213*/	CDCLK_NOT_SUPPORTED,
			/*5'b01111*/	200,
			/*5'b10000*/	CDCLK_INVALID,
			/*5'b10001*/	/*178*/	CDCLK_NOT_SUPPORTED,
			/*5'b10010*/	CDCLK_INVALID,
			/*5'b10011*/	/*160*/	CDCLK_NOT_SUPPORTED,
			/*5'b10100*/	CDCLK_INVALID,
			/*5'b10101*/	CDCLK_INVALID,
			/*5'b10110*/	CDCLK_INVALID,
			/*5'b10111*/	/*133*/	CDCLK_NOT_SUPPORTED,
			/*5'b11000*/	CDCLK_INVALID,
			/*5'b11001*/	CDCLK_INVALID,
			/*5'b11010*/	CDCLK_INVALID,
			/*5'b11011*/	CDCLK_INVALID,
			/*5'b11100*/	CDCLK_INVALID,
			/*5'b11101*/	/*107*/	CDCLK_NOT_SUPPORTED,
			/*5'b11110*/	CDCLK_INVALID,
			/*5'b11111*/	/*100*/	CDCLK_NOT_SUPPORTED,
		},
		/* SKU333 VCO 2.0 GHz */
		{	/*5'b00000*/	CDCLK_INVALID,
			/*5'b00001*/	/*2000*/	CDCLK_NOT_SUPPORTED,
			/*5'b00010*/	/*1333*/	CDCLK_NOT_SUPPORTED,
			/*5'b00011*/	/*1000*/	CDCLK_NOT_SUPPORTED,
			/*5'b00100*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*5'b00101*/	/*667*/	CDCLK_NOT_SUPPORTED,
			/*5'b00110*/	CDCLK_INVALID,
			/*5'b00111*/	/*500*/	CDCLK_NOT_SUPPORTED,
			/*5'b01000*/	/*444*/	CDCLK_NOT_SUPPORTED,
			/*5'b01001*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*5'b01010*/	CDCLK_INVALID,
			/*5'b01011*/	333,
			/*5'b01100*/	CDCLK_INVALID,
			/*5'b01101*/	CDCLK_INVALID,
			/*5'b01110*/	267,
			/*5'b01111*/	/*250*/	CDCLK_NOT_SUPPORTED,
			/*5'b10000*/	CDCLK_INVALID,
			/*5'b10001*/	/*222*/	CDCLK_NOT_SUPPORTED,
			/*5'b10010*/	CDCLK_INVALID,
			/*5'b10011*/	200,
			/*5'b10100*/	CDCLK_INVALID,
			/*5'b10101*/	CDCLK_INVALID,
			/*5'b10110*/	CDCLK_INVALID,
			/*5'b10111*/	/*167*/	CDCLK_NOT_SUPPORTED,
			/*5'b11000*/	CDCLK_INVALID,
			/*5'b11001*/	CDCLK_INVALID,
			/*5'b11010*/	CDCLK_INVALID,
			/*5'b11011*/	CDCLK_INVALID,
			/*5'b11100*/	CDCLK_INVALID,
			/*5'b11101*/	/*133*/	CDCLK_NOT_SUPPORTED,
			/*5'b11110*/	CDCLK_INVALID,
			/*5'b11111*/	/*125*/	CDCLK_NOT_SUPPORTED,
		},
};

static  int	czclk[3][32] =
{
		/* SKU200 VCO 800 MHz */
		{	/*4'b0000*/	CDCLK_INVALID,
			/*4'b0001*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*4'b0010*/	/*533*/	CDCLK_NOT_SUPPORTED,
			/*4'b0011*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*4'b0100*/	/*320*/ CDCLK_NOT_SUPPORTED,
			/*4'b0101*/	/*267*/ CDCLK_NOT_SUPPORTED,
			/*4'b0110*/	CDCLK_INVALID,
			/*4'b0111*/	200,
			/*4'b1000*/	/*178*/	CDCLK_NOT_SUPPORTED,
			/*4'b1001*/	/*160*/	CDCLK_NOT_SUPPORTED,
			/*4'b1010*/	CDCLK_INVALID,
			/*4'b1011*/	/*133*/	CDCLK_NOT_SUPPORTED,
			/*4'b1100*/	CDCLK_INVALID,
			/*4'b1101*/	CDCLK_INVALID,
			/*4'b1110*/	/*107*/	CDCLK_NOT_SUPPORTED,
			/*4'b1111*/	/*100*/	CDCLK_NOT_SUPPORTED,
		},
		/* SKU266 VCO 1.6 GHz */
		{	/*4'b0000*/	CDCLK_INVALID,
			/*4'b0001*/	/*1600*/	CDCLK_NOT_SUPPORTED,
			/*4'b0010*/	/*1067*/	CDCLK_NOT_SUPPORTED,
			/*4'b0011*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*4'b0100*/	/*640*/	CDCLK_NOT_SUPPORTED,
			/*4'b0101*/	/*533*/	CDCLK_NOT_SUPPORTED,
			/*4'b0110*/	CDCLK_INVALID,
			/*4'b0111*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*4'b1000*/	/*356*/	CDCLK_NOT_SUPPORTED,
			/*4'b1001*/	/*320*/ CDCLK_NOT_SUPPORTED,
			/*4'b1010*/	CDCLK_INVALID,
			/*4'b1011*/	267,
			/*4'b1100*/	CDCLK_INVALID,
			/*4'b1101*/	CDCLK_INVALID,
			/*4'b1110*/	/*213*/	CDCLK_NOT_SUPPORTED,
			/*4'b1111*/	200,
		},
		/* SKU333 VCO 2.0 GHz */
		{	/*4'b00000*/	CDCLK_INVALID,
			/*4'b0001*/	/*2000*/	CDCLK_NOT_SUPPORTED,
			/*4'b0010*/	/*1333*/	CDCLK_NOT_SUPPORTED,
			/*4'b0011*/	/*1000*/	CDCLK_NOT_SUPPORTED,
			/*4'b0100*/	/*800*/	CDCLK_NOT_SUPPORTED,
			/*4'b0101*/	/*667*/	CDCLK_NOT_SUPPORTED,
			/*4'b0110*/	CDCLK_INVALID,
			/*4'b0111*/	/*500*/	CDCLK_NOT_SUPPORTED,
			/*4'b1000*/	/*444*/	CDCLK_NOT_SUPPORTED,
			/*4'b1001*/	/*400*/	CDCLK_NOT_SUPPORTED,
			/*4'b1010*/	CDCLK_INVALID,
			/*4'b1011*/	333,
			/*4'b1100*/	CDCLK_INVALID,
			/*4'b1101*/	CDCLK_INVALID,
			/*4'b1110*/	/*267*/ CDCLK_NOT_SUPPORTED,
			/*4'b1111*/	/*250*/	CDCLK_NOT_SUPPORTED,
		},
};

/*!
 * freq_init_vlv initializes the GMBUS controller reference frequency
 * @param context
 *
 * @return TRUE(1) on success
 * @return FALSE(0) on failure
 */
static int freq_init_vlv(igd_context_t *context)
{
	czclk_cdclk_freq_ratio_reg_t	czclk_cdclk_freq_ratio_reg;
	int	idx = 0;
	unsigned int cd_clk = 0, cz_clk = 0;
	unsigned int gmbusfreq_value;
	platform_context_vlv_t *platform_context;
	unsigned int soc_sku;
	unsigned int vco_clk;
	unsigned char *mmio;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("emgd: freq_init_vlv ENTER\n");

	platform_context = (platform_context_vlv_t *)context->platform_context;
	soc_sku = platform_context->igfx_dev.soc_sku;
	vco_clk = platform_context->igfx_dev.vco_clk;
	mmio = context->device_context.virt_mmadr;

	EMGD_DEBUG("emgd: platform_context->igfx_dev->soc_sku = %08x\n", soc_sku);
	EMGD_DEBUG("emgd: platform_context->igfx_dev->vco_clk = %08x\n", vco_clk);

	/* Obtain the CDCLK freq encoded value */
	if ((soc_sku==SOC_200 || soc_sku==SOC_200_1) &&
			(vco_clk==VCO_CLOCK_800M))
	{
		idx = 0;		/* SKU200 VCO 800 MHz */
	}
	else if ((soc_sku==SOC_266) && (vco_clk==VCO_CLOCK_1600M))
	{
		idx = 1;		/* SKU266 VCO 1.6 GHz */
	}
	else if ((soc_sku==SOC_333) && (vco_clk==VCO_CLOCK_2000M))
	{
		idx = 2;		/* SKU333 VCO 2.0 GHz */
	}
	else
	{
		EMGD_ERROR_EXIT("Wrong SKU");
		return 0;
	}
	czclk_cdclk_freq_ratio_reg.reg = READ_MMIO_REG_VLV(mmio, CZCLK_CDCLK_FREQ_RATIO);

	EMGD_DEBUG("emgd: czclk_cdclk_freq_ratio_reg.reg = %08x\n", czclk_cdclk_freq_ratio_reg.reg);

	/* Obtain the CDCLK frequency */
	gmbusfreq_value = cd_clk = cdclk[idx][czclk_cdclk_freq_ratio_reg.cdclk];
	if (gmbusfreq_value==CDCLK_NOT_SUPPORTED || gmbusfreq_value==CDCLK_INVALID)
	{
		EMGD_ERROR_EXIT("Wrong CDCLK");
		return 0;
	}
	EMGD_DEBUG("emgd: GMBUSFREQ = %08x\n", gmbusfreq_value);

	/* Program GMBUSFREQ to obtain the 1Mz reference clock */
	/* bit[9:0] should be programmed to the number of CDCLK that generates the 1Mhz
	 * reference clock freq which is used to generate GMBus clock.
	 * Note: Bspec says 4MHz reference clock
	 */
	if (gmbusfreq_value > 0x3ff)
	{
		EMGD_ERROR_EXIT("Wrong GMBUSFREQ");
		return 0;
	}

	context->device_context.core_display_freq = (unsigned short) cd_clk; /* Core Display Freq */


	/*Absolute maximum dot clock is 90% of cd_clk. */
	context->device_context.max_dclk = (unsigned long) ((90 * cd_clk * 1000)/100) ; /* in kHz*/

	cz_clk = czclk[idx][czclk_cdclk_freq_ratio_reg.czclk];
	context->device_context.core_z_freq = (unsigned short) cz_clk; /* Core Freq */
	
	//READ_GMCH_REG(REG_GMBUSFREQ);
	WRITE_MMIO_REG_VLV(gmbusfreq_value, mmio, GMBUSFREQ);

	EMGD_DEBUG("emgd: freq_init_vlv EXIT.\n");
	EMGD_TRACE_EXIT;

	return 1;

}

/*!
 * gtfifoctl_init_vlv - GT FIFO Control initialization routine
 * To enable SEC to access G-unit registers via IOSF sideband interface.
 * This is originally done in BIOS GraphicsDxeInit().
 * For a BIOS-less bootloader, this is enabled via here.
 * @param context
 *
 * @return TRUE(1) on success
 * @return FALSE(0) on failure
 */
static int gtfifoctl_init_vlv(igd_context_t *context)
{
	unsigned int	gtfifoctl_value;
	unsigned char *mmio;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG("emgd: gtfifoctl_init_vlv ENTER\n");

	mmio = context->device_context.virt_mmadr;

    // Enable IOSFSB reads
	gtfifoctl_value = READ_MMIO_REG_VLV(mmio, GTFIFOCTL);
	EMGD_DEBUG("emgd: BEFORE gtfifoctl_value = %08x\n", gtfifoctl_value);

	gtfifoctl_value |= GT_IOSFSB_READ_POLICY;
	EMGD_DEBUG("emgd: AFTER gtfifoctl_value = %08x\n", gtfifoctl_value);

	//READ_GMCH_REG(REG_GMBUSFREQ);
	WRITE_MMIO_REG_VLV(gtfifoctl_value, mmio, GTFIFOCTL);

	EMGD_DEBUG("emgd: gtfifoctl_init_vlv EXIT.\n");
	EMGD_TRACE_EXIT;

	return 1;

}

