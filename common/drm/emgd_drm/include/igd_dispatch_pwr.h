/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_dispatch_pwr.h
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
 *  This header file defines the interfaces that can be called directly by
 *  the upper KMS layer of the emgd driver - this contains both the DI
 *  (Device Independent) and DD (Device Dependent) layer of the power module
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPATCH_PWR_H
#define _IGD_DISPATCH_PWR_H

#include <igd_pwr.h>


typedef struct _igd_pwr_dispatch {
	/*
	 * ------------------------------------------------------------
	 *       DEVICE-INDEPENDENT PWR EXPORTED FUNCTIONS --->
	 *       PREFIX = pwr_ SUFFIX = NONE
	 * ------------------------------------------------------------
	 * These are the Device Independant functions that the power module
	 * can call. Please carefully add new interfaces to the right location
	 * to ensure no duplication of code that is common across hardware
	 */
	/* Queries the power state */
	int (*pwr_query)(unsigned int power_state);
	/*
	 * ------------------------------------------------------------
	 *        DEVICE-DEPENDENT PWR EXPORTED FUNCTIONS --->
	 *        PREFIX = pwr_ HIDDEN_SUFFIX = _corename (_snb / _vlv)
	 * ------------------------------------------------------------
	 * These are the Device Dependant functions that the power module can
	 * call. Please carefully add new interfaces to the right location to
	 * ensure no duplication of code that is common across hardware
	 */
	/* Sets power state. */
	int (*pwr_set)(igd_context_t *context, unsigned int power_state);

	/* Initializes power management module. */
	int (*pwr_init)(igd_context_t *context);
	/*
	 * ------------------------------------------------------------
	 *    DEVICE-DEPENDENT INTERMODULE EXPORTED FUNCTIONS --->
	 *    PREFIX = im_ HIDDEN_SUFFIX = _corename (_snb / _vlv)
	 * ------------------------------------------------------------
	 * These are the Device Dependant functions that power module DOES NOT call
	 * Other HAL modules need to use for internally managed functionality
	 * Please carefully add new interfaces to the right location to
	 * ensure no duplication of code that is common across hardware
	 */
	/* Initializes all the module clock gating / chicken bit / silicon
	 * workarounds for display pipeline as well as the GT pipeline
	 */
	void (*im_pwr_init_clock_gating)(struct drm_device *dev);
} igd_pwr_dispatch_t;

/*
 * NOTE: Some of these externs are declared with the struct name because the
 * contents of that struct are unavailable at the DI layer. The symbol
 * is used as the generic mode_dispatch_t which is a subset.
 */


#ifdef CONFIG_SNB
extern igd_pwr_dispatch_t pwr_dispatch_snb;
#endif
#ifdef CONFIG_VLV
extern igd_pwr_dispatch_t pwr_dispatch_vlv;
#endif


#endif /*_IGD_DISPATCH_MODE_H*/
