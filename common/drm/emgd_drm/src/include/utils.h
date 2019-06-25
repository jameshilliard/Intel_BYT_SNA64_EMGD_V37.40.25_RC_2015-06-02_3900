/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: utils.h
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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <config.h>
//#include <igd_core_structs.h>
#include <general.h>
/* FIXME: Do we need these header files,they are causing compilation error
#include <i915/intel_ringbuffer.h>
#include "drm_emgd_private.h"
*/

/*typedef struct _igd_context igd_context_t;*/

/* Get the MMIO pointer from the crtc context*/
#define MMIO(crtc) ((igd_context_t *)(crtc->igd_context))->device_context.virt_mmadr

/* Definition: value = READ_MMIO_REG(igd_display_context_t *, reg) */
#define READ_MMIO_REG(crtc, reg) \
	EMGD_READ32( MMIO(crtc) +(reg) )

/* Definition: WRITE_MMIO_REG(igd_display_context_t *, reg, value) */
#define WRITE_MMIO_REG(crtc, reg, value) \
	EMGD_WRITE32(value, ( MMIO(crtc) + (reg) ) )

/* Definition:WRITE_MMIO_REG_BITS(igd_display_context_t *, reg, value, mask) */
#define WRITE_MMIO_REG_BITS(crtc, reg, data, mask) \
{                                                 \
	unsigned long tmp;                            \
	tmp = READ_MMIO_REG((crtc), (reg));            \
	tmp &= (~(mask));                             \
	tmp |= ((data) & (mask));                     \
	WRITE_MMIO_REG((crtc), (reg), (tmp));          \
}

#define PORT_TYPE(d) (PORT(d, (d->port_number))->port_type)
#define PORT_TYPE_DH(dh) \
	(PORT(dh, (((igd_display_context_t *)dh)->port_number))->port_type)

/*
 * These are temporary macros used only within this header.
 * Individual config options use these macros to generate macros that look
 * like this:
 *
 * If CONFIG_FOO is defined
 *  OPT_FOO_SYMBOL(a)
 *  OPT_FOO_VALUE(a, b)
 *  OPT_FOO_VOID_CALL(fn)
 *  OPT_FOO_CALL(fn)
 */
#define OPTIONAL_VOID_CALL(fn) fn
#define OPTIONAL_CALL(fn)			\
	{								\
		int __ret;					\
		__ret = fn;					\
		if(__ret) {					\
			EMGD_ERROR_EXIT("EXIT");	\
			return __ret;			\
		}							\
	}

#define OPTIONAL_CALL_RET(ret, fn)      (ret) = (fn)

/*
 * Debug call macro should be used to call debug printing functions
 * that will only exist in debug builds.
 */
#ifdef DEBUG_BUILD_TYPE
#define OPT_DEBUG_SYMBOL(sym)      sym
#define OPT_DEBUG_VALUE(val, alt)  val
#define OPT_DEBUG_VOID_CALL(fn)    OPTIONAL_VOID_CALL(fn)
#define OPT_DEBUG_CALL(fn)         OPTIONAL_CALL(fn)
#define OPT_DEBUG_INLINE
#else
#define OPT_DEBUG_SYMBOL(sym)
#define OPT_DEBUG_VALUE(val, alt)  alt
#define OPT_DEBUG_VOID_CALL(fn)
#define OPT_DEBUG_CALL(fn)
#define OPT_DEBUG_INLINE           static __inline
#endif

/*
 * Micro Symbols are only used when CONFIG_MICRO is not defined.
 */
#ifndef CONFIG_MICRO
#define OPT_MICRO_SYMBOL(sym)         sym
#define OPT_MICRO_VALUE(val, alt)     val
#define OPT_MICRO_VOID_CALL(fn)       OPTIONAL_VOID_CALL(fn)
#define OPT_MICRO_CALL(fn)            OPTIONAL_CALL(fn)
#define OPT_MICRO_CALL_RET(ret, fn)   OPTIONAL_CALL_RET(ret, fn)
#else
#define OPT_MICRO_SYMBOL(sym)
#define OPT_MICRO_VALUE(val, alt)  alt
#define OPT_MICRO_VOID_CALL(fn)
#define OPT_MICRO_CALL(fn)
#define OPT_MICRO_CALL_RET(ret, fn)
#endif


#ifdef CONFIG_SNB
#define DISPATCH_SNB(p) {PCI_DEVICE_ID_VGA_SNB, p},
#else
#define DISPATCH_SNB(p)
#endif
#ifdef CONFIG_IVB
#define DISPATCH_IVB(p) {PCI_DEVICE_ID_VGA_IVB, p},
#else
#define DISPATCH_IVB(p)
#endif
#ifdef CONFIG_VLV
#define DISPATCH_VLV(p) {PCI_DEVICE_ID_VGA_VLV, p},
#define DISPATCH_VLV2(p) {PCI_DEVICE_ID_VGA_VLV2, p},
#define DISPATCH_VLV3(p) {PCI_DEVICE_ID_VGA_VLV3, p},
#define DISPATCH_VLV4(p) {PCI_DEVICE_ID_VGA_VLV4, p},
#else
#define DISPATCH_VLV(p)
#define DISPATCH_VLV2(p)
#define DISPATCH_VLV3(p)
#define DISPATCH_VLV4(p)
#endif

#define DISPATCH_END {0, NULL}


#endif // _UTILS_H_

