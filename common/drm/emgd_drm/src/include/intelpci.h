/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: intelpci.h
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
 *  Contains PCI bus transaction definitions
 *-----------------------------------------------------------------------------
 */

/* PCI */
#define PCI_VENDOR_ID_INTEL             0x8086
#ifndef PCI_VENDOR_ID_STMICRO
#define PCI_VENDOR_ID_STMICRO           0x104A
#endif

/* Sandybridge */
#define PCI_DEVICE_ID_VGA_SNB           0x0102
#define PCI_DEVICE_ID_BRIDGE_SNB        0x0100

/* Ivybridge */
#define PCI_DEVICE_ID_VGA_IVB           0x0152
#define PCI_DEVICE_ID_BRIDGE_IVB        0x0150

/* Valleyview */
#define PCI_DEVICE_ID_BRIDGE_VLV        0x0F00
#define PCI_DEVICE_ID_VGA_VLV           0x0f30
#define PCI_DEVICE_ID_VGA_VLV2          0x0f31
#define PCI_DEVICE_ID_VGA_VLV3          0x0f32
#define PCI_DEVICE_ID_VGA_VLV4          0x0f33
#define PCI_DEVICE_ID_VGA_CDV0          0x0BE0
#define PCI_DEVICE_ID_VGA_CDV1          0x0BE1
#define PCI_DEVICE_ID_VGA_CDV2          0x0BE2
#define PCI_DEVICE_ID_VGA_CDV3          0x0BE3

/* Start: Southbridge specific */
#define PCI_DEVICE_ID_LPC_82801AA       0x2410
#define PCI_DEVICE_ID_LPC_82801AB       0x2420
#define PCI_DEVICE_ID_LPC_82801BA       0x2440
#define PCI_DEVICE_ID_LPC_82801BAM      0x244c
#define PCI_DEVICE_ID_LPC_82801E        0x2450
#define PCI_DEVICE_ID_LPC_82801CA       0x2480
#define PCI_DEVICE_ID_LPC_82801DB       0x24c0
#define PCI_DEVICE_ID_LPC_82801DBM      0x24cc
#define PCI_DEVICE_ID_LPC_82801EB       0x24d0
#define PCI_DEVICE_ID_LPC_82801EBM      0x24dc
#define PCI_DEVICE_ID_LPC_80001ESB      0x25a1  /* LPC on HanceRapids ICH */
#define PCI_DEVICE_ID_LPC_82801FB       0x2640  /* ICH6/ICH6R */
#define PCI_DEVICE_ID_LPC_82801FBM      0x2641  /* ICH6M/ICH6MR */
#define PCI_DEVICE_ID_LPC_82801FW       0x2642  /* ICH6W/ICH6WR */
#define PCI_DEVICE_ID_LPC_82801FWM      0x2643  /* ICH6MW/ICH6MWR */
#define PCI_DEVICE_ID_LPC_Q35DES        0x2910  /* ICH9 */
#define PCI_DEVICE_ID_LPC_Q35DHES       0x2912  /* ICH9 */
#define PCI_DEVICE_ID_LPC_Q35DOES       0x2914  /* ICH9 */
#define PCI_DEVICE_ID_LPC_Q35RES        0x2916  /* ICH9 */
#define PCI_DEVICE_ID_LPC_Q35BES        0x2918  /* ICH9 */



#define INTEL_PTE_ALLIGNMENT                0xFFFFF000

