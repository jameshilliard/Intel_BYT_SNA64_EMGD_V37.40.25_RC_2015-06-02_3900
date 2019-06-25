/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: hdmi_port.h
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
 *  Main include file for HDMI port driver
 *-----------------------------------------------------------------------------
 */

#ifndef _HDMI_PORT_H_
#define _HDMI_PORT_H_

#include <config.h>
#include <igd_pd.h>
#include <pd.h>
#include <pd_print.h>

/*  ............................................................................ */
#ifndef CONFIG_MICRO
#define MAKE_NAME(x)    x
#define	HDMI_GET_ATTR_NAME(p_attr)	p_attr->name
#else
#define MAKE_NAME(x)    NULL
#define HDMI_GET_ATTR_NAME(p_attr)	""
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(p) (sizeof(p)/sizeof((p)[0]))
#endif

#ifndef MAKE_DWORD
#define MAKE_DWORD(byte1,byte2,byte3,byte4) ((unsigned long)byte1|(unsigned long)byte2<<8|(unsigned long)byte3<<16|(unsigned long)byte4<<24)
#endif

/* General HDMI definition */
#define PORT_B_NAME		"HDMI B"
#define PORT_C_NAME		"HDMI C"
#define PORT_D_NAME		"HDMI D"
#define PORT_B_NUMBER	2
#define PORT_C_NUMBER	3
#define PORT_D_NUMBER	1
/* Port Control register */
#define HDMI_CONTROL_REG_B			0xE1140
#define HDMI_CONTROL_REG_C			0xE1150
#define HDMI_CONTROL_REG_D			0xE1160

#define AUD_CONFIG_A                    0xE5000
#define AUD_MISC_CTRL_A                 0xE5010
#define AUD_CTS_ENABLE_A	            0xE5028
#define AUD_HDMIW_HDMIEDID_A            0xE5050
#define AUD_HDMIW_INFOFR_A              0xE5054
#define AUD_OUT_DIG_CNVT_A              0xE5080
#define AUD_OUT_STR_DESC_A              0xE5084
#define AUD_CNTL_ST_A                   0xE50B4
#define VIDEO_DIP_CTL_A                 0xE0200
#define VIDEO_DIP_DATA_A                0xE0208

#define AUD_CONFIG_B                    0xE5100
#define AUD_MISC_CTRL_B                 0xE5110
#define AUD_CTS_ENABLE_B	            0xE5128
#define AUD_HDMIW_HDMIEDID_B            0xE5150	
#define AUD_HDMIW_INFOFR_B              0xE5154
#define AUD_OUT_DIG_CNVT_B              0xE5180
#define AUD_OUT_STR_DESC_B              0xE5184
#define AUD_CNTL_ST_B                   0xE51B4
#define VIDEO_DIP_CTL_B                 0xE1200 
#define VIDEO_DIP_DATA_B                0xE1208 

#define AUD_CONFIG_C                    0xE5200
#define AUD_MISC_CTRL_C                 0xE5210
#define AUD_CTS_ENABLE_C	            0xE5228
#define AUD_HDMIW_HDMIEDID_C            0xE5250	
#define AUD_HDMIW_INFOFR_C              0xE5254
#define AUD_OUT_DIG_CNVT_C              0xE5280
#define AUD_OUT_STR_DESC_C              0xE5284
#define AUD_CNTL_ST_C                   0xE52B4
#define VIDEO_DIP_CTL_C                 0xE2200 
#define VIDEO_DIP_DATA_C                0xE2208

/* VLV Port Control register */
#define VLV_HDMI_CONTROL_REG_B			0x61140
#define VLV_HDMI_CONTROL_REG_C			0x61160

/* VLV HD Audio and HDMI registers for Pipe A */
#define VLV_AUD_CONFIG_A                    0x62000
#define VLV_AUD_MISC_CTRL_A                 0x62010
#define VLV_AUD_CTS_ENABLE_A	            0x62028
#define VLV_AUD_HDMIW_HDMIEDID_A            0x62050
#define VLV_AUD_HDMIW_INFOFR_A              0x62054
#define VLV_AUD_OUT_DIG_CNVT_A              0x62080
#define VLV_AUD_OUT_STR_DESC_A              0x62084
#define VLV_AUD_CNTL_ST_A                   0x620B4
#define VLV_VIDEO_DIP_CTL_A                 0x60200
#define VLV_VIDEO_DIP_DATA_A                0x60208

/* VLV HD Audio and HDMI registers for Pipe B */
#define VLV_AUD_CONFIG_B                    0x62100
#define VLV_AUD_MISC_CTRL_B                 0x62110
#define VLV_AUD_CTS_ENABLE_B	            0x62128
#define VLV_AUD_HDMIW_HDMIEDID_B            0x62150
#define VLV_AUD_HDMIW_INFOFR_B              0x62154
#define VLV_AUD_OUT_DIG_CNVT_B              0x62180
#define VLV_AUD_OUT_STR_DESC_B              0x62184
#define VLV_AUD_CNTL_ST_B                   0x621B4
#define VLV_VIDEO_DIP_CTL_B                 0x61170
#define VLV_VIDEO_DIP_DATA_B                0x61174

/* Data Island Packet Definition */
#define HDMI_ELD_BUFFER_SIZE			256
#define HDMI_AVI_BUFFER_SIZE			17
#define HDMI_SPD_BUFFER_SIZE			29
#define HDMI_AVI_INFO_TYPE				0x82		/* AVI buffer type */
#define HDMI_SPD_INFO_TYPE				0x83		/* SPD buffer type */
#define HDMI_VENDOR_NAME				"Intel"
#define HDMI_VENDOR_DESCRIPTION		    "EMGD Driver"
#define HDMI_INTEL_VENDOR_NAME_SIZE		6
#define HDMI_IEGD_VENDOR_DESCRIPTION_SIZE	12
#define HDMI_VENDOR_NAME_SIZE			8
#define HDMI_VENDOR_DESCRIPTION_SIZE	16
#define HDMI_SPD_SOURCE_PC				0x09

/* Vendor Specific Info Frame */
#define HDMI_VENDOR_INFO_TYPE			0x81
#define HDMI_VENDOR_INFO_VERSION		0x01
#define HDMI_VENDOR_INFO_BUFFER_SIZE	10

#define THIS_PORT(a)  (a->p_callback->port_num==PORT_B_NUMBER)? \
						HDMI_CONTROL_REG_B:HDMI_CONTROL_REG_C
#define OTHER_PORT(a) (a->p_callback->port_num==PORT_B_NUMBER)? \
						HDMI_CONTROL_REG_C:HDMI_CONTROL_REG_B

typedef uint32_t mmio_reg_t;

typedef struct _hdmi_audio_port_regs {
	mmio_reg_t aud_config;
	mmio_reg_t aud_misc_ctrl;
	mmio_reg_t aud_cts_enable;	
	mmio_reg_t aud_hdmiw_hdmiedid;					
	mmio_reg_t aud_hdmiw_infofr;
	mmio_reg_t aud_out_dig_cnvt;
	mmio_reg_t aud_out_str_desc;
	mmio_reg_t aud_cntl_st;
	mmio_reg_t video_dip_ctl;
	mmio_reg_t video_dip_data;	
} hdmi_audio_port_regs_t;

typedef struct _hdmi_device_context {
    pd_callback_t             *p_callback;
	pd_attr_t                 *p_attr_table;
	unsigned long              num_attrs;
	pd_timing_t				  *timing_table;
	pd_timing_t               *native_dtd;
	uint32_t				  control_reg;
	unsigned long             power_state;
    hdmi_audio_port_regs_t    *hdmi_audio_port_regs;
	unsigned short            chipset;
	unsigned char            s3d_enable;
	unsigned char            s3d_structure;
	unsigned char            s3d_ext_data;
} hdmi_device_context_t;

unsigned char hdmi_read_mmio_reg(hdmi_device_context_t *p_ctx, uint32_t reg,
						mmio_reg_t *p_Value);
unsigned char hdmi_write_mmio_reg(hdmi_device_context_t *p_ctx, uint32_t reg,
						mmio_reg_t p_Value);
typedef enum {
		DIP_AVI,
		DIP_VSD,
		DIP_GMP,
		DIP_SPD,
} dip_type_t;

typedef unsigned char i2c_reg_t;
int hdmi_read_ddc_reg
	(hdmi_device_context_t *p_ctx, unsigned char offset,	i2c_reg_t *p_Value);
int hdmi_read_ddc_edid_header(hdmi_device_context_t *p_ctx, unsigned char offset);
int hdmi_audio_characteristic(hdmi_device_context_t *p_context);
int hdmi_configure(hdmi_device_context_t *p_ctx);
int hdmi_send_eld(hdmi_device_context_t *p_ctx);
int hdmi_avi_info_frame(hdmi_device_context_t *p_context);
int hdmi_spd_info_frame(hdmi_device_context_t *p_context);
int hdmi_vendor_info_frame(hdmi_device_context_t *p_context);
int hdmi_write_dip(hdmi_device_context_t *p_ctx, unsigned char *input,
	unsigned long dip_len, dip_type_t dip_type);
int hdmi_map_audio_video_dip_regs(hdmi_device_context_t *p_ctx);

/* AVI Info Frames version 2 */
typedef struct
{
	unsigned char type;
	unsigned char version;
	unsigned char length;
	unsigned char chksum;
} hdmi_info_header_t;

typedef union
{
	unsigned char data[HDMI_AVI_BUFFER_SIZE];
	struct {
		hdmi_info_header_t	header;
		union {
			unsigned char data1;
			struct {
				unsigned char scan_info: 2;
				unsigned char bar_info: 2;
				unsigned char active_format: 1;
				unsigned char color_space: 2;
				unsigned char reserved1: 1;
			};
		};
		union {
			unsigned char data2;
			struct {
				unsigned char format_aspect_ratio: 4;
				unsigned char pic_aspect_ratio: 2;
				unsigned char colorimetry: 2;
			};
		};
		union {
			unsigned char data3;
			struct {
				unsigned char pic_scaling: 2;
				unsigned char reserved2: 6;
			};
		};
		union {
			unsigned char data4;
			struct {
				unsigned char video_id_code: 7;
				unsigned char reserved3: 1;
			};
		};
		union {
			unsigned char data5;
			struct {
				unsigned char pixel_rep: 4;
				unsigned char reserved4: 4;
			};
		};
		unsigned char bar1;
		unsigned char bar2;
		unsigned char bar3;
		unsigned char bar4;
		unsigned char bar5;
		unsigned char bar6;
		unsigned char bar7;
		unsigned char bar8;
	};
} hdmi_avi_info_t;

typedef union
{
	unsigned char data[HDMI_SPD_BUFFER_SIZE];
	struct {
		hdmi_info_header_t	header;
		unsigned char vendor_name[HDMI_VENDOR_NAME_SIZE];
		unsigned char product_desc[HDMI_VENDOR_DESCRIPTION_SIZE];
		unsigned char source_device;
	};
} hdmi_spd_info_t;

typedef union
{
	unsigned char data[HDMI_VENDOR_INFO_BUFFER_SIZE];
	struct {
		hdmi_info_header_t	header;
		unsigned char ieee[3];
		unsigned char hdmi_video_format; /* last 3 bits are used */
		unsigned char s3d_struct;        /* last 4 bits are used */
		unsigned char s3d_ext_data;      /* last 4 bits are used */
	};
} hdmi_vendor_info_t;

hdmi_audio_port_regs_t g_hdmi_audio_video_dip_regs_a = {AUD_CONFIG_A,
                                                        AUD_MISC_CTRL_A,
                                                        AUD_CTS_ENABLE_A,
                                                        AUD_HDMIW_HDMIEDID_A,
                                                        AUD_HDMIW_INFOFR_A,
                                                        AUD_OUT_DIG_CNVT_A,
                                                        AUD_OUT_STR_DESC_A,
                                                        AUD_CNTL_ST_A,
                                                        VIDEO_DIP_CTL_A,
                                                        VIDEO_DIP_DATA_A,
                                                        };

hdmi_audio_port_regs_t g_hdmi_audio_video_dip_regs_b = {AUD_CONFIG_B,
                                                        AUD_MISC_CTRL_B,
                                                        AUD_CTS_ENABLE_B,
                                                        AUD_HDMIW_HDMIEDID_B,
                                                        AUD_HDMIW_INFOFR_B,
                                                        AUD_OUT_DIG_CNVT_B,
                                                        AUD_OUT_STR_DESC_B,
                                                        AUD_CNTL_ST_B,
                                                        VIDEO_DIP_CTL_B,
                                                        VIDEO_DIP_DATA_B,
                                                        };

hdmi_audio_port_regs_t g_hdmi_audio_video_dip_regs_c = {AUD_CONFIG_C,
                                                        AUD_MISC_CTRL_C,
                                                        AUD_CTS_ENABLE_C,
                                                        AUD_HDMIW_HDMIEDID_C,
                                                        AUD_HDMIW_INFOFR_C,
                                                        AUD_OUT_DIG_CNVT_C,
                                                        AUD_OUT_STR_DESC_C,
                                                        AUD_CNTL_ST_C,
                                                        VIDEO_DIP_CTL_C,
                                                        VIDEO_DIP_DATA_C,
                                                        };

hdmi_audio_port_regs_t g_vlv_hdmi_audio_video_dip_regs_a = {VLV_AUD_CONFIG_A,
                                                            VLV_AUD_MISC_CTRL_A,
                                                            VLV_AUD_CTS_ENABLE_A,
                                                            VLV_AUD_HDMIW_HDMIEDID_A,
                                                            VLV_AUD_HDMIW_INFOFR_A,
                                                            VLV_AUD_OUT_DIG_CNVT_A,
                                                            VLV_AUD_OUT_STR_DESC_A,
                                                            VLV_AUD_CNTL_ST_A,
                                                            VLV_VIDEO_DIP_CTL_A,
                                                            VLV_VIDEO_DIP_DATA_A,
                                                            };

hdmi_audio_port_regs_t g_vlv_hdmi_audio_video_dip_regs_b = {VLV_AUD_CONFIG_B,
                                                            VLV_AUD_MISC_CTRL_B,
                                                            VLV_AUD_CTS_ENABLE_B,
                                                            VLV_AUD_HDMIW_HDMIEDID_B,
                                                            VLV_AUD_HDMIW_INFOFR_B,
                                                            VLV_AUD_OUT_DIG_CNVT_B,
                                                            VLV_AUD_OUT_STR_DESC_B,
                                                            VLV_AUD_CNTL_ST_B,
                                                            VLV_VIDEO_DIP_CTL_B,
                                                            VLV_VIDEO_DIP_DATA_B,
                                                            };

#endif  /*  _HDMI_PORT_H_ */
