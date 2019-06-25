/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: dp_port.h
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
 *  Main include file for DP port driver
 *-----------------------------------------------------------------------------
 */

#ifndef _DP_PORT_H_
#define _DP_PORT_H_

#include <config.h>
#include <igd_pd.h>
#include <pd.h>
#include <gn7/regs.h>
#include <pd_print.h>
#include <intelpci.h>
#include <general.h>
#include <linux/delay.h>
#include <ossched.h>
#include <igd_core_structs.h>

#define CONFIG_VBIOS_RUNTIME 0

/*  ............................................................................ */
#ifndef CONFIG_MICRO
#define MAKE_NAME(x)    x
#define	DP_GET_ATTR_NAME(p_attr)	p_attr->name
#else
#define MAKE_NAME(x)    NULL
#define DP_GET_ATTR_NAME(p_attr)	""
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(p) (sizeof(p)/sizeof((p)[0]))
#endif

#ifndef MAKE_DWORD
#define MAKE_DWORD(byte1,byte2,byte3,byte4) ((unsigned long)byte1|(unsigned long)byte2<<8|(unsigned long)byte3<<16|(unsigned long)byte4<<24)
#endif

#ifndef GET_BYTE
#define GET_BYTE(dwValue, nStartPos)         ((dwValue >> nStartPos) & 0xFF)
#endif

#ifndef SWAP_ENDIAN
#define SWAP_ENDIAN(dwValue)                 ((GET_BYTE(dwValue, 0) << 24) | (GET_BYTE(dwValue, 8) << 16) | (GET_BYTE(dwValue, 16) << 8) | (GET_BYTE(dwValue, 24)))
#endif

/* General DP definition */
#define DP_PORT_B_NAME		"DP B"
#define DP_PORT_C_NAME		"DP C"
#define DP_PORT_B_NUMBER	6
#define DP_PORT_C_NUMBER	7
/* Port Control register */
#define DP_CONTROL_REG_B			0x64100
#define DP_CONTROL_REG_C			0x64200

#define DP_AUX_CH_CTL_B 			0x64110
#define DP_AUX_CH_CTL_C 			0x64210

/* DP Port Data Register */
#define DPB_AUX_CH_DATA1			0x64114
#define DPB_AUX_CH_DATA2			0x64118
#define DPB_AUX_CH_DATA3			0x6411C
#define DPB_AUX_CH_DATA4			0x64120
#define DPB_AUX_CH_DATA5			0x64124

#define DPC_AUX_CH_DATA1			0x64214
#define DPC_AUX_CH_DATA2			0x64218
#define DPC_AUX_CH_DATA3			0x6421C
#define DPC_AUX_CH_DATA4			0x64220
#define DPC_AUX_CH_DATA5			0x64224


/* VLV HD Audio and HDMI registers for Pipe A */
#define VLV_AUD_CONFIG_A                    0x62000
#define VLV_AUD_MISC_CTRL_A                 0x62010
#define VLV_AUD_CTS_ENABLE_A                0x62028
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
#define VLV_AUD_CTS_ENABLE_B                0x62128
#define VLV_AUD_HDMIW_HDMIEDID_B            0x62150
#define VLV_AUD_HDMIW_INFOFR_B              0x62154
#define VLV_AUD_OUT_DIG_CNVT_B              0x62180
#define VLV_AUD_OUT_STR_DESC_B              0x62184
#define VLV_AUD_CNTL_ST_B                   0x621B4
#define VLV_VIDEO_DIP_CTL_B                 0x61170
#define VLV_VIDEO_DIP_DATA_B                0x61174


/* Display Port Configuration Data Address */
#define DPCD_REV							0x00000
#define	DPCD_MAX_LINK_RATE					0x00001
#define DPCD_MAX_LANE_COUNT					0x00002
#define DPCD_MAX_DOWNSPREAD					0x00003
#define DPCD_NO_RX_PORT_COUNT				0x00004
#define DPCD_DOWNSTREAM_PRESENT				0x00005
#define DPCD_MAX_CHANNEL_CODING				0x00006
#define DPCD_DOWNSTREAM_PORT_COUNT			0x00007
#define DPCD_RECEIVE_PORT_0_CAP_0			0x00008
#define DPCD_RECEIVE_PORT_0_CAP_1			0x00009
#define DPCD_RECEIVE_PORT_1_CAP_0			0x0000A
#define DPCD_RECEIVE_PORT_1_CAP_1			0x0000B
#define DPCD_I2C_CONTROL_CAP				0x0000C
#define DPCD_EDP_CONFIGURATION_CAP			0x0000D
#define DPCD_TRAINING_AUX_RD_INTERVAL		0x0000E
#define DPCD_LINK_BW_SET_ADDR				0x00100
#define DPCD_LANE_COUNT_SET_ADDR			0x00101
#define DPCD_TRAINING_PATTERN_SET_ADDR		0x00102
#define DPCD_TRAINING_LANE0_SET_ADDR		0x00103
#define DPCD_TRAINING_LANE1_SET_ADDR		0x00104
#define DPCD_TRAINING_LANE2_SET_ADDR		0x00105
#define DPCD_TRAINING_LANE3_SET_ADDR		0x00106
#define DPCD_DOWNSPREAD_CTRL				0x00107
#define DPCD_MAX_LINK_CHANNEL_CODING_SET	0x00108
#define DPCD_LANE0_1_STATUS_ADDR			0x00202
#define DPCD_LANE2_3_STATUS_ADDR			0x00203
#define DPCD_LANE_ALIGN_STATUS_ADDR			0x00204
#define DPCD_ADJUST_REQUEST_LANE0_1_ADDR	0x00206
#define DPCD_ADJUST_REQUEST_LANE2_3_ADDR	0x00207
#define DPCD_ADJUST_D_STATE_ADDR			0x00600

/* DP / eDP Specific Timing Registers */
#define TransADataM1  	  0x60030
#define TransADataN1  	  0x60034
#define TransADataM2  	  0x60038
#define TransADataN2  	  0x6003C
#define TransADPLinkM1 	  0x60040
#define TransADPLinkN1 	  0x60044
#define TransADPLinkM2 	  0x60048
#define TransADPLinkN2 	  0x6004C

#define TransBDataM1  	  0x61030
#define TransBDataN1  	  0x61034
#define TransBDataM2  	  0x61038
#define TransBDataN2  	  0x61038
#define TransBDPLinkM1 	  0x61040
#define TransBDPLinkN1 	  0x61044
#define TransBDPLinkM2 	  0x61048
#define TransBDPLinkN2 	  0x6104C

/* Panel Fitting */
#define PFIT_CONTROL      0x61230  /* Panel Fitting Control */
#define PFIT_PGM_RATIOS   0x61234  /* Programmed Panel Fitting Ratios */

/* Data Island Packet Definition: Confirm with DP Spec */  
#define DP_ELD_BUFFER_SIZE			256 /* TODO check this */
#define DP_AVI_BUFFER_SIZE			17
#define DP_SPD_BUFFER_SIZE			29
#define DP_AVI_INFO_TYPE				0x82		/* AVI buffer type */
#define DP_SPD_INFO_TYPE				0x83		/* SPD buffer type */
#define DP_VENDOR_NAME				"Intel"
#define DP_VENDOR_DESCRIPTION		    "EMGD Driver"
#define DP_INTEL_VENDOR_NAME_SIZE		6
#define DP_IEGD_VENDOR_DESCRIPTION_SIZE	12
#define DP_VENDOR_NAME_SIZE			8
#define DP_VENDOR_DESCRIPTION_SIZE	16
#define DP_SPD_SOURCE_PC				0x09

#define DP_THIS_PORT(a)  (a->p_callback->port_num==DP_PORT_B_NUMBER)? \
						DP_CONTROL_REG_B:DP_CONTROL_REG_C
#define DP_OTHER_PORT(a) (a->p_callback->port_num==DP_PORT_B_NUMBER)? \
						DP_CONTROL_REG_C:DP_CONTROL_REG_B


#define MAX_DP_DATA_SIZE 16
#define MAX_AUX_NO_RESPONSE_RETRY 150000
#define MAX_WRITE_RETRIES 50
#define MAX_READ_RETRIES 50
#define MAX_DP_ADJUSTMENT_REQUEST 5
#define DP_MAX_PREEMPHASIS_LEVEL        preemp_9_5dB
#define DP_MAX_SWING_LEVEL              voltage_1_2
#define DP_MAX_EQUALIZATION_ITERATION   5
#define AUX_DATAREG_OFFSET 4
#define AUX_MAX_BYTES 5*4

#define DP_LINKBW_1_62_GBPS 0x6
#define DP_LINKBW_2_7_GBPS 0xA

#define PIPEA_CONF             0x70008
#define PIPEB_CONF             0x71008
#define PLANEA_CNTR			0x70180
#define PLANEB_CNTR			0x71180
/* #define DPLLACNTR			0x6014 */
/* #define DPLLBCNTR			0x6018 */

#define M_BITS_MASK						0xFF000000
#define P_BITS_MASK						0xC400F000
#define N_VCO_BITS_MASK					0xFFFF0000
#define DPLL_REFSFRSEL2_PO_FIXVALUE		0x0068a701

/* Sideband Register Offsets */
#define DPLL0_MDIV_REG          0x8008    /* DPLL A */
#define DPLL1_MDIV_REG          0x8028    /* DPLL B */

#define DPLL_LANE_CTRL0_REG             0x0220
#define DPLL_LANE_CTRL1_REG             0x0420
#define DPLL_LANE_CTRL2_REG             0x2620
#define DPLL_LANE_CTRL3_REG             0x2820


typedef unsigned long mmio_reg_t;

static unsigned int rd_intervals[5] =
{
	400, 4000, 8000, 12000, 16000
};

/*
static unsigned long csr_vco_sel [4][2] =
{
	{ 1800000L, 2300000L},
	{ 2300000L, 2750000L},
	{ 2750000L, 3300000L},
	{ 3300000L, 3600000L},
};
*/

/*
static unsigned long csr_cb_tune [4][2] =
{
	{ 0,        2200000L},
	{ 2200000L, 2600000L},
	{ 2600000L, 3100000L},
	{ 3100000L, 9999999L},
};
*/

typedef enum 
{
	voltage_0_4 = 0,
	voltage_0_6,
	voltage_0_8,
	voltage_1_2
}gmch_dp_voltage_swing_level;

typedef enum
{
	preemp_no = 0,
	preemp_3_5dB,
	preemp_6dB,
	preemp_9_5dB
}gmch_dp_preemphasis_level;

typedef enum 
{
    /* Native AUX */
    eAUXWrite = 8,  /* 1000 */
    eAUXRead  = 9,  /* 1001 */

    /* I2C on AUX */
    eI2CAUXWrite = 0,     /* 0000 */
    eI2CAUXRead  = 1,     /* 0001 */
    eI2CAUXWriteStatusReq = 2,  /* 0010 */
    /* I2C with MOT set */
    eI2CAUXWrite_MOT = 4,   /* 0100 */
    eI2CAUXRead_MOT  = 5,   /* 0101 */
    eI2CAUXWriteStatusReq_MOT = 6 /* 0110 */
}aux_request_command_types;

typedef enum
{
    /* Native AUX reply */
    eAUX_ACK = 0,   /* 0000 */
    eAUX_NACK = 1,  /* 0001 */
    eAUX_DEFER = 2, /* 0010 */

    /* I2C over AUX reply */
    eI2CAUX_ACK = 0,   /* 0000 */
    eI2CAUX_NACK = 4,  /* 0100 */
    eI2CAUX_DEFER = 8, /* 1000 */
}aux_reply_command_types;

typedef struct {
	unsigned char content_protection_flag[2];
	struct{
		unsigned char minor_HDCP_rev :4;
		unsigned char major_HDCP_rev :4;
	};

	unsigned char support_CGMS_standards;
	unsigned char hdcp_feature;
} dp_hdcp_support_t;

typedef struct _dp_clock_info {
	bool 			valid; /*1-valid clock programmed, 0-otherwise*/
	bool 			lane_valid; /*1-valid clock programmed, 0-otherwise*/
	unsigned long 	ref_freq;
	unsigned long 	dclk;
} dp_clock_info;


typedef struct _dp_audio_port_regs {
	mmio_reg_t aud_config;
	mmio_reg_t aud_misc_ctrl;
	mmio_reg_t aud_cts_enable;
	mmio_reg_t aud_dpw_dpedid;
	mmio_reg_t aud_dpw_infofr;
	mmio_reg_t aud_out_dig_cnvt;
	mmio_reg_t aud_out_str_desc;
	mmio_reg_t aud_cntl_st;
	mmio_reg_t video_dip_ctl;
	mmio_reg_t video_dip_data;
} dp_audio_port_regs_t;

typedef struct _dp_device_context {
    pd_callback_t             *p_callback;
	pd_attr_t                 *p_attr_table;
	unsigned long              num_attrs;
	pd_timing_t				  *timing_table;
	pd_timing_t               *native_dtd;
	unsigned long			  control_reg;
	unsigned long             power_state;
	dp_hdcp_support_t hdcp_cap;
	unsigned short            chipset;

	unsigned short		fp_width;
	unsigned short		fp_height;
	unsigned long		pipe;
	pd_timing_t			*current_mode;
	unsigned short		aspect_ratio;
	unsigned long		text_tune;
	unsigned short         	core_freq;
	unsigned long 	 		pwm_intensity;
	unsigned long  			inverter_freq;
	unsigned long  			blm_legacy_mode;

	unsigned long		dpcd_version;
	unsigned long		dpcd_max_rate;
	unsigned long		dpcd_max_count;
	unsigned long		dpcd_max_spread;
	unsigned long		dpcd_rd_interval;
	unsigned long		embedded_panel;
	unsigned short		bits_per_color;		
	unsigned short		edid_present;
	dp_clock_info		clock_info_pipe[2];
	bool				pwm_status[2];
	bool				dpcd_info_valid[2];
	unsigned long		pwr_method;
	dp_audio_port_regs_t    *dp_audio_port_regs;
} dp_device_context_t;

typedef struct _dp_aux_control_t {
	union{
		mmio_reg_t reg;
		struct{
			mmio_reg_t x2_clock_divider		: 11;
			mmio_reg_t double_precharge		: 1;
			mmio_reg_t disable_de_glitch	: 1;
			mmio_reg_t sync_clock_recovery	: 1;
			mmio_reg_t invert_manchester	: 1;
			mmio_reg_t Aksv_buffer			: 1;
			mmio_reg_t precharge_time		: 4;
			mmio_reg_t message_size			: 5;
			mmio_reg_t receive_error		: 1;
			mmio_reg_t timeout_timer		: 2;
			mmio_reg_t timeout_error		: 1;
			mmio_reg_t enable_interrupt		: 1;
			mmio_reg_t transaction_done		: 1;
			mmio_reg_t send_or_busy			: 1;
		};
	};
}dp_aux_control_t;

/* Data registers (DPn_AUX_CTRL_REG+4 to DPn_AUX_CTRL_REG+0x24 - 5 registers of 4 bytes each = 20 bytes) 
 * Note: Data transmit order is MSB --> LSB within a data register
 * Between data registers order Data register 1 --> 5
 */
typedef struct _dp_aux_data_t {
	union{
		mmio_reg_t reg;
		struct{
			mmio_reg_t Data4	:8; /* Bits 7:0 */
			mmio_reg_t Data3	:8;
			mmio_reg_t Data2	:8;
			mmio_reg_t Data1	:8; /* First byte to transmit */
		};
	};
}dp_aux_data_t;

typedef struct _dp_aux_write_syntax_t {
	union{
		unsigned long auxHeader;
		struct{
			unsigned long address19_16	:4;
			unsigned long command		:4; /* UMG: This will be transmitted first */
			unsigned long address15_8	:8;
			unsigned long address7_0	:8;
			unsigned long length		:8;
		}Request;
	};
	unsigned char auxData[MAX_DP_DATA_SIZE];	/* 16 bytes of data auxData[0] will be transmitted first */
}dp_aux_write_syntax_t;

typedef struct _dp_aux_read_syntax_t {
	union{
		unsigned long auxHeader;
		struct{
			unsigned long address19_16	:4;
			unsigned long command		:4; /* UMG: This will be transmitted first */
			unsigned long address15_8	:8;
			unsigned long address7_0	:8;
			unsigned long length		:8;
		}Request;
	};
}dp_aux_read_syntax_t;

/* Reply for Writing to AUX
 * SYNC < COMM3:0|0000 < (DATA0-7:0 < ) STOP
 * Reply for Write: Completed write or Incomplete write
 *  SYNC < ACK|0000 < STOP
 *  SYNC < NACK|0000 < DATA0-7:0 < STOP
 */
typedef struct _dp_aux_reply_forwrite_syntax_t {
    /* First byte will be the reply command info */
    union
    {
        unsigned char header;
        struct
        {
            unsigned char reserved    : 4; /* Has to be 0 */
            unsigned char command     : 4; /* This will be read first (can be ACK or NACK) */
        }Reply;
    };

    /* For Native Aux
     * One extra byte "in case of incomplete write" indicating the #bytes written to sink
	 */
    unsigned char bytesWritten;
}dp_aux_reply_forwrite_syntax_t;

/* Reply for Reading from AUX: 
 * 1. Command received but do not have requested data: 
 *     SYNC < NACK|0000 < STOP
 * 2. Receiver is not ready to reply with the read data: 
 *     SYNC < DEFER|0000 < STOP
 * 3. Receiver is ready to transmit back all requested data:
 *     SYNC < ACK|0000 < DATA0-7:0 <  < DATAN-7:0 < STOP
 * 4. Receiver is ready to transmit only M (< N) data bytes:
 *     SYNC < ACK|0000 < DATA0-7:0 <  < DATAM-7:0 < STOP
 */
typedef struct _dp_aux_reply_forread_syntax_t {
    /* First byte will be the reply command info */
    union
    {
        unsigned char header;
        struct
        {
            unsigned char reserved    : 4; /* Has to be 0 */
            unsigned char command     : 4; /* This will be read first (can be ACK or NACK) */
        }Reply;
    };

    /* Return data valid if command == ACK
     * Size of this is (Message size returned by GMCH - 1) bytes
	 * changed the max size to 20
	 */
    unsigned char data[AUX_MAX_BYTES];
}dp_aux_reply_forread_syntax_t;

typedef struct _dp_adjust_d_state_t {
	union{
		unsigned char value;
		struct
		{
			unsigned char set_power_state	:2;
			unsigned char reserved			:6;
		};
	};
}dp_adjust_d_state_t;

typedef struct _dp_training_pattern_set {
	union{
		unsigned char value;
		struct
		{
			unsigned char training_pattern_set		:2;
			unsigned char link_qual_pattern_set		:2;
			unsigned char recovered_clock_out_en	:1;
			unsigned char scrambling_disable		:1;
			unsigned char symbol_error_count_sel	:2;
		};
	};
}dp_training_pattern_set;

typedef struct _dp_training_lane_set {
	union{
		unsigned char value;
		struct
		{
			unsigned char voltage_swing_set	:2;
			unsigned char max_swing			:1;
			unsigned char preemphasis_set	:2;
			unsigned char max_preemphasis	:1;
			unsigned char reserved			:2;
		};
	};
}dp_training_lane_set;

typedef struct _dp_lane_count_t {
	union{
		unsigned char value;
		struct
		{
			unsigned char lane_count_set	:5;
			unsigned char reserved			:2;
			unsigned char enhanced_frame_en	:1;
		};
	};
}dp_lane_count_t;

typedef struct _dp_lane_status {
	union{
		unsigned char value;
		struct
		{
			unsigned char lane0_2_cr_done		:1;
			unsigned char lane0_2_eq_done		:1;
			unsigned char lane0_2_symbol_locked	:1;
			unsigned char reserved_0			:1;
			unsigned char lane1_3_cr_done		:1;
			unsigned char lane1_3_eq_done		:1;
			unsigned char lane1_3_symbol_locked	:1;
			unsigned char reserved_1			:1;
		};
	};
}dp_lane_status;

typedef struct _dp_adjust_request_lane_t {
	union{
		unsigned char value;
		struct
		{
			unsigned char vswing_lane_0_2	:2;
			unsigned char preemphasis_0_2	:2;
			unsigned char vswing_lane_1_3	:2;
			unsigned char preemphasis_1_3	:2;
		};
	};
}dp_adjust_request_lane_t;

typedef struct _dp_lane_align_status_updated_t {
	union{
		unsigned char value;
		struct
		{
			unsigned char interlane_align_done	:1;
			unsigned char reserved				:5;
			unsigned char down_port_updated		:1;
			unsigned char link_status_updated	:1;
		};
	};
}dp_lane_align_status_updated_t;

unsigned char dp_read_mmio_reg(dp_device_context_t *p_ctx, unsigned long reg,
						mmio_reg_t *p_Value);
unsigned char dp_write_mmio_reg(dp_device_context_t *p_ctx, unsigned long reg,
						mmio_reg_t p_Value);

typedef enum {
		DP_DIP_AVI,
		DP_DIP_VSD,
		DP_DIP_SPD,
} dp_dip_type_t;

typedef unsigned char i2c_reg_t;
int dp_read_i2c_reg
	(dp_device_context_t *p_ctx, unsigned char offset,	i2c_reg_t *p_Value);
int dp_read_ddc_reg
	(dp_device_context_t *p_ctx, unsigned char offset,	i2c_reg_t *p_Value);
int dp_audio_characteristic(dp_device_context_t *p_context);
int dp_configure(dp_device_context_t *p_ctx);
int dp_send_eld(dp_device_context_t *p_ctx);
int dp_avi_info_frame(dp_device_context_t *p_context);
int dp_spd_info_frame(dp_device_context_t *p_context);
int dp_write_dip(dp_device_context_t *p_ctx, unsigned char *input,
	unsigned long dip_len, dp_dip_type_t dip_type);
int dp_map_audio_video_dip_regs(dp_device_context_t *p_ctx);

int dp_link_training(void *p_ctx, pd_timing_t *p_mode, unsigned long max_rate, unsigned long flags);
int dp_read_rx_capability(dp_device_context_t *p_ctx);
int dp_write_dpio_register(dp_device_context_t *p_ctx, unsigned long reg, unsigned long value);
unsigned long dp_read_dpio_register(dp_device_context_t *p_ctx, unsigned long reg);
int dp_dump_vswing_preemp_dpio(dp_device_context_t *p_ctx);
int dp_program_mntu_regs(dp_device_context_t *p_ctx, pd_timing_t *p_mode, unsigned long flags, unsigned long rate, unsigned long bpc);
int dp_program_clock(dp_device_context_t *p_ctx, pd_timing_t *p_mode, unsigned long link_rate, unsigned long flags);
int dp_program_dpll(dp_device_context_t *p_ctx, unsigned long link_rate, unsigned long flags);
int dp_program_lane(dp_device_context_t *p_ctx, unsigned long flags);

unsigned long dpcdRead(dp_device_context_t *p_ctx, unsigned char ddi_cnt, unsigned long addr, unsigned char byte_no);
int dpcdWrite(dp_device_context_t *p_ctx, unsigned char ddi_cnt, unsigned short addr, unsigned char byte_no, unsigned long data);

#ifndef CONFIG_MICRO 
/* Function declaration for driver only */
unsigned int SendCommand_AUX(dp_device_context_t *p_ctx, dp_aux_port_t *aux);
unsigned int Start_Xmit(dp_device_context_t *p_ctx, unsigned long portAddr, unsigned long numByte, unsigned long *retData);
#endif

/* AVI Info Frames version 2 */
typedef struct
{
	unsigned char type;
	unsigned char version;
	unsigned char length;
	unsigned char chksum;
} dp_info_header_t;

typedef union
{
	unsigned char data[DP_AVI_BUFFER_SIZE];
	struct {
		dp_info_header_t	header;
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
} dp_avi_info_t;

typedef union
{
	unsigned char data[DP_SPD_BUFFER_SIZE];
	struct {
		dp_info_header_t	header;
		unsigned char vendor_name[DP_VENDOR_NAME_SIZE];
		unsigned char product_desc[DP_VENDOR_DESCRIPTION_SIZE];
		unsigned char source_device;
	};
} dp_spd_info_t;

/*	============================================================================
	Function	:	dp_set_power is called to change the power state of the
					device

	Parameters	:	p_context: Pointer to port driver allocated context structure
					state	: New power state

	Remarks 	:
	
				IEGD  SPWG     eDP    Program bits     min     max		Description
			 *  ----  ----     --    ---------------- ------ ---------  ------------
			 *  T1    T1+T2    T3     0x61208 [28:16]  1 ms   400 ms    Power-Up Delay
			 *  T2    T5       T7     0x61208 [12:00]  0 ms   50  ms    Power-On to backlight Enable Delay
			 *  T3    TX       T9     0x6120C [12:00]  - ms   -   ms    Backlight-Off to Power-Down Delay   
			 *  T4    T3       T10    0x6120C [28:16]  0 ms   500 ms    Power-Down Delay
			 *  T5    T4       T12    0x61210 [04:00]  500 ms  -  ms    Power Cycle Delay
				
				eDP power sequence table	eDP spec (Description from BSpec)
											------------------------------------------
				200	ms		 				T3 (Power-Up Delay T1+T2)
				1	ms 			 			T7 (Power-On to backlight Enable Delay T3)
				200	ms		  				T9 (Backlight-Off to Power-Down Delay T4)
				50	ms		  				T10 (Power-Down Delay T5)
				500	ms		  				T12 (Power Cycle Delay)
			(Note: Convert to 100us when writing to registers)
					*  Reg     = Value
					*  ------    -------
					*  0x61208 = [T1 T2]
					*  0x6120C = [T4 T3]
					*  0x61210 = [   T5]

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
/* Tables required by the optimization codes in dp_set_power() */
typedef struct {
	unsigned char id1;
	unsigned char id2;
	unsigned char bit;
	unsigned long reg_offset;
} opt_set_power_t;

/* Sideband Register Offsets - DPLL A */
#define DPLLA_M_K_N_P_DIV       0x800C    /* M-K-N-P-Divisor */
#define DPLLA_EN_DCLK                   0x801C     /* PLLA_DWORD7 */
#define DPLLA_REF_CLOCK_SRC             0x8014          /*write this register with 0x0570CC00*/
#define DPLLA_DG_LOOP_FILTER    0x8048          /*Digital Loop Filter*/
#define DPLLA_EN_DCLKP                  0x801C          /*Enable dclkp to core*/
#define DPLLA_CML_TLINE			0x804C      /*CML Tline need to set to x8 lane*/

/* Sideband Register Offsets - DPLL B */
#define DPLLB_M_K_N_P_DIV       0x802C    /* M-Divisor */
#define DPLLB_EN_DCLK                   0x803C     /* PLLA_DWORD7 */
#define DPLLB_REF_CLOCK_SRC             0x8034          /*write this register with 0x0570CC00*/
#define DPLLB_DG_LOOP_FILTER    0x8068          /*Digital Loop Filter*/
#define DPLLB_EN_DCLKP                  0x803C          /*Enable dclkp to core*/
#define DPLLB_CML_TLINE			0x806C      /*CML Tline need to set to x8 lane*/


dp_audio_port_regs_t g_vlv_dp_audio_video_dip_regs_a = {VLV_AUD_CONFIG_A,
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

dp_audio_port_regs_t g_vlv_dp_audio_video_dip_regs_b = {VLV_AUD_CONFIG_B,
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


#endif  /*  _DP_PORT_H_ */
