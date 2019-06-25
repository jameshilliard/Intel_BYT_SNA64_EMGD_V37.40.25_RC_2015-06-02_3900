/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: module_init.h
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
 *  This file contains the init/power/shutdown prototypes for all optional
 *  (and possibly other) modules.
 *-----------------------------------------------------------------------------
 */

#ifndef _MODULE_INIT_H
#define _MODULE_INIT_H

#include <config.h>

extern int mode_init(igd_context_t *context);
extern int dsp_init(igd_context_t *context);
extern int pi_init(igd_context_t *context);
extern int _overlay_full_init(igd_context_t *context, igd_param_t *params);
extern int _pwr_init(igd_context_t *context);
extern int _reset_init(igd_context_t *context);
extern int _reg_init(igd_context_t *context, unsigned long flags);


#ifdef CONFIG_OVERLAY
#define OVERLAY_INIT(c, p) _overlay_full_init(c, p)
#else
#define OVERLAY_INIT(a, p) 0
#endif

#ifdef CONFIG_POWER
#define PWR_INIT(a) _pwr_init(a);
#else
#define PWR_INIT(a) 0
#endif

#ifdef CONFIG_REG
#define REG_INIT(a, b) _reg_init(a, b);
#else
#define REG_INIT(a, b) 0
#endif

#ifdef CONFIG_RESET
#define RESET_INIT(a) _reset_init(a);
#else
#define RESET_INIT(a) 0
#endif

#endif
