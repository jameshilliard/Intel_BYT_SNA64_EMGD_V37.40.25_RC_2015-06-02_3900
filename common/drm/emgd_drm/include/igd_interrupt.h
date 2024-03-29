/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_interrupt.h
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
 *  The is the IGD exported interrupt access functions. It should be included
 *  by client drivers making use of interrupt functionality of IGD.
 *  This file Should NOT be included by any IGD modules because in some
 *  IGD environments the interrupt module will not be available.
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_INTERRUPT_H
#define _IGD_INTERRUPT_H

typedef struct _oal_callbacks {
	void (*spin_lock_irqsave)(void *lock);
	void (*spin_unlock_irqrestore)(void *lock);
	void (*spin_lock_isr)(void *lock);
	void (*spin_unlock_isr)(void *lock);
	void (*spin_lock)(void *lock);
	void (*spin_unlock)(void *lock);
} oal_callbacks_t;

#define CB_FUNC(a, b, c) if(a && a->b) { a->b(c); }

/* Responses returned to the client from drm */
#define INT_INVALID  -1
#define INT_NOOP      0
#define INT_CLEARED   1
#define INT_HANDLED   2
#define INT_READ      3
#define INT_STORED    4
#define INT_TIMEOUT   5

/* Response from the ISR helper to the ISR routine */
#define ISR_NEED_DPC  0
#define ISR_HANDLED   1
#define ISR_NOT_OURS  2

typedef struct _interrupt_data {
	unsigned long irq_display;
	unsigned long irq_graphtrans;
	unsigned long irq_enabled;
	unsigned long irq_pm;
	unsigned long irq_pmiir;
	unsigned long irq_hpd;

} interrupt_data_t;

#define IGD_IRQ_DISP_VBLANK_A       0x00000001
#define IGD_IRQ_DISP_VBLANK_B       0x00000002
#define IGD_IRQ_DISP_FLIP_A         0x00000010
#define IGD_IRQ_DISP_FLIP_B         0x00000020
#define IGD_IRQ_SPRITE_FLIP_A       0x00000100
#define IGD_IRQ_SPRITE_FLIP_B       0x00000200
#define IGD_IRQ_SPRITE_FLIP_C       0x00000400
#define IGD_IRQ_SPRITE_FLIP_D       0x00000800

#define IGD_IRQ_HPD_DIG_B		0x00000001
#define IGD_IRQ_HPD_DIG_C		0x00000002
#define IGD_IRQ_HPD_DIG_B_SHORT		0x00000004
#define IGD_IRQ_HPD_DIG_B_LONG		0x00000008
#define IGD_IRQ_HPD_DIG_C_SHORT		0x00000010
#define IGD_IRQ_HPD_DIG_C_LONG		0x00000020
#define IGD_IRQ_HPD_DPB_AUX		0x00000040
#define IGD_IRQ_HPD_DPC_AUX		0x00000080
#define IGD_IRQ_HPD_CRT			0x00000100
#define IGD_IRQ_HPD_GENERAL		0x00001000

#define IGD_IRQ_GT_RND              0x00000001
#define IGD_IRQ_GT_VID              0x00000100
#define IGD_IRQ_GT_BLT              0x00010000
#define IGD_IRQ_PM_UP_EVENT	    0x00100000
#define IGD_IRQ_PM_DOWN_EVENT	    0x00200000

/*----------------------------------------------------------------------
 * Typedef: igd_interrupt_handler_t
 *
 * Description:
 *  This function type is used for OS independent interrupt handlers
 *  that will be called by the hal to handle a requested interrupt.
 *
 * Members:
 *
 *----------------------------------------------------------------------
 */
typedef void (igd_interrupt_handler_t)(unsigned long type,
	void *call_data,
	void *user_data);


/*----------------------------------------------------------------------
 * Typedef: igd_interrupt_t
 *
 * Description:
 *  An opaque handle to identify an interrupt to be handled
 *
 * Members:
 *
 *----------------------------------------------------------------------
 */
typedef void * igd_interrupt_t;

#define IGD_INTERRUPT_VBLANK 0x1
#define IGD_INTERRUPT_SYNC   0x2

/*----------------------------------------------------------------------
 * Function:
 *  isr_helper
 *
 * Description:
 *  This function helps the OAL/DRM module in doing all chipset specific
 *  functionality like register sets when an ISR is fired
 * Parameters:
 *	void
 * Returns:
 *  int - 0 on the interrupt was handled and we need to queue a DPC for it
 *      - 1 on the interrupt was handled but wasn't one to do further
 *          processing on
 *      - 2 on failure
 *----------------------------------------------------------------------
 */
int isr_helper(void);

/*----------------------------------------------------------------------
 * Function:
 *  unmask_int_helper
 *
 * Description:
 *  This function helps the OAL/DRM module in unmasking the Interrupt
 *  registers for a particular chipset so that the Interrupts can start
 *  firing
 * Parameters:
 *	void
 * Returns:
 *  int - 0 on SUCCESS
 *      - 1 on FAILURE
 *----------------------------------------------------------------------
 */
int unmask_interrupt_helper(interrupt_data_t *int_info_data);

/*----------------------------------------------------------------------
 * Function:
 *  mask_int_helper
 *
 * Description:
 *  This function helps the OAL/DRM module in masking the Interrupt
 *  registers for a particular chipset so that the Interrupts can stop
 *  firing
 * Parameters:
 *	void
 * Returns:
 *  int - 0 on SUCCESS
 *      - 1 on FAILURE
 *----------------------------------------------------------------------
 */
int mask_interrupt_helper(interrupt_data_t *int_info_data);

/*----------------------------------------------------------------------
 * Function:
 *  request_int_info_helper
 *
 * Description:
 *  This function helps the OAL module to request interrupt information 
 *  and status
 * Parameters:
 *	void *int_info_data 
 * Returns:
 *  int -  1 on Interrupt events supported and previous events occured
 *      -  0 on Interrupt events supported but no previous events occured
 *      - -1 on FAILURE or Interrupt events type not supported
 *----------------------------------------------------------------------
 */
int request_interrupt_info(interrupt_data_t *int_info_data);

/*----------------------------------------------------------------------
 * Function:
 *  clear_occured_int_helper
 *
 * Description:
 *  This function helps the OAL module to request interrupt information 
 *  and status
 * Parameters:
 *	void *int_info_data 
 * Returns:
 *  void
 *----------------------------------------------------------------------
 */
void clear_occured_int_helper(void *int_info_data);

/*----------------------------------------------------------------------
 * Function:
 *  mask_int_helper
 *
 * Description:
 *  This function helps the OAL/DRM module in clearing the cached copy
 *  of Interrupt data
 * Parameters:
 *	void
 * Returns:
 *	void
 *----------------------------------------------------------------------
 */
void clear_interrupt_cache(void);

/*----------------------------------------------------------------------
 * Function:
 *  dpc_helper
 *
 * Description:
 *  This function helps the OAL/DRM module in doing all chipset specific
 *  functionality like register sets when an DPC is fired
 * Parameters:
 *  interrupt_data_t **int_data
 * Returns:
 *  int - 0 on SUCCESS
 *      - 1 on FAILURE
 *----------------------------------------------------------------------
 */
int dpc_helper(void **int_data);


/*----------------------------------------------------------------------
 * Function:
 *  interrupt_init
 *
 * Description:
 *  This function initializees the interrupt helper module
 * Parameters:
 *  unsigned short did - Device ID for main video device
 *  oal_callbacks_t *oal_callbacks_table - Callback for
 *              spin lock related functions
 * Returns:
 *  int - 0 on SUCCESS
 *      - 1 on FAILURE
 *----------------------------------------------------------------------
 */
int interrupt_init(unsigned short did,
	unsigned char *virt_mmadr,
	oal_callbacks_t *oal_callbacks_table);

/*----------------------------------------------------------------------
 * Function:
 *  pre_init_interrupt
 *
 * Description:
 *  This function helps the OAL/DRM module in doing all chipset specific
 *  functionality like register sets before an ISR is initialized
 * Parameters:
 *  unsigned char *virt_mmadr - The MMIO base address
 * Returns:
 *  void
 *----------------------------------------------------------------------
 */
void interrupt_stop_irq(void);

/*----------------------------------------------------------------------
 * Function:
 *  post_init_interrupt
 *
 * Description:
 *  This function helps the OAL/DRM module in doing all chipset specific
 *  functionality like register sets after an ISR is initialized
 * Parameters:
 *  unsigned char *virt_mmadr - The MMIO base address
 * Returns:
 *  void
 *----------------------------------------------------------------------
 */
void interrupt_start_irq(void);

/*----------------------------------------------------------------------
 * Function:
 *  uninstall_interrupt
 *
 * Description:
 *  This function helps the OAL/DRM module in doing all chipset specific
 *  functionality like register sets for removing Interrupt support
 * Parameters:
 *  unsigned char *virt_mmadr - The MMIO base address
 * Returns:
 *  void
 *----------------------------------------------------------------------
 */
void uninstall_interrupt(void);

#endif
