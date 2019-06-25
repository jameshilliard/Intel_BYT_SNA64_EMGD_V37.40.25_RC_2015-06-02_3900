/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: dsp.c
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
 *  This file contains all the necessary functions for display resource
 *  manager. This module abstracts all hardware resources and manages them.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.dsp

#include <io.h>
#include <memory.h>
#include <ossched.h>

#include <igd_core_structs.h>

#include <dsp.h>
#include <debug.h>
#include <pd_init.h>
#include "dsp_dispatch.h"

/*!
 * @addtogroup display_group
 * @{
 */

static dispatch_table_t dsp_dispatch_list[] = {

#ifdef CONFIG_SNB
	{PCI_DEVICE_ID_VGA_SNB, &dsp_dispatch_snb},
#endif
#ifdef CONFIG_IVB
	{PCI_DEVICE_ID_VGA_IVB, &dsp_dispatch_vlv},
#endif
#ifdef CONFIG_VLV
	{PCI_DEVICE_ID_VGA_VLV, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV2, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV3, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_VLV4, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_CDV0, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_CDV1, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_CDV2, &dsp_dispatch_vlv},
	{PCI_DEVICE_ID_VGA_CDV3, &dsp_dispatch_vlv},
#endif
	{0, NULL}
};

static int dsp_init_cursor(igd_context_t *context, igd_cursor_t *cursor);
static igd_display_port_t *dsp_get_next_port(igd_context_t *context,
	igd_display_port_t *last, int reverse);
static unsigned long dsp_valid_dc(unsigned long dc, unsigned long flags);

/*
 * This simple structure wraps the display configuration list so that
 * it is easier to manipulate.
 *    count is the current number of items in the list.
 *    size is the maximum number of items the list can hold
 *    dc_list is a pointer to the array of DC's
 */
typedef struct _dsp_dc_list_t {
	int count;
	int size;
	unsigned long *dc_list;
} dsp_dc_list_t;

typedef struct _dsp_context {

	dsp_dispatch_t *dispatch;
	igd_context_t *context;

	unsigned long num_dsp_planes;
	unsigned long num_dsp_pipes;
	unsigned long display_flags;

	unsigned long current_dc;
	unsigned long fw_dc; /* The DC programmed by the EFI or VBIOS */
	igd_display_context_t display_list[MAX_DISPLAYS];
	dsp_dc_list_t dsp_dc_list;
	/* holds pointer to port based on port numbers (1 based not zero based) */
	igd_display_port_t *port_list[IGD_MAX_PORTS + 1];

	/* holds pointer to display context based on port numbers
	 * (1 based not zero based) */
	//igd_display_context_t *display_ptr_list[IGD_MAX_PORTS + 1];
} dsp_context_t;

static dsp_context_t dsp_context;

/*
 * This macro is used to check the validity of a clone/extended DC. It
 * makes sure that the ports are capable of using both pipes.  p1 and
 * p2 need to be on unique pipes, if they both require the same pipe,
 * this check returns false.
 *
 * Note that this is only enabled for the driver since it does add some
 * code and currently we have no hardware that could be configured like
 * this.
 */
#define DSP_PIPE_OK(p1, p2) ( \
		((p1->port_features & IGD_PORT_USE_PIPE_MASK) | \
		(p2->port_features & IGD_PORT_USE_PIPE_MASK)) == \
		(IGD_PORT_USE_PIPEA | IGD_PORT_USE_PIPEB))

/* Design Notes:
 *
 *   1. The heart of display resources is the display configuration list.
 *      During initialization, the list display configuration list is
 *      created with all valid pipe/plane/port combinations.  Allocations
 *      are limited to combinations specified on the list.
 *
 *   2. Two display handles are maintaine corresponding to the two
 *      display pipes. To support hardware with more than two pipes,
 *      the DC value will need to expand beyond the current 32 bit value
 *      and the number of display handles increased appropriately.
 */

/*!
 * Re-construct the DC ( Display Configuration ) that the Gfx hardware
 * was programmed by Video BIOS or EFI Video Driver.
 *
 * TODO: This function does not handle three-display scenario.
 *
 * @param context
 *
 * @return fw_dc
 */
static unsigned long dsp_get_fw_dc(igd_context_t *context)
{
	igd_display_port_t *p = NULL;
	int pipeb_allocated = 0, pipea_allocated = 0;
	int port1 = 0, port2 = 0, mode = 0;
	int port_allocated = 0;
	unsigned long port_value;
	unsigned long fw_dc = 0;
	unsigned char *mmio = context->device_context.virt_mmadr;

	EMGD_TRACE_ENTER;

	/* Go through the port table */
	while ((p = dsp_get_next_port(context, p, 0)) != NULL) {

		port_value = EMGD_READ32(mmio + p->port_reg);
		if(port_value & BIT(31)) { /* is the port ON? */

			if(port1) {
				port2 = p->port_number;
			} else {
				port1 = p->port_number;
			}
			port_allocated++;

			if (port_value & BIT(30)) {
				pipeb_allocated++;
			} else	{
				pipea_allocated++;
			}
		}
	} /* while */

	if( pipea_allocated > 1 || pipeb_allocated > 1)	{
		/* Twin Mode */
		EMGD_DEBUG("Twin mode");
		mode = IGD_DISPLAY_CONFIG_TWIN;

		/* Re-construct the DC back */
		fw_dc = (port2 & 0x0F)<< 8 | (port1 & 0x0F)<< 4 | (mode & 0x0F);

		EMGD_DEBUG("fw_dc in Twin Mode: 0x%08lx", fw_dc);

		/* Check the fw_dc is in the list */
		if(!dsp_valid_dc(fw_dc, 0)) {
			EMGD_DEBUG("check the fw_dc  again in the DC list");

			/* If the assumed dc doesn't match reverse the order and
			 * and check again
			 */
			fw_dc = (port1 & 0x0F)<< 8 | (port2 & 0x0F)<< 4 | (mode & 0x0F);
			EMGD_DEBUG("hw_dc: 0x%08lx", fw_dc);

			if(!dsp_valid_dc(fw_dc, 0)) {
				EMGD_DEBUG("FW DC doesnt match dc list!!");
				fw_dc = 0L;
			}
		}

	} else if(port_allocated == 1) {

		EMGD_DEBUG("Single Configuration");
		mode = IGD_DISPLAY_CONFIG_SINGLE;
		/* Single Mode */
		fw_dc = (port2 & 0x0F)<< 20 |(port1 & 0x0F)<<4 |(mode & 0x0F);

		EMGD_DEBUG("fw_dc in Single Config  = 0x%08lx", fw_dc);

		/* Checking the assumed dc with the dc list */
		if(!dsp_valid_dc(fw_dc, 0)) {
			EMGD_DEBUG("check again");

			/* If the assumed dc doesn't match reverse the order and
			 * and check again
			 */
			fw_dc = (port1 & 0x0F)<< 20 |(port2 & 0x0F)<<4 |(mode & 0x0F);
			if(!dsp_valid_dc(fw_dc, 0)) {
				EMGD_DEBUG("FW DC doesnt match dc list!!!");
				fw_dc = 0L;
			}
		}

	} else {

		/*  VBIOS does not support extended. So i'ts most
		 *  likely clone.
		 */
		EMGD_DEBUG("Extended/Clone");
		mode = IGD_DISPLAY_CONFIG_CLONE;

		/* Re-construct the DC back */
		fw_dc = (port2 & 0x0F) << 20 | (port1 & 0x0F) << 4 | (mode & 0x0F);

		EMGD_DEBUG("fw_dc in Clone Mode:a 0x%08lx", fw_dc);

		/* Check the fw_dc is in the list */
		if(!dsp_valid_dc(fw_dc, 0)) {
			EMGD_DEBUG("Check again. This time reverse primary and secondary");

			/* if the assumed dc doesn't match reverse the order and
			 * check again
			 */
			fw_dc = (port1 & 0x0F) << 20 | (port2 & 0x0F) << 4 | (mode & 0x0F);
			if(!dsp_valid_dc(fw_dc, 0)) {
				EMGD_DEBUG("FW DC doesnot match dc list!!!");
				fw_dc = 0L;
			}
		}
	} /* else */

	EMGD_DEBUG("DC programmed by the firmware = 0x%08lx", fw_dc);
	EMGD_TRACE_EXIT;
	//*dc_list=&current_dc;
	return fw_dc;

} /* end of dsp_get_fw_dc */

/*!
 * This function gets the next value in a plane, pipe, or port table. The
 * caller provides a pointer to the last entry retrieved or NULL to get
 * the first entry.
 *
 * @param list
 * @param last
 * @param reverse
 *
 * @return void
 */
static void *dsp_get_next(void **list, void *last, int reverse)

{
	void **list_end = list;
	void **list_start = list;
	void **last_p = NULL;
	int off = 1;

	while(*list_end) {
		if(*list_end == last) {
			last_p = list_end;
		}
		list_end++;
	}
	list_end--;

	if(reverse) {
		list_start = list_end;
		list_end = list;
		off = -1;
	}
	if(!last) {
		return *list_start;
	}
	if((last == *list_end) || !last_p) {
		return NULL;
	}
	return last_p[off];
}

/*!
 * This function allows the caller to loop through the (port | plane | pipe)
 * table list and get each entry in turn. The call must provide a pointer
 * to the last entry retrieved (or NULL to get the first port).
 *
 * These functions are called from many places, including some functions
 * external to the DSP module.
 *
 * @param context pointer to the current driver context.
 * @param last pointer to the last entry retrieved.
 * @param reverse
 *
 * @return igd_display_port_t pointer to the next entry or NULL if no more ports available.
 */
static igd_display_port_t *dsp_get_next_port(igd_context_t *context,
	igd_display_port_t *last, int reverse)
{
	EMGD_DEBUG("Entry, dsp_get_next_port");

	return (igd_display_port_t *)dsp_get_next(
		(void **)dsp_context.dispatch->ports, (void *)last, reverse);
}

static igd_display_plane_t *dsp_get_next_plane(igd_context_t *context,
	igd_display_plane_t *last, int reverse)
{
	EMGD_DEBUG("Entry, dsp_get_next_plane");

	return (igd_display_plane_t *)dsp_get_next(
		(void **)dsp_context.dispatch->planes, (void *)last, reverse);
}

static igd_display_pipe_t *dsp_get_next_pipe(igd_context_t *context,
	igd_display_pipe_t *last, int reverse)
{
	EMGD_DEBUG("Entry, dsp_get_next_pipe");

	return (igd_display_pipe_t *)dsp_get_next(
		(void **)dsp_context.dispatch->pipes, (void *)last, reverse);
}

/*!
 * Check the DC to see if it is in the list.
 *
 * Flags:
 *       IGD_DC_EXACT_MATCH
 *       IGD_DC_CLOSEST_MATCH
 *
 * Flags are currently ignored.
 *
 * @param dc
 * @param flags
 *
 * @return DC
 * @return 0 if no match
 */
static unsigned long dsp_valid_dc(unsigned long dc, unsigned long flags)
{
	int i;

	for (i = 0; i < dsp_context.dsp_dc_list.count; i++) {
		if (dc == dsp_context.dsp_dc_list.dc_list[i]) {
			return dsp_context.dsp_dc_list.dc_list[i];
		}
	}

	/* Need to define how a closest match is determined */

	return 0;
}

/*!
 * Convert a DC value into to bitmasks, one for each pipe.
 *
 * @param dc
 * @param pipe1
 * @param pipe2
 *
 * @return void
 */
static void dsp_dc_to_masks(unsigned long dc,
		unsigned char *pipe1,
		unsigned char *pipe2)
{
	int i;
	int pn;

	*pipe1 = 0;
	*pipe2 = 0;

	for (i = 1; i < 8; i++) {
		if ((pn = DC_PORT_NUMBER(dc, i)) > 0) {
			if ((i < 5)) {
				*pipe1 |= (1 << pn);
			} else {
				*pipe2 |= (1 << pn);
			}
		}
	}
}

/*!
 *
 * @param context
 * @param port
 *
 * @return 1 if connected
 * @return 0 if not connected
 */
static int dsp_display_connected(igd_context_t *context,
	igd_display_port_t *port)
{
	unsigned long port_list[IGD_MAX_PORTS];
	unsigned long connected_ports;
	int ret;
	pd_port_status_t port_status;

	/* Display detection -
	 *   First, check if the VBIOS has provided the "connected_bits"
	 *   information. If it has, then check if the passed in port is in
	 *   the list.  Of course, this is only valid in the driver, vBIOS
	 *   can't check vBIOS bits.
	 *
	 *   If there isn't any "connected_bits" info, then have the port
	 *   driver try and detect the display.  If the display is detected
	 *   or if the port driver is incapabile of doing display detection,
	 *   then return connected.
	 */
	OS_MEMSET(port_list, 0, sizeof(unsigned long) * IGD_MAX_PORTS);
	connected_ports = 0;
	if (igd_get_param((igd_driver_h)context, IGD_PARAM_PORT_LIST,
				port_list) == 0) {
		while ((connected_ports < IGD_MAX_PORTS) &&
				port_list[connected_ports]) {
			if(port_list[connected_ports] == port->port_number) {
					return 1;
			}
			connected_ports++;
		}
		return 0;
	}

	/* No vBIOS info, so do runtime detection if possible. */
	if (port->pd_context == NULL) {
		return 0;
	}

	ret = port->pd_driver->pd_get_port_status(port->pd_context, &port_status);
	if ((ret != PD_SUCCESS) ||
			(port_status.connected == PD_DISP_STATUS_DETACHED)) {
		return 0;
	}

	/* Port is connected (as far as we can tell) and OK to use */
	return 1;
}

/*!
 * Checks a given port to make sure it is ok to use it in a
 * display configuration. Two things can make a port unusable.
 * First it can be marked inuse, which means it isn't listed in
 * the current port_order.
 * Second, if display detect is enabled and no display is detected.
 *
 * @param context
 * @param port
 * @param display_detect
 *
 * @return 0 if not ok to use
 * @return 1 if ok to use
 */
static int dsp_port_is_ok(igd_context_t *context,
	igd_display_port_t *port,
	int display_detect)
{
	/*
	 * The port->inuse value is used in two different ways.  This is
	 * causing a conflict now.
	 *
	 *   - It is set if the port doesn't show up in the port order.
	 *   - It is set if the port is allocated by dsp_alloc_pipe_planes()
	 *
	 * This is ok if this function is called prior to any dsp_alloc_pipe_planes
	 *
	 * Maybe what is really needed here is a check to see if the port
	 * shows up in the port order.
	 */

	/* If it is inuse, then it isn't currently ok to use */
	if (port->inuse == 0x80) {
		EMGD_DEBUG("port_ok? Port is marked unusable");
		return 0;
	}

	/* Does port have a port driver? */
	if (port->pd_driver == NULL) {
		EMGD_DEBUG("port %d has no port driver", port->port_number);
		return 0;
	}

	/* If display detect, then check to make sure display is present */
	if (display_detect) {
		return dsp_display_connected(context, port);
	}
	return 1;
}

/*!
 * Add a new DC to the end of the list. Checks to make sure there is
 * space in the allocated list first.
 *
 * @param dc_list
 * @param dc
 * @param ext
 *
 * @return void
 */
static void dsp_add_to_dc_list(dsp_dc_list_t *dc_list, unsigned long dc,
		unsigned long ext)
{
	unsigned char pipe1, pipe2;
	unsigned char p1, p2;
	int d;
	int ok = 1;

	/* Check for duplicate DC first */
	if ((dc_list->count != 0) && (dc != 0)) {
		dsp_dc_to_masks(dc, &pipe1, &pipe2);
		for (d = 0; d < dc_list->count; d++) {
			/* build a bitmap of port used by DC under test */
			dsp_dc_to_masks(dc_list->dc_list[d], &p1, &p2);

			/*
			 * Compare the bitmaps.
			 *
			 * There are two bitmaps, one for each pipe. This is needed so
			 * 0x00500232 and 0x00200532 aren't flaged as duplicate.
			 */
			if ((pipe1 == p1) && (pipe2 == p2)) {
				ok = 0; /* duplicate */
			}
			if ((pipe1 == p2) && (pipe2 == p1)) {
				ok = 0; /* a pipe reversed duplicate DC */
			}
		}
	}

	if (ok) {
		if (dc_list->count < dc_list->size) {
			dc_list->dc_list[dc_list->count] = dc;
			dc_list->count++;
		}
		if (ext && (dc_list->count < dc_list->size)) {
			dc = (dc & 0xfffffff0) | 8;
			dc_list->dc_list[dc_list->count] = dc;
			dc_list->count++;
		}
	}
}


/*!
 * Given a index to the display pipeline, find the lice display crtc context for
 * that display pipeline.
 *
 * @param INPUT pipeline_index (index to the internal display_list array)
 * @param OUTPUT emgd_crtc (ptr to ptr of igd_display_context_t)
 *
 * @return void
 */
static void dsp_get_display_crtc(unsigned short pipeline_index,
	igd_display_context_t **emgd_crtc)
{
	*emgd_crtc = &(dsp_context.display_list[pipeline_index]);

}

/*!
 * Given a port number, get the display_port context for
 *
 * @param port_number
 * @param display
 * @param port
 * @param display_detect
 *
 * @return void
 */
static void dsp_get_display_port(unsigned short port_number,
	igd_display_port_t **_port, int display_detect)
{
	igd_display_port_t *port = NULL;

	if(_port) {
		while ((port = dsp_get_next_port(dsp_context.context, port, 0))) {
			if (port->port_number == port_number) {
				*_port = port;
				if (display_detect &&
					(dsp_display_connected(dsp_context.context, port) != 1)) {
					EMGD_DEBUG("Usable but unattached port found");
					*_port = NULL;
				}
				break;
			}
		}
	}

	return;
}

/*!
 *
 * @param dc
 * @param primary
 * @param secondary
 *
 * @return void
 */
static void dsp_get_dc(unsigned long *dc,
	igd_display_context_t **primary,
	igd_display_context_t **secondary)
{
	/*EMGD_TRACE_ENTER;*/
	if(dc) {
		*dc = dsp_context.current_dc;
	}
	if(primary) {
		*primary =
			&(dsp_context.display_list[0]);
	}
	if(secondary) {
		*secondary =
			&(dsp_context.display_list[1]);
	}
	/*EMGD_TRACE_EXIT;*/
}

/*!
 *
 * @param primary_display_plane
 * @param secondary_display_plane
 * @param primary_pipe
 * @param secondary_pipe
 *
 * @return void
 */
static void dsp_get_planes_pipes(igd_display_plane_t **primary_display_plane,
	igd_display_plane_t **secondary_display_plane,
	igd_display_pipe_t **primary_pipe,
	igd_display_pipe_t **secondary_pipe)
{
	igd_display_plane_t          **plane;
	igd_display_pipe_t   **pipe;
	int is_primary = 1;

	EMGD_TRACE_ENTER;

	if(!primary_display_plane || !secondary_display_plane ||
		!primary_pipe || !secondary_pipe) {

		EMGD_ERROR("Invalid parameters");
	} else {

		plane = dsp_context.dispatch->planes;

		while (*plane) {
			if ((*plane)->plane_features & IGD_PLANE_DISPLAY) {

				if(is_primary) {
					*primary_display_plane = *plane;
					is_primary = 0;
				} else {
					*secondary_display_plane = *plane;
					break;
				}
			}
			plane++;
		}

		pipe = dsp_context.dispatch->pipes;
		is_primary = 1;

		while (*pipe) {
			if(is_primary) {
				*primary_pipe = *pipe;
				is_primary = 0;
			} else {
				*secondary_pipe = *pipe;
				break;
			}
			pipe++;
		}
	}

	EMGD_TRACE_EXIT;
}

/*!
 * Check port features of multiple ports to see if they can share a pipe.
 * Ports that are passed in as NULL are not considered.  There must be
 * at least two ports.
 *
 * Sort the ports so that any port that must be pipe master is listed
 * first. Return the sorted port order in a format that can be easily
 * merged into a full DC value.
 *
 * @param m
 * @param p1
 * @param p2
 * @param p3
 *
 * @return 0 if error
 * @return DC
 */
static unsigned long dsp_shareable(igd_display_port_t *m,
		igd_display_port_t *p1,
		igd_display_port_t *p2,
		igd_display_port_t *p3)
{
	unsigned long features;
	unsigned long shareable;
	unsigned long dc;

	if (m == NULL) {
		/* Error checking */
		EMGD_ERROR("dsp_shareable: port pointer is NULL.");
		return 0;
	}

	if (p1 == NULL) {
		dc = m->port_number;
		return dc;
	}

	/*
	 * OR all the port secondary port type bits together.
	 * If ANDing this with the primary port's feature bits changes
	 * the value, then all the ports are not shareable.
	 *
	 * This does assume that if port A can share with port B,
	 * then port B can share with port A.
	 */
	features = m->port_features & IGD_PORT_SHARE_MASK;
	shareable = (p1->port_type & IGD_PORT_SHARE_MASK);
	if (p2 != NULL) {
		shareable |= (p2->port_type & IGD_PORT_SHARE_MASK);
		if (p3 != NULL) {
			shareable |= (p3->port_type & IGD_PORT_SHARE_MASK);
		}
	}

	/* make features a true/false type value */
	features = ((features & shareable) == shareable);

	/*
	 * This section re-arranges the order of the ports so that the
	 * port that needs to be the pipe master is always listed first.
	 *
	 * FIXME:
	 *   This doesn't support the case where more than one port must
	 *   be pipe master.
	 *
	 * FIXME:
	 *   Not sure if this is handling CLOCK_MASTER correctly. CLOCK_MASTER
	 *   can share a pipe with other ports but wants the PLL to be programmed
	 *   with its clock.
	 */
	if (features) {
		dc = m->port_number;
		if (p1->pd_flags & PD_FLAG_PIPE_MASTER) {
			dc = (dc << 4) | p1->port_number;
		} else if (p1->pd_flags & PD_FLAG_CLOCK_MASTER) {
			dc = (dc << 4) | p1->port_number;
		} else {
			dc = dc | (p1->port_number << 4);
		}
		if (p2 != NULL) {
			if (p2->pd_flags & PD_FLAG_PIPE_MASTER) {
				dc = (dc << 4) | p2->port_number;
			} else if (p2->pd_flags & PD_FLAG_CLOCK_MASTER) {
				dc = (dc << 4) | p2->port_number;
			} else {
				dc = dc | (p2->port_number << 8);
			}
		}
		if (p3 != NULL) {
			if (p3->pd_flags & PD_FLAG_PIPE_MASTER) {
				dc = (dc << 4) | p3->port_number;
			} else if (p3->pd_flags & PD_FLAG_CLOCK_MASTER) {
				dc = (dc << 4) | p3->port_number;
			} else {
				dc = dc | (p3->port_number << 12);
			}
		}
		return dc;
	} else {
		return 0;
	}
}

/*!
 * Given a group of ports and some basic infomation like how many pipes
 * are available, construct a set of valid display configurations. The
 * number of ports passed in is variable so that if a port doesn't exist
 * it should be passed in as a NULL.  At least one port must be passed
 * in. Also, NULL ports should be at the end of the list. Don't pass in
 * NULL, NULL, NULL, vaid_port because it will break this function.
 *
 * The flags value is used to enabled dual pipe support and extended
 * support.  If neither are set, only TWIN configurations will be returned.
 *
 * @param m
 * @param t1
 * @param t2
 * @param t3
 * @param flags
 * @param dc_list
 *
 * @return void
 */
static void dsp_build_display_configs(igd_display_port_t *m,
		igd_display_port_t *t1,
		igd_display_port_t *t2,
		igd_display_port_t *t3,
		unsigned long flags, dsp_dc_list_t *dc_list)
{

	unsigned long dc, dc2;

	/* If two pipes, do all clone/extended configurations first */
	if ((flags & 0x0f) > 1) {
		/* first two ports on different pipes, all remaining ports on pipe 2 */
		/* can port 0 share with port1 and port 2 */
		if ((dc = dsp_shareable(t1, t2, t3, NULL)) && DSP_PIPE_OK(m, t1)) {
			dc = (dc << 20) | 2 | ((m->port_number & 0x0f) << 4);
			dsp_add_to_dc_list(dc_list, dc, (flags & IGD_PLANE_DIH));
		}
		/* first two ports on different pipes, all remaining ports on pipe 1 */
		if ((dc = dsp_shareable(m, t2, t3, NULL)) && DSP_PIPE_OK(m, t1)) {
			dc = (dc << 4) | 2 | ((t1->port_number & 0x0f) << 20);
			dsp_add_to_dc_list(dc_list, dc, (flags & IGD_PLANE_DIH));
		}

		/* first two ports on different pipes, remianing ports split */
		if ((dc = dsp_shareable(m, t2, NULL, NULL)) &&
				(dc2 = dsp_shareable(t1, t3, NULL, NULL)) &&
				DSP_PIPE_OK(m, t1)) {
			dc = (dc << 4) | 2 | (dc2 << 20);
			dsp_add_to_dc_list(dc_list, dc, (flags & IGD_PLANE_DIH));
		}

		/* first two ports on different pipes, remianing ports split */
		if ((dc = dsp_shareable(m, t3, NULL, NULL)) &&
				(dc2 = dsp_shareable(t1, t2, NULL, NULL)) &&
				DSP_PIPE_OK(m, t1)) {
			dc = (dc << 4) | 2 | (dc2 << 20);
			dsp_add_to_dc_list(dc_list, dc, (flags & IGD_PLANE_DIH));
		}
	}

	/* Twinned type configurations, all ports on first pipe */
	if ((dc = dsp_shareable(m, t1, t2, t3))) {
		dc = (dc << 4) | 4;
		dsp_add_to_dc_list(dc_list, dc, 0);
	}
}

/*!
 * This routine puts the port table entries as per user (via IAL) mentioned
 * port order & firmware settings.
 *
 * @param context
 * @param port_order
 *
 * @return void
 */
static void do_port_order(igd_context_t *context, unsigned long *port_order)
{
	igd_display_port_t **port_table;
	igd_display_port_t *p;
	unsigned long i, j, k, num_ports;
	unsigned long num_ports1;

	/* Adjust port detect/allocation order with user/caller preference */
	port_table = dsp_context.dispatch->ports;
	num_ports = 0;
	while (port_table[num_ports]) {
		/* Build a port number to port array */
		dsp_context.port_list[port_table[num_ports]->port_number] =
			port_table[num_ports];
		num_ports++;
	}

	num_ports1 = 0;
	while ((num_ports1 < IGD_MAX_PORTS) && port_order[num_ports1]) {
		num_ports1++;
	}

	/*
	 * If port order is specified, then the ports that are not present in the
	 * port order list should not be allowed to be allocated. Since it is not
	 * possible to remove ports from port_table, mark ports that cannot be
	 * allocated as inuse with 0x80.  Ports marked in-use by the display
	 * allocation function will set this to 0x01.  This is used in the
	 * dsp_dc_init() function to detmine what ports are usable.
	 */
	if (num_ports1 != 0) {
		for (i = 0; i < num_ports; i++) {
			port_table[i]->inuse = 0x80;
		}
	} else {
#ifndef CONFIG_MICRO
		/* Copying back the port order to the mode_context's port order */
		for (i = 0; i < num_ports; i++) {
			port_order[i] = port_table[i]->port_number;
		}
#endif
		return;
	}

	/* Arrange port_table list to match IAL provided port_order list.
	 * Dont forget to unmark its in-use flag to ensure it's allowed
	 * for usage. */
	k = 0;
	for (i = 0; i < IGD_MAX_PORTS; i++) {
		/* Find the port for given port number */
		for (j = k; j < num_ports; j++) {
			if (port_table[j]->port_number == port_order[i]) {
				port_table[j]->inuse = 0;
				p = port_table[k];
				port_table[k] = port_table[j];
				port_table[j] = p;
				k++;
				break;
			}
		}
	}
	return;
} /* end do_port_order() */

/*!
 *
 * @param context
 * @param config_info
 *
 * @return 0
 */
static int dsp_get_config_info(igd_context_t *context,
	igd_config_info_t *config_info)
{
	EMGD_ASSERT(context, "Null context", -IGD_ERROR_INVAL);
	EMGD_ASSERT(config_info, "Null config_info", -IGD_ERROR_INVAL);

	config_info->num_dsp_planes    = dsp_context.num_dsp_planes;
	config_info->num_dsp_pipes     = dsp_context.num_dsp_pipes;
	config_info->fb_caps           = dsp_context.dispatch->caps;

	return 0;
}


/*!
 * Print out the DC list for debug purposes
 *
 * @param void
 *
 * @return void
 */
void __always_unused debug_print_dc_list( void )
{
	int i = 0;
	unsigned long mask;
	int x, c, d;
	int port;
	char tstr[50];
	char *pname[6];

	pname[0] = "Unused";
	pname[1] = "TV";
	pname[2] = "DVOB";
	pname[3] = "DVOC";
	pname[4] = "LVDS";
	pname[5] = "CRT";

	for (d = 0; d < dsp_context.dsp_dc_list.count; d++) {
		sprintf(tstr, "%3d: ", i);
		mask = 0x0000000f;
		c = 0;
		for (x = 0; x < 7; x++) {
			mask = mask << 4;
			port = (dsp_context.dsp_dc_list.dc_list[d] & mask) >> (4 * (x+1));
			if ((x == 4) && (port != 0)) {
				strcat(tstr, " + ");
				c = 0;
			}
			if (port != 0) {
				if (c) {
					strcat(tstr, ",");
				}
				strcat(tstr, pname[port]);
				c = 1;
			}
		}
		if ((dsp_context.dsp_dc_list.dc_list[d] & 0x0000000f) == 8) {
			strcat(tstr, "/e");
		}
		EMGD_ERROR("%-20s 0x%08lx", tstr, dsp_context.dsp_dc_list.dc_list[d]);
		i++;
	}
}

/*!
 * Build a list of valid display configurations (DC) and store for later
 * use by the driver.
 *
 * @param context
 *
 * @return void
 */
void dsp_dc_init(igd_context_t *context)
{
	igd_display_port_t *p0 = NULL, *p1 = NULL, *p2 = NULL, *p3 = NULL;
	unsigned long dc_flags;
	int num_ports;
	igd_display_port_t **port_table;
	igd_display_plane_t **plane;
	unsigned long display_detect;
	int sec_display_detected = 0;
	int count;

	/* Build the display configuration list */
	dc_flags = dsp_context.num_dsp_pipes & 0x0f;

	/* Check plane features to see if DIH is possible (IGD_PLANE_DIH) */
	plane = dsp_context.dispatch->planes;
	if (*plane) {
		if ((*plane)->plane_features & IGD_PLANE_DIH) {
			dc_flags |= IGD_PLANE_DIH;
		}
	}

	display_detect = (dsp_context.display_flags & IGD_DISPLAY_DETECT)?1:0;

	/*
	 * Given the number of ports and number of pipes can the max number
	 * of DC's be calculated?
	 *
	 * Number of ports squared.
	 * If clone double.
	 * If DIH, double again.
	 */

	/* Find the number of ports */
	port_table = dsp_context.dispatch->ports;
	num_ports = 0;
	while (port_table[num_ports]) {
		num_ports++;
	}
	EMGD_DEBUG("ports = %d, pipes = %ld", num_ports,
		dsp_context.num_dsp_pipes);


	/* If this is the first time called, then allocate memory for the list */
	if (dsp_context.dsp_dc_list.dc_list == NULL) {
		dsp_context.dsp_dc_list.size = num_ports * num_ports;
		if (dsp_context.num_dsp_pipes > 1) {
			dsp_context.dsp_dc_list.size *= 2;
			if (dc_flags & IGD_PLANE_DIH) { /* extended modes */
				dsp_context.dsp_dc_list.size *= 2;
			}
		}

		/* Need to allocate memory for list */
		dsp_context.dsp_dc_list.dc_list =
			OS_ALLOC(sizeof(unsigned long) * dsp_context.dsp_dc_list.size);
	}
	dsp_context.dsp_dc_list.count = 0;

	if (dsp_context.dsp_dc_list.dc_list == NULL) {
		EMGD_DEBUG("Memory allocation error, can't allocate DC list");
		return;
	}

	p0 = NULL;
	while ((p0 = dsp_get_next_port(context, p0, 0)) != NULL) {
		if (dsp_port_is_ok(context, p0, display_detect)) {
			dsp_add_to_dc_list(&dsp_context.dsp_dc_list,
					(1 | ((p0->port_number & 0x0f) << 4)), 0);
			count = 0;
			/*
			 * Here we have to loop for the second time if the secondary
			 * port detection at the first attempt with display_detect on
			 * fails. In this case, rerun the detection and turn display
			 * detect off instead to get port from default_config.
			 */
			while (count < 2) {
				p1 = NULL;
				while ((p1 = dsp_get_next_port(context, p1, 0)) != NULL) {
					if ((dsp_context.display_flags & IGD_DISPLAY_HOTPLUG) &&
					    !(dsp_context.display_flags & IGD_DISPLAY_DETECT)) {
						/*
						 * If this is the first detection, turn on display detect
						 * to make sure the presence of any detectable port.
						 * If no port is detected, run this detection  again with
						 * display detect off to get port from default_config.
						 */
						if (count)
							display_detect = 0;
						else
							display_detect = 1;
					}
					if ((p1 != p0) && dsp_port_is_ok(context, p1, display_detect)) {
						dsp_build_display_configs(p0, p1, NULL, NULL, dc_flags,
								&dsp_context.dsp_dc_list);
						if ((dsp_context.display_flags & IGD_DISPLAY_HOTPLUG) &&
						    !(dsp_context.display_flags & IGD_DISPLAY_DETECT))
							/*
							 * A port is identified for secondary display.
							 */
							sec_display_detected = 1;
						p2 = NULL;
						while ((p2 = dsp_get_next_port(context, p2, 0)) != NULL) {
							if ((p2 != p0) && (p2 != p1) &&
									dsp_port_is_ok(context, p2, display_detect)) {
								dsp_build_display_configs(p0, p1, p2, NULL,
										dc_flags, &dsp_context.dsp_dc_list);
								while ((p3 = dsp_get_next_port(context, p3, 0)) !=
										NULL) {
									if ((p3 != p0) && (p3 != p2) && (p3 != p1) &&
											dsp_port_is_ok(context, p3, display_detect)) {
										dsp_build_display_configs(p0, p1, p2, p3,
												dc_flags, &dsp_context.dsp_dc_list);
									}
								}
							}
						}
					}
				}
				/*
				 * Check whether the secondary port is identified.
				 * If it is not, rerun the detection above with
				 * display detect off to get the port from
				 * default_config.
				 */
				if ((dsp_context.display_flags & IGD_DISPLAY_HOTPLUG) &&
				    !(dsp_context.display_flags & IGD_DISPLAY_DETECT) &&
				    !sec_display_detected) {
					count++;
				} else {
					/*
					 * Break the loop as secondary port is
					 * detected and identified.
					 */
					break;
				}
			}
		}
	}

	/* zero terminate the list */
	dsp_add_to_dc_list(&dsp_context.dsp_dc_list, 0L, 0L);

	/*If quickboot, get the dc that was programmed by the firware
	 * i.e. Video BIOS or EFI Video Driver
	 */

#ifndef CONFIG_MICRO  /* We don't need this in firmware code */
	dsp_context.fw_dc = dsp_get_fw_dc(context);
	dsp_context.context->module_dispatch.dsp_fw_dc = dsp_context.fw_dc;

#endif

	/*OPT_DEBUG_VOID_CALL(debug_print_dc_list());*/

} /* end dsp_dc_init() */


/*!
 * Return the list of valid display configurations. The list returned is
 * currently the "live" list. The IAL should not modify it in any way.
 *
 * For the driver, this will re-initialize the DC list every time it's
 * called so that the list will reflect displays that have been attached
 * or detached since the last call.  If the list is empty (i.e. no
 * displays attached and/or no port drivers loaded), then fall back to
 * the following default behavior:
 *   1. Make sure analog port is marked as available
 *   2. Turn off the display detect option.
 *   3. Re-initialize the DC list.
 * This guarentees that we'll always have at least one valid display (CRT)
 * on the list.
 *
 * @param driver_handle
 *
 * @param request - The requested display mode. The first DC in the list
 *    with a matching display mode is returned when the IGD_QUERY_DC_INIT
 *    flag is set.
 *
 * @param dc_list - The returned DC list. This will point to the live list
 *    and as such, should not be freed or altered.
 *
 * @param flags - Determines what portion of the list is returned.
 *
 * @return 0: success.
 * @return -IGD_ERROR_INVAL: No valid DC list can be returned.
 */
static int igd_query_dc(igd_driver_h driver_handle,
	unsigned long request,
	unsigned long **dc_list,
	unsigned long flags)
{
	igd_context_t *context = (igd_context_t *)driver_handle;
	int d;

	EMGD_DEBUG ("Enter igd_query_dc");

	dsp_dc_init(context);
	/*
	 * If the list is empty at this point, add single/analog so that
	 * the driver can run.  Under normal conditions, this shouldn't
	 * happen.  In this case, turn off display detect too?
	 */
	if (dsp_context.dsp_dc_list.count == 1) {

		EMGD_ERROR("No displays detected or available:");

		/* If display detect is on, turn it off and try again */
		if (dsp_context.display_flags & IGD_DISPLAY_DETECT) {

			dsp_context.display_flags &= ~IGD_DISPLAY_DETECT;

			/* Turn off display detect */
			EMGD_ERROR("  Disabling display detect.");
			dsp_dc_init(context);
		}
			/*
			 * Turn the display detect on back for later hotplug
			 * use. Without doing this, port selection might not
			 * be selecting the correct port if a port is
			 * hotplugging in after this.
			 */
			if (dsp_context.display_flags & IGD_DISPLAY_HOTPLUG)
				dsp_context.display_flags |= IGD_DISPLAY_DETECT;
	}

	if (flags == IGD_QUERY_DC_ALL) {
		*dc_list = dsp_context.dsp_dc_list.dc_list;
		return 0;
	} else if (flags == IGD_QUERY_DC_PREFERRED) {
		/*
		 * FIXME:
		 *  This case is not yet implemented. No IAL currently uses this
		 *  capability.
		 *
		 *  Somehow this needs to build a new smaller list and return
		 *  that.  The caller would then be responsibile for freeing the
		 *  memory?
		 *
		 *  For now, this returns the full list so that if an IAL tries
		 *  to use it, it doesn't break.
		 */
		*dc_list = dsp_context.dsp_dc_list.dc_list;
		return 0;
	} else if (flags == IGD_QUERY_DC_INIT) {
		/* what is the current display config? */
		for(d = 0; d < dsp_context.dsp_dc_list.count; d++) {
			if (dsp_context.dsp_dc_list.dc_list[d] == request) {
				*dc_list = &dsp_context.dsp_dc_list.dc_list[d];
				return 0;
			}
		}
		/* Find first match of same type */
		for(d = 0; d < dsp_context.dsp_dc_list.count; d++) {
			if (dsp_context.dsp_dc_list.dc_list[d] & (request & 0x0f)) {
				*dc_list = &dsp_context.dsp_dc_list.dc_list[d];
				return 0;
			}
		}
	} else if (flags == IGD_QUERY_DC_EXISTING) {
		*dc_list = &dsp_context.fw_dc;
		return 0;
	}
	EMGD_ERROR ("igd_query_dc found no match for requested dc = %ld", request);
	*dc_list = NULL;
	return -IGD_ERROR_INVAL;
}

/*!
 * Mark a plane as available. If there are surfaces allocated to the
 * plane, free them.
 *
 * @param context
 * @param plane
 *
 * @return void
 */
static void free_plane(igd_context_t *context, igd_display_plane_t *plane)
{
	igd_cursor_info_t *ci;

	EMGD_DEBUG("free_plane Entry");

	if(!plane) {
		EMGD_ERROR("Attempt to free NULL plane");
		return;
	}

	/* If a framebuffer was allocated for this plane, free it */
	if (!plane->mirror && (plane->fb_info != NULL)) {
		if ((IGD_PLANE_CURSOR & plane->plane_features) == IGD_PLANE_CURSOR){
			ci = (igd_cursor_info_t *)plane->fb_info;
			EMGD_DEBUG("Freeing cursor image @ 0x%08lx", ci->xor_offset);
			/* FIXME - We need to call GEM to free the cursor surface!!! */
			EMGD_DEBUG("Freeing cursor image @ 0x%08lx", ci->argb_offset);
			/* FIXME - We need to call GEM to free the cursor surface!!! */
			/* Free plane info */
			if(plane->fb_info) {
				EMGD_ERROR("Found emgd_framebuffer cursor in HAL's freeplane! GEM freed?");
				plane->fb_info = NULL;
			}
		} else {
			if(plane->fb_info) {
				EMGD_ERROR("Found emgd_framebuffer ptr in HAL's freeplane! GEM freed?");
				plane->fb_info = NULL;
			}
		}
	} else {
		/* If this is mirrored (for clone displays), break the mirror */
		if(!plane->mirror) {
			EMGD_ERROR("Attempt to free NULL plane mirror");
			return;
		}
		plane->mirror->mirror = NULL;
		plane->mirror = NULL;
		/* Note: The following fixes a kernel Oops when the DRM module
		 * initializes the display when it's loaded, and then the X driver uses
		 * a different port order in its DC (and a dsp_shutdown() frees this,
		 * followed by an dsp_alloc_pipe_planes() which needs to
		 * allocate a new cursor).
		 * That's because the primary and secondary ports/planes switch, and
		 * plane->fb_info in this context becomes cursor->cursor_info in
		 * dsp_alloc_pipe_planes(), and it must be NULL so that a new
		 * cursor is allocated (otherwise the old memory location is re-used,
		 * even though the
		 * memory was freed and reallocated for another purpose):
		 */
		if ((IGD_PLANE_CURSOR & plane->plane_features) == IGD_PLANE_CURSOR){
			plane->fb_info = NULL;
		}
	}

	EMGD_DEBUG("De-allocating plane 0x%lx", plane->plane_reg);
	plane->inuse = 0;
} /* end free_plane() */

/*!
 * Take a DC and configure the primary and secondary display handles.
 * The two display handles are returned to the caller.
 *
 * It should be valid to use the 0/1 indexes for pipes & planes here
 * because we should never get a DC that isn't valid. So if the hardware
 * can't support extended, and extended DC will never show up.
 *
 * @param context
 * @param dc
 * @param flags
 *
 * @return 0: success.
 * @return -IGD_INVAL: No valid DC list can be returned.
 */
static int dsp_alloc_pipe_planes(igd_context_t *context,
	unsigned long dc,
	unsigned long flags)
{
	igd_display_plane_t *plane = NULL, *plane2 = NULL, *temp = NULL;
	igd_display_plane_t **plane_tbl;
	igd_cursor_t *cursor = NULL, *cursor2 = NULL;
	igd_cursor_t *cursora = NULL, *cursorb = NULL;
	igd_display_pipe_t *pipe = NULL;
	igd_display_port_t *port = NULL;
	unsigned short port_number;
	int swap_plane = 0;
	int secondary_pipe = 1;
	int i;

	/* Free any allocated pipes */
	if (dsp_context.display_list[0].igd_pipe) {
		dsp_context.display_list[0].igd_pipe = 0;
	}
	if (dsp_context.display_list[1].igd_pipe) {
		dsp_context.display_list[1].igd_pipe = 0;
	}

	/* Set up planes... */
	plane_tbl = dsp_context.dispatch->planes;
	while(*plane_tbl) {
		(*plane_tbl)->inuse = 0;

		if ((IGD_PLANE_CURSOR & (*plane_tbl)->plane_features) ==
				IGD_PLANE_CURSOR) {
			if ((cursora == NULL) &&
				(IGD_CURSOR_USE_PIPEA & (*plane_tbl)->plane_features)) {
				cursora = (igd_cursor_t *)(*plane_tbl);
				cursor = cursora;
			} else if (cursorb == NULL) {
				cursorb = (igd_cursor_t *)(*plane_tbl);
				cursor2 = cursorb;
			}
		} else {
			if (plane == NULL) {
				plane = *plane_tbl;
				/* plane->mirror = NULL; */
			} else if (plane2 == NULL) {
				plane2 = *plane_tbl;
				/* plane2->mirror = NULL; */
			}
		}
		plane_tbl++;
	}


	/*
	 * Get the first port from the DC and hook it up as master.
	 * FIXME:
	 *   If this fails becaue the display isn't attached, and there
	 *   are more choices, this should move on and try the other
	 *   choices, maybe?
	 */
	port_number = (unsigned short)((dc & 0x000000f0) >> 4);
	dsp_context.display_list[0].ports[0] = port_number;
	dsp_get_display_port(port_number, &port, 0);
	if(!port){
		EMGD_ERROR("Error, Cant get port from DC for pipe-share-bits"
				" - will use first pipe!");
	}

	if (port) {
		/*
		 * Allocate a pipe for this display. Depending on the port
		 * requirements this could be either pipe.
		 */
		if (port->port_features & IGD_PORT_USE_PIPEA) {
			pipe = dsp_context.dispatch->pipes[0];
			cursor = cursora;
			cursor2 = cursorb;
			secondary_pipe = 1;
			if(plane->plane_features & IGD_PLANE_USE_PIPEB){
				swap_plane = 1;
			}
		} else if (port->port_features & IGD_PORT_USE_PIPEB) {
			pipe = dsp_context.dispatch->pipes[1];
			cursor2 = cursora;
			cursor = cursorb;
			secondary_pipe = 0;
			if(plane->plane_features & IGD_PLANE_USE_PIPEA){
				swap_plane = 1;
			}
		} else {
			EMGD_ERROR("Port numbers from DC cant work with any pipe!"
					"Using Pipes in order!");
			return -IGD_INVAL;
		}
		if(swap_plane){
			temp = plane;
			plane = plane2;
			plane2 = temp;
		}

		if(pipe == NULL){
			EMGD_ERROR("Port numbers from DC didn't work with any pipe!!");
			return -IGD_INVAL;
		}

		pipe->inuse = 0;
		/* set up the display handle */
		dsp_context.display_list[0].igd_plane = (void *)plane;
		dsp_context.display_list[0].igd_cursor = (void *)cursor;
		dsp_context.display_list[0].igd_pipe = (void *)pipe;
		dsp_context.display_list[0].igd_context = context;
		dsp_context.display_list[0].ports[0] = port_number;
		pipe->inuse = 1;

		if (cursor) {
			cursor->inuse = 1;
			if (cursor->cursor_info == NULL) {
				cursor->cursor_info = OS_ALLOC(sizeof(igd_cursor_info_t));
				if(!cursor->cursor_info) {
					EMGD_ERROR("Error, memory allocation for cursor_info "
							"failed.");
					cursor->inuse = 0;
				} else {
					if (dsp_init_cursor(context, cursor) != 0) {
						cursor->inuse = 0;
					}
				}
			}
		}
	}

	if (dsp_context.display_flags & IGD_DISPLAY_HOTPLUG) {
		/* Check if it is SINGLE, set the DC to temporarily acts as EXTENDED
		 * to make it falls to IGD_DISPLAY_CONFIG_EXTENDED.
		 *
		 */
		if ((dc & 0x0000000f) & IGD_DISPLAY_CONFIG_SINGLE) {
			dc |= IGD_DISPLAY_CONFIG_EXTENDED;
			dc &= ~(IGD_DISPLAY_CONFIG_SINGLE);
		}
	}

	/* Set up secondary. How it is set up depends on the display config */
	switch (dc & 0x0000000f) {
	case IGD_DISPLAY_CONFIG_SINGLE:
	case IGD_DISPLAY_CONFIG_TWIN:
	case 0:
		/*
		 * If display 1 was previouslly allocated, then we were in either
		 * clone or extended. In either case, we just want to make sure
		 * the display is no longer allocated and any clone mirror links
		 * are removed.
		 *
		 * Note: This doesn't reduce any reference counts or free any
		 * resources allocated to display 1.
		 */
		if (dsp_context.display_list[1].igd_pipe) {
			dsp_context.display_list[1].igd_pipe = NULL;
			dsp_context.display_list[1].igd_plane = NULL;
			dsp_context.display_list[1].igd_cursor = NULL;

			/* If the cursor and/or fb were cloned, break the clone */
			if (cursor->mirror != NULL) {
				cursor->mirror = NULL;
				cursor2->mirror = NULL;
				cursor2->cursor_info = NULL;
			}

			/* remove the plane mirroring */
			if (plane->mirror != NULL) {
				plane->mirror = NULL;
				plane2->mirror = NULL;
			}
		}
		break;
	case IGD_DISPLAY_CONFIG_CLONE:
		/*
		 * When switching to clone we need to free any resources allocated
		 * to the second display.  This can happen if we were ever in
		 * extended mode prior to this.
		 *
		 * NOTE:
		 *   This then needs to fall through to the extended branch because
		 *   the rest of the set up is the same. DO NOT ADD A BREAK STATEMENT!
		 */

		/*
		 * If display_list[1].plane->fb_info exist and the plane_info
		 * reference count is 1, then this was allocated as an independent
		 * plane.  The surface must be freed and the plane_info record reset.
		 *
		 * The same logic applies to the secondary cursor info.  Does this
		 * imply that the display_list needs to have a cursor pointer too?
		 * What about moving the cursor from the pipe to the display?
		 */
		if (dsp_context.display_list[1].igd_plane && !plane2->mirror) {
			free_plane(context, dsp_context.display_list[1].igd_plane);
		}
		if (dsp_context.display_list[1].igd_cursor && !cursor2->mirror) {
			free_plane(context, (igd_display_plane_t *)cursor2);
		}

		/* Mirror the cursor and framebuffer to the second display */
		plane->mirror = plane2;
		plane2->mirror = plane;
		/*
		 * Note: plane_info points to a static data structure so we have
		 * to get the original pointer back when going into extended.
		 */
		plane2->fb_info = plane->fb_info;

		/*
		 * WinCE VEXT uses clone mode, but needs indpendent cursors. Thus,
		 * we don't want to do this mirroring in this one specific case.
		 */
		cursor->mirror = cursor2;
		cursor2->mirror = cursor;
		cursor2->cursor_info = cursor->cursor_info;

	case IGD_DISPLAY_CONFIG_EXTENDED:
		if (plane2) {

#ifndef CONFIG_MICRO
			/* This condition is always true, not sure why this check is
			 * required */
			if (!(dc & IGD_DISPLAY_CONFIG_CLONE)) {
				/* If this is really extended and not clone, break mirrors */
				plane->mirror = NULL;
				plane2->mirror = NULL;
				cursor->mirror = NULL;
				cursor2->mirror = NULL;
				if (cursor2->cursor_info == cursor->cursor_info) {
					cursor2->cursor_info = NULL;
				}
			}
#endif

			port_number = IGD_DC_SECONDARY(dc);
			dsp_context.display_list[1].igd_plane = (void *)plane2;
			dsp_context.display_list[1].igd_context = context;
			dsp_context.display_list[1].igd_cursor = (void *)cursor2;
			/* Allocate which ever pipe wasn't used above */
			pipe = dsp_context.dispatch->pipes[secondary_pipe];

			if (pipe) {
				pipe->inuse = 1;
				dsp_context.display_list[1].igd_pipe = (void *)pipe;
				dsp_get_display_port(port_number, &port, 0);
				if (port) {
					dsp_context.display_list[1].ports[0] = port_number;
				} else {
					EMGD_ERROR("Failed to get port %d", port_number);
					dsp_context.display_list[1].ports[0] = 0;
				}

#ifndef CONFIG_MICRO
				if (cursor2) {
					cursor2->inuse = 1;
					if (cursor2->cursor_info == NULL) {
						cursor2->cursor_info =
							OS_ALLOC(sizeof(igd_cursor_info_t));
						if(!cursor2->cursor_info) {
							EMGD_ERROR("Error, memory allocation for cursor_info "
									"failed.");
							cursor2->inuse = 0;
						} else {
							if (dsp_init_cursor(context, cursor2) != 0) {
								cursor2->inuse = 0;
							}
						}
					}
				}
#endif
			} else {
				EMGD_ERROR("Failed to get pipe #1");
			}
		} else {
			EMGD_ERROR("Plane #1 not available");
		}
		break;
	default:
		EMGD_DEBUG("DC has an invalid display config!  0x%08lx", dc);
		break;
	}

	/* Assign all twin'd ports to the proper displays */
	for (i = 7; i > 1; i--) {
		port_number = DC_PORT_NUMBER(dc, i);
		if ((port_number) && (i != 5)) { /* Skip owner ports, handled above */
			dsp_get_display_port(port_number, &port, 0);
			if (i < 5) {
				if (port) {
					dsp_context.display_list[0].ports[i-1] = port_number;
				} else {
					dsp_context.display_list[0].ports[i-1] = 0;
				}
			} else {
				if (port) {
					dsp_context.display_list[1].ports[i-5] = port_number;
				} else {
					dsp_context.display_list[1].ports[i-5] = 0;
				}
			}
		}
	}

	/*
	 * DC doesn't have a bit to assign a 4th port to the second CRTC.
	 * Thus, it must always be unused.
	 */
	dsp_context.display_list[1].ports[3] = 0;

	/*
	 * FIXME: dsp_alloc_pipe_planes setting current_dc during pipe allocation
	 * only - assuming port numbers will be correct later
	 */
	dsp_context.current_dc = dc;
	return 0;
}

static void dsp_free_pipe_planes(igd_context_t *context,
	igd_display_context_t *emgd_crtc)
{
	int i;

	for(i = 0; i < MAX_DISPLAYS; i++) {
		if(emgd_crtc == &(dsp_context.display_list[i])){
			emgd_crtc->igd_pipe->inuse = 0;
			emgd_crtc->igd_pipe = NULL;
			free_plane(context, emgd_crtc->igd_plane);
			emgd_crtc->igd_plane = NULL;
			emgd_crtc->igd_cursor->inuse = 0;
			emgd_crtc->igd_cursor = NULL;
		}
	}
}

static int dsp_alloc_port(igd_context_t *context, int port_number,
	unsigned long flags)
{
	igd_display_port_t * port = NULL;

	dsp_get_display_port(port_number, &port, 0);
	if (!port) {
		EMGD_ERROR("Error, cant find valid port based on port number!");
		return -1;
	}

	if(port){
		if(0x80 == port->inuse){
			EMGD_ERROR("Error, requested port %d - unavailable!", port_number);
			return -1;
		} else if(1 == port->inuse){
			EMGD_ERROR("Error, requested port %d - already used!", port_number);
			return -1;
		} else {
			port->inuse = 1;
		}
	}

	return 0;
}

static void dsp_free_port(igd_context_t *context, int port_number)
{
	igd_display_port_t * port = NULL;

	dsp_get_display_port(port_number, &port, 0);

	if(port) {
		if(0x80 == port->inuse) {
			EMGD_ERROR("Error, requested port %d - unavailable!", port_number);
			return;
		} else {
			port->inuse = 0;
		}
	} else {
		EMGD_ERROR("Error, cant find requested port %d!", port_number);
	}

	return ;
}

/*!
 * This function returns the list of available pixel formats for the
 * framebuffer and cursor if the pointers are not NULL.
 *
 * @param display_handle A igd_display_h type returned from a previous
 * igd_alloc_display call.
 * @param fb_list_pfs Returns the list of pixel formats for framebuffer.
 * @param cu_list_pfs Returns the list of pixel formats for the cursor.
 * Both of the above lists ends with 0. Dframebuffer and cursor if the
 * pointers are not NULL.
 * @param overlay_pfs
 * @param render_pfs
 * @param texture_pfs
 *
 * @return 0 on success
 * @return -IGD_INVAL on failure
 */
static int igd_get_pixelformats(igd_display_h display_handle,
	unsigned long **fb_list_pfs, unsigned long **cu_list_pfs,
	unsigned long **overlay_pfs, unsigned long **render_pfs,
	unsigned long **texture_pfs)
{
	igd_display_context_t *display = (igd_display_context_t *)display_handle;

	if (!display || !PLANE(display) || !PIPE(display)) {
		return -IGD_INVAL;
	}

	if (fb_list_pfs) {
		*fb_list_pfs = PLANE(display)->pixel_formats;
	}
	if (cu_list_pfs) {
		*cu_list_pfs = CURSOR(display)->pixel_formats;
	}
	if (overlay_pfs) {
		*overlay_pfs = dsp_context.dispatch->overlay_pfs;
	}
	if (render_pfs) {
		*render_pfs = dsp_context.dispatch->render_pfs;
	}
	if (texture_pfs) {
		*texture_pfs = dsp_context.dispatch->texture_pfs;
	}

	return 0;

} /* end igd_get_pixelformats() */

/*!
 * Browse through the list of displays and frees the display.
 *
 * @param context
 *
 * @return void
 */
void dsp_shutdown(igd_context_t *context)
{
	int i;
	igd_display_port_t *port = NULL;
	igd_display_plane_t *plane = NULL;
	igd_display_pipe_t *temp_pipe = NULL;

	EMGD_DEBUG("dsp_shutdown Entry");

	/*
	 * Free all the ports pt_info's. Need to add this here because there
	 * is no longer a 1-to-1 relationship between displays and ports.
	 */
	while ((port = dsp_get_next_port(context, port, 0)) != NULL) {
		if (port->pt_info != NULL) {
			OS_FREE(port->pt_info);
			port->pt_info = NULL;
		}
		if (port->dtd_list != NULL) {
			OS_FREE(port->dtd_list);
			port->dtd_list = NULL;
		}
		if (port->attr_list != NULL) {
			if (port->attr_list->attr != NULL) {
				OS_FREE(port->attr_list->attr);
			}
			OS_FREE(port->attr_list);
			port->attr_list = NULL;
		}
	}


	/* Free pipes, cursors, and any allocated command queues */
	while ((temp_pipe = dsp_get_next_pipe(context, temp_pipe, 0)) != NULL) {
		/* probably not needed since shutting down */
		temp_pipe->inuse = 0;
		temp_pipe->timing = NULL;
	}

	/* Free planes */
	while ((plane = dsp_get_next_plane(context, plane, 0)) != NULL) {
		if (plane->fb_info) {
			free_plane(context, plane);
		}
	}

	/* Clear display list */
	for(i=0; i<MAX_DISPLAYS; i++) {
		if(dsp_context.display_list[i].igd_pipe) {
			OS_MEMSET(&dsp_context.display_list[i], 0,
				sizeof(igd_display_context_t));
		}
	}

	if (dsp_context.dsp_dc_list.dc_list != NULL) {
		OS_FREE(dsp_context.dsp_dc_list.dc_list);
		dsp_context.dsp_dc_list.dc_list = NULL;
	}

	return;
} /* end dsp_shutdown() */

/*!
 * Does the initialization of dsp module including initializing
 * unset values in the plane, pipe and port tables.
 *
 * @param context
 *
 * @return 0: success.
 * @return -IGD_ERROR_NODEV
 */
int dsp_init(igd_context_t *context)
{
	igd_display_params_t *display_params;
	igd_display_plane_t  **plane;
	igd_display_pipe_t   **pipe;
	unsigned long        i;
	unsigned long        hal_attr_index, init_attr_index;
	igd_param_t   *params = context->module_dispatch.init_params;
	int ret;

	EMGD_DEBUG("Enter dsp_init()");

	OS_MEMSET(&dsp_context, 0, sizeof(dsp_context));

	/* Hook up plane, pipe, port, cursor lists here */
	dsp_context.dispatch = (dsp_dispatch_t *)dispatch_acquire(context,
		dsp_dispatch_list);
	if(!dsp_context.dispatch) {
		return -IGD_ERROR_NODEV;
	}

	dsp_context.context = context;

	if(dsp_context.dispatch->dsp_init) {
		ret = dsp_context.dispatch->dsp_init(context);
		if (ret) {
			return ret;
		}
	}

	/* Hook up top-level dispatch functions */
	context->drv_dispatch.query_dc                = igd_query_dc;
	context->drv_dispatch.get_pixelformats        = igd_get_pixelformats;

	/* Hook up inter-module dispatch functions */
	context->module_dispatch.dsp_get_config_info  = dsp_get_config_info;

	context->module_dispatch.dsp_get_next_plane   = dsp_get_next_plane;
	context->module_dispatch.dsp_get_next_pipe    = dsp_get_next_pipe;
	context->module_dispatch.dsp_get_next_port    = dsp_get_next_port;
	context->module_dispatch.dsp_get_dc           = dsp_get_dc;

	context->module_dispatch.dsp_alloc_pipe_planes = dsp_alloc_pipe_planes;
	context->module_dispatch.dsp_get_display_crtc = dsp_get_display_crtc;
	context->module_dispatch.dsp_free_pipe_planes = dsp_free_pipe_planes;
	context->module_dispatch.dsp_get_planes_pipes = dsp_get_planes_pipes;

	context->module_dispatch.dsp_alloc_port       = dsp_alloc_port;
	context->module_dispatch.dsp_get_display_port = dsp_get_display_port;
	context->module_dispatch.dsp_free_port        = dsp_free_port;

	context->module_dispatch.dsp_shutdown         = dsp_shutdown;

	/* Dsp data members in inter module dispatch. This is done to make
	 * these data members available for vBIOS after init when dsp is
	 * discarded. */
	context->module_dispatch.dsp_current_dc       = &dsp_context.current_dc;
	context->module_dispatch.dsp_port_list        = dsp_context.port_list;

	dsp_context.display_flags = params->display_flags;

	/* TODO: In future, we should interrogate the display_flags param
	 * and see if we need to account for a new IGD_VBT_DETECT to interrogate
	 * VBT table from VBIOS or firmware to extract the port_order from
	 * there. If that exists, then we override the port order here.
	 */

	/* Fill the number of planes and number of pipes in mode_context */
	plane = dsp_context.dispatch->planes;
	pipe  = dsp_context.dispatch->pipes;

	i=0;
	while (*plane) {
		if ((*plane)->plane_features & IGD_PLANE_DISPLAY) {
			/*
			 * Initialize the offsets of the frame buffer to zero.
			 */
			(*(igd_display_plane_t **)plane)->fb_info = NULL;
			dsp_context.num_dsp_planes++;
		}
		plane++;
	}

	while (*pipe) {
		dsp_context.num_dsp_pipes++;
		pipe++;
	}

	dsp_context.dsp_dc_list.dc_list = NULL;

	/* Initialize port table with user specified display parameters. */
	do_port_order(context, params->port_order);

	for (i = 1, display_params = params->display_params; i <= 5; i++,
			 display_params++) {
		unsigned long temp;
		igd_display_port_t *port = NULL;

		dsp_get_display_port((unsigned short)display_params->port_number, &port, 0);
		if (!port) {
			/* If the port number is unknown/undefined,
			 * then skip this parameter */
			continue;
		}

		/* process present_params flags */
		if (IGD_PARAM_DDC_GPIO & display_params->present_params) {
			temp = display_params->ddc_gpio;
			if (temp >= IGD_PARAM_GPIO_PAIR_MAXTYPES) {
				EMGD_DEBUG("Invalid GPIO pin pair %ld specified for DDC.",
						temp);
			} else {
				port->ddc_reg = temp;
			}
		}

		if (IGD_PARAM_DDC_SPEED & display_params->present_params) {
			temp = display_params->ddc_speed;
			if (temp > 400 || temp < 10) {
				EMGD_DEBUG("DDC speed %ld is outside [10-400KHz] range.",
						temp);
			} else {
				port->ddc_speed = temp;
			}
		}

		if (IGD_PARAM_DDC_DAB & display_params->present_params) {
			port->ddc_dab = display_params->ddc_dab;
		}

		if (IGD_PARAM_I2C_GPIO & display_params->present_params) {
			temp = display_params->i2c_gpio;
			if (temp >= IGD_PARAM_GPIO_PAIR_MAXTYPES) {
				EMGD_DEBUG("Invalid GPIO pin pair %ld specified for I2C.",
						temp);
			} else {
				port->i2c_reg = temp;
			}
		}

		if (IGD_PARAM_I2C_SPEED & display_params->present_params) {
			temp = display_params->i2c_speed;
			if (temp > 400 || temp < 10) {
				EMGD_DEBUG("I2C speed %ld is outside [10-400KHz] range.",
						temp);
			} else {
				port->i2c_speed = temp;
			}
		}
		if (IGD_PARAM_DAB & display_params->present_params) {
			port->i2c_dab = display_params->i2c_dab;
		}

		if (IGD_PARAM_FP_INFO & display_params->present_params) {
			port->fp_info = (igd_param_fp_info_t *)
				OS_ALLOC(sizeof(igd_param_fp_info_t));
			if (NULL != port->fp_info) {
				OS_MEMCPY(port->fp_info, &display_params->fp_info,
					sizeof(igd_param_fp_info_t));
				EMGD_DEBUG("IGD_PARAM_FP_INFO: FP width %ld height %ld",
					port->fp_info->fp_width, port->fp_info->fp_height);
			} else {
				EMGD_DEBUG("IGD_PARAM_FP_INFO: allocation of igd_param_fp_info_t "
						"struct failed");
			}
		}

		if (IGD_PARAM_DTD_LIST & display_params->present_params) {
			port->dtd_list = OS_ALLOC(sizeof(igd_param_dtd_list_t));
			if (NULL != port->dtd_list) {
				OS_MEMCPY(port->dtd_list, &display_params->dtd_list,
						sizeof(*port->dtd_list));
			} else {
				EMGD_DEBUG("IGD_PARAM_DTD_LIST: allocation of "
						"igd_param_dtd_list_t struct failed");
			}
		}

		if (IGD_PARAM_ATTR_LIST & display_params->present_params) {
			port->attr_list = OS_ALLOC(sizeof(igd_param_attr_list_t));
			if (NULL != port->attr_list) {
				OS_MEMCPY(port->attr_list, &display_params->attr_list,
						sizeof(*port->attr_list));
				/* Now allocate memory for attributes */
				port->attr_list->attr = OS_ALLOC(sizeof(igd_param_attr_t) *
								port->attr_list->num_attrs);
				if (NULL != port->attr_list->attr) {
					OS_MEMCPY(port->attr_list->attr,
						display_params->attr_list.attr,
						sizeof(igd_param_attr_t) * port->attr_list->num_attrs);
				}
			} else {
				EMGD_DEBUG("IGD_PARAM_ATTR_LIST: allocation of "
						"igd_param_attr_list_t struct failed");
			}

			if ((NULL != port->attr_list) && (NULL != port->attr_list->attr)) {
				/* Initialize HAL's attributes */
				for( init_attr_index = 0;
						init_attr_index < port->attr_list->num_attrs;
						init_attr_index++ ) {

					hal_attr_index = 0;

					while (PD_ATTR_LIST_END != port->attributes[hal_attr_index].id) {
						if (port->attributes[hal_attr_index].id ==
							port->attr_list->attr[init_attr_index].id) {

							port->attributes[hal_attr_index].current_value =
								port->attr_list->attr[init_attr_index].value;
						}
						hal_attr_index++;
					}
				} /* for: initialize HAL's attributes */
			}
		}
	}

	/*
	 * Build the list of valid display configurations.
	 */
	dsp_dc_init(context);

	return 0;
} /* end dsp_init() */

/*!
 * Given a newly allocated cursor_info record, allocate the
 * surfaces needed for both the xor and argb cursor images. Also
 * fill in the cursor_info as much as possible.
 *
 * @param context
 * @param cursor
 *
 * @return 0 on success
 * @return -IGD_ERROR_NOMEM on failure
 */
static int dsp_init_cursor(igd_context_t *context, igd_cursor_t *cursor)
{
	unsigned long buffer = 0;
	void* cursor_mem = NULL;
	unsigned int width = 64;
	unsigned int height = 64;
	unsigned int pitch = 0;
	unsigned long size = 0;
	unsigned long flags = IGD_SURFACE_CURSOR;
	int ret;
	int has_rgb32 = 0;
	unsigned long *tmp;

	tmp = (unsigned long *)cursor->pixel_formats;
	while (*tmp) {
		if (*tmp == IGD_PF_ARGB32) {
			has_rgb32 = 1;
			break;
		}
		tmp++;
	}

	OS_MEMSET(cursor->cursor_info, 0, sizeof(igd_cursor_info_t));

	if (has_rgb32) {
		/* ARGB32 is used for any 32bit format. */

		/* FIXME GEM: need to call GEM to allocate actual cursor surface */
		/* buffer = gem_alloc_bo? */
		EMGD_DEBUG("NOT Allocating GEM cursor surface-argb !!!!");
		cursor->cursor_info->argb_offset = buffer;
		cursor->cursor_info->argb_pitch = pitch;

		/* FIXME GEM: need to get the physical address in RAM? */
		/* Actually for Gen6 and Gen7, we dont need this?*/
		/* buffer_phys = gem_get_phys_dram_from_bo */

		/* FIXME GEM: need to memory map the cursor surface */
		/* cursor_mem = mmap??? GMMADR??? - leave it NULL for now so we catch this*/
		cursor_mem = NULL;
		if(cursor_mem){
			OS_MEMSET(cursor_mem, 0, size);
		}
		cursor->cursor_info->argb_offset = buffer;
		cursor->cursor_info->argb_pitch = pitch;
	}
	flags = IGD_SURFACE_CURSOR;

	/* FIXME - need to call GEM to allocate actual cursor surface */
	/* buffer = gem_alloc_bo? */
	EMGD_DEBUG("NOT Allocating GEM cursor surface-xor !!!!");
	cursor->cursor_info->xor_pitch = pitch;
	cursor->cursor_info->xor_offset = buffer;
	ret = 0;
	if (ret) {
		if(has_rgb32) {
			/* FIXME GEM: call GEM to free the earlier argb buffer */
			cursor->cursor_info->argb_offset = 0;
		}
		EMGD_ERROR("ERROR: No memory for XOR cursor!");
		return -IGD_ERROR_NOMEM;
	}

	/* FIXME GEM: need to get the physical address in RAM? */
	/* Actually for Gen6 and Gen7, we dont need this?*/
	/* buffer_phys = gem_get_phys_dram_from_bo */

	/*
	 * FIXME GEM: need to memory map the cursor surface
	 * cursor_mem = mmap??? GMMADR??? - leave it NULL for now so we catch this
	 */
	cursor_mem = NULL;
	if(cursor_mem){
		OS_MEMSET(cursor_mem, 0, size);
	}

	/* Set the dsp cursor resource table */
	cursor->cursor_info->width = width;
	cursor->cursor_info->height = height;
	cursor->cursor_info->pixel_format =
		(has_rgb32)?IGD_PF_ARGB32:IGD_PF_RGB_2;

	return 0;
}


