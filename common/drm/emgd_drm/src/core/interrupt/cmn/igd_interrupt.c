/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_interrupt.c
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
 *
 *-----------------------------------------------------------------------------
 * Description:
 *  This file contains the implementations for the IGD interrupt functions.
 *-----------------------------------------------------------------------------
 */

#include <igd_core_structs.h>
#include "interrupt_dispatch.h"


/*
 * NOTE: Do not add comma's to this dispatch table. The macro's
 * remove the entire entry.
 */
static dispatch_table_t interrupt_helper_dispatch[] = {
	DISPATCH_SNB( &interrupt_helper_snb )
	DISPATCH_VLV( &interrupt_helper_vlv )
	DISPATCH_VLV2( &interrupt_helper_vlv )
	DISPATCH_VLV3( &interrupt_helper_vlv )
	DISPATCH_VLV4( &interrupt_helper_vlv )
	DISPATCH_END
};

static interrupt_helper_dispatch_t *helper_dispatch;
static oal_callbacks_t *local_sl_table;

/*!
 *
 * @param void
 *
 * @return 0 on the interrupt was handled and we need to queue a DPC for it
 * @return 1 on the interrupt was handled but wasn't one to do further
 *  processing on
 * @return 2 on the interrupt was not ours
 */
int isr_helper()
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return ISR_NOT_OURS;
	}

	/* else we call the ISR helper routine of that chipset */
	return helper_dispatch->isr_helper(local_sl_table);
}

/*!
 *
 * @param void
 *
 * @return 1 on failure
 * @return 0 on success
 */
int unmask_interrupt_helper(interrupt_data_t *int_info_data)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return -1;
	}

	/* else we call the unmask Interrupt helper routine of that chipset */
	helper_dispatch->unmask_int_helper(local_sl_table, int_info_data);
	return 0;
}

/*!
 *
 * @param void
 *
 * @return 1 on failure
 * @return 0 on success
 */
int mask_interrupt_helper(interrupt_data_t *int_info_data)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return 1;
	}

	/* else we call the mask Interrupt helper routine of that chipset */
	helper_dispatch->mask_int_helper(local_sl_table, int_info_data);
	return 0;
}

/*!
 *
 * @param void *int_info_data
 *
 * @return 1 on Interrupt events supported and previous events occured
 * @return 0 on Interrupt events supported but no previous events occured
 * @return -1 on FAILURE or Interrupt events type not supported
 */
int request_interrupt_info(interrupt_data_t *int_info_data)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return -1;
	}

	return helper_dispatch->request_int_info_helper(int_info_data);
}

/*!
 *
 * @param void *int_info_data
 *
 * @return void
 */
void clear_occured_interrupt(void *int_info_data)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return;
	}

	/* else we call the clear outstanding Interrupt helper routine of that 
	 * chipset
	 */
	helper_dispatch->clear_occured_int_helper(int_info_data);
}

/*!
 *
 * @param void
 *
 * @return void
 */
void clear_interrupt_cache()
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return;
	}

	/* else we call the mask Interrupt helper routine of that chipset */
	helper_dispatch->clear_cache_int_helper(local_sl_table);
}

/*!
 *
 * @param interrupt_data_t **int_data
 *
 * @return 1 on failure
 * @return 0 on success
 */
int dpc_helper(void **int_data)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return 1;
	}

	/* else we call the DPC helper routine of that chipset */
	return helper_dispatch->dpc_helper(int_data);
}

/*!
 *
 * @param context
 *
 * @return void
 */
void interrupt_stop_irq()
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return;
	}

	/* else we call the pre_init helper routine of that chipset */
	helper_dispatch->stop_irq_helper(local_sl_table);
}

/*!
 *
 * @param context
 *
 * @return void
 */
void interrupt_start_irq()
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return;
	}

	/* else we call the post_init helper routine of that chipset */
	helper_dispatch->start_irq_helper(local_sl_table);
}

/*!
 *
 * @param none
 *
 * @return void
 */
void interrupt_cleanup(void)
{
	/*
	 * If we couldn't find any dispatch helper entry point, then that means we
	 * don't have support for that particular chipset family, so we need to
	 * return a failure immediately.
	 */
	if(!helper_dispatch) {
		return;
	}

	/* else we call the post_init helper routine of that chipset */
	helper_dispatch->uninstall_int_helper(local_sl_table);
}

/*!
 *
 * @param did - Device ID for main video device
 * @param virt_mmadr - virt address to access hardware registers
 * @param oal_callbacks_t *oal_callbacks_table - Callback for
 *              spin lock related functions
 *
 * @return 1 on failure
 * @return 0 on success
 */
int interrupt_init(unsigned short did,
	unsigned char *virt_mmadr,
	oal_callbacks_t *oal_callbacks_table)
{
	helper_dispatch = dispatch_acquire_did(did, interrupt_helper_dispatch);
	if(helper_dispatch) {
		local_sl_table = oal_callbacks_table;
		helper_dispatch->init_interrupt(virt_mmadr);
	}

	return (helper_dispatch) ? 0 : 1;
}

