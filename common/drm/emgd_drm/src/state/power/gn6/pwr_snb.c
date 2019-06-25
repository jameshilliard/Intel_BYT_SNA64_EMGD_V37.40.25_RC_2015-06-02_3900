/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: pwr_gn6.c
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
 *  
 *-----------------------------------------------------------------------------
 */

/* copied from napa */

#include <io.h>

#include <igd_core_structs.h>

#include "igd_dispatch_pwr.h"
#include "gn6/regs.h"
#include "instr_common.h"
#include "regs_common.h"
#include "drm_emgd_private.h"

/*!
 * @addtogroup state_group
 * @{
 */

static int pwr_query_full_snb(unsigned int power_state);
static int pwr_set_snb(igd_context_t *context, unsigned int power_state);
static int pwr_init_snb(igd_context_t *context);
static void im_pwr_init_clock_gating_snb(struct drm_device *dev);

extern void gen6_init_clock_gating(struct drm_device *dev);

igd_pwr_dispatch_t pwr_dispatch_snb = {
	pwr_query_full_snb,
	pwr_set_snb,
	pwr_init_snb,
	im_pwr_init_clock_gating_snb,
};

/*!
 * This function returns "0" for all ACPI system states.
 * 
 * @param power_state the requested power state
 *
 * @return 0
 */
static int pwr_query_full_snb(unsigned int power_state)
{                                                          /* pwr_query_full */
	switch( power_state ) {

	case IGD_ADAPTERPOWERSTATE_ON:
	case IGD_ADAPTERPOWERSTATE_STANDBY:
	case IGD_ADAPTERPOWERSTATE_OFF:
	case IGD_ADAPTERPOWERSTATE_SUSPEND:
	case IGD_ADAPTERPOWERSTATE_HIBERNATE:
	default:
		return 0;
		break;
	}
}                                                          /* pwr_query_full */

/*!
 * 
 * @patam context
 * @param power_state the requested power state
 *
 * @return 0
 */
static int pwr_set_snb(igd_context_t *context, unsigned int power_state)
{
	unsigned char *mmio;

	mmio = context->device_context.virt_mmadr;

	switch (power_state) {
	case IGD_POWERSTATE_D0:
		break;
	case IGD_POWERSTATE_D1:
		break;
	case IGD_POWERSTATE_D2:
		break;
	case IGD_POWERSTATE_D3:
		break;

	default:
		break;
	}
	return 0;
}

/*!
 * 
 * @param context
 *
 * @return 0
 */
static int pwr_init_snb(igd_context_t *context)
{
	unsigned char *mmio;

	mmio = context->device_context.virt_mmadr;

	/* Reset CLKGATECTL and CLKGATECTLOVR */
	/*
	EMGD_WRITE32(0x1111111, mmio + PSB_CR_CLKGATECTL);
	EMGD_WRITE32(0, mmio + PSB_CR_CLKGATECTL + 8);
	*/

	return 0;
}

/*!
 *
 * im_pwr_init_clock_gating_snb
 *
 * Initialize all the module clock gating / chicken bit / silicon
 * workarounds for display pipeline as well as the GT pipeline
 *
 * @param context
 *
 * @return void
 */
static void im_pwr_init_clock_gating_snb(struct drm_device *dev)
{
	gen6_init_clock_gating(dev);
	return;
}


