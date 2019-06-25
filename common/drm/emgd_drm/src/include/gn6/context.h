/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: context.h
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

#ifndef _HAL_SNB_CONTEXT_H
#define _HAL_SNB_CONTEXT_H

#include <pci.h>

#define PLATFORM_CONTEXT_T platform_context_snb_t

typedef struct _dramc_device_snb {
	unsigned short did;
	os_pci_dev_t pcidev;
} dramc_device_snb_t;

typedef struct _igfx_device_snb {
	unsigned short did;
	unsigned char rid;

	int irq;

	unsigned char *hw_status_virt;
	unsigned long hw_status_phys;
	unsigned long hw_status_offset;

	os_pci_dev_t pcidev0;
} igfx_device_snb_t;

typedef struct _platform_context_snb {
	dramc_device_snb_t dramc_dev;
	igfx_device_snb_t igfx_dev;
	unsigned long ich_gpio_base;
} platform_context_snb_t;

#endif
