/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: config_convert.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2012,2013, Intel Corporation.
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
 *  This file converts a previous version of user_config.c and image_header.h
 *  file to a binary blob, which can be used with the EMGD driver.
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>
#include "image_data.h"
#include "user_config.h"


/* This is the driver main configuration structure. */
extern emgd_drm_config_t config_drm;
extern emgd_fb_config_t initial_fbs[];

/* Which version of the binary blob/text files is this for. */
#define CONFIG_VERSION 1

int main(int argc, char **argv)
{
	FILE *fp;
	int i, j, bit;
	int config_version = CONFIG_VERSION;
	int num_crtcs = 2;
	int num_fbs = 2;
	int num_hal_params = 1;
	unsigned long image_size, null_ptr = 0;

#if __x86_64__
	fp = fopen("emgd_64.bin", "w");
	bit = 64;
#else
	fp = fopen("emgd_32.bin", "w");
	bit = 32;
#endif

	if (fp == NULL) {
		printf("Can't open output file emgd_%d.bin.\n", bit);
		return 1;
	} else {
		printf("Opened output file emgd_%d.bin.\n", bit);
	}

	fwrite(&config_version, sizeof(int), 1, fp);

	fwrite(&num_crtcs, sizeof(int), 1, fp);
	fwrite(config_drm.crtcs, sizeof(emgd_crtc_config_t), num_crtcs, fp);

	fwrite(&num_fbs, sizeof(int), 1, fp);
	fwrite(initial_fbs, sizeof(emgd_fb_config_t), num_fbs, fp);

	fwrite(config_drm.ovl_config, sizeof(emgd_ovl_config_t), 1, fp);

	fwrite(config_drm.ss_data, sizeof(emgd_drm_splash_screen_t), 1, fp);
	/* Need to additionally create space for image_pointer */
	fwrite(&null_ptr, sizeof(unsigned long), 1, fp);
	/* Need to additionally write image_size */
	image_size = sizeof(image_data);
	fwrite(&image_size, sizeof(unsigned long), 1, fp);
	fwrite(config_drm.sv_data, sizeof(emgd_drm_splash_video_t), 1, fp);
	fwrite(&num_hal_params, sizeof(int), 1, fp);
	for (i=0; i<num_hal_params; i++) {
		fwrite(config_drm.hal_params[i], sizeof(igd_param_t), 1, fp);
	}

	for (i=0; i<num_hal_params; i++) {
		for (j=0; j<IGD_MAX_PORTS; j++) {
			if (config_drm.hal_params[i]->display_params[j].dtd_list.num_dtds > 0) {
				fwrite(config_drm.hal_params[i]->display_params[j].dtd_list.dtd,
						sizeof(igd_display_info_t),
						config_drm.hal_params[i]->display_params[j].dtd_list.num_dtds,
						fp);
			}
			if (config_drm.hal_params[i]->display_params[j].attr_list.num_attrs > 0) {
				fwrite(config_drm.hal_params[i]->display_params[j].attr_list.attr,
						sizeof(igd_param_attr_t),
						config_drm.hal_params[i]->display_params[j].attr_list.num_attrs,
						fp);
			}
		}
	}

	if(image_size > 0) {
		fwrite(image_data, image_size, 1, fp);
	}

	fclose(fp);

	return 0;
}
