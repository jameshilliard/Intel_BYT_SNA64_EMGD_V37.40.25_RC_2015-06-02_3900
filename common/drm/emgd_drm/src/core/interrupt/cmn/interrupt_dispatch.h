/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: interrupt_dispatch.h
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
 * 
 *-----------------------------------------------------------------------------
 * Description:
 *  
 *-----------------------------------------------------------------------------
 */

#ifndef _INTERRUPT_DISPATCH_H
#define _INTERRUPT_DISPATCH_H

#include <pci.h>
#include <dispatch.h>
#include <igd_interrupt.h>
#include "default_config.h"

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

typedef struct _interrupt_helper_dispatch {
	/* Initialization routine, needed first before any other functions */
	void (*init_interrupt)(unsigned char *mmadr);
	/* Stop IRQs generation */
	void (*stop_irq_helper)(oal_callbacks_t *oal_callbacks_table);
	/* Start IRQs generation */
	void (*start_irq_helper)(oal_callbacks_t *oal_callbacks_table);
	/* ISR helper routine */
	int (*isr_helper)(oal_callbacks_t *oal_callbacks_table);
	/* Mask interrupt helper routine */
	void (*mask_int_helper)(oal_callbacks_t *oal_callbacks_table,
		interrupt_data_t *int_mask);
	/* Unmask interrupt helper routine */
	void (*unmask_int_helper)(oal_callbacks_t *oal_callbacks_table,
		interrupt_data_t *int_unmask);
	/* Request interrupt information helper routine */
	int (*request_int_info_helper)(interrupt_data_t *int_info_data);
	/* Clear outstanding interrupt events helper routine */
	void (*clear_occured_int_helper)(void *int_info_data);
	/* Clear the cached copy of interrupt data */
	void (*clear_cache_int_helper)(oal_callbacks_t *oal_callbacks_table);
	/* DPC helper routine */
	int (*dpc_helper)(void *int_data);
	/* Uninstall helper routine */
	void (*uninstall_int_helper)(oal_callbacks_t *oal_callbacks_table);
} interrupt_helper_dispatch_t;


extern interrupt_helper_dispatch_t interrupt_helper_snb;
extern interrupt_helper_dispatch_t interrupt_helper_vlv;

extern emgd_drm_config_t *config_drm;
#endif
