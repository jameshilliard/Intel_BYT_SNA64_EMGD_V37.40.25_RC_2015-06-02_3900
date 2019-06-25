/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: fw_tool.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2012-2014, Intel Corporation.
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
extern emgd_drm_config_t *config_drm;
int bit;

/* Which version of the binary blob/text files is this for. */
#define CONFIG_VERSION 1

void print_help();

int main(int argc, char **argv)
{
	FILE *fp;
	int i, k, j;
	unsigned long iter = 0, blob_length;
	char *buffer;
	int config_version = CONFIG_VERSION;
	emgd_drm_config_t * config_drm;

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
	fp = fopen("emgd_64.bin", "r");
#else
        fp = fopen("emgd_32.bin", "r");
#endif

	if (fp == NULL) {
		printf("Can't open input file emgd_%d.bin.\n", bit);
		return 1;
	} else {
		printf("Opened input file emgd_%d.bin.\n", bit);
	}

	config_drm = (emgd_drm_config_t *)malloc(sizeof(emgd_drm_config_t));
	if (config_drm == NULL) {
		printf("Can't allocate memory for config_drm\n");
		return 2;
	}
	memset(config_drm, 0, sizeof(emgd_drm_config_t));

	fseek(fp, 0, SEEK_END);
	blob_length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = (char *) malloc(blob_length + 1);
	if (!buffer) {
		printf("Can't allocate memory to read input file.\n");
		return 2;
	}

	fread(buffer, blob_length, 1, fp);

	fclose(fp);

	fp = fopen("new_user_config.c", "w");

	if (fp == NULL) {
		printf("Can't open output file %s.\n", "new_user_config.c");
		return 1;
	} else {
		printf("Opened output file %s.\n", "new_user_config.c");
	}

	fprintf(fp, "/* -*- pse-c -*-\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Filename: default_config.c\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Copyright (c) 2002-2013, Intel Corporation.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * Permission is hereby granted, free of charge, to any person obtaining a copy\n");
	fprintf(fp, " * of this software and associated documentation files (the \"Software\"), to deal\n");
	fprintf(fp, " * in the Software without restriction, including without limitation the rights\n");
	fprintf(fp, " * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
	fprintf(fp, " * copies of the Software, and to permit persons to whom the Software is\n");
	fprintf(fp, " * furnished to do so, subject to the following conditions:\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * The above copyright notice and this permission notice shall be included in\n");
	fprintf(fp, " * all copies or substantial portions of the Software.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
	fprintf(fp, " * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
	fprintf(fp, " * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
	fprintf(fp, " * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
	fprintf(fp, " * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
	fprintf(fp, " * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n");
	fprintf(fp, " * THE SOFTWARE.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Description:\n");
	fprintf(fp, " *  A file that contains the initial display configuration information of the\n");
	fprintf(fp, " *  EMGD kernel module.  A user can edit this file in order to affect the way\n");
	fprintf(fp, " *  that the kernel initially configures the displays.  This file is compiled\n");
	fprintf(fp, " *  into the EMGD kernel module.\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " */\n");
	fprintf(fp, "#include \"default_config.h\"\n");
	fprintf(fp, "#include \"image_data.h\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "#warning ****\n");
	fprintf(fp, "#warning **** This driver has NOT been configured your your system.  You are\n");
	fprintf(fp, "#warning **** building with a sample default_config.c configuration that may or\n");
	fprintf(fp, "#warning **** may not meet your needs.  It is recommended that you run CED to\n");
	fprintf(fp, "#warning **** generate an appropriate configuration or see the User Guide\n");
	fprintf(fp, "#warning **** for more information about driver configuration.\n");
	fprintf(fp, "#warning ****\n\n");

	/* Allocate space for version number */
	memcpy(&config_version, &buffer[iter], sizeof(int));
	iter += sizeof(int);
	fprintf(fp, "\n/* Config version: %d */\n", config_version);

	config_drm->num_crtcs = buffer[iter];
	iter += sizeof(int);
	config_drm->crtcs = (emgd_crtc_config_t *)&buffer[iter];
	iter += sizeof(emgd_crtc_config_t) * config_drm->num_crtcs;

	config_drm->num_fbs = buffer[iter];
	iter += sizeof(int);
	config_drm->fbs = (emgd_fb_config_t *)&buffer[iter];
	iter += sizeof(emgd_fb_config_t) * config_drm->num_fbs;

	config_drm->ovl_config = (emgd_ovl_config_t *)&buffer[iter];
	iter += sizeof(emgd_ovl_config_t);

	config_drm->ss_data = (emgd_drm_splash_screen_t *)&buffer[iter];
	iter += sizeof(emgd_drm_splash_screen_t);

	config_drm->sv_data = (emgd_drm_splash_video_t *)&buffer[iter];
	iter += sizeof(emgd_drm_splash_video_t);

	config_drm->num_hal_params = buffer[iter];
	iter += sizeof(int);
	config_drm->hal_params = (igd_param_t *)&buffer[iter];
	iter += sizeof(igd_param_t) * config_drm->num_hal_params;

	for (i=0; i<config_drm->num_hal_params; i++) {
		for (j=0; j<IGD_MAX_PORTS; j++) {
			if (config_drm->hal_params[i].display_params[j].dtd_list.num_dtds > 0) {
				config_drm->hal_params[i].display_params[j].dtd_list.dtd =
					(igd_display_info_t *)&buffer[iter];
				iter += sizeof(igd_display_info_t) *
					config_drm->hal_params[i].display_params[j].dtd_list.num_dtds;
			}

			if (config_drm->hal_params[i].display_params[j].attr_list.num_attrs > 0) {
				config_drm->hal_params[i].display_params[j].attr_list.attr =
					(igd_param_attr_t *)&buffer[iter];
				iter += sizeof(igd_param_attr_t) *
					config_drm->hal_params[i].display_params[j].attr_list.num_attrs;
			}
		}
	}

	if (config_drm->ss_data->image_size > 0) {
		config_drm->ss_data->image_data = (unsigned char *)&buffer[iter];
	}

	fprintf(fp, "/*\n");
	fprintf(fp, " * One array of attribute pairs may exist for each configured port.  See the\n");
	fprintf(fp, " * \"include/igd_pd.h\" file for attributes.\n");
	fprintf(fp, " */\n");

	for (k=0; k<config_drm->num_hal_params; k++) {
		for (i=0; i<IGD_MAX_PORTS; i++) {
			if (config_drm->hal_params[k].display_params[i].attr_list.num_attrs > 0) {
				fprintf(fp, "static igd_param_attr_t attrs_config%d_port%ld[] = {\n",
						k, config_drm->hal_params[k].display_params[i].port_number);
				for (j=0; j<config_drm->hal_params[k].display_params[i].attr_list.num_attrs; j++) {
					fprintf(fp, "\t{0x%lx, 0x%lx},\n",
							config_drm->hal_params[k].display_params[i].attr_list.attr[j].id,
							config_drm->hal_params[k].display_params[i].attr_list.attr[j].value);
				}
				fprintf(fp, "};\n");
			}
		}
	}

	fprintf(fp, "/*\n");
	fprintf(fp, " * One array of igd_display_info_t structures should exist for each port that\n");
	fprintf(fp, " * needs to provide a DTD list.  Each igd_display_info_t contains the DTD\n");
	fprintf(fp, " * information for a given resolution/refresh-rate.  This is especially needed\n");
	fprintf(fp, " * for analog/VGA ports.\n");
	fprintf(fp, " */\n");

	for (k=0; k<config_drm->num_hal_params; k++) {
		for (i=0; i<IGD_MAX_PORTS; i++) {
			if (config_drm->hal_params[k].display_params[i].dtd_list.num_dtds > 0) {
				fprintf(fp, "static igd_display_info_t dtd_config%d_port%ld_dtdlist[] = {\n",
						k, config_drm->hal_params[k].display_params[i].port_number);

				for (j=0; j<config_drm->hal_params[k].display_params[i].dtd_list.num_dtds; j++) {
					fprintf(fp, "\t{\n");
					fprintf(fp, "\t\t%d,\t\t/* Width */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].width);
					fprintf(fp, "\t\t%d,\t\t/* Height */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].height);
					fprintf(fp, "\t\t%d,\t\t/* Refresh Rate */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].refresh);
					fprintf(fp, "\t\t%ld,\t\t/* Dot Clock (in KHz) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].dclk);
					fprintf(fp, "\t\t%d,\t\t/* Horizontal Total (horizontal synch end) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].htotal);
					fprintf(fp, "\t\t%d,\t\t/* Horizontal Blank Start (h_active-1) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].hblank_start);
					fprintf(fp, "\t\t%d,\t\t/* Horizontal Blank End (start + h_blank) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].hblank_end);
					fprintf(fp, "\t\t%d,\t\t/* Horizontal Sync Start (h_active+h_synch-1) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].hsync_start);
					fprintf(fp, "\t\t%d,\t\t/* Horizontal Sync End (start + h_syncp) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].hsync_end);
					fprintf(fp, "\t\t%d,\t\t/* Vertical Total (Vertical synch end) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].vtotal);
					fprintf(fp, "\t\t%d,\t\t/* Vertical Blank Start (v_active-1) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].vblank_start);
					fprintf(fp, "\t\t%d,\t\t/* Vertical Blank End (start + v_blank) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].vblank_end);
					fprintf(fp, "\t\t%d,\t\t/* Vertical Sync Start (v_active+v_synch-1) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].vsync_start);
					fprintf(fp, "\t\t%d,\t\t/* Vertical Sync End (start + v_synchp) */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].vsync_end);
					fprintf(fp, "\t\t%d,\t\t/* Mode Number */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].mode_number);
					fprintf(fp, "\t\t0x%lx,\t\t/* Flags */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].flags);
					fprintf(fp, "\t\t%d,\t\t/* X Offset */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].x_offset);
					fprintf(fp, "\t\t%d,\t\t/* Y Offset */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].y_offset);
					fprintf(fp, "\t\t0x%lx,\t\t/* pd extension pointer */\n",
							(unsigned long)config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].private.extn_ptr);
					fprintf(fp, "\t\t%d,%d\t\t/* mode extension pointer */\n",
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].reserved_dd,
							config_drm->hal_params[k].display_params[i].dtd_list.dtd[j].reserved_dd_ext);
					fprintf(fp, "\t},\n");
				}
				fprintf(fp, "};\n\n");
			}
		}
	}

	fprintf(fp, "/*\n");
	fprintf(fp, " * Splash screen and splash video functionality is mutually exclusive.\n");
	fprintf(fp, " * If splash_video_data->immediate is enabled then splash screen will not\n");
	fprintf(fp, " * be called. To enable splash screen ensure width and height is non-zero.\n");
	fprintf(fp, " */\n");

	fprintf(fp, "static emgd_drm_splash_screen_t splash_screen_data = {\n");
	fprintf(fp, "\t0x%lx,\t\t/* bg_color */\n",
			config_drm->ss_data->bg_color);
	fprintf(fp, "\t%ld,\t\t/* x */\n",
			config_drm->ss_data->x);
	fprintf(fp, "\t%ld,\t\t/* y */\n",
			config_drm->ss_data->y);
	fprintf(fp, "\t%ld,\t\t/* width */\n",
			config_drm->ss_data->width);
	fprintf(fp, "\t%ld,\t\t/* height */\n",
			config_drm->ss_data->height);
	fprintf(fp, "\timage_data,\t\t/* pointer to image data */\n");
	fprintf(fp, "\tsizeof(image_data),\t\t/* image_size */\n");
	fprintf(fp, "};\n\n");

	fprintf(fp, "static emgd_drm_splash_video_t splash_video_data = {\n");
	fprintf(fp, "\t%ld,\t\t/* built_in_test */\n",
			config_drm->sv_data->built_in_test);
	fprintf(fp, "\t%ld,\t\t/* immediate (as opposed to using sysfs) */\n",
			config_drm->sv_data->immediate);
	fprintf(fp, "\t0x%llx,\t\t/* phys_contig_dram_start */\n",
			config_drm->sv_data->phys_contig_dram_start);
	fprintf(fp, "\t0x%lx,\t\t/* igd_pixel_format */\n",
			config_drm->sv_data->igd_pixel_format);
	fprintf(fp, "\t%ld,\t\t/* src_width */\n",
			config_drm->sv_data->src_width);
	fprintf(fp, "\t%ld,\t\t/* src_height */\n",
			config_drm->sv_data->src_height);
	fprintf(fp, "\t%ld,\t\t/* src_pitch */\n",
			config_drm->sv_data->src_pitch);
	fprintf(fp, "\t%ld,\t\t/* dst_x */\n",
			config_drm->sv_data->dst_x);
	fprintf(fp, "\t%ld,\t\t/* dst_y */\n",
			config_drm->sv_data->dst_y);
	fprintf(fp, "\t%ld,\t\t/* dst_width */\n",
			config_drm->sv_data->dst_width);
	fprintf(fp, "\t%ld,\t\t/* dst_height */\n",
			config_drm->sv_data->dst_height);
	fprintf(fp, "};\n\n");

	fprintf(fp, "/*\n");
	fprintf(fp, " * The igd_param_t structure contains many configuration values used by the\n");
	fprintf(fp, " * EMGD kernel module.\n");
	fprintf(fp, " */\n");
	fprintf(fp, "igd_param_t config_params[] = {\n");
	for (k=0; k<config_drm->num_hal_params; k++) {
		fprintf(fp, "\t{ /* config_%d */\n", k);
		fprintf(fp, "\t\t%ld,\t\t/* Limit maximum pages usable by GEM (0 = no limit) */\n",
				config_drm->hal_params[k].page_request);
		fprintf(fp, "\t\t%ld,\t\t/* Max frame buffer size (0 = no limit) */\n",
				config_drm->hal_params[k].max_fb_size);
		fprintf(fp, "\t\t%d,\t\t/* Preserve registers (should be 1, so VT switches work and so\n",
				config_drm->hal_params[k].preserve_regs);
		fprintf(fp, "\t\t\t\t * that the console will be restored after X server exits).\n");
		fprintf(fp, "\t\t\t\t */\n");
		fprintf(fp, "\t\t0x%lx,\t\t/* Display flags (bitfield, where:\n",
				config_drm->hal_params[k].display_flags);
		fprintf(fp, "\t\t\t\t * IGD_DISPLAY_MULTI_DVO\n");
		fprintf(fp, "\t\t\t\t * IGD_DISPLAY_DETECT\n");
		fprintf(fp, "\t\t\t\t * IGD_DISPLAY_FB_BLEND_OVL\n");
		fprintf(fp, "\t\t\t\t */\n");
		fprintf(fp, "\t\t{  /* Display port order, corresponds to the \"portorder\" module parameter\n");
		fprintf(fp, "\t\t    *   Valid port types are:\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_TV\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_SDVOB\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_SDVOC\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_LVDS\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_ANALOG\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_DPB\n");
		fprintf(fp, "\t\t    *   IGD_PORT_TYPE_DPC\n");
		fprintf(fp, "\t\t    */\n");

		for (i=0; i<IGD_MAX_PORTS; i++) {
			fprintf(fp, "\t\t\t0x%lx,\n",
					config_drm->hal_params[k].port_order[i]);
		}
		fprintf(fp, "\t\t},\n");

		fprintf(fp, "\t\t{\t\t/* Display Params: */\n");
		for (i=0; i<IGD_MAX_PORTS; i++) {
			fprintf(fp, "\t\t\t{\t\t/* Port:%ld */\n",
					config_drm->hal_params[k].display_params[i].port_number);

			if (i == 0) {
				fprintf(fp, "\t\t\t\t%ld,\t\t/* Display port number (0 if not configured) */\n",
						config_drm->hal_params[k].display_params[i].port_number);
				fprintf(fp, "\t\t\t\t0x%lx,\t\t/* Parameters present (see above) */\n",
						config_drm->hal_params[k].display_params[i].present_params);
				fprintf(fp, "\t\t\t\t0x%lx,\t\t/* EDID flag */\n",
						config_drm->hal_params[k].display_params[i].flags);
				fprintf(fp, "\t\t\t\t0x%x,\t\t/* Flags when EDID is available (see above) */\n",
						config_drm->hal_params[k].display_params[i].edid_avail);
				fprintf(fp, "\t\t\t\t0x%x,\t\t/* Flags when EDID is not available (see above) */\n",
						config_drm->hal_params[k].display_params[i].edid_not_avail);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* DDC GPIO pins */\n",
						config_drm->hal_params[k].display_params[i].ddc_gpio);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* DDC speed */\n",
						config_drm->hal_params[k].display_params[i].ddc_speed);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* DDC DAB */\n",
						config_drm->hal_params[k].display_params[i].ddc_dab);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* I2C GPIO pins */\n",
						config_drm->hal_params[k].display_params[i].i2c_gpio);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* I2C speed */\n",
						config_drm->hal_params[k].display_params[i].i2c_speed);
				fprintf(fp, "\t\t\t\t%ld,\t\t/* I2C DAB */\n",
						config_drm->hal_params[k].display_params[i].i2c_dab);

				fprintf(fp, "\t\t\t\t{\t\t\t/* Flat Panel Info: */\n");
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* Flat Panel width */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_width);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* Flat Panel height */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_height);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* Flat Panel power method */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_method);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* VDD active & DVO clock/data active */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t1);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* DVO clock/data active & backlight enable */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t2);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* backlight disable & DVO clock/data inactive */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t3);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* DVO clock/data inactive & VDD inactive */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t4);
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* VDD inactive & VDD active */\n",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t5);
				fprintf(fp, "\t\t\t\t},\n");

				fprintf(fp, "\t\t\t\t{\t/* DTD Info */\n");
				fprintf(fp, "\t\t\t\t\t%ld,\t\t/* number of DTD's */\n",
						config_drm->hal_params[k].display_params[i].dtd_list.num_dtds);
				if (config_drm->hal_params[k].display_params[i].dtd_list.num_dtds == 0) {
					fprintf(fp, "\t\t\t\t\tNULL /* DTD name */\n");
				} else {
					fprintf(fp, "\t\t\t\t\tdtd_config%d_port%ld_dtdlist\n",
							k, config_drm->hal_params[k].display_params[i].port_number);
				}
				fprintf(fp, "\t\t\t\t},\n");
				fprintf(fp, "\t\t\t\t{\t/* Attribute Info */\n");
				if (config_drm->hal_params[k].display_params[i].attr_list.num_attrs == 0) {
					fprintf(fp, "\t\t\t\t\t0,   /* number of attrs */\n");
					fprintf(fp, "\t\t\t\t\tNULL /* Attr name */\n");
				} else {
					fprintf(fp, "\t\t\t\t\tsizeof(attrs_config%d_port%ld)/sizeof(igd_param_attr_t),\t\t/* number or attrs */\n",
						k, config_drm->hal_params[k].display_params[i].port_number);
					fprintf(fp, "\t\t\t\t\tattrs_config%d_port%ld\n",
							k, config_drm->hal_params[k].display_params[i].port_number);
				}
				fprintf(fp, "\t\t\t\t},\n");
			} else {
				fprintf(fp, "\t\t\t\t%ld,\t/* Display port number (0 if not configured) */\n",
						config_drm->hal_params[k].display_params[i].port_number);
				fprintf(fp, "\t\t\t\t0x%lx,\n",
						config_drm->hal_params[k].display_params[i].present_params);
				fprintf(fp, "\t\t\t\t0x%lx, ",
						config_drm->hal_params[k].display_params[i].flags);
				fprintf(fp, "0x%x, ",
						config_drm->hal_params[k].display_params[i].edid_avail);
				fprintf(fp, "0x%x, ",
						config_drm->hal_params[k].display_params[i].edid_not_avail);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].ddc_gpio);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].ddc_speed);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].ddc_dab);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].i2c_gpio);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].i2c_speed);
				fprintf(fp, "%ld,\n",
						config_drm->hal_params[k].display_params[i].i2c_dab);

				fprintf(fp, "\t\t\t\t{ %ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_width);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_height);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_method);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t1);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t2);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t3);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t4);
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].fp_info.fp_pwr_t5);
				fprintf(fp, " },\n");

				fprintf(fp, "\t\t\t\t{ ");
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].dtd_list.num_dtds);
				if (config_drm->hal_params[k].display_params[i].dtd_list.num_dtds == 0) {
					fprintf(fp, "NULL");
				} else {
					fprintf(fp, "dtd_config%d_port%ld_dtdlist",
							k, config_drm->hal_params[k].display_params[i].port_number);
				}
				fprintf(fp, " },\n");
				fprintf(fp, "\t\t\t\t{ ");
				fprintf(fp, "%ld, ",
						config_drm->hal_params[k].display_params[i].attr_list.num_attrs);
				if (config_drm->hal_params[k].display_params[i].attr_list.num_attrs == 0) {
					fprintf(fp, "NULL");
				} else {
					fprintf(fp, "attrs_config%d_port%ld",
							k, config_drm->hal_params[k].display_params[i].port_number);
				}
				fprintf(fp, " },\n");

			}
			fprintf(fp, "\t\t\t},\n");
		}
		fprintf(fp, "\t\t},\n");

		fprintf(fp, "\t\t%ld,\t/* Quickboot flags (refer to header or docs for more info) */\n",
				config_drm->hal_params[k].quickboot);
		fprintf(fp, "\t\t%d,\t/* Quickboot seamless (1 = enabled) */\n",
				config_drm->hal_params[k].qb_seamless);
		fprintf(fp, "\t\t%ld,\t/* Quickboot video input (1 = enabled) */\n",
				config_drm->hal_params[k].qb_video_input);
		fprintf(fp, "\t\t%d,\t/* Quickboot splash (1 = enabled) */\n",
				config_drm->hal_params[k].qb_splash);
		fprintf(fp, "\t\t0x%x,\n",
				config_drm->hal_params[k].polling);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].ref_freq);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].en_reg_override);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].disp_arb);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark1);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark2);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark3);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark4);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark5);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].fifo_watermark6);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].gvd_hp_control);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].bunit_chicken_bits);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].bunit_write_flush);
		fprintf(fp, "\t\t0x%lx,\n",
				config_drm->hal_params[k].disp_chicken_bits);
		fprintf(fp, "\t},\n");
	}
	fprintf(fp, "};\n\n");

	fprintf(fp, "emgd_fb_config_t initial_fbs[] = {\n");
	for (i = 0; i<config_drm->num_fbs; i++) {
		fprintf(fp, "\t{\t/* frame buffer %d */\n", i);
		fprintf(fp, "\t\t%d, /* frame buffer width */\n",
				config_drm->fbs[i].width);
		fprintf(fp, "\t\t%d, /* frame buffer height */\n",
				config_drm->fbs[i].height);
		fprintf(fp, "\t\t%d, /* frame buffer depdth */\n",
				config_drm->fbs[i].depth);
		fprintf(fp, "\t\t%d, /* frame buffer bpp */\n",
				config_drm->fbs[i].bpp);
		fprintf(fp, "\t},\n");

	}
	fprintf(fp, "};\n\n");

	fprintf(fp, "emgd_ovl_config_t initial_ovl_config = {\n");
	fprintf(fp, "\t/* emgd_drm_ovl_plane_config_t array X 4 - known defaults*/\n");
	fprintf(fp, "\t{\n");

	for (i = 0; i<4; i++) {
		fprintf(fp, "\t\t/* Sprite-%c */\n", (int)'A' + i);
		fprintf(fp, "\t\t{\n");
		fprintf(fp, "\t\t\t{\n");
		fprintf(fp, "\t\t\t\t/* igd_ovl_gamma_info          */\n");
		fprintf(fp, "\t\t\t\t/*******************************/\n");
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned int red;     */\n",
				config_drm->ovl_config->ovl_configs[i].gamma.red);
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned int green;   */\n",
				config_drm->ovl_config->ovl_configs[i].gamma.green);
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned int blue;    */\n",
				config_drm->ovl_config->ovl_configs[i].gamma.blue);
		fprintf(fp, "\t\t\t\t0x%x /* unsigned int flags;*/\n",
				config_drm->ovl_config->ovl_configs[i].gamma.flags);
		fprintf(fp, "\t\t\t},\n");
		fprintf(fp, "\t\t\t{\n");
		fprintf(fp, "\t\t\t\t/* igd_ovl_color_correct_info_t */\n");
		fprintf(fp, "\t\t\t\t/********************************/\n");
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned short brightness; */\n",
				config_drm->ovl_config->ovl_configs[i].color_correct.brightness);
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned short contrast;   */\n",
				config_drm->ovl_config->ovl_configs[i].color_correct.contrast);
		fprintf(fp, "\t\t\t\t0x%x,/* unsigned short saturation; */\n",
				config_drm->ovl_config->ovl_configs[i].color_correct.saturation);
		fprintf(fp, "\t\t\t\t0x%x /* unsigned short hue;         */\n",
				config_drm->ovl_config->ovl_configs[i].color_correct.hue);
		fprintf(fp, "\t\t\t}\n");
		fprintf(fp, "\t\t},\n");
	}
	fprintf(fp, "\t},\n");
	fprintf(fp, "\t/* igd_ovl_pipeblend_t X 1 - known defaults */\n");
	fprintf(fp, "\t{\n");
	for (i=0; i<2; i++) {
		fprintf(fp, "\t\t/* Pipe %c */\n", (int)'A' + i);
		fprintf(fp, "\t\t{\n");
		fprintf(fp, "\t\t\t0x%lx, /* unsigned long zorder_combined*/\n",
				config_drm->ovl_config->ovl_pipeblend[i].zorder_combined);
		fprintf(fp, "\t\t\t0x%lx, /* unsigned long fb_blend_ovl*/\n",
				config_drm->ovl_config->ovl_pipeblend[i].fb_blend_ovl);
		fprintf(fp, "\t\t\t0x%lx, /* unsigned long has_const_alpha*/\n",
				config_drm->ovl_config->ovl_pipeblend[i].has_const_alpha);
		fprintf(fp, "\t\t\t0x%lx, /* unsigned long const_alpha*/\n",
				config_drm->ovl_config->ovl_pipeblend[i].const_alpha);
		fprintf(fp, "\t\t\t0x%lx, /* unsigned long enable_flags*/\n",
				config_drm->ovl_config->ovl_pipeblend[i].enable_flags);
		fprintf(fp, "\t\t},\n");
	}
	fprintf(fp, "\t}\n");

	fprintf(fp, "};\n\n");

	fprintf(fp, "emgd_crtc_config_t crtcs[] = {\n");

	for (i = 0; i<config_drm->num_crtcs; i++) {
		fprintf(fp, "\t{\t/* CRTC %d */\n", i);
		fprintf(fp, "\t\t%d, /* frame buffer ID to associate with this crtc */\n",
				config_drm->crtcs[i].framebuffer_id);
		fprintf(fp, "\t\t%d, /* Display width used to find initial display timings */\n",
				config_drm->crtcs[i].display_width);
		fprintf(fp, "\t\t%d, /* Display height used to find initial display timings */\n",
				config_drm->crtcs[i].display_height);
		fprintf(fp, "\t\t%d, /* Display vertical refresh rate to use */\n",
				config_drm->crtcs[i].refresh);
		fprintf(fp, "\t\t%d, /* X offset */\n",
				config_drm->crtcs[i].x_offset);
		fprintf(fp, "\t\t%d, /* Y offset */\n",
				config_drm->crtcs[i].y_offset);
		fprintf(fp, "\t},\n");
	}

	fprintf(fp, "};\n");

	fprintf(fp, "/*\n");
	fprintf(fp, " * The emgd_drm_config_t structure is the main configuration structure\n");
	fprintf(fp, " * for the EMGD kernel module.\n");
	fprintf(fp, " */\n");
	fprintf(fp, "emgd_drm_config_t default_config_drm = {\n");
	fprintf(fp, "\t%d,\n", config_drm->num_crtcs);
	fprintf(fp, "\tcrtcs,\n");
	fprintf(fp, "\t%d,\n", config_drm->num_fbs);
	fprintf(fp, "\tinitial_fbs,\n");
	fprintf(fp, "\t&initial_ovl_config,/* overlay plane initial configuration (can be NULL) */\n");
	fprintf(fp, "\t&splash_screen_data,\n");
	fprintf(fp, "\t&splash_video_data,\n");
	fprintf(fp, "\t%d,\n", config_drm->num_hal_params);
	fprintf(fp, "\tconfig_params,  /* driver parameters from above */\n");
	fprintf(fp, "\t0\n");
	fprintf(fp, "};\n\n");
	fprintf(fp, "emgd_drm_config_t *config_drm;\n");
	fclose(fp);

	fp = fopen("new_image_data.h", "w");

	if (fp == NULL) {
		printf("Can't open output file %s.\n", "new_image_data.h");
		return 1;
	} else {
		printf("Opened output file %s.\n", "new_image_data.h");
	}

	fprintf(fp, "/* -*- pse-c -*-\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Filename: image_data.h\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Copyright (c) 2002-2013, Intel Corporation.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * Permission is hereby granted, free of charge, to any person obtaining a copy\n");
	fprintf(fp, " * of this software and associated documentation files (the \"Software\"), to deal\n");
	fprintf(fp, " * in the Software without restriction, including without limitation the rights\n");
	fprintf(fp, " * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
	fprintf(fp, " * copies of the Software, and to permit persons to whom the Software is\n");
	fprintf(fp, " * furnished to do so, subject to the following conditions:\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * The above copyright notice and this permission notice shall be included in\n");
	fprintf(fp, " * all copies or substantial portions of the Software.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
	fprintf(fp, " * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
	fprintf(fp, " * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
	fprintf(fp, " * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
	fprintf(fp, " * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
	fprintf(fp, " * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n");
	fprintf(fp, " * THE SOFTWARE.\n");
	fprintf(fp, " *\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " * Description:\n");
	fprintf(fp, " *\n");
	fprintf(fp, " *-----------------------------------------------------------------------------\n");
	fprintf(fp, " */\n");
	fprintf(fp, "#define DECODE_PNG\n");
	fprintf(fp, "static unsigned char image_data[] = {\n");

	for (j = 0; j < config_drm->ss_data->image_size; j++) {
		fprintf(fp, "0x%02X,", config_drm->ss_data->image_data[j]);
		if (j % 8 == 7) {
			fprintf(fp, "\n");
		}
	}

	fprintf(fp, "};");

	fclose(fp);

	return 0;
}

void print_help() {
	printf("Usage: rev_fw_tool\n");
    printf("   This will take an emgd_%d.bin in the same directory and create\n",bit);
    printf("   new_user_config.c and new_image_data.h, representing the data\n");
    printf("   contained in the emgd_%d.bin file. You can build this tool\n",bit);
    printf("   individually by running 'make rev_fw_tool' or it will get created\n");
    printf("   if you run 'make'.  If you run 'make' you will also need to include\n");
    printf("   a user_config.c and image_data.h because fw_tool will also be build.\n");
	printf("\n");
	printf("-- EXAMPLES --\n");
	printf("   rev_fw_tool\n");
	printf("      emgd_%d.bin ---> new_user_config.c + new_image_data.h\n",bit);

	return;
}


