/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: ovl_cache_util.h
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
 *  Header file for utility functions and definitions for overlay caching support.
 *-----------------------------------------------------------------------------
 */
/* Flags for things that might have changed in the cache */
#define OVL_UPDATE_DEST     (1 << 0)
#define OVL_UPDATE_SRC      (1 << 1)
#define OVL_UPDATE_SURF     (1 << 2)
#define OVL_UPDATE_INFO     (1 << 3)
#define OVL_UPDATE_FLAGS    (1 << 4)
#define OVL_UPDATE_CC       (1 << 5)
#define OVL_UPDATE_GAMMA    (1 << 6)
#define OVL_UPDATE_COLORKEY (1 << 7)
#define OVL_UPDATE_SCALING  (1 << 8)
#define OVL_UPDATE_CONST_ALPHA (1 << 9)

int is_changed_rect(
		igd_rect_t *old_rect,
		igd_rect_t *new_rect);
int is_changed_surf(
		emgd_framebuffer_t *old_surf,
		emgd_framebuffer_t *new_surf);
int is_changed_color_key(
		igd_ovl_info_t *old_info,
		igd_ovl_info_t *new_info);
int is_changed_colorcorrect(
		igd_ovl_info_t *old_info,
		igd_ovl_info_t *new_info);
int is_changed_gamma(
		igd_ovl_info_t *old_info,
		igd_ovl_info_t *new_info);
int is_changed_scaling_ratio(
		igd_rect_t * old_src_rect,
		igd_rect_t * new_src_rect,
		igd_rect_t * old_dst_rect,
		igd_rect_t * new_dst_rect);
int is_changed_pipeblend(
		igd_ovl_pipeblend_t * pipeblend);

void copy_or_clear_rect(
		igd_rect_t *cache_rect,
		igd_rect_t * new_rect);
void copy_or_clear_surf(
		emgd_framebuffer_t * cache_surf,
		emgd_framebuffer_t * new_surf);
void copy_or_clear_color_key(
		igd_ovl_info_t *cache_info,
		igd_ovl_info_t * new_info);
void copy_or_clear_colorcorrect(
		igd_ovl_info_t *cache_info,
		igd_ovl_info_t * new_info);
void copy_or_clear_gamma(
		igd_ovl_info_t *cache_info,
		igd_ovl_info_t * new_info);
void copy_or_clear_pipeblend(
		igd_ovl_info_t *cache_info,
		igd_ovl_info_t * new_info);

