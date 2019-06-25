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

#ifndef _MODE_SNB_H_
#define _MODE_SNB_H_

/* FIXME: This file has not been checked for Gen4. */


/*
 * Declarations
 */
int igd_alter_cursor_pos_snb(igd_display_h display_handle,
	igd_cursor_info_t *cursor_info);
int igd_set_palette_entry_snb(igd_display_h display_handle,
	unsigned long palette_color, unsigned long palette_entry);
int igd_get_palette_entry_snb(igd_display_h display_handle,
	unsigned long palette_entry, unsigned long *palette_color);
int igd_set_palette_entries_snb(igd_display_h display_handle,
	unsigned long *palette_colors, unsigned int start_index,
	unsigned int count);
void igd_wait_vblank_snb(igd_display_h display_handle);
void wait_for_vblank_snb(unsigned char *mmio,
	unsigned long pipe_reg);
int igd_wait_vsync_snb(igd_display_h display_handle);
int mode_get_stride_snb(igd_display_context_t *display,
	unsigned long *stride,
	unsigned long flags);
int igd_query_in_vblank_snb(igd_display_h display_handle);
int igd_get_scanline_snb(igd_display_h display_handle, int *scanline);
int flip_display_snb(igd_display_h display_handle, int priority);
void program_plane_snb(igd_display_context_t *display,
	unsigned long status);
void program_pipe_snb(igd_display_context_t *display,
	unsigned long status);
void reset_plane_pipe_ports_snb(igd_context_t *context);
int program_port_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int post_program_port_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_analog_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_lvds_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_sdvo_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);
int program_port_intv_snb(igd_display_context_t *display,
	unsigned short port_number,
	unsigned long status);		/* For Integrated TV */
unsigned long get_gpio_sets_snb(unsigned long **);
void program_cursor_snb(igd_display_context_t *display,
	unsigned long status);
int set_display_base_snb(igd_display_context_t *display,
		igd_framebuffer_info_t *fb, unsigned long *x, unsigned long *y);

int igd_get_surface_snb(igd_display_h display_handle,
	igd_buffertype_t type,
	igd_surface_t *surface,
	igd_appcontext_h appcontext);
int igd_set_surface_snb(igd_display_h display_handle,
	int priority,
	igd_buffertype_t type,
	igd_surface_t *surface,
	igd_appcontext_h appcontext,
	unsigned long flags);
int igd_query_event_snb(igd_display_h display_handle,
	igd_event_t event, unsigned long *status);
int fp_power_snb(igd_context_t *context, igd_display_port_t *port,
	unsigned long power_state);
int bl_power_snb(igd_context_t *context, igd_display_port_t *port,
	unsigned long power_state);
int get_bl_power_snb(igd_context_t *context, igd_display_port_t *port,
	unsigned long *power_state);
int program_clock_snb(igd_display_context_t *display,
	igd_clock_t *clock, unsigned long dclk,
	unsigned long status, unsigned short operation);
void filter_modes_snb(igd_context_t *context, pd_timing_t *in_list);
int igd_set_color_correct_snb(igd_display_context_t *display);



#ifdef CONFIG_MODE
#define IGD_SETUP_DISPLAY_SNB       NULL
#define IGD_ALTER_CURSOR_POS_SNB    igd_alter_cursor_pos_snb
#define IGD_SET_PALETTE_ENTRIES_SNB igd_set_palette_entries_snb
#define IGD_WAIT_VSYNC_SNB          igd_wait_vsync_snb
#define IGD_QUERY_IN_VBLANK_SNB     igd_query_in_vblank_snb
#define IGD_GET_SCANLINE_SNB        igd_get_scanline_snb
#define SET_DISPLAY_BASE_SNB        set_display_base_snb
#define PROGRAM_CURSOR_SNB          program_cursor_snb
#define IGD_GET_SURFACE_SNB         igd_get_surface_snb
#define IGD_SET_SURFACE_SNB         igd_set_surface_snb
#define IGD_QUERY_EVENT_SNB         igd_query_event_snb
#define GET_BL_POWER_SNB            get_bl_power_snb
#define IGD_SET_COLOR_CORRECT_SNB   igd_set_color_correct_snb
#else
#define IGD_SETUP_DISPLAY_SNB       NULL
#define IGD_ALTER_CURSOR_POS_SNB    NULL
#define IGD_SET_PALETTE_ENTRIES_SNB NULL
#define IGD_WAIT_VSYNC_SNB          NULL
#define IGD_QUERY_IN_VBLANK_SNB     NULL
#define IGD_GET_SCANLINE_SNB        NULL
#define SET_DISPLAY_BASE_SNB        NULL
#define PROGRAM_CURSOR_SNB          NULL
#define IGD_GET_SURFACE_SNB         NULL
#define IGD_SET_SURFACE_SNB         NULL
#define IGD_QUERY_EVENT_SNB         NULL
#define GET_BL_POWER_SNB            NULL
#define IGD_SET_COLOR_CORRECT_SNB   NULL
#endif

#endif /* _MODE_SNB_H_ */
