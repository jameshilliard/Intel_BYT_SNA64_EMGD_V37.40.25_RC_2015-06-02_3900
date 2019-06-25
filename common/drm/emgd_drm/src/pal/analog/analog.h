/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: analog.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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
 *  This is header file for ANALOG PORT DRIVER.
 *-----------------------------------------------------------------------------
 */

#ifndef _PD_ANALOG_H
#define _PD_ANALOG_H

typedef struct _analog_context {
	pd_callback_t *callback;
	pd_timing_t   *timing_table;
	unsigned long powerstate;
    pd_display_status_t crt_status;
    int dual_pipe;
	unsigned short detect_method;
	/* Add anything more required */
} analog_context_t;


#define ANALOG_PORT_REG_SNB      0x000E1100    /* Analog port register for SNB*/
#define ANALOG_PORT_REG          0x00061100    /* Analog port register */

extern int PD_MODULE_INIT(analog_init, (void *handle));
extern int PD_MODULE_EXIT(analog_exit, (void));
extern unsigned long analog_validate(unsigned long cookie);
extern int analog_open(pd_callback_t *callback, void **context);
extern int analog_init_device(void *context);
extern int analog_close(void *context);
extern int analog_set_mode(void *context, pd_timing_t *mode,
		unsigned long flags);
extern int analog_set_attrs(void *context, unsigned long num, pd_attr_t *list);
extern int analog_get_attrs(void *context, unsigned long *num, pd_attr_t**list);
extern int analog_get_timing_list(void *context, pd_timing_t *in_list,
		pd_timing_t **list);
extern int analog_set_power(void *context, uint32_t state);
extern int analog_get_power(void *context, uint32_t *state);
extern int analog_save(void *context, void **state, unsigned long flags);
extern int analog_restore(void *context, void *state, unsigned long flags);
extern int analog_get_port_status(void *context, pd_port_status_t *port_status);

int analog_read_reg(analog_context_t * pd_context, unsigned long reg,
		    unsigned long *value);
int analog_write_reg(analog_context_t * pd_context, unsigned long reg,
		     unsigned long value);

pd_display_status_t analog_sense_detect_crt(analog_context_t *pd_context);
#endif
/*----------------------------------------------------------------------------
 * File Revision History
 * $Id: analog.h,v 1.1.2.4 2012/02/22 01:55:01 jsim2 Exp $
 * $Source: /nfs/sie/disks/sie-cidgit_disk001/git_repos/GFX/cvs_fri22/koheo/linux/egd_drm/emgd/pal/analog/Attic/analog.h,v $
 *----------------------------------------------------------------------------
 */
