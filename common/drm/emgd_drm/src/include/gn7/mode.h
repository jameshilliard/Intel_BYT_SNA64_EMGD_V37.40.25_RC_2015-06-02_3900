/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: mode.h
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
 *  Inter-module mode header file.
 *-----------------------------------------------------------------------------
 */

#ifndef _MODE_VLV_H_
#define _MODE_VLV_H_

/* FIXME: This file has not been checked for Gen7. */


/*
 * Declarations
 */
int igd_alter_cursor_pos_vlv(igd_display_h display_handle,
	igd_cursor_info_t *cursor_info);
int igd_set_palette_entry_vlv(igd_display_h display_handle,
	unsigned long palette_color, unsigned long palette_entry);
int igd_get_palette_entry_vlv(igd_display_h display_handle,
	unsigned long palette_entry, unsigned long *palette_color);
int igd_set_palette_entries_vlv(igd_display_h display_handle,
	unsigned long *palette_colors, unsigned int start_index,
	unsigned int count);
int igd_wait_vblank_vlv(igd_display_h display_handle);
void wait_for_vblank_vlv(unsigned char *mmio,
	unsigned long pipe_reg);
int igd_wait_vsync_vlv(igd_display_h display_handle);
int mode_get_stride_vlv(igd_display_context_t *display,
	unsigned long *stride,
	unsigned long flags);
int igd_query_in_vblank_vlv(igd_display_h display_handle);
int igd_get_scanline_vlv(igd_display_h display_handle, int *scanline);
int flip_display_vlv(igd_display_h display_handle, int priority);
void program_plane_vlv(igd_display_context_t *display,
	unsigned long status);
void program_pipe_vlv(igd_display_context_t *display,
	unsigned long status);
void reset_plane_pipe_ports_vlv(igd_context_t *context);
int program_port_vlv(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int post_program_port_vlv(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_analog_vlv(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_sdvo_vlv(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_intv_vlv(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);		/* For Integrated TV */
unsigned long get_gpio_sets_vlv(unsigned long **);
void program_cursor_vlv(igd_display_context_t *display,
	unsigned long status);
int set_display_base_vlv(igd_display_context_t *display,
		igd_framebuffer_info_t *fb, unsigned long *x, unsigned long *y);

int igd_get_surface_vlv(igd_display_h display_handle,
	igd_buffertype_t type,
	igd_surface_t *surface,
	igd_appcontext_h appcontext);
int igd_set_surface_vlv(igd_display_h display_handle,
	int priority,
	igd_buffertype_t type,
	igd_surface_t *surface,
	igd_appcontext_h appcontext,
	unsigned long flags);
int igd_query_event_vlv(igd_display_h display_handle,
	igd_event_t event, unsigned long *status);
int fp_power_vlv(igd_context_t *context, igd_display_port_t *port,
	unsigned long power_state);
int bl_power_vlv(igd_context_t *context, igd_display_port_t *port,
	unsigned long power_state);
int get_bl_power_vlv(igd_context_t *context, igd_display_port_t *port,
	unsigned long *power_state);
int program_clock_vlv(igd_display_context_t *display,
	igd_clock_t *clock, unsigned long dclk,
	unsigned long status, unsigned short operation);
void filter_modes_vlv(igd_context_t *context, pd_timing_t *in_list);
int igd_set_color_correct_vlv(igd_display_context_t *display);



#ifdef CONFIG_MODE
#define IGD_SETUP_DISPLAY_VLV       NULL
#define IGD_ALTER_CURSOR_POS_VLV    igd_alter_cursor_pos_vlv
#define IGD_SET_PALETTE_ENTRIES_VLV igd_set_palette_entries_vlv
#define IGD_WAIT_VSYNC_VLV          igd_wait_vsync_vlv
#define IGD_QUERY_IN_VBLANK_VLV     igd_query_in_vblank_vlv
#define IGD_GET_SCANLINE_VLV        igd_get_scanline_vlv
#define SET_DISPLAY_BASE_VLV        set_display_base_vlv
#define PROGRAM_CURSOR_VLV          program_cursor_vlv
#define IGD_GET_SURFACE_VLV         igd_get_surface_vlv
#define IGD_SET_SURFACE_VLV         igd_set_surface_vlv
#define IGD_QUERY_EVENT_VLV         igd_query_event_vlv
#define GET_BL_POWER_VLV            get_bl_power_vlv
#define IGD_SET_COLOR_CORRECT_VLV   igd_set_color_correct_vlv
#else
#define IGD_SETUP_DISPLAY_VLV       NULL
#define IGD_ALTER_CURSOR_POS_VLV    NULL
#define IGD_SET_PALETTE_ENTRIES_VLV NULL
#define IGD_WAIT_VSYNC_VLV          NULL
#define IGD_QUERY_IN_VBLANK_VLV     NULL
#define IGD_GET_SCANLINE_VLV        NULL
#define SET_DISPLAY_BASE_VLV        NULL
#define PROGRAM_CURSOR_VLV          NULL
#define IGD_GET_SURFACE_VLV         NULL
#define IGD_SET_SURFACE_VLV         NULL
#define IGD_QUERY_EVENT_VLV         NULL
#define GET_BL_POWER_VLV            NULL
#define IGD_SET_COLOR_CORRECT_VLV   NULL
#endif

#endif /* _MODE_VLV_H_ */
