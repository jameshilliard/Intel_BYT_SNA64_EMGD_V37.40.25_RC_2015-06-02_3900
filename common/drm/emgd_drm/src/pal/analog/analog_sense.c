/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: analog_sense.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * 
 *-----------------------------------------------------------------------------
 * Description:
 *  This file contains functions to detect the presence of CRT using
 *  Analog Sense.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.port

#include <igd_core_structs.h>
#include <pd_print.h>
#include "analog.h"

/*  ......................................................................... */
#ifndef ArraySize
#define ArraySize(p) (sizeof(p)/sizeof((p)[0]))
#endif

/*  ......................................................................... */

#if defined(CONFIG_VLV)
int analog_sense_detect_vlv(analog_context_t * pd_context);
int analog_sense_detect_cdv(analog_context_t * pd_context);
#endif
#if defined(CONFIG_SNB)
int analog_sense_detect_snb(analog_context_t * pd_context);
#endif

void analog_read_dac_reg(analog_context_t * pd_context,
			 unsigned char index, unsigned char *red,
			 unsigned char *green, unsigned char *blue);
void analog_write_dac_reg(analog_context_t * pd_context,
			  unsigned char index, unsigned char red,
			  unsigned char green, unsigned char blue);
void analog_read_vga_reg(analog_context_t * pd_context, unsigned long reg,
			 unsigned char index, unsigned char *data);
void analog_write_vga_reg(analog_context_t * pd_context, unsigned long reg,
			  unsigned char index, unsigned char data);
int analog_read_reg8(analog_context_t * pd_context, unsigned long reg,
		     unsigned char *value);
int analog_write_reg8(analog_context_t * pd_context, unsigned long reg,
		      unsigned char value);

/*  ......................................................................... */
/*  ......................................................................... */
pd_display_status_t analog_sense_detect_crt(analog_context_t * pd_context)
{
	int rc;
	unsigned short dev_id;
	pd_reg_t reg_list[2];

	/*  ..................................................................... */
	/*  Read Device ID from PCI config space of GMCH                          */
	reg_list[0].reg = 2;
	reg_list[1].reg = PD_REG_LIST_END;

	rc = pd_context->callback->read_regs(pd_context->callback->
					     callback_context, reg_list,
					     PD_REG_PCI);
	if (rc != PD_SUCCESS) {

		return PD_DISP_STATUS_UNKNOWN;
	}

	dev_id = (unsigned short) (reg_list[0].value & 0xFFFF);

	pd_context->dual_pipe = TRUE;

	/*  ..................................................................... */
	switch (dev_id) {

#if defined(CONFIG_SNB)
	case PCI_DEVICE_ID_VGA_SNB:
		rc = analog_sense_detect_snb(pd_context);
		break;
#endif
#if defined(CONFIG_VLV)
	case PCI_DEVICE_ID_VGA_IVB:
	case PCI_DEVICE_ID_VGA_VLV:
	case PCI_DEVICE_ID_VGA_VLV2:
	case PCI_DEVICE_ID_VGA_VLV3:
	case PCI_DEVICE_ID_VGA_VLV4:
		rc = analog_sense_detect_vlv(pd_context);
		break;
#endif

	default:
		EMGD_ERROR("! analog_sense_detect_crt : "
			  "Unknown Device ID = %#x", dev_id);
		return PD_DISP_STATUS_UNKNOWN;
	}

	return (rc) ? PD_DISP_STATUS_ATTACHED : PD_DISP_STATUS_DETACHED;
}

#if defined(CONFIG_VLV)
int analog_sense_detect_vlv(analog_context_t * pd_context)
{
	unsigned long reg=0, i;
	int ret = 0;
	int crt_detected = FALSE;

	/* Force CRT detection cycle */
	/* Set "Force CRT detect trigger" (bit 16) of */
	/* "Port hotplug register" (0x61110) */
	analog_read_reg(pd_context, ANALOG_PORT_REG, &reg);
	reg |= BIT(16);
	analog_write_reg(pd_context, ANALOG_PORT_REG, reg);

	for (i = 0; i < 5000; i++) {
		/* Wait for HW detection process to complete */
		/* Bit 16 of 0x61110 should get cleared */
		analog_read_reg(pd_context, ANALOG_PORT_REG, &reg);
		if ((reg & BIT(16)) == BIT(16)) {
			pd_usleep(100);
			continue;
		}

		/*
		 * Read "CRT Hot-Plug Detection status" bits
		 * If blue channel (bit 24) and green channel (bit 25) are attached
		 * it is color crt.  If only green channel is attached it is
		 * monochrome crt
		 */
		ret = analog_read_reg(pd_context, ANALOG_PORT_REG, &reg);

		if (!ret && (reg & (BIT(25) | BIT(24)))) {
			EMGD_DEBUG("analog_sense_detect_crt_hw : Color CRT detected");
			crt_detected = TRUE;

		} else if (!ret && (reg & BIT(25))) {

			analog_write_reg(pd_context, ANALOG_PORT_REG, (reg | BIT(1)));

			EMGD_DEBUG("analog_sense_detect_crt_hw : Mono CRT detected");
			crt_detected = TRUE;
		}

		break;
	}
	return crt_detected;
}
int analog_sense_detect_cdv(analog_context_t *pd_context)
{
	unsigned long reg=0, i;
	int ret;
	int crt_detected = FALSE;

	/* According to B-Specs, before we start the CRT detection, We need to
 	* clear bit 11 (CRT interrupt status) in the hot plug status register
	* (0x61114). This is set the first time the Force CRT detect trigger
	* is used after reset.
	* TOCHECK: Not sure if this applies to ALL chipsets.
	*/
	analog_read_reg(pd_context, 0x61114, &reg);

	if(reg & BIT(11)) { /* if TRUE, clear BIT#11 */

		/* You need to write a '1' to clear the bit. */
		analog_write_reg(pd_context, 0x61114, BIT(11));
	}

	/* According to B-Specs, BIT8 (0x61110) Hot Plug Enable register
	 * needs to be set 1  for Cantiga for the CRT DAC detection to
 	 * work properly.
 	 */
	analog_read_reg(pd_context, 0x61110, &reg);
	reg |= BIT(8);
	analog_write_reg(pd_context, 0x61110, reg);

	/* Force CRT detection cycle.
	* According to B-Specs,  we need to set "Force CRT detect
	* trigger" (BIT 3) of Port Hotplug register -> 0x61110
	*/
	analog_read_reg(pd_context, 0x61110, &reg);
	reg |= BIT(3);
	analog_write_reg(pd_context, 0x61110, reg);

	for(i = 0; i < 1000; i++) {

		/* Wait for HW detection process to complete
		* Bit 3 of 0x61110 should get cleared
		*/
		analog_read_reg(pd_context, 0x61110, &reg);
		if ((reg & BIT(3)) == BIT(3)) {

			pd_usleep(1000);
			continue;
		}

		/* BIT 3 is cleared. */

		/*
		 * Read "Port hotplug status register" (0x61114)
		 * If blue channel (bit 8) and green channel (bit 9) are attached
		 * it is color crt.  If only green channel is attached it is
		 * monochrome crt
		 */
		ret = analog_read_reg(pd_context, 0x61114, &reg);

		if (!ret && (reg & (BIT(9) | BIT(8)))) {

			EMGD_DEBUG("analog_sense_detect_cdv : Color CRT detected");
			crt_detected = TRUE;

		} else if (!ret && (reg & BIT(9))) {

			EMGD_DEBUG("analog_sense_detect_cdv : Mono CRT detected");
			crt_detected = TRUE;
		}

		break;
	}

	return crt_detected;
} /* end  analog_sense_detect_cdv */
#endif


#if defined(CONFIG_SNB)
int analog_sense_detect_snb(analog_context_t *pd_context)
{
	unsigned long reg=0, i, save_reg;
	int ret = 1;
	int crt_detected = FALSE;
	
	/* Force CRT detection cycle */
	/* Set "Force CRT detect trigger" (bit 16) of */
	/* "Port hotplug register" (0x61110) */
	analog_read_reg(pd_context, ANALOG_PORT_REG_SNB, &reg);
	save_reg = reg;
	reg |= BIT(16);
	if(!(reg & BIT31)){
		reg |= BIT(31);
	}
	analog_write_reg(pd_context, ANALOG_PORT_REG_SNB, reg);
	
	for (i = 0; i < 5000; i++) {
		/* Wait for HW detection process to complete */
		/* Bit 16 of 0x61110 should get cleared */
		analog_read_reg(pd_context, ANALOG_PORT_REG_SNB, &reg);
		if ((reg & BIT(16)) == BIT(16)) {
			pd_usleep(100);
			continue;
		}
		
		/*
		 * Read "CRT Hot-Plug Detection status" bits
		 * If blue channel (bit 24) and green channel (bit 25) are attached
		 * it is color crt.  If only green channel is attached it is
		 * monochrome crt
		 */
		ret = analog_read_reg(pd_context, ANALOG_PORT_REG_SNB, &reg);

		if (!ret && (reg & (BIT(25) | BIT(24)))) {
			crt_detected = TRUE;
		} else if (!ret && (reg & BIT(25))) {
			crt_detected = TRUE;
		}

		/* Dont forget to set back to original port enable stat
		 * and to clear the CRT Hoit Plug Detection Status bits */
		analog_write_reg(pd_context, ANALOG_PORT_REG_SNB, (save_reg | BIT24 | BIT25));

		break;
	}
		
	EMGD_DEBUG ("crt_detected = %d", crt_detected);
	return crt_detected;

} /* end  analog_sense_detect_snb */
#endif  /* if CONFIG_SNB */

void analog_read_dac_reg(analog_context_t * pd_context,
			 unsigned char index, unsigned char *red,
			 unsigned char *green, unsigned char *blue)
{
	analog_write_reg8(pd_context, 0x3C7, index);

	analog_read_reg8(pd_context, 0x3C9, red);
	analog_read_reg8(pd_context, 0x3C9, green);
	analog_read_reg8(pd_context, 0x3C9, blue);
}


void analog_write_dac_reg(analog_context_t * pd_context,
			  unsigned char index, unsigned char red,
			  unsigned char green, unsigned char blue)
{
	analog_write_reg8(pd_context, 0x3C8, index);

	analog_write_reg8(pd_context, 0x3C9, red);
	analog_write_reg8(pd_context, 0x3C9, green);
	analog_write_reg8(pd_context, 0x3C9, blue);
}


void analog_read_vga_reg(analog_context_t * pd_context, unsigned long reg,
			 unsigned char index, unsigned char *data)
{
	analog_write_reg8(pd_context, reg, index);
	analog_read_reg8(pd_context, reg + 1, data);
}


void analog_write_vga_reg(analog_context_t * pd_context, unsigned long reg,
			  unsigned char index, unsigned char data)
{
	analog_write_reg8(pd_context, reg, index);
	analog_write_reg8(pd_context, reg + 1, data);
}


int analog_read_reg(analog_context_t * pd_context, unsigned long reg,
		    unsigned long *value)
{
	pd_reg_t reg_list[2];
	int rc;

	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	rc = pd_context->callback->read_regs(pd_context->callback->
					     callback_context, reg_list,
					     PD_REG_MIO);
	if (rc != PD_SUCCESS) {

		EMGD_ERROR("! analog_read_reg : read_regs(%#lx) failed with error=%d", reg, rc);

	} else {

		*value = reg_list[0].value;
	}

	return rc;
}


int analog_write_reg(analog_context_t * pd_context, unsigned long reg,
		     unsigned long value)
{
	pd_reg_t reg_list[2];
	int rc;

	reg_list[0].reg = reg;
	reg_list[0].value = value;

	reg_list[1].reg = PD_REG_LIST_END;

	rc = pd_context->callback->write_regs(pd_context->callback->
					      callback_context, reg_list,
					      PD_REG_MIO);
	if (rc != PD_SUCCESS) {

		EMGD_ERROR("! analog_write_reg : write_regs(%#lx) failed with error=%d", reg, rc);
	}

	return rc;
}


int analog_read_reg8(analog_context_t * pd_context, unsigned long reg,
		     unsigned char *value)
{
	pd_reg_t reg_list[2];
	int rc;

	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	rc = pd_context->callback->read_regs(pd_context->callback->
					     callback_context, reg_list,
					     PD_REG_MIO8);
	if (rc != PD_SUCCESS) {

		EMGD_ERROR("! analog_read_reg : read_regs(%#lx) failed with error=%d", reg, rc);

	} else {

		*value = (unsigned char) reg_list[0].value;
	}

	return rc;
}


int analog_write_reg8(analog_context_t * pd_context, unsigned long reg,
		      unsigned char value)
{
	pd_reg_t reg_list[2];
	int rc;

	reg_list[0].reg = reg;
	reg_list[0].value = value;

	reg_list[1].reg = PD_REG_LIST_END;

	rc = pd_context->callback->write_regs(pd_context->callback->
					      callback_context, reg_list,
					      PD_REG_MIO8);
	if (rc != PD_SUCCESS) {

		EMGD_ERROR("! analog_write_reg : write_regs(%#lx) failed with error=%d", reg, rc);
	}

	return rc;
}


/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: analog_sense.c,v 1.1.2.14 2012/02/22 01:55:01 jsim2 Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/pal/analog/Attic/analog_sense.c,v $
 *----------------------------------------------------------------------------
 */
