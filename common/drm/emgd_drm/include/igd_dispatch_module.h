/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_inter_dispatch.h
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
 *
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPATCH_MODULE_H
#define _IGD_DISPATCH_MODULE_H

typedef struct _igd_context igd_context_t;

/*
 * Intermodule-dispatch is for calling from one module to another. All
 * functions should be considered optional and the caller should check
 * for null before using.
 */
typedef struct _igd_module_dispatch {
	struct _igd_param *init_params;


	/***********************************************
	 *
	 *             DSP Module interfaces
	 *
	 ***********************************************/
	int (*dsp_get_config_info)(igd_context_t *context,
		igd_config_info_t *config_info);
	void (*dsp_get_dc)(unsigned long *dc,
		igd_display_context_t **primary,
		igd_display_context_t **secondary);
	igd_display_port_t *(*dsp_get_next_port)(igd_context_t *context,
		igd_display_port_t *port, int reverse);
	igd_display_plane_t *(*dsp_get_next_plane)(igd_context_t *context,
		igd_display_plane_t *plane, int reverse);
	igd_display_pipe_t *(*dsp_get_next_pipe)(igd_context_t *context,
		igd_display_pipe_t *pipe, int reverse);
	int (*dsp_alloc_pipe_planes)(igd_context_t *context, unsigned long dc,
		unsigned long flags);
	void (*dsp_get_display_crtc)(unsigned short pipeline_index,
		igd_display_context_t **emgd_crtc);
	void (*dsp_free_pipe_planes)(igd_context_t *context, igd_display_context_t *emgd_crtc);
	void (*dsp_get_planes_pipes)(igd_display_plane_t **primary_display_plane,
		igd_display_plane_t **secondary_display_plane,
		igd_display_pipe_t  **primary_pipe,
		igd_display_pipe_t  **secondary_pipe);
	int (*dsp_alloc_port)(igd_context_t *context, int port_number,
		unsigned long flags);
	void (*dsp_get_display_port)(unsigned short port_number,
		igd_display_port_t **port, int display_detect);
	void (*dsp_free_port)(igd_context_t *context, int port_number);
	void (*dsp_shutdown)(igd_context_t *context);

	igd_display_port_t **dsp_port_list;
	/* Currently programmed DC. Info accessible by the mode module.*/
	unsigned long *dsp_current_dc;
	/* Firmware programmed DC. Info accessible by the mode module.*/
	unsigned long dsp_fw_dc;

	/***********************************************
	 *
	 *             MODE Module interfaces
	 *
	 ***********************************************/
	void (*mode_reset_plane_pipe_ports)(igd_context_t *context);
	void (*mode_filter_modes)(igd_context_t *context,
			igd_display_port_t *port, pd_timing_t *in_list);
	int (*mode_save)(igd_context_t *context, module_state_h *state,
		unsigned long *flags);
	int (*mode_restore)(igd_context_t *context, module_state_h *state,
		unsigned long *flags);
	void (*mode_shutdown)(igd_context_t *context);


	/***********************************************
	 *
	 *             PI Module interfaces
	 *
	 ***********************************************/
	int (*pi_get_config_info)(igd_context_t *context,
		igd_config_info_t *config_info);
	int (*pi_i2c_read_regs)(igd_context_t *context,
		igd_display_port_t * port,
		unsigned long i2c_bus, unsigned long i2c_speed,
		unsigned long dab, unsigned char reg,
		unsigned char *buffer, unsigned long num_bytes);
	int (*pi_i2c_write_reg_list)(igd_context_t *context,
		igd_display_port_t * port,
		unsigned long i2c_bus, unsigned long i2c_speed,
		unsigned long dab, struct _pd_reg *reg_list,
		unsigned long flags);
	void (*pi_shutdown)(igd_context_t *context);
	void (*convert_displayid_to_edid)(igd_display_port_t *port,
		unsigned char *edid_buf);
	int (*update_timing_table)(igd_display_port_t *port);

	/***********************************************
	 *
	 *             OVL Module interfaces
	 *
	 ***********************************************/
	void (*overlay_shutdown)(igd_context_t *context);


	/***********************************************
	 *
	 *             REG Module interfaces
	 *
	 ***********************************************/
	void *(*reg_alloc)(igd_context_t *context, unsigned long flags);
	void (*reg_free)(igd_context_t *context, void *reg_set);
	int (*reg_save)(igd_context_t *context, void *reg_set);
	int (*reg_restore)(igd_context_t *context, void *reg_set);
	int (*reg_get_mod_state)(reg_state_id_t id, module_state_h **state,
		unsigned long **flags);
	void (*reg_shutdown)(igd_context_t *context);


	/***********************************************
	 *
	 *             PWR Module interfaces
	 *
	 ***********************************************/
	void (*pwr_shutdown)(igd_context_t *context);
} igd_module_dispatch_t;

#endif /*_IGD_DISPATCH_MODULE_H*/

