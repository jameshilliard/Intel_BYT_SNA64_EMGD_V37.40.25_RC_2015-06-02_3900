/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: i2c_vlv.c
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

#include <io.h>
#include <memory.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <gn7/regs.h>
#include <gn7/context.h>

#include <intelpci.h>

#include "../cmn/i2c_dispatch.h"

#define USE_GMBUS_STYLE		1	/* Temporary define as 1. Will replace with 
					 * a pointer to dsp_vlv table which has a 
					 * variable named can_use_gmbus. 
					 * 1 = yes, i2c_DD layer can use GMBUS,
				 	 * 0 = no, i2c_DD layer must use GPIO */

/* DP_AUX_MAX_READ_RETRIES is ported from previous implementation.
 * This number can be reduced if it takes too long to retry */
#define DP_AUX_MAX_READ_RETRIES		50
#define DP_AUX_CTL_SEND_BUSY		BIT31
#define DP_AUX_CTL_DONE				BIT30
#define DP_AUX_CTL_TIMEOUT_ERR		BIT28
#define DP_AUX_ERR_NOT_DONE			1
#define DP_AUX_ERR_NOT_ACK			2

/*!
 * @addtogroup display_group
 * @{
 */
#if !USE_GMBUS_STYLE
static int i2c_error_recovery_vlv(
	unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time);

static int i2c_write_byte_vlv(
	unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned char value,
	unsigned long hold_time);

static int i2c_read_byte_vlv(
	unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned char *value,
	unsigned char ack,
	unsigned long hold_time);

static int i2c_start_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time);

static int i2c_stop_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time);
#endif


/*	......................................................................... */
extern igd_display_port_t dvob_port_vlv;
extern igd_display_port_t dvoc_port_vlv;

/*	......................................................................... */
static int i2c_read_regs_gmbus_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes);

static int i2c_write_reg_list_gmbus_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	pd_reg_t *reg_list,
	unsigned long flags);

#if !USE_GMBUS_STYLE
static int i2c_read_regs_gpio_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes);

static int i2c_write_reg_list_gpio_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	pd_reg_t *reg_list,
	unsigned long flags);
#endif


static int i2c_update_i2cddc_regs_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long pd_type);

i2c_dispatch_t i2c_dispatch_vlv = {
#if USE_GMBUS_STYLE
	i2c_read_regs_gmbus_vlv,
	i2c_write_reg_list_gmbus_vlv,
#else
	i2c_read_regs_gpio_vlv,
	i2c_write_reg_list_gpio_vlv,
#endif	
	i2c_update_i2cddc_regs_vlv
};

static int i2c_aux_read_regs_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes);

/* .......................................................................... */
typedef enum {
	GMBUS_SPEED_50K         = 0x0100,
	GMBUS_SPEED_100K	= 0x0000,
	GMBUS_SPEED_400K	= 0x0200,
	GMBUS_SPEED_1000K	= 0x0300,
} gmbus_speed_t;



unsigned long gmbus_pin_pair_select_bits_vlv[IGD_PARAM_GPIO_PAIR_MAXTYPES] = \
	{
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_NONE = 0*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_LVDS = 1*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_SSC = 2*/
		GMBUS_PINS_ANALOG, /*IGD_PARAM_GPIO_PAIR_ANALOG = 3*/
		GMBUS_PINS_DP_HDMI_B, /*IGD_PARAM_GPIO_PAIR_DIGITALB = 4*/
		GMBUS_PINS_DP_HDMI_C, /*IGD_PARAM_GPIO_PAIR_DIGITALC = 5*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_DIGITALD = 6*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_SDVOB_CHILD_I2C = 7*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_SDVOB_CHILD_DDC = 8*/
		GMBUS_PINS_NONE, /*IGD_PARAM_GPIO_PAIR_SDVOC_CHILD_I2C = 9*/
		GMBUS_PINS_NONE /*IGD_PARAM_GPIO_PAIR_SDVOC_CHILD_DDC = 10*/
	};

typedef enum {
	I2C_WRITE = 0,
	I2C_READ  = 1,
} i2c_bus_dir_t;

/* .......................................................................... */

/*
 * In 16-bit, the mmio is a 16-bit pointer, the watcom 1.2 compiler will have
 * error if directly convert it to unsigned long.  Normally, have to cast it to
 * unsigned short first then cast again to unsigned long; then, it will be
 * correct.  But this type of casting may cause some error in the 32 and 64 bit
 * code.  Since mmio will be equal to zero for 16-bit code.  Add the checking
 * for MICRO definition code to correct the macro by remove mmio.
 */
#define WRITE_GMCH_REG(reg, data)	WRITE_MMIO_REG_VLV(data, mmio, reg)

static int gmbus_init(unsigned char *mmio, unsigned long i2c_bus,
	unsigned long i2c_speed);

static int gmbus_read_edid(unsigned char *mmio,
	unsigned long ddc_addr,
	unsigned long slave_addr,
	unsigned long index,
	unsigned long num_bytes,
	unsigned char *buffer);

#if !USE_GMBUS_STYLE
static int gmbus_read_reg(unsigned char *mmio,
	unsigned long slave_addr,
	unsigned long index,
	unsigned char *data);
#endif

static int gmbus_write_reg(unsigned char *mmio,
	unsigned long slave_addr,
	unsigned long index,
	unsigned char data);



static unsigned long gmbus_assemble_command(unsigned long slave_addr, unsigned long index,
	unsigned long num_bytes, unsigned long flags,
	i2c_bus_dir_t i2c_dir);

static int gmbus_wait_event_one(unsigned char *mmio, unsigned long bit);
static int gmbus_wait_event_zero(unsigned char *mmio, unsigned long bit);
static int gmbus_error_handler(unsigned char *mmio);

/*!
 * gmbus_lock_nowait() is called to acquire GMBUS resource and lock it so that
 * other software component will not disrupt the GMBUS communication currently
 * in progress.  This is done by using the hardware bit, INUSE (bit15) of the
 * GMBUS2 register.
 *
 * @param mmio
 *
 * @return 0 on Fail locking GMBUS
 * @return 1 on Success locking GMBUS
 */
static int gmbus_lock_nowait(unsigned char *mmio)
{
	int				lock_status=FALSE;
	unsigned long	status = READ_MMIO_REG_VLV(mmio, GMBUS2);

	// if bit BIT_INUSE contains a ZERO indicate that GMBUS is now acquired.
	// Subsequent read of this register will now have this bit set.
	// Until a 1 is written to this bit.
	if ( ! (status & INUSE) )
	{
		lock_status = TRUE;
	}
	return lock_status;
}

/*!
 * gmbus_unlock() is called to release GMBUS resource.
 *
 * @param mmio
 *
 * @return 1 always
 */

static int gmbus_unlock(unsigned char *mmio)
{
	int				lock_status=TRUE;

	WRITE_GMCH_REG(GMBUS2, INUSE);

	return	lock_status;
}

/*!
 * i2c_update_i2cddc_regs_vlv is called by the PI module during the enumeration of
 * port driver loadings so that the device dependant codes can
 * swap I2C and / or DDC registers depending on what port driver is being attempted
 * for loading. For example, the DDC reg for SDVO port driver would be different
 * than HDMI port driver even though both PD's could potentially connect to the same
 * port structure if it detected itself (or if display detect is off?)
 *
 * @param context
 * @param port - ptr to the port structure from dsp_snb to be tried for loading
 * @param pd_type - port driver type that wants to detect itself on this port
 *
 * @return 0 on success
 * @return 1 on failure (could also mean this HW has no such port?)
 */
static int i2c_update_i2cddc_regs_vlv(
	igd_context_t *context,
	igd_display_port_t *port,
	unsigned long pd_type)
{

	/* For VLV, our device dependant dsp port structures are all unique 
	 * for all possible port drivers in terms of I2C / DDC values and
	 * so this function has nothing to replace in the port structure
	 */
	return 0;
}

/*!
 * i2c_read_regs_gmbus_vlv is called to read Edid or a single sDVO register
 * 
 * @param context
 * @param i2c_bus port->ddc_reg, port->i2c_reg
 * @param i2c_speed 50, 100, 400, 1000 (Khz)
 * @param dab 0x70/0x72 (sDVO Regs), 0xA0/0xA2 (sDVO/Analog DDC)
 * @param reg I2C Reg Index
 * @param num_bytes <= 508
 * @param buffer Data read
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_read_regs_gmbus_vlv(igd_context_t *context,
	igd_display_port_t * port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes)
{
	unsigned char *mmio = context->device_context.virt_mmadr;

	platform_context_vlv_t *platform_context;

	mutex_lock(&context->gmbus_lock);

	EMGD_DEBUG("i2c_bus=0x%lx, i2c_speed=0x%lx, dab=0x%lx, reg=0x%x", i2c_bus, i2c_speed, dab, reg);

	/* For DP, use auxiliary channel instead of GMBUS */
	if ( i2c_bus == DPBAUXCNTR || i2c_bus == DPCAUXCNTR ) {
		mutex_unlock(&context->gmbus_lock);
		return i2c_aux_read_regs_vlv(mmio, i2c_bus, dab, reg, buffer, num_bytes);
	}

	if (gmbus_lock_nowait(mmio) == FALSE)
	{
		EMGD_DEBUG("Error ! i2c_read_regs_gmbus_vlv : gmbus_lock_nowait() failed");
		mutex_unlock(&context->gmbus_lock);
		return 1;
	}

	if (! gmbus_init(mmio, i2c_bus, i2c_speed)) {
		EMGD_DEBUG("Error ! i2c_read_regs_gmbus_vlv : gmbus_init() failed");
		gmbus_unlock(mmio);
		mutex_unlock(&context->gmbus_lock);
		return 1;
	}

	platform_context = (platform_context_vlv_t *)context->platform_context;

	if (!gmbus_read_edid(mmio, dab, 0, reg, num_bytes, buffer)) {
		EMGD_DEBUG("Error ! i2c_read_regs_gmbus_vlv : gmbus_read_edid() failed");
		gmbus_unlock(mmio);
		mutex_unlock(&context->gmbus_lock);
		return 1;
	}

	gmbus_unlock(mmio);
	mutex_unlock(&context->gmbus_lock);
	return 0;
}
/*!
 * i2c_write_reg_list_gpio_vlv is called to write a list of i2c registers to sDVO 
 * device
 * 
 * @param context
 * @param i2c_bus VLV_GMBUS_DVOB_DDC/VLV_GMBUS_DVOC_DDC
 * @param i2c_speed 1000 Khz
 * @param dab 0x70/0x72
 * @param reg_list List of i2c indexes and data, terminated with register index
 *  set to PD_REG_LIST_END
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_write_reg_list_gmbus_vlv(igd_context_t *context,
	igd_display_port_t * port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	pd_reg_t *reg_list,
	unsigned long flags)
{
	unsigned char *mmio = context->device_context.virt_mmadr;
	unsigned long reg_num = 0;
	platform_context_vlv_t *platform_context;
	platform_context = (platform_context_vlv_t *)context->platform_context;

	mutex_lock(&context->gmbus_lock);

	if (gmbus_lock_nowait(mmio) == FALSE)
	{
		EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : gmbus_lock_nowait() failed");
		mutex_unlock(&context->gmbus_lock);
		return 1;
	}

	if (! gmbus_init(mmio, i2c_bus, i2c_speed)) {
		EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : gmbus_init() failed");
		gmbus_unlock(mmio);
		mutex_unlock(&context->gmbus_lock);
		return 1;
	}

	/* Send Aksv key */
	if(flags & PD_REG_WRITE_AKSV_PRI){

		unsigned long bus0 = 0, gmbus1_cmd = 0, bus7 = 0;
		
		gmbus_error_handler(mmio);	
		
		/* FIXME: For VLV, do we use the fuse value of the Aksv ?*/
		bus7 = READ_MMIO_REG_VLV(mmio, GMBUS7);
		bus7 |= BIT(8);
		WRITE_GMCH_REG(GMBUS7, bus7);

		/* Set the AKSV buffer select bit */
		bus0 = READ_MMIO_REG_VLV(mmio, GMBUS0);
		bus0 |= BIT(11);
		WRITE_GMCH_REG(GMBUS0, bus0);
		/* Aksv is 5 byte in size */
		gmbus1_cmd = gmbus_assemble_command(dab, reg_list[reg_num].reg, 5,
											STA, I2C_WRITE);
		/*gmbus1_cmd = gmbus_assemble_command(dab, reg_list[reg_num].reg, 5,
										STO | STA, I2C_WRITE);*/
		WRITE_GMCH_REG(GMBUS3, 0x0);
		WRITE_GMCH_REG(GMBUS1, gmbus1_cmd);

		if (! gmbus_wait_event_one(mmio, HW_RDY)) {
			EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : Failed to get HW_RDY");
			gmbus_unlock(mmio);
			mutex_unlock(&context->gmbus_lock);
			return 1;
		}
		if (gmbus_error_handler(mmio)) {
			EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : gmbus error");
			gmbus_unlock(mmio);
			mutex_unlock(&context->gmbus_lock);
			return 1;
		}
		/* Send the 5th byte */
		WRITE_GMCH_REG(GMBUS3, 0x0);
		if (! gmbus_wait_event_one(mmio, HW_RDY)) {
			EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : Failed to get HW_RDY");
			gmbus_unlock(mmio);
			mutex_unlock(&context->gmbus_lock);
			return 1;
		}
		if (gmbus_error_handler(mmio)) {
			EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : gmbus error");
			gmbus_unlock(mmio);
			mutex_unlock(&context->gmbus_lock);
			return 1;
		}
		/*...................................................................... */
		/* Issue a Stop Command */
		/*
		gmbus_wait_event_one(mmio, HW_WAIT);
		WRITE_GMCH_REG(GMBUS1, STO | SW_RDY | ddc_addr);
		gmbus_wait_event_one(mmio, HW_RDY);
		gmbus_wait_event_zero(mmio, GA);
		gmbus_error_handler(mmio);
		WRITE_GMCH_REG(GMBUS1, SW_RDY);
		WRITE_GMCH_REG(GMBUS1, SW_CLR_INT);
		WRITE_GMCH_REG(GMBUS1, 0);
		WRITE_GMCH_REG(GMBUS5, 0);
		WRITE_GMCH_REG(GMBUS0, 0);
		*/
		/*...................................................................... */
		gmbus_unlock(mmio);
		mutex_unlock(&context->gmbus_lock);
		return 0;
	}

	while (reg_list[reg_num].reg != PD_REG_LIST_END) {
		if (! gmbus_write_reg(mmio, dab, reg_list[reg_num].reg,
				  (unsigned char)reg_list[reg_num].value)) {

			EMGD_DEBUG("Error ! i2c_write_reg_list_gmbus_vlv : gmbus_write_reg() failed, reg_num=%lu",
				reg_num);

			gmbus_unlock(mmio);
			mutex_unlock(&context->gmbus_lock);
			return 1;
		}
		reg_num++;
	}

	gmbus_unlock(mmio);
	mutex_unlock(&context->gmbus_lock);
	return 0;
}
/*!
 * 
 * @param context
 * @param i2c_bus
 * @param i2c_speed
 * @param dab
 * @param reg
 * @param buffer
 * @param num_bytes
 *
 * @return 0 on success
 * @return 1 on failure
 */
#if !USE_GMBUS_STYLE
static int i2c_read_regs_gpio_vlv(igd_context_t *context,
	igd_display_port_t * port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes)
{
	unsigned long hold_time;
	unsigned char temp;
	unsigned char *mmio = context->device_context.virt_mmadr;
	unsigned short did = context->device_context.did;
	int i;

	if (!i2c_speed) {
		EMGD_DEBUG("i2c Speed failed.");
		return 1;
	}

	/*
	 * We are holding the clock LOW for "hold_time" and then HIGH for
	 * "hold_time". Therefore, we double the clock speed in this calculation.
	 */
	hold_time = 1000/(i2c_speed * 2);


	if (i2c_start_vlv(mmio, i2c_bus, did, hold_time)) {
		EMGD_DEBUG("i2c Start failed.");
		return 1;
	}

	if (i2c_write_byte_vlv(mmio, i2c_bus, did, (unsigned char)dab & 0xFE,
			hold_time)) {
		EMGD_DEBUG("i2c DAB(W) failed.");
		return 1;
	}

	if (i2c_write_byte_vlv(mmio, i2c_bus, did, reg, hold_time)) {
		EMGD_DEBUG("RAB failed.");
		return 1;
	}

	if (i2c_start_vlv(mmio, i2c_bus, did, hold_time)) {
		EMGD_DEBUG("i2c ReStart failed");
		return 1;
	}

	if (i2c_write_byte_vlv(mmio, i2c_bus, did, (unsigned char)dab | 0x01,
			hold_time)) {
		EMGD_DEBUG("i2c DAB(R) failed");
		return 1;
	}


	/* Read the requested number of bytes */
	for(i=0; i<(int)(num_bytes-1); i++) {
		/*
		 * Use a local temp so that the FAR pointer doesn't have to
		 * get passed down.
		 */
		if (i2c_read_byte_vlv(mmio, i2c_bus, did, &temp, 1, hold_time)) {
			EMGD_DEBUG("Read data byte %d failed", i);
			EMGD_DEBUG("Exit i2c_read_regs_gpio_vlv with error");
			return 1;
		}
		buffer[i] = temp;
	}

	/* No ACK on the last read */
	if(i2c_read_byte_vlv(mmio, i2c_bus, did, &temp, 0, hold_time)) {
		EMGD_DEBUG("Read Data %d Failed", i);
		EMGD_DEBUG("Exit i2c_read_regs_gpio_vlv with error");
		return 1;
	}
	buffer[i] = temp;

	i2c_stop_vlv(mmio, i2c_bus, did, hold_time);
	i2c_stop_vlv(mmio, i2c_bus, did, hold_time);

	return 0;
}
/*!
 * 
 * @param context
 * @param i2c_bus
 * @param i2c_speed
 * @param dab
 * @param reg
 * @param buffer
 * @param num_bytes
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_write_reg_list_gpio_vlv(igd_context_t *context,
	igd_display_port_t *port,
	unsigned long i2c_bus,
	unsigned long i2c_speed,
	unsigned long dab,
	pd_reg_t *reg_list,
	unsigned long flags)
{
	unsigned long hold_time;
	unsigned char *mmio = context->device_context.virt_mmadr;
	unsigned short did = context->device_context.did;

	if (!i2c_speed) {
		return 1;
	}

	/*
	 * We are holding the clock LOW for "hold_time" and then HIGH for
	 * "hold_time". Therefore, we double the clock speed in this calculation.
	 */
	hold_time = 1000/(i2c_speed * 2);

	while(reg_list->reg != PD_REG_LIST_END) {
		if (i2c_start_vlv(mmio, i2c_bus, did, hold_time)) {
			EMGD_DEBUG("Start failed");
			return 1;
		}

		if (i2c_write_byte_vlv(mmio, i2c_bus, did, (unsigned char)dab & 0xFE,
				hold_time)) {
			EMGD_DEBUG("DAB(W) failed");
			return 1;
		}

		/* Register Address */
		if (i2c_write_byte_vlv(mmio, i2c_bus, did,
				(unsigned char)reg_list->reg, hold_time)) {
			EMGD_DEBUG("RAB failed");
			return 1;
		}

		do {
			/*  New Value */
			if (i2c_write_byte_vlv(mmio, i2c_bus, did,
					(unsigned char)reg_list->value, hold_time)) {
				EMGD_DEBUG("Data failed");
				return 1;
			}

			if(reg_list[1].reg != (reg_list[0].reg + 1)) {
				reg_list++;
				break;
			}

			EMGD_DEBUG("I2C Multi-Write Reg[%x] = 0x%x",
				(unsigned short)reg_list->reg,
				(unsigned short)reg_list->value);
			reg_list++;
		} while(flags & IGD_I2C_SERIAL_WRITE);


		i2c_stop_vlv(mmio, i2c_bus, did, hold_time);
		i2c_stop_vlv(mmio, i2c_bus, did, hold_time);
	}

	return 0;
}

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param clock
 * @param data
 *
 * @return void
 */
static void i2c_get(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long *clock,
	unsigned long *data)
{
	unsigned long d,c;

/*	if ((i2c_bus == 0x5024) && (did == PCI_DEVICE_ID_VGA_855)) {*/
	if ((i2c_bus == 0x5024)) {
		WRITE_MMIO_REG_VLV(0, mmio, i2c_bus);
		d = READ_MMIO_REG_VLV(mmio, i2c_bus)>>4;
		c = (d>>8) & 1;
		d &= 1;
	} else {
		WRITE_MMIO_REG_VLV(0, mmio, i2c_bus);
		c = READ_MMIO_REG_VLV(mmio, i2c_bus)>>4;
		d = (c>>8) & 1;
		c &= 1;
	}
	*data = d;
	*clock = c;
}

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param data
 * @param hold_time
 *
 * @return void
 */
static void i2c_set_data(unsigned char *mmio,
	unsigned long i2c_bus,
	int data,
	unsigned long hold_time)
{
	WRITE_MMIO_REG_VLV(data ? 0x500 : 0x700, mmio, i2c_bus);
	WRITE_MMIO_REG_VLV(data ? 0x400 : 0x600, mmio, i2c_bus);
	//EMGD_SLEEP(hold_time);
}

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param clock
 * @param hold_time
 *
 * @return void
 */
static void i2c_set_clock(unsigned char *mmio,
	unsigned long i2c_bus,
	int clock,
	unsigned long hold_time)
{
	WRITE_MMIO_REG_VLV(clock ? 0x5 : 0x7, mmio, i2c_bus);
	WRITE_MMIO_REG_VLV(clock ? 0x4 : 0x6, mmio, i2c_bus);
	//EMGD_SLEEP(hold_time);
}

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param hold_time
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_start_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time)
{
	unsigned long sc, sd;

	/* set sd high */
	i2c_set_data(mmio, i2c_bus, 1, hold_time);

	/* set clock high */
	i2c_set_clock(mmio, i2c_bus, 1, hold_time);

	/* Start condition happens when sd goes high to low when sc is high */
	i2c_get(mmio, i2c_bus, did, &sc, &sd);

	if( 0 == sc ) {
		// Data must be high
		i2c_error_recovery_vlv(mmio, i2c_bus, did, hold_time);
		return 1;
	}

	i2c_set_data(mmio, i2c_bus, 0, hold_time);
	i2c_set_clock(mmio, i2c_bus, 0, hold_time);

	return 0;
} /* end i2c_start */

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param hold_time
 *
 * @return 0
 */
static int i2c_stop_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time)
{
	/* Stop condition happens when sd goes low to high when sc is high */
	unsigned long sc,sd;

	i2c_set_clock(mmio, i2c_bus, 0, hold_time);
	i2c_set_data(mmio, i2c_bus, 0, hold_time);

	i2c_set_clock(mmio, i2c_bus, 1, hold_time);

	i2c_get(mmio, i2c_bus, did, &sc, &sd);
	/* Try another time */
	if (sc == 0) {
		i2c_set_clock(mmio, i2c_bus, 1, hold_time);
	}
	i2c_set_data(mmio, i2c_bus, 1, hold_time);

	return 0;
} /* end i2c_stop */

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param hold_time
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_error_recovery_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned long hold_time)
{
	unsigned char max_retries = 9;
	unsigned long sc, sd;

	while (max_retries--) {
		i2c_get(mmio, i2c_bus, did, &sc, &sd);
		if (sd == 1 && sc == 1) {
			return 0;
		} else {
			i2c_stop_vlv(mmio, i2c_bus, did, hold_time);
		}
	}
	EMGD_ERROR("Cannot recover I2C error.");

	return 1;
}

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param value
 * @param hold_time
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_write_byte_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned char value,
	unsigned long hold_time)
{
	int i;
	unsigned long sc,sd;

	/* I2C_DEBUG("i2c_write_byte"); */
	for(i=7; i>=0; i--) {
		i2c_set_clock(mmio, i2c_bus, 0, hold_time);
		i2c_set_data(mmio, i2c_bus, value>>i & 1, hold_time);

		i2c_set_clock(mmio, i2c_bus, 1, hold_time);
	}

	/* Get ACK */
	i2c_set_clock(mmio, i2c_bus, 0, hold_time);
	i2c_set_clock(mmio, i2c_bus, 1, hold_time);
	//EMGD_SLEEP(hold_time);

	i2c_get(mmio, i2c_bus, did, &sc, &sd);

	i2c_set_clock(mmio, i2c_bus, 0, hold_time);

	if (sd != 0) {
		EMGD_DEBUG("No ACK for byte 0x%x", value);
		i2c_error_recovery_vlv(mmio, i2c_bus, did, hold_time);
		return 1;
	}

	return 0;

} /* end i2c_write_byte */

/*!
 * 
 * @param mmio
 * @param i2c_bus
 * @param did
 * @param value
 * @param ack
 * @param hold_time
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_read_byte_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long did,
	unsigned char *value,
	unsigned char ack,
	unsigned long hold_time)
{
	int i;
	unsigned long sc, sd;

	*value = 0;
	for(i=7; i>=0; i--) {
		i2c_set_clock(mmio, i2c_bus, 1, hold_time);
		i2c_get(mmio, i2c_bus, did, &sc, &sd);
		//EMGD_SLEEP(hold_time);
		if(!sc) {
			EMGD_DEBUG("Clock low on read %d", i);
			i2c_error_recovery_vlv(mmio, i2c_bus, did, hold_time);
			return 1;
		}
		*value |= (sd & 1)<<i;
		i2c_set_clock(mmio, i2c_bus, 0, hold_time);
	}

	if (ack) {
		i2c_set_data(mmio, i2c_bus, 0, hold_time);
	}

	/* Master does not ACK */
	i2c_set_clock(mmio, i2c_bus, 1, hold_time);
	i2c_set_clock(mmio, i2c_bus, 0, hold_time);

	if (ack) {
		i2c_set_data(mmio, i2c_bus, 1, hold_time);
	}

	return 0;
} /* end i2c_read_byte */
#endif

/*!
 * gmbus_init initializes the GMBUS controller with specified bus and speed
 * 
 * @param mmio
 * @param i2c_bus sDVO B/C Reg/DDC or Analog DDC
 * @param i2c_speed 50/100/400/1000 Khz
 *
 * @return TRUE(1) on success
 * @return FALSE(0) on failure
 */
static int gmbus_init(unsigned char *mmio, unsigned long i2c_bus,
	unsigned long i2c_speed)
{
	unsigned long pin_pair_select_bits;
	gmbus_speed_t bus_speed;

	
	if (i2c_bus > IGD_PARAM_GPIO_PAIR_MAXTYPES){
		EMGD_ERROR("Error#1 ! gmbus_init : Invalid i2c_bus IGD level =0x%lx", i2c_bus);
		return 0;
	} 

	pin_pair_select_bits = gmbus_pin_pair_select_bits_vlv[i2c_bus];
	if(pin_pair_select_bits == GMBUS_PINS_NONE){
		EMGD_ERROR("Error#2 ! gmbus_init : Invalid i2c_bus for this HW =0x%lx", i2c_bus);
		return 0;
	}

	switch (i2c_speed) {
		case 50 :      /* Slow speed */
			bus_speed = GMBUS_SPEED_50K;
			break;
		case 400 :     /* SPD */
			bus_speed = GMBUS_SPEED_400K;
			break;
		case 1000 :    /* sDVO Registers */
			bus_speed = GMBUS_SPEED_1000K;
			break;
		case 100 :     /* DDC */
		default :
			bus_speed = GMBUS_SPEED_100K;
			break;
	}

	WRITE_GMCH_REG(GMBUS5, 0);   /* Clear the word index reg */
	WRITE_GMCH_REG(GMBUS0, pin_pair_select_bits | bus_speed);

	return 1;
}

/*!
 * gmbus_wait_event_zero waits for specified GMBUS2 register bit to be deasserted
 * 
 * @param mmio
 * @param bit
 *
 * @return TRUE(1) on success. The bit was deasserted in the specified timeout period
 * @return FALSE(0) on failure
 */
static int gmbus_wait_event_zero(unsigned char *mmio, unsigned long bit)
{
	unsigned long i;
	unsigned long status;

	for (i = 0; i < 0x1000; i++) {

		status = READ_MMIO_REG_VLV(mmio, GMBUS2);

		if ((status & bit) == 0) {

			return 1;
		}
	}

	EMGD_DEBUG("Error ! gmbus_wait_event_zero : Failed : bit=0x%lx, status=0x%lx",
		bit, status);

	return 0;
}

/*!
 * gmbus_wait_event_one wait for specified GMBUS2 register bits to be asserted
 * 
 * @param mmio
 * @param bit
 *
 * @return TRUE(1) on success. The bit was asserted in the specified timeout period
 * @return FALSE(0) on failure
 */
static int gmbus_wait_event_one(unsigned char *mmio, unsigned long bit)
{
	unsigned long i;
	unsigned long status;

	for (i = 0; i < 0x10000; i++) {

		status = READ_MMIO_REG_VLV(mmio, GMBUS2);
		if ((status & bit) != 0) {

			return 1;
		}
	}

	EMGD_DEBUG("Error ! gmbus_wait_event_one : Failed : bit=0x%lx, status=0x%lx",
		bit, status);

	return 0;
}

/*!
 * gmbus_error_handler attempts to recover from timeout error
 * 
 * @param mmio
 *
 * @return TRUE(1) error was detected and handled
 * @return FALSE(0) there was no error
 */
static int gmbus_error_handler(unsigned char *mmio)
{
	unsigned long status = READ_MMIO_REG_VLV(mmio, GMBUS2);

	/* Clear the SW_INT, wait for HWRDY and GMBus active (GA) */
	if ((status & HW_BUS_ERR) || (status & HW_TMOUT)) {

		EMGD_DEBUG("Error ! gmbus_error_handler : Resolving error=0x%lx",
			status);

		WRITE_GMCH_REG(GMBUS1, SW_RDY);
		WRITE_GMCH_REG(GMBUS1, SW_CLR_INT);
		WRITE_GMCH_REG(GMBUS1, 0);

		gmbus_wait_event_zero(mmio, GA);

		return 1;	/* Handled the error */
	}

	return 0;	/* There was no error */
}

/*!
 * Assemble 32 bit GMBUS1 command
 * 
 * @param slave_addr 0x70/0x72
 * @param index 0 - 256
 * @param num_bytes Bytes to transfer
 * @param flags Bits 25-31 of GMBUS1
 * @param i2c_dir I2C_READ / I2C_WRITE
 *
 * @return The assembled command
 */
static unsigned long gmbus_assemble_command(unsigned long slave_addr, unsigned long index,
	unsigned long num_bytes, unsigned long flags,
	i2c_bus_dir_t i2c_dir)
{
	unsigned long cmd = flags | ENIDX | ENT | (num_bytes << 16) | (index << 8) |
						slave_addr | i2c_dir;

	return cmd;
}

/*!
 * gmbus_recv_pkt reads a block of data from specified i2c slave device
 * 
 * @param mmio
 * @param slave_addr I2C device address
 * @param index Starting i2c register index
 * @param pkt_size 1 - 508 bytes
 * @param pkt Bytes to send
 *
 * @return TRUE(1) if successful in receiving specified number of bytes
 * @return FALSE(0) on failure
 */
static int gmbus_recv_pkt(unsigned char *mmio,
	unsigned long slave_addr, unsigned long index,
	unsigned long pkt_size, void *pkt)
{
	unsigned long gmbus1_cmd;
	unsigned long bytes_rcvd;
	uint32_t *data;

	if ((pkt_size == 0) || (pkt == NULL) || (pkt_size > 508)) {

		return 0;
	}

	data = (uint32_t *)pkt;

	/* ...................................................................... */
	gmbus_error_handler(mmio);

	/* Program the command */
	gmbus1_cmd = gmbus_assemble_command(slave_addr, index, pkt_size,
										STA | SW_RDY, I2C_READ);
	WRITE_GMCH_REG(GMBUS1, gmbus1_cmd);

	/* ...................................................................... */
	bytes_rcvd = 0;
	do {

		unsigned long gmbus3_data;
		unsigned long bytes_left = pkt_size - bytes_rcvd;

		if (! gmbus_wait_event_one(mmio, HW_RDY)) {

			EMGD_DEBUG("Error ! gmbus_recv_pkt : Failed to get HW_RDY, "
				"bytes_rcvd=%ld", bytes_rcvd);
			break;
		}

		if (gmbus_error_handler(mmio)) {

			EMGD_DEBUG("Error ! gmbus_recv_pkt : gmbus error, bytes_rcvd=%ld",
				bytes_rcvd);
			break;
		}

		gmbus3_data = READ_MMIO_REG_VLV(mmio, GMBUS3);

		switch (bytes_left) {

		case 1 :
			*(unsigned char *)data = (unsigned char)gmbus3_data;
			break;

		case 2 :
			*(unsigned short *)data = (unsigned short)gmbus3_data;
			break;

		case 3 :
		{
			unsigned char *dest = (unsigned char *)data;
			 unsigned char *src  = (unsigned char *)&(gmbus3_data);
			dest[0] = src[0];
			dest[1] = src[1];
			dest[2] = src[2];

			break;
		}

		default :	/* >= 4 */
			*data = gmbus3_data;
			break;
		}

		if (bytes_left > 4) {
			bytes_rcvd += 4;
			data++;

		} else {
			bytes_rcvd += bytes_left;

		}

	} while (bytes_rcvd < pkt_size);

	/* ...................................................................... */
	if (bytes_rcvd < pkt_size) {
		return 0;

	} else {
		return 1;
	}
}


/*!
 * gmbus_read_edid reads specified number of Edid data bytes
 * 
 * @param mmio
 * @param ddc_addr 0xA0/0xA2 (DDC1/DDC2)
 * @param slave_addr 0x70/0x72 (sDVOB, sDVOC), 0 Analog
 * @param index i2c register index
 * @param num_bytes <= 508
 * @param buffer Edid data read from the display
 *
 * @return TRUE(1) if successful in reading Edid
 * @return FALSE(0) on failure
 */
static int gmbus_read_edid(unsigned char *mmio,
	unsigned long ddc_addr,
	unsigned long slave_addr,
	unsigned long index,
	unsigned long num_bytes,
	unsigned char *buffer)
{
	int status;


	/* Reset the bus */
	gmbus_recv_pkt(mmio, ddc_addr, 0, 1, buffer);


	status = gmbus_recv_pkt(mmio, ddc_addr, index, num_bytes, buffer);
	if (! status) {

		EMGD_DEBUG("Error ! gmbus_read_edid : gmbus_recv_pkt() failed");
	}

	/* ...................................................................... */
	/* Issue a Stop Command */

	gmbus_wait_event_one(mmio, HW_WAIT);
	WRITE_GMCH_REG(GMBUS1, STO | SW_RDY | ddc_addr);
	gmbus_wait_event_one(mmio, HW_RDY);

	gmbus_wait_event_zero(mmio, GA);

	gmbus_error_handler(mmio);
	WRITE_GMCH_REG(GMBUS1, SW_RDY);
	WRITE_GMCH_REG(GMBUS1, SW_CLR_INT);
	WRITE_GMCH_REG(GMBUS1, 0);
	WRITE_GMCH_REG(GMBUS5, 0);
	WRITE_GMCH_REG(GMBUS0, 0);

	/* ...................................................................... */
	return status;
}

#if !USE_GMBUS_STYLE
/*!
 * gmbus_read_reg reads one i2c register
 * 
 * @param mmio
 * @param slave_addr 0x70/0x72 (sDVOB, sDVOC)
 * @param index i2c register index
 * @param data register data
 *
 * @return TRUE(1) if successful in reading the i2c register
 * @return FALSE(0) on failure
 */
static int gmbus_read_reg(unsigned char *mmio,
	unsigned long slave_addr,
	unsigned long index,
	unsigned char *data)
{
	unsigned long gmbus1_cmd;

	WRITE_GMCH_REG(GMBUS5, 0x0);		/* Clear Word Index register */

	if (! gmbus_wait_event_zero(mmio, GA)) {

		EMGD_DEBUG("Error ! gmbus_read_reg : Failed to get GA(1)");

		return 0;
	}

	gmbus1_cmd = gmbus_assemble_command(slave_addr, index, 1,
										STO | STA, I2C_READ);
	WRITE_GMCH_REG(GMBUS1, gmbus1_cmd);

	if (! gmbus_wait_event_zero(mmio, GA)) {

		EMGD_DEBUG("Error ! gmbus_read_reg : Failed to get GA(2)");

		return 0;
	}

	*data = (unsigned char)READ_MMIO_REG_VLV(mmio, GMBUS3);

	return 1;
}
#endif

/*!
 * gmbus_write_reg writes one i2c register
 * 
 * @param mmio
 * @param slave_addr 0x70/0x72 (sDVOB, sDVOC)
 * @param index i2c register index
 * @param data register data
 *
 * @return TRUE(1) if successful in updating the i2c register
 * @return FALSE(0) if failed to update the register
 */
static int gmbus_write_reg(unsigned char *mmio,
	unsigned long slave_addr,
	unsigned long index,
	unsigned char data)
{
	unsigned long gmbus1_cmd;

	WRITE_GMCH_REG(GMBUS5, 0x0);		/* Clear Word Index register */

	if (! gmbus_wait_event_zero(mmio, GA)) {

		EMGD_DEBUG("Error ! gmbus_write_reg : Failed to get GA(1)");

		return 0;
	}

	WRITE_GMCH_REG(GMBUS3, data);

	gmbus1_cmd = gmbus_assemble_command(slave_addr, index, 1,
										STO | STA, I2C_WRITE);
	WRITE_GMCH_REG(GMBUS1, gmbus1_cmd);

	if (! gmbus_wait_event_zero(mmio, GA)) {

		EMGD_DEBUG("Error ! gmbus_write_reg : Failed to get GA(2)");
		return 0;
	}

	return 1;
}

/*!
 * i2c_aux_check_status Check DP AUX channel transmission status
 *
 * @param mmio
 * @param i2c_bus
 * @param aux_timeout
 *
 * @return 0 on success
 * @return 1 on not done failure
 * @return 2 on not acknowledge failure
 */
static int i2c_aux_check_status(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned int aux_timeout)
{
	unsigned long dp_aux_control;
	unsigned long dp_aux_data;
	os_alarm_t timeout;

	/* Check transmission status & ACK */
	timeout = OS_SET_ALARM(3000);
	do {
		udelay(aux_timeout);
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
	} while ((dp_aux_control & DP_AUX_CTL_SEND_BUSY) && (!OS_TEST_ALARM(timeout)));

	if ((dp_aux_control & DP_AUX_CTL_DONE) &&
			!(dp_aux_control & DP_AUX_CTL_TIMEOUT_ERR)) {
		dp_aux_data = READ_MMIO_REG_VLV(mmio, i2c_bus + 0x04);
		if (dp_aux_data & 0xF0000000) {
			return DP_AUX_ERR_NOT_ACK;
		}
	} else {
		return DP_AUX_ERR_NOT_DONE;
	}
	return 0;
}

/*!
 * i2c_aux_read_regs_vlv read EDID from DP AUX channel
 *
 * @param mmio
 * @param i2c_bus
 * @param dab
 * @param reg
 * @param buffer
 * @param num_bytes
 *
 * @return 0 on success
 * @return 1 on failure
 */
static int i2c_aux_read_regs_vlv(unsigned char *mmio,
	unsigned long i2c_bus,
	unsigned long dab,
	unsigned char reg,
	unsigned char *buffer,
	unsigned long num_bytes)
{
	unsigned long dp_aux_control;
	unsigned long dp_aux_data;
	unsigned long temp;
	unsigned int aux_timeout[4] = { 400, 600, 800, 1600 };
	unsigned long aux_timeout_index;
	unsigned long addr;
	unsigned long num_retries = 0;
	unsigned long failed = 0;

	int i;
	int ret;

	addr = ((dab & 0xFE) >> 1);
	dp_aux_data = READ_MMIO_REG_VLV(mmio, i2c_bus + 0x04);

	/* Check the AUX channel is still busy */
	dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
	aux_timeout_index = (dp_aux_control & 0x0C000000) >> 26;
	if (dp_aux_control & DP_AUX_CTL_SEND_BUSY) {
		udelay(aux_timeout[aux_timeout_index]);
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		if (dp_aux_control & DP_AUX_CTL_SEND_BUSY) {
			EMGD_ERROR("AUX channel stuck with busy.");
			return 1;
		}
	}

	/* The codes here are ported from previous implementation with
	 * some code optimization added. There is no document found
	 * on the steps to read EDID from DP AUX channel */
	do {
		failed = 0;
		/* I2C start for write byte */
		/* Clear AUX channel status bits */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control &= 0xFFF0F800;
		dp_aux_control |= 0x520A0064;
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Write data to register */
		WRITE_MMIO_REG_VLV((0x04 << 28) | (addr << 8), mmio, i2c_bus + 0x04);

		/* Write AUX channel bytes count & start transmit */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control &= 0xFE0FFFFF;
		dp_aux_control |= ((3 << 20) | DP_AUX_CTL_SEND_BUSY);
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Check transmission status & ACK */
		ret = i2c_aux_check_status(mmio, i2c_bus,
				aux_timeout[aux_timeout_index]);
		if (ret == DP_AUX_ERR_NOT_ACK) {
			EMGD_DEBUG("AUX i2c not return ACK for write start.");
			num_retries++;
			failed = 1;
		} else if (ret == DP_AUX_ERR_NOT_DONE) {
			EMGD_ERROR("AUX channel failed to write start.");
			return 1;
		}
	} while (failed && (num_retries < DP_AUX_MAX_READ_RETRIES));

	if (failed) {
		EMGD_ERROR("AUX i2c write start failed.");
		return 1;
	}

	num_retries = 0;
	do {
		failed = 0;
		/* I2C write register address byte */
		/* Clear AUX channel status bits */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control |= 0x52000000;
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Write data to register */
		WRITE_MMIO_REG_VLV((0x04 << 28) | (addr << 8), mmio, i2c_bus + 0x04);
		WRITE_MMIO_REG_VLV((unsigned char)reg << 24, mmio, i2c_bus + 0x08);

		/* Write AUX channel bytes count & start transmit */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control &= 0xFE0FFFFF;
		dp_aux_control |= ((5 << 20) | DP_AUX_CTL_SEND_BUSY);
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Check transmission status & ACK */
		ret = i2c_aux_check_status(mmio, i2c_bus,
				aux_timeout[aux_timeout_index]);
		if (ret == DP_AUX_ERR_NOT_ACK) {
			EMGD_DEBUG("AUX i2c not return ACK for write byte.");
			num_retries++;
			failed = 1;
		}  else if (ret == DP_AUX_ERR_NOT_DONE) {
			EMGD_ERROR("AUX channel failed to write byte.");
			return 1;
		}
	} while (failed && (num_retries < DP_AUX_MAX_READ_RETRIES));

	if (failed) {
		EMGD_ERROR("AUX i2c write byte failed.");
		return 1;
	}

	num_retries = 0;
	do {
		failed = 0;
		/* I2C re-start for read data */
		/* Clear AUX channel status bits */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control |= 0x52000000;
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Write data to register */
		WRITE_MMIO_REG_VLV((0x05 << 28) | (addr << 8), mmio, i2c_bus + 0x04);

		/* Write AUX channel bytes count & start transmit */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control &= 0xFE0FFFFF;
		dp_aux_control |= ((3 << 20) | DP_AUX_CTL_SEND_BUSY);
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Check transmission status & ACK */
		ret = i2c_aux_check_status(mmio, i2c_bus,
				aux_timeout[aux_timeout_index]);
		if (ret == DP_AUX_ERR_NOT_ACK) {
			EMGD_DEBUG("AUX i2c not return ACK for re-start.");
			num_retries++;
			failed = 1;
		} else if (ret == DP_AUX_ERR_NOT_DONE) {
			EMGD_ERROR("AUX channel failed to restart.");
			return 1;
		}
	} while (failed && (num_retries < DP_AUX_MAX_READ_RETRIES));

	if (failed) {
		EMGD_ERROR("AUX i2c re-start failed.");
		return 1;
	}

	/* I2C read bytes */
	for ( i = 0; i < (int)num_bytes; i++ ) {
		num_retries = 0;
		do {
			failed = 0;
			/* Clear AUX channel status bits */
			dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
			dp_aux_control |= 0x52000000;
			WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

			/* Write data to register */
			WRITE_MMIO_REG_VLV((0x05 << 28) | (addr << 8),
				mmio, i2c_bus + 0x04);

			/* Write AUX channel bytes count & start transmit */
			dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
			dp_aux_control &= 0xFE0FFFFF;
			dp_aux_control |= ((4 << 20) | DP_AUX_CTL_SEND_BUSY);
			WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

			/* Check transmission status & ACK */
			ret = i2c_aux_check_status(mmio, i2c_bus,
					aux_timeout[aux_timeout_index]);
			if (ret == DP_AUX_ERR_NOT_ACK) {
				EMGD_DEBUG("AUX i2c not return ACK for read byte.");
				num_retries++;
				failed = 1;
			} else if (ret == DP_AUX_ERR_NOT_DONE) {
				EMGD_ERROR("AUX channel failed to read byte.");
				return 1;
			}
		} while (failed && (num_retries < DP_AUX_MAX_READ_RETRIES));

		if (failed) {
			EMGD_ERROR("AUX i2c read byte failed.");
			return 1;
		}

		/* Write back the read data to buffer */
		temp = READ_MMIO_REG_VLV(mmio, i2c_bus + 0x04) >> 16;
		buffer[i] = (unsigned char)(temp & 0xFF);
	}

	num_retries = 0;
	do {
		failed = 0;
		/* I2C stop */
		/* Clear AUX channel status bits */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control |= 0x52000000;
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Write data to register */
		WRITE_MMIO_REG_VLV((0x01 << 28) | (addr << 8), mmio, i2c_bus + 0x04);

		/* Write AUX channel bytes count & start transmit */
		dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
		dp_aux_control &= 0xFE0FFFFF;
		dp_aux_control |= ((3 << 20) | DP_AUX_CTL_SEND_BUSY);
		WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);

		/* Check transmission status & ACK */
		ret = i2c_aux_check_status(mmio, i2c_bus,
				aux_timeout[aux_timeout_index]);
		if (ret == DP_AUX_ERR_NOT_ACK) {
			EMGD_DEBUG("AUX i2c not return ACK for write stop.");
			num_retries++;
			failed = 1;
		} else if (ret == DP_AUX_ERR_NOT_DONE) {
			EMGD_ERROR("AUX channel failed to write stop.");
			return 1;
		}
	} while (failed && (num_retries < DP_AUX_MAX_READ_RETRIES));

	if (failed) {
		EMGD_ERROR("AUX i2c write stop failed.");
		return 1;
	}

	/* Clear AUX channel status bits */
	dp_aux_control = READ_MMIO_REG_VLV(mmio, i2c_bus);
	dp_aux_control |= 0x52000000;
	WRITE_MMIO_REG_VLV(dp_aux_control, mmio, i2c_bus);
	return 0;
}

/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: i2c_vlv.c,v 1.1.2.13 2012/04/30 07:49:03 aalteres Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/display/pi/gn7/Attic/i2c_vlv.c,v $
 *----------------------------------------------------------------------------
 */
