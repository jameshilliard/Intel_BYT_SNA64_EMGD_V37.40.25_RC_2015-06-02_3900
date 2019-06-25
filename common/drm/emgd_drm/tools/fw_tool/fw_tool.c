/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: fw_tool.c
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
 *  This file converts a user_config.c and image_header.h file to a binary blob,
 *  which can be used with the EMGD driver.  It can also convert a binary blob
 *  back into human readable user_config.c and image_header.h files.
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>
#include "default_config.h"

/* This is the driver main configuration structure. */
extern emgd_drm_config_t default_config_drm;
extern emgd_drm_config_t *config_drm;

/* Which version of the binary blob/text files is this for. */
#define CONFIG_VERSION 1

void print_help();
int bit;

int main(int argc, char **argv)
{
	FILE *fp;
	int i, j;
	int config_version = CONFIG_VERSION;

	config_drm = &default_config_drm;

#if __x86_64__
	bit = 64;
#else
	bit = 32;
#endif

	for (i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			print_help();
			return 0;
		}
	}

#if __x86_64__
	fp = fopen("emgd_64.bin", "w");
#else
	fp = fopen("emgd_32.bin", "w");
#endif

	if (fp == NULL) {
		printf("Can't open output file emgd_%d.bin.\n", bit);
		return 1;
	} else {
		printf("Opened output file emgd_%d.bin.\n", bit);
	}

	fwrite(&config_version, sizeof(int), 1, fp);

	fwrite(&config_drm->num_crtcs, sizeof(int), 1, fp);
	fwrite(config_drm->crtcs, sizeof(emgd_crtc_config_t),
			config_drm->num_crtcs, fp);

	fwrite(&config_drm->num_fbs, sizeof(int), 1, fp);
	fwrite(config_drm->fbs, sizeof(emgd_fb_config_t),
			config_drm->num_fbs, fp);

	fwrite(config_drm->ovl_config, sizeof(emgd_ovl_config_t), 1, fp);
	fwrite(config_drm->ss_data, sizeof(emgd_drm_splash_screen_t), 1, fp);
	fwrite(config_drm->sv_data, sizeof(emgd_drm_splash_video_t), 1, fp);
	fwrite(&config_drm->num_hal_params, sizeof(int), 1, fp);
	for (i=0; i<config_drm->num_hal_params; i++) {
		fwrite(&config_drm->hal_params[i], sizeof(igd_param_t), 1, fp);
	}

	for (i=0; i<config_drm->num_hal_params; i++) {
		for (j=0; j<IGD_MAX_PORTS; j++) {
			if (config_drm->hal_params[i].display_params[j].dtd_list.num_dtds > 0) {
				fwrite(config_drm->hal_params[i].display_params[j].dtd_list.dtd,
						sizeof(igd_display_info_t),
						config_drm->hal_params[i].display_params[j].dtd_list.num_dtds,
						fp);
			}
			if (config_drm->hal_params[i].display_params[j].attr_list.num_attrs > 0) {
				fwrite(config_drm->hal_params[i].display_params[j].attr_list.attr,
						sizeof(igd_param_attr_t),
						config_drm->hal_params[i].display_params[j].attr_list.num_attrs,
						fp);
			}
		}
	}

	if(config_drm->ss_data->image_size > 0) {
		fwrite(config_drm->ss_data->image_data,
				config_drm->ss_data->image_size, 1, fp);
	}
	fclose(fp);

	return 0;
}

void print_help() {
	printf("Usage: fw_tool\n");
	printf("This tool converts a user_config.c and image_data.h into an emgd_%d.bin\n",bit);
	printf("firmware binary blob to be used to configure the EMGD Linux driver.\n");
	printf("user_config.c and image_data.h should be placed in the fw_tool\n");
	printf("directory and then compiled by running 'make' or 'make fw_tool'.\n");
	printf("Once its compiled into the fw_tool executable it can be run to do\n");
	printf("the conversion.\n");
	printf("\n");
	printf("-- EXAMPLES --\n");
	printf("   fw_tool\n");
	printf("      compiled(user_config.c + image_data.h) ---> emgd_%d.bin\n",bit);

	return;
}


