/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: dp_port.c
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
 *  Port driver interface functions
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.port

#include "dp_port.h"

/* This constant = 10,000,000.  The value is used to
 * get effective results from the integer math, and
 * to not divide by 0. */
#define PWM_FREQ_CALC_CONSTANT_1        0x00989680
/* This constant is 1,000,000 - to multiply to get
 * the Display Clock Frequency to the order of Mhz */
#define PWM_FREQ_CALC_CONSTANT_2        0x000F4240
/*Maximum bit field for PWM freq*/
#define BLC_MAX_PWM_CTL_REG_FREQ_VALUE 0xFFFF

/* The value for PWM freq 
 * is based on common LVDS and eDP.*/
#define INVERTER_FREQ_DEFAULT 0x1DF /* Exceed typical Freq of majority eDP panel - 479 Hz*/
#define INVERTER_FREQ_MAX 0x4E20 /* This is 20 kHz - Typical maximum freq for eDP panel */
#define INVERTER_FREQ_MIN 0x64 /* This is 100 Hz - Typical minimum freq for eDP panel  */

/* Include supported chipset here */
static unsigned long supported_chipset[] =
{
	PCI_DEVICE_ID_VGA_SNB,
	PCI_DEVICE_ID_VGA_IVB,
	PCI_DEVICE_ID_VGA_VLV,
	PCI_DEVICE_ID_VGA_VLV2,
	PCI_DEVICE_ID_VGA_VLV3,
	PCI_DEVICE_ID_VGA_VLV4,
};

/* .......................................................................... */
int dp_open(pd_callback_t *p_callback, void **p_context);
int dp_init_device(void *p_context);
int dp_get_timing_list(void *p_context, pd_timing_t *p_in_list,
	pd_timing_t **pp_out_list);
int dp_set_mode(void *p_context, pd_timing_t *p_mode, unsigned long flags);
int dp_post_set_mode(void *p_context, pd_timing_t *p_mode,
	unsigned long status);
int dp_get_attributes(void *p_context, unsigned long *p_num_attr,
	pd_attr_t **pp_list);
int dp_set_attributes(void *p_context, unsigned long num_attr,
	pd_attr_t *p_list);
unsigned long dp_validate(unsigned long cookie);
int dp_close(void *p_context);
int dp_set_power(void *p_context, uint32_t state);
int dp_get_power(void *p_context, uint32_t *p_state);
int dp_save(void *p_context, void **pp_state, unsigned long flags);
int dp_restore(void *p_context, void *p_state, unsigned long flags);
int dp_get_port_status(void *p_context, pd_port_status_t *port_status);
int dp_init_attribute_table(dp_device_context_t *p_ctx);
int dp_pipe_info(void *p_context, pd_timing_t *p_mode, unsigned long flags);
int dp_port_clock(void *p_context, pd_timing_t *p_mode, unsigned long dclk, unsigned long flags, unsigned long bpc);
int dp_get_brightness(void *p_context);
int dp_set_brightness(void *p_context, int level);
int dp_pre_link_training(void *p_context, pd_timing_t *p_mode, unsigned long flags);
static void edp_panel_fit(dp_device_context_t *p_ctx);
static void edp_enable_vdd(dp_device_context_t *p_ctx, int vdd);
static void edp_panel_protect(dp_device_context_t *p_ctx, int protect);
static void edp_panel_backlight(dp_device_context_t *p_ctx, int backlight);
static void edp_panel_power(dp_device_context_t *p_ctx, int power);
static void edp_panel_port_select(dp_device_context_t *p_ctx, int select);
static int edp_panel_pwm(dp_device_context_t *p_ctx);
static int dp_set_vswing_preemphasis(void *p_ctx, unsigned long vswing,
							  unsigned long preemphasis);

static int dp_perform_equalization(void *p_context,
		unsigned long vol_level, unsigned long emphasis_level);
static int dp_get_request_equalization(void *p_context,
		unsigned long* ptr_vswing, unsigned long* ptr_preemp);
static int dp_set_training_level(void *p_context, unsigned long vswing,
		unsigned long preemphasis, unsigned long max_vswing, unsigned long max_preemp);
static int dp_wait_vblank(dp_device_context_t *p_ctx, unsigned long flags);

static pd_version_t  g_dp_version = {1, 0, 0, 0};
/* May include dab list for SDVO here if we would like to do SDVO card detection
here instead of HAL */
static unsigned long g_dp_dab_list[] = {0x70, 0x72, PD_DAB_LIST_END};

static pd_driver_t	 g_dp_drv = {
	PD_SDK_VERSION,
	"DP Port Driver",
	0,
	&g_dp_version,
	PD_DISPLAY_DP_INT,
	PD_FLAG_CLOCK_MASTER | PD_FLAG_UP_SCALING,
	g_dp_dab_list,
	1000,
	dp_validate,
	dp_open,
	dp_init_device,
	dp_close,
	dp_set_mode,
	dp_post_set_mode,
	dp_set_attributes,
	dp_get_attributes,
	dp_get_timing_list,
	dp_set_power,
	dp_get_power,
	dp_save,
	dp_restore,
	dp_get_port_status,
	dp_pipe_info,
	dp_link_training,
	dp_pre_link_training,
	dp_port_clock,
	dp_get_brightness,
	dp_set_brightness,
};

/* ............................ DP Attributes ............................ */
static pd_attr_t edp_attrs_data[] =
/*	ID,	Type, */
/*Name,	Flag, default_value, current_value,pad0/min, pad1/max, pad2/step */
{
 		/*Power sequnce timing attributes*/
         /*<-------ID----------->              		<----TYPE-------->  <---NAME----->  	<----flag---->            <DEF_VAL> <CUR_VAL>   min  max  st */
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_METHOD,   	PD_ATTR_TYPE_BOOL, 	"FP Power Method",  PD_ATTR_FLAG_USER_INVISIBLE, 0,   0,        0,   0,     0),
	PD_MAKE_ATTR(PD_ATTR_ID_FP_BACKLIGHT_EN,	PD_ATTR_TYPE_BOOL,	"FP Backlight",	    PD_ATTR_FLAG_USER_INVISIBLE, 0,   1,	0,   0,     0),
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_T1,   		PD_ATTR_TYPE_RANGE, "FP Power T1",  	PD_ATTR_FLAG_USER_INVISIBLE, 200,     0,        0,   4095,  1),
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_T2,   		PD_ATTR_TYPE_RANGE, "FP Power T2",  	PD_ATTR_FLAG_USER_INVISIBLE, 1,       0,        0,   4095,  1),
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_T3,   		PD_ATTR_TYPE_RANGE, "FP Power T3",  	PD_ATTR_FLAG_USER_INVISIBLE, 200,     0,        0,   4095,  1),
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_T4,   		PD_ATTR_TYPE_RANGE, "FP Power T4",  	PD_ATTR_FLAG_USER_INVISIBLE, 50,      0,        0,   4095,  1),
        PD_MAKE_ATTR (PD_ATTR_ID_FP_PWR_T5,   		PD_ATTR_TYPE_RANGE, "FP Power T5",  	PD_ATTR_FLAG_USER_INVISIBLE, 500,     0,        0,   4095,  1),
 		PD_MAKE_ATTR (PD_ATTR_ID_PWM_INTENSITY,   	PD_ATTR_TYPE_RANGE, "PWM cycle",		0, 
											MAX_BRIGHTNESS,     MAX_BRIGHTNESS,        1,   MAX_BRIGHTNESS,   1),
		PD_MAKE_ATTR (PD_ATTR_ID_INVERTER_FREQ,		PD_ATTR_TYPE_RANGE, "Inverter Frequency",	0,   
											INVERTER_FREQ_DEFAULT,        0,     INVERTER_FREQ_MIN,  INVERTER_FREQ_MAX,  1),
};

static pd_attr_t dp_attrs_data[] = 
{
	PD_MAKE_ATTR(PD_ATTR_ID_DPCD_MAX, PD_ATTR_TYPE_RANGE,
		"DP Port Maximum DPCD Rate", PD_ATTR_FLAG_USER_INVISIBLE, 0, 
		0, DP_LINKBW_1_62_GBPS, DP_LINKBW_2_7_GBPS, 1),

	PD_MAKE_ATTR(PD_ATTR_ID_DP_DPCD_BPC, PD_ATTR_TYPE_RANGE,
		"DP/eDP Bits Per Color", PD_ATTR_FLAG_USER_INVISIBLE,
		0, 0, IGD_DISPLAY_BPC_6, IGD_DISPLAY_BPC_16, 4),

	PD_MAKE_ATTR(PD_ATTR_ID_PANEL_FIT, PD_ATTR_TYPE_BOOL,
		"Panel Upscale", 0,
		0, 0, 0, 0, 0),

	PD_MAKE_ATTR(PD_ATTR_ID_MAINTAIN_ASPECT_RATIO, PD_ATTR_TYPE_BOOL,
		"Maintain Aspec Ratio", 0,
		0, 0, 0, 0, 0),
	
	PD_MAKE_ATTR(PD_ATTR_ID_TEXT_TUNING, PD_ATTR_TYPE_RANGE,
		"Text Enhancement", 0,
		0, 0, DP_TEXT_TUNING_FUZZY, DP_TEXT_TUNING_MEDIAN, 1),
};

/* .......................................................................... */
/* .......................................................................... */
/*============================================================================
	Function	:	pd_init is the first function that is invoked by IEG driver.

	Parameters	:	handle : not used

	Remarks     :   pd_init initializes pd_driver_t structure and registers the
					port driver with IEG driver by calling pd_register function

	Returns     :	Status returned by pd_register function.
	------------------------------------------------------------------------- */
int PD_MODULE_INIT(dp_init, (void *handle))
{
	int status;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: pd_init()");

	status = pd_register(handle, &g_dp_drv);
	if (status != PD_SUCCESS) {
		EMGD_DEBUG("dp: Error ! pd_init: pd_register() failed with "
			"status=%#x", status);
	}
	EMGD_TRACE_EXIT;
	return status;
}


/*----------------------------------------------------------------------
 * Function: dp_exit()
 *
 * Description: This is the exit function for dp port driver to unload
 *              the driver.
 *
 * Parameters:  None.
 *
 * Return:      PD_SUCCESS(0)  success
 *              PD_ERR_XXXXXX  otherwise
 *----------------------------------------------------------------------*/
int PD_MODULE_EXIT(dp_exit, (void))
{
	EMGD_TRACE_ENTER;
	EMGD_TRACE_EXIT;
	return (PD_SUCCESS);
} /* end dp_exit() */


/*	============================================================================
	Function	:	dp_open is called for each combination of port and dab
					registers to detect the dp device.

	Parameters	:	p_callback : Contains pointers to read_regs/write_regs
								functions to access I2C registes.

					pp_context	  : Pointer to port driver allocated context
								structure is returned in this argument

	Remarks 	:	dp_open detects the presence of dp device for specified
					port.

	Returns 	:	PD_SUCCESS If dp device is detected
					PD_ERR_xxx On Failure
	------------------------------------------------------------------------- */
int dp_open(pd_callback_t *p_callback, void **pp_context)
{
	dp_device_context_t *p_ctx;
	pd_reg_t reg_list[2];
	unsigned short chipset;
	int ret,i,valid_chipset = 0;
	pd_attr_t   *dp_attr      = NULL; 

	EMGD_TRACE_ENTER;
	EMGD_DEBUG ("Enter dp_open()");

	if (!p_callback || !pp_context) {
		EMGD_ERROR("Invalid parameter");
		return (PD_ERR_NULL_PTR);
	}

	/* Verify that this is an GMCH with Internal DP available */
	reg_list[0].reg = 2;
	reg_list[1].reg = PD_REG_LIST_END;
	ret = p_callback->read_regs(p_callback->callback_context, reg_list, PD_REG_PCI);
	if(ret != PD_SUCCESS) {
		EMGD_ERROR("Read regs failed.");
		return ret;
	}
	chipset = (unsigned short)(reg_list[0].value & 0xffff);
	/* Loop through supported chipset */
	for (i = 0; i < ARRAY_SIZE(supported_chipset); i++) {
		if (chipset == supported_chipset[i]){
			valid_chipset = 1;
			break;
		}
	}
	if(!valid_chipset){
		EMGD_ERROR("Not valid chipset.");
		return PD_ERR_NODEV;
	}

	/* Allocate DP context */
	p_ctx = pd_malloc(sizeof(dp_device_context_t));
	if (p_ctx == NULL) {
		EMGD_ERROR("dp: Error ! dp_open: pd_malloc() failed");
		return PD_ERR_NOMEM;
	}
	pd_memset(p_ctx, 0, sizeof(dp_device_context_t));
	*pp_context = p_ctx;
	p_ctx->p_callback = p_callback;
	p_ctx->chipset = chipset;

	/*FIXME: eDP workaround here*/

	if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
		p_ctx->control_reg = DP_CONTROL_REG_B;
	}else if(p_ctx->p_callback->port_num == DP_PORT_C_NUMBER){
		p_ctx->control_reg = DP_CONTROL_REG_C;
	}else{
		EMGD_ERROR("Unsupported port number for internal DP");
		return PD_ERR_INTERNAL;
	}
	g_dp_drv.type = PD_DISPLAY_DP_INT;


	/*	Create attribute table if not already created */
	if (p_ctx->p_attr_table == NULL) {
		dp_init_attribute_table(p_ctx);
	}
 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_FP_PWR_METHOD, 0);
	p_ctx->pwr_method = dp_attr->current_value;
	p_ctx->power_state = PD_POWER_MODE_D0;  /* default state is ON */

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_MAINTAIN_ASPECT_RATIO, 0);
	p_ctx->aspect_ratio = dp_attr->current_value; /* default is OFF */

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_TEXT_TUNING, 0);
	p_ctx->text_tune = dp_attr->current_value; /* default is FUZZY */

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_PWM_INTENSITY, 0);
	if ((dp_attr->current_value >= 1) && (dp_attr->current_value <=100)){
		p_ctx->pwm_intensity = dp_attr->current_value; /* PWM Intensity in %*/
	} else {
		p_ctx->pwm_intensity = dp_attr->default_value; /* PWM Intensity 100%*/
	}

	/* Inverter Frequency for PWM Backlight. */
 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_INVERTER_FREQ, 0);
	/* EMGD_DEBUG ("Inverter Freq Attr = %lu", dp_attr->current_value);*/
	if ((dp_attr->current_value >= INVERTER_FREQ_MIN) 
			&& (dp_attr->current_value <= INVERTER_FREQ_MAX)) {
		p_ctx->inverter_freq = dp_attr->current_value; 
		EMGD_DEBUG ("Current Inverter Freq =%ld", p_ctx->inverter_freq);
	} else {
		p_ctx->inverter_freq = dp_attr->default_value; 
		EMGD_DEBUG ("Default Inverter Freq =%ld", p_ctx->inverter_freq);
	}

	p_ctx->edid_present = 0;
	p_ctx->blm_legacy_mode = 0;			/* FIXME: MT System doesnt have BLM Legacy Mode */

	p_ctx->clock_info_pipe[0].valid = FALSE;
	p_ctx->clock_info_pipe[0].lane_valid = FALSE;
	p_ctx->clock_info_pipe[0].ref_freq = 0;
	p_ctx->clock_info_pipe[0].dclk = 0;

	p_ctx->clock_info_pipe[1].valid = FALSE;
	p_ctx->clock_info_pipe[1].lane_valid = FALSE;
	p_ctx->clock_info_pipe[1].ref_freq = 0;
	p_ctx->clock_info_pipe[1].dclk = 0;
	
	p_ctx->pwm_status[0] = FALSE;	
	p_ctx->pwm_status[1] = FALSE;

	p_ctx->dpcd_info_valid[0] = FALSE;	
	p_ctx->dpcd_info_valid[1] = FALSE;	

 	/* Note: This is ported over from LVDS for PWM Control. 
	 * The ifdefs and the condition is changed, because PLB/TNC is not supporting
	 * eDP. */
	/* Note: The method to obtain graphics frequency is same as PLB/TNC,by sending 
	 * an opcode to port 5 in the SCH Message Network. We call the read_regs with
 	 * PD_REG_BRIDGE_OPCODE specifically for this purpose only.
	 *
 	 * The input for this read_reg is the opcode that register data that
 	 * we send into the Message control register*/
	reg_list[0].reg =  200; /* initialize defaults least common MHz */
	reg_list[1].reg = PD_REG_LIST_END;
	ret = p_callback->read_regs(p_callback->callback_context,
			reg_list, PD_REG_BRIDGE_OPCODE);
	if(ret != PD_SUCCESS) {
		p_ctx->core_freq = 200; /* Lowest possible SKU as backup.*/
		EMGD_ERROR ("DEVICE CONTEXT is NULL!");
	} else {
		/*set the graphics frequency*/
		p_ctx->core_freq = (unsigned short) reg_list[0].value;
		EMGD_DEBUG("dp: core_freq is 0x%08x ", reg_list[0].value);
	}


	EMGD_DEBUG ("Exit dp_open() successfully.");
	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

int dp_get_brightness (void *p_context){
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	return p_ctx->pwm_intensity;
}

int dp_set_brightness (void *p_context, int level){
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	int max_brightness = 0;
	int real_brightness = 0;
	unsigned long blc_pwm_ctl = 0;
	unsigned long bl_ctl_reg = 0;
	unsigned long temp = 0;

	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("Setting brightness for Pipe A");
		bl_ctl_reg = 0x61254;
	} else {
		EMGD_DEBUG ("Setting brightness for Pipe B");
		bl_ctl_reg = 0x61354;
	}
	
	dp_read_mmio_reg(p_ctx, bl_ctl_reg, &temp);
	max_brightness = temp >> 16;

	
	if (level == MAX_BRIGHTNESS) {
		real_brightness = max_brightness;
	} else if( level == 0 ){
		real_brightness = 0; /* Turn off  */
	} else {
		real_brightness = (level * max_brightness) / 100;
	}	

	dp_read_mmio_reg(p_ctx, bl_ctl_reg, &blc_pwm_ctl);
	blc_pwm_ctl &= 0xFFFF0000; 
	
	dp_write_mmio_reg(p_ctx, bl_ctl_reg, blc_pwm_ctl | real_brightness);

	p_ctx->pwm_intensity = level;

	return 0;

}

int dp_pipe_info(void *p_context, pd_timing_t *p_mode, unsigned long flags){

	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned long aux_ctrl, vdd_delay = 0;
	pd_attr_t      *dp_attr      = NULL; 
	int ret;

	EMGD_TRACE_ENTER;

	EMGD_DEBUG ("flags = %ld", flags);

	p_ctx->current_mode = p_mode;
	p_ctx->pipe = flags;

	/*	Create attribute table if not already created */
	if (p_ctx->p_attr_table == NULL) {
		dp_init_attribute_table(p_ctx);
	}
 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_FP_PWR_METHOD, 0);
	p_ctx->pwr_method = dp_attr->current_value;

	EMGD_DEBUG ("DPCD version = %lX, max_rate = %lX, max_count = %lX\n",
		p_ctx->dpcd_version,
		p_ctx->dpcd_max_rate,
		p_ctx->dpcd_max_count);

	if (p_ctx->dpcd_info_valid[flags - 1] == TRUE){
		if ((p_ctx->dpcd_version == 0x0 ) ||
			((p_ctx->dpcd_max_rate != DP_LINKBW_1_62_GBPS) && 
				(p_ctx->dpcd_max_rate != DP_LINKBW_2_7_GBPS) &&
				(p_ctx->dpcd_max_rate != 0x14 ))||  /* 5.4 Gbps */
			(((p_ctx->dpcd_max_count&0xF) != 0x1)&& ((p_ctx->dpcd_max_count&0xF) != 0x2) && 
				((p_ctx->dpcd_max_count&0xF) != 0x4) )){
			p_ctx->dpcd_info_valid[flags - 1] = FALSE;
		} else {
			return 0;
		}
	}

	/* The sink(panel)  must support AUX_CH polling by
 	* the source (port). So it is ok to try first without
 	* enabling VDD.	*/	

	aux_ctrl = (0x00100000 | ((unsigned long)0xa << 16) | 0x64);
	dp_write_mmio_reg(p_ctx, p_ctx->control_reg+0x10, aux_ctrl);

	ret = dp_read_rx_capability(p_ctx);

	if ((ret != PD_SUCCESS) && p_ctx->pwr_method){
		EMGD_DEBUG ("Second try of reading DPCD. This only happen with eDP.");
		edp_panel_protect(p_ctx, FALSE);
		if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
			edp_panel_port_select (p_ctx, 1); /* 1 for eDP B */
		} else {
			edp_panel_port_select (p_ctx, 2); /* 2 for eDP C */
		}
		/* We need to enable VDD before DPCD read until Link Training is done. */
		edp_enable_vdd(p_ctx, TRUE);	
		/* Need  T3 delay before we can access DPCD after VDD turned on.*/
		udelay (500); /* Power rail rise time, 0.5 ms*/ 

 		dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_FP_PWR_T1, 0);
		vdd_delay = dp_attr->current_value;
		
		EMGD_DEBUG ("eDP: VDD Rise delay = %ld",vdd_delay);
		mdelay (vdd_delay);		

		ret = dp_read_rx_capability(p_ctx);

		/*FIXME: This is a temporary workaround for eDP on Pipe B.
 		* I am guessing we need to modify our P-P-P programming order.	
 		* Port should be enabled earlier. */
		if (ret && (flags & PD_SET_MODE_PIPE_B)){
			unsigned long dpll= 0, dpll_ctl = 0, temp = 0;	
			unsigned long timeout = 0;
			unsigned long ref_clk_src, dg_loop_filter, en_idclkp, cml_tline;
			unsigned long ctrl_reg = 0;
	
			EMGD_DEBUG ("PIPE B Workaround for eDP.");
			dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &ctrl_reg);

			if (flags & PD_SET_MODE_PIPE_B) {
				/* This is the pipe B */
				ctrl_reg |= BIT(30);
			}

			/* Turn on port */
			ctrl_reg |= BIT31;
			dp_write_mmio_reg(p_ctx, p_ctx->control_reg ,
							ctrl_reg);

			ref_clk_src = DPLLB_REF_CLOCK_SRC;              /*write this register with 0x0570CC00*/
			dg_loop_filter = DPLLB_DG_LOOP_FILTER;          /*Digital Loop Filter*/
			en_idclkp = DPLLB_EN_DCLKP;
			cml_tline = DPLLB_CML_TLINE;
			dpll_ctl = DPLLBCNTR;


			/* Need to set the reference clock BIT29 for side band to work */
    		dp_read_mmio_reg(p_ctx, DPLLBCNTR, &temp);
	    	temp|= 0x20000000;
    		dp_write_mmio_reg(p_ctx, DPLLBCNTR, temp);

			/* PIPE B needed this*/
			dpll = 0x70006000; 
	 		dp_write_mmio_reg(p_ctx, dpll_ctl, dpll);

			/*0x8014/8034 bit 27 - PLLREFSEL Override Enable. Default 1 - core clock 
 			* The value should be from the choice for Ref Clock Source.*/
			temp = 0x0DF70000;
			dp_write_dpio_register(p_ctx, ref_clk_src, temp);
			EMGD_DEBUG("DP: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx",  (unsigned long)  ref_clk_src, temp);

			/*Enable bit 24, Enable idclkp.*/
			temp = 0x01C00000; /*Enable bit 24 and ... , Enable idclkp.*/
			dp_write_dpio_register(p_ctx, en_idclkp, temp);
			EMGD_DEBUG("DP: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx",  en_idclkp, temp);

			temp = 0x87871000; 
			dp_write_dpio_register(p_ctx, cml_tline, temp);
			EMGD_DEBUG("DP: WRITE_MMIO_REG_VLV: 0x%lx = 0x%lx",  cml_tline, temp);

			dpll |= (1 << 31);
    		dp_write_mmio_reg(p_ctx, dpll_ctl, dpll);

    		timeout = OS_SET_ALARM(200);
    		do {
    			dp_read_mmio_reg(p_ctx, dpll_ctl, &temp);
				temp = temp & BIT15;
    		} while ((temp == 0x00) && (!OS_TEST_ALARM(timeout)));

			if (temp == 0) {
				EMGD_ERROR("Wait for DPLL lock timed out!");
			}
		}
		ret = dp_read_rx_capability(p_ctx);

		if (ret){
			EMGD_ERROR("Error getting DPCD info!");
		}

		if ((p_ctx->embedded_panel == 0) &&
				(p_ctx->pwr_method != 1)){
			/*If not EDP.*/
			edp_enable_vdd(p_ctx, FALSE);	
			edp_panel_port_select (p_ctx, 0); /* 0 for nothing */
		}
	}	

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_DPCD_MAX, 0);
	dp_attr->current_value = p_ctx->dpcd_max_rate;

 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_DP_DPCD_BPC, 0);
	dp_attr->current_value = p_ctx->bits_per_color;

	if (p_ctx->pwr_method){
		if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
			edp_panel_port_select (p_ctx, 1); /* 1 for eDP B */
		} else {
			edp_panel_port_select (p_ctx, 2); /* 2 for eDP C */
                }
		edp_panel_pwm(p_ctx);
	}

	if (ret == PD_SUCCESS){
		p_ctx->dpcd_info_valid[flags - 1] = TRUE;
	}

	EMGD_TRACE_EXIT;

	return 0;

}

/*	============================================================================
	Function	:	dp_init_device is called to initialize a dp device

	Parameters	:	p_context : Pointer to port driver allocated context
					structure

	Remarks 	:

	Returns 	:	PD_SUCCESS	If initialization is successful
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_init_device(void *p_context)
{
	EMGD_TRACE_ENTER;
	EMGD_DEBUG ("dp: Enter dp_init_device()");

	g_dp_drv.num_devices++;
	/* Should we continue even no panel is available? */
	/* status = dp_audio_characteristic(p_context); */
	dp_audio_characteristic(p_context);
	
	EMGD_DEBUG ("dp: Exit dp_init_device()");
	EMGD_TRACE_EXIT;


	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_get_timing_list is called to get the list of display
					modes supported by the dp device and the display.

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_in_list: List of display modes supported by the IEG driver
					pp_out_list: List of modes supported by the dp device

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_get_timing_list(void *p_context, pd_timing_t *p_in_list,
	pd_timing_t **pp_out_list)
{
	int ret = 0;
	dp_device_context_t *pd_context = (dp_device_context_t *)p_context;
	pd_dvo_info_t dp_info = {0, 0, 0, 0, 0, 0, 0, 0};
	pd_display_info_t dp_display_info = {0, 0, 0, 0, NULL};

	EMGD_TRACE_ENTER;
	EMGD_DEBUG ("dp: ENTER dp_get_timing_list()");

	ret = pd_filter_timings(pd_context->p_callback->callback_context,
		p_in_list, &pd_context->timing_table, &dp_info, &dp_display_info);
	*pp_out_list = pd_context->timing_table;

	pd_context->native_dtd = dp_display_info.native_dtd;
	pd_context->fp_width = dp_display_info.width;
	pd_context->fp_height = dp_display_info.height;

	EMGD_TRACE_EXIT;
	return ret;
}

/*	============================================================================
	Function	:	dp_set_mode is called to test if specified mode can be
					supported or to set it.

	Parameters	:	p_context: Pointer to port driver allocated context
					p_mode	: New mode
					flags	: In test mode it is set to PD_SET_MODE_FLAG_TEST

	Remarks 	:	dp_set_mode first verifies that the new mode is
					supportable.
					If not it returns an error status
					If the flags is not set to PD_SET_MODE_FLAG_TEST it sets the
					new mode.

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_set_mode(void *p_context, pd_timing_t *p_mode, unsigned long flags)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	mmio_reg_t	ctrl_reg = (mmio_reg_t) 0x0;
	unsigned long aud_config_reg = 0;


	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_set_mode()");

	/* set mode flag test */
	if (flags & PD_SET_MODE_FLAG_TEST) {
		return PD_SUCCESS;
	}

	/* program port */
	dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &ctrl_reg);

	/* Settings based on DPCD Read */
	/* Rest the Port Width based on Max Lane Count */
	ctrl_reg &= ~(BIT21|BIT20|BIT19);
	switch(p_ctx->dpcd_max_count & 0xF) {
	case 1: break;
	case 2: ctrl_reg |= BIT19;
		break;
	case 4: ctrl_reg |= (BIT20|BIT19);
		break;
	default:
		/* Error */
		break;
	}
	
	/* Enhanced Framing Bit */
	if(p_ctx->dpcd_max_count & BIT7) {
		ctrl_reg |= BIT18;
	}

	if (flags & PD_SET_MODE_PIPE_B) {
		/* This is the pipe B */
		EMGD_DEBUG ("DP set to pipe B");
		ctrl_reg |= BIT(30);
	} else {
		EMGD_DEBUG ("DP set to pipe A");
		ctrl_reg &= ~BIT(30);
	}

	/* VSync and HSync Values */
	if(p_mode->flags & PD_VSYNC_HIGH) {
		ctrl_reg |= BIT(4);
	} else {
		ctrl_reg &= ~BIT(4);
	}
	if(p_mode->flags & PD_HSYNC_HIGH) {
		ctrl_reg |= BIT(3);
	} else {
		ctrl_reg &= ~BIT(3);
	}

	/* Color Range 0-255 */
	ctrl_reg &= ~BIT(8);

	/* Turn on port */
	ctrl_reg |= BIT31;

	dp_write_mmio_reg(p_ctx, p_ctx->control_reg ,
						ctrl_reg);

	p_ctx->current_mode = p_mode;
	p_ctx->pipe = flags;

	/* Audio code below */

	/* DP port can be connected to different Pipe.
	 * Map the audio and video_dip registers to pipe in use.
	 * The registers will be used during post_set_mode
	 */
	if (dp_map_audio_video_dip_regs(p_ctx) != PD_SUCCESS) {
		EMGD_ERROR("Failed to map audio and video_dip registers to pipe in use");
		return PD_ERR_UNSUCCESSFUL;
	}

	/*audio_pixel_clk is used only in HDMI for setting aud_config*/
	dp_read_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_config, &aud_config_reg);
	aud_config_reg |= BIT(29); /* 0-HDMI, 1-DP */
	dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_config, aud_config_reg);

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

int dp_pre_link_training(void *p_context, pd_timing_t *p_mode,
	unsigned long status){

	unsigned long flags;

	unsigned long aux_ctrl = 0;
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_post_set_mode()");

	if (p_ctx == NULL){
		return PD_ERR_UNSUCCESSFUL;
	}

	flags = p_ctx->pipe;
	/* Checking for SINK availability */
	if(p_ctx->dpcd_max_rate == 0 || p_ctx->dpcd_max_count == 0) {
		/* If SINK is not available, return as well */
		EMGD_DEBUG("dp: dp_post_set_mode return success with no sink availability.");
		return PD_SUCCESS;
	}

	p_ctx->current_mode = p_mode;

	/* Initialize DisplayPort by setting 2x Bit Clock Divider and Precharge Time */
	/* Fixed values: 
	* 2x Bit Clock Divider = 100MHz (HRAW_CLK = 200MHz)
	* Precharge Time = 10us
	*/
	aux_ctrl = (0x00100000 | ((unsigned long)0xa << 16) | 0x64);
	dp_write_mmio_reg(p_ctx, p_ctx->control_reg+0x10, aux_ctrl);

	/* Set Display Power through DPCD */
	dpcdWrite(p_ctx, 0, DPCD_ADJUST_D_STATE_ADDR, 1, 1);

	EMGD_DEBUG("dp: Before Link Training");

    /* Write protect key off. */
	edp_panel_protect(p_ctx, FALSE);
	return PD_SUCCESS;
}

int dp_port_clock(void *p_context, pd_timing_t *p_mode, unsigned long dclk,
	unsigned long flags, unsigned long bpc ){
	int ret = 0;
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	
	ret = dp_program_mntu_regs(p_ctx, p_mode, flags, dclk, bpc);
	

	/* Panel Fitting - for eDP only */
	edp_panel_fit(p_ctx);

	/* Wait for VBlank */
	dp_wait_vblank(p_ctx, flags);

	return ret;
}

/*	============================================================================
	Function	:	dp_post_set_mode

	Parameters	:	p_context: Pointer to port driver allocated context
					p_mode	:
					flags	:

	Remarks 	:

	Returns 	:
	------------------------------------------------------------------------- */
int dp_post_set_mode(void *p_context, pd_timing_t *p_mode,
	unsigned long status)
{
	int pd_status = PD_SUCCESS;
	mmio_reg_t  ctrl_reg = 0;
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	EMGD_DEBUG("dp: dp_post_set_mode()");
	if ((p_ctx->p_callback->eld) != NULL) {
		if ((*p_ctx->p_callback->eld)->audio_support) {
			pd_status = dp_configure(p_context);
		} else {
			/* Disable audio. This looks like a HDMI-DVI connection */
			dp_read_mmio_reg(p_ctx, p_ctx->control_reg,
					&ctrl_reg);
			ctrl_reg &= ~BIT(6);
			dp_write_mmio_reg(p_ctx, p_ctx->control_reg,
					ctrl_reg);
			dp_read_mmio_reg(p_ctx, p_ctx->control_reg,
					&ctrl_reg);
		}
	}
	  return pd_status;
}


/*	============================================================================
	Function	:	dp_get_attributes is called to get the list of all the
					available attributes

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_Num	: Return the total number of attributes
					pp_list	: Return the list of port driver attributes

	Remarks 	:	dp_get_attributes calls dp interface functions to get all
					available range,bool and list attributes supported by the
					dp device

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_get_attributes(void *p_context, unsigned long *p_num_attr,
	pd_attr_t **pp_list)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	int status = PD_SUCCESS;
	pd_attr_t      *dp_attr      = NULL; 

	EMGD_TRACE_ENTER;
	/*	Create attribute table if not already created */
	if (p_ctx->p_attr_table == NULL) {
		dp_init_attribute_table(p_ctx);
	}

	*pp_list	= p_ctx->p_attr_table;
	*p_num_attr	= p_ctx->num_attrs;

	if (p_ctx->dpcd_max_rate != 0){
	 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_DPCD_MAX, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->dpcd_max_rate;
		}
	}
	if (p_ctx->bits_per_color != 0){
	 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_DP_DPCD_BPC, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->bits_per_color;
		}
	}
	if (p_ctx->aspect_ratio != 0){
	 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_MAINTAIN_ASPECT_RATIO, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->aspect_ratio;
		}
	}
	if (p_ctx->text_tune != 0){
	 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_TEXT_TUNING, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->text_tune;
		}
	}
	if (p_ctx->pwm_intensity != 0){
	 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_PWM_INTENSITY, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->pwm_intensity;
		}
	}
	if (p_ctx->inverter_freq != 0){
	 	dp_attr =  pd_get_attr(p_ctx->p_attr_table,
    	             p_ctx->num_attrs, PD_ATTR_ID_INVERTER_FREQ, 0);
		if (dp_attr != NULL){
			dp_attr->current_value = p_ctx->inverter_freq;
		}
	}

	EMGD_DEBUG("dp: dp_get_attributes()");
	EMGD_TRACE_EXIT;
	return status;
}


/*	============================================================================
	Function	:	dp_set_attributes is called to modify one or more display
					attributes

	Parameters	:	p_context: Pointer to port driver allocated context structure
					num 	: Number of attributes
					p_list	: List of attributes

	Remarks 	:	dp_set_attributes scans the attribute list to find the ones
					that are to be modified by checking flags field in each
					attribute for PD_ATTR_FLAG_VALUE_CHANGED bit. If this bit is
					set it will call dp interface functions to set the new
					value for the attribute.

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_set_attributes(void *p_context, unsigned long num_attrs,
	pd_attr_t *p_list)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned long i;

	EMGD_TRACE_ENTER;
	/*	Create attribute table if not already created                         */
	if (p_ctx->p_attr_table == NULL) {
		dp_init_attribute_table(p_ctx);
	}

	for (i = 0; i < num_attrs; i++) {
		pd_attr_t *p_attr;
		unsigned long new_value;

		if ((p_list[i].flags & PD_ATTR_FLAG_VALUE_CHANGED) == 0) {
			continue;
		}

		/*	Clear attribute changed flag */
		p_list[i].flags &= ~PD_ATTR_FLAG_VALUE_CHANGED;
		new_value = p_list[i].current_value;

		p_attr = pd_get_attr(p_ctx->p_attr_table, p_ctx->num_attrs,
			 p_list[i].id, 0);

		if (p_attr == NULL) {
			EMGD_DEBUG("dp: Error ! pd_get_attr() failed for attr "
				"id=%ld", (unsigned long)p_list[i].id);
			continue;
		}

		switch (p_list[i].id) {
			case PD_ATTR_ID_FP_PWR_METHOD:
			case PD_ATTR_ID_FP_PWR_T1:
			case PD_ATTR_ID_FP_PWR_T2:
			case PD_ATTR_ID_FP_PWR_T3:
			case PD_ATTR_ID_FP_PWR_T4:
			case PD_ATTR_ID_FP_PWR_T5:
				/* current_value should not exceed the predefined max value */
				/* FIXME: These arent even DP attributes??? Maybe a To-Do?
				 * If so, then we should have some DP standard defined min
				 * and max values for the power panel sequencing to check?
				 */
				p_attr->current_value = p_list[i].current_value;
				break;
			case PD_ATTR_ID_MAINTAIN_ASPECT_RATIO:
				p_attr->current_value = p_list[i].current_value;
				p_ctx->aspect_ratio = p_attr->current_value;
				break;
			case PD_ATTR_ID_TEXT_TUNING:
				p_attr->current_value = p_list[i].current_value;
				p_ctx->text_tune = p_attr->current_value;
				break;
			case PD_ATTR_ID_PWM_INTENSITY:
				p_attr->current_value = p_list[i].current_value;
				p_ctx->pwm_intensity = p_attr->current_value;
				break;
			case PD_ATTR_ID_INVERTER_FREQ:
				p_attr->current_value = p_list[i].current_value;
				p_ctx->inverter_freq = p_attr->current_value;
				break;


		}
	
		p_attr->current_value = new_value;
		EMGD_DEBUG("dp: Success ! dp_set_attributes: "
			"attr='%s', id=%ld, current_value=%ld",
			DP_GET_ATTR_NAME(p_attr), (unsigned long)p_attr->id,
			(unsigned long)p_attr->current_value);


	}
	EMGD_DEBUG("dp: dp_set_attributes()");

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}



/* 	Panel Power Status Register 			61200h
Panel Power Control Register 	 		61204h
Panel power on sequencing delays  		61208h
Panel power off sequencing delays 		6120Ch
Panel power cycle delay and Reference Divisor 61210h
*/
static opt_set_power_t table_set_power[] = {
	/* id1                  id2                   bit    reg_offset */
	{ PD_ATTR_ID_FP_PWR_T1, PD_ATTR_ID_FP_PWR_T2, 1,     0x08 },  /* D0 */
	{ PD_ATTR_ID_FP_PWR_T4, PD_ATTR_ID_FP_PWR_T3, 0,     0x0C },  /* Dx */
};


#if defined(CONFIG_MICRO) && (CONFIG_VBIOS_INITTIME == 1)
void *get_table_set_power_offset()
{
	return (void *)table_set_power;
}
#endif

/*	============================================================================
	Function	:	dp_set_power is called to change the power state of the
					device

	Parameters	:	p_context: Pointer to port driver allocated context structure
					state	: New power state

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_set_power(void *p_context, uint32_t state)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	mmio_reg_t	ctrl_reg = 0;
	pd_attr_t      *dp_attr      = NULL;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_set_power()");
					

	/* This is needed even the panel is not embedded. */
	edp_panel_protect(p_ctx, FALSE);

	/* FIXME: temp changes for D2700MT */
	if (state == p_ctx->power_state) {
		EMGD_DEBUG("dp: dp_set_power - Power state set already!");
		return PD_SUCCESS;
	}
	switch(state) {
	case PD_POWER_MODE_D0:

		EMGD_DEBUG("dp: dp_set_power: Power state: PD_POWER_MODE_D0");
		if (p_ctx->embedded_panel){
			if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
				edp_panel_port_select (p_ctx, 1); /* 1 for eDP B */
			} else {
				edp_panel_port_select (p_ctx, 2); /* 2 for eDP C */
			}
		}else {
			EMGD_DEBUG ("NON-eDP connected.\n");			
			edp_panel_port_select (p_ctx, 0);
		}
		if(p_ctx->pwr_method) { 
			/* NOTE: Pre-condition:
			Embedded Panel Port is Programmed Enabled.*/
			
			EMGD_DEBUG ("eDP connected.\n");			
			edp_panel_pwm(p_ctx);

			/*
 			* (00)0-Reserved
 			* (01)1-DisplayPort B
 			* (10)2-DisplayPort C
 			* (11)3-Reserved
 			*/
			/* eDP always tied to DisplayPortC on AlpineValley*/
			if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
				edp_panel_port_select (p_ctx, 1); /* 1 for eDP B */
			} else {
				edp_panel_port_select (p_ctx, 2); /* 2 for eDP C */
			}
		

			edp_panel_power(p_ctx, TRUE);

			/*
			 * Use the current value of the backlight enable attribute to set
			 * the backlight on/off.
			 */
			dp_attr = pd_get_attr(p_ctx->p_attr_table,
				p_ctx->num_attrs, PD_ATTR_ID_FP_BACKLIGHT_EN, 0);

			if (dp_attr)
				edp_panel_backlight(p_ctx, dp_attr->current_value);
			else
				edp_panel_backlight(p_ctx, TRUE);

		}	
		break;

	case PD_POWER_MODE_D1:
	case PD_POWER_MODE_D2:
	case PD_POWER_MODE_D3:

		EMGD_DEBUG("dp: dp_set_power: Power state: PD_POWER_MODE_D1/D2/D3");
		if(p_ctx->pwr_method) {
			edp_panel_pwm(p_ctx);

			edp_panel_backlight(p_ctx, FALSE);

			edp_panel_power(p_ctx, FALSE); 
		}

		/* Do no reset the pipe when turning off port */
		dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &ctrl_reg);
		/* Disable audio */
		ctrl_reg &= ~BIT(6);
		ctrl_reg &= ~BIT(31);
		dp_write_mmio_reg(p_ctx, p_ctx->control_reg, ctrl_reg);

		break;

	default:
		EMGD_ERROR("dp: Error, Invalid Power state received!");
		return (PD_ERR_INVALID_POWER);
	}

	/* update power state */
	p_ctx->power_state = state;


	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	dp_get_power is called to get the current power state

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_state	: Returns the current power state

	Remarks 	:

	Returns 	:	PD_SUCCESS	On Success
					PD_ERR_xxx	On Failure
	------------------------------------------------------------------------- */
int dp_get_power(void *p_context, uint32_t *p_state)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_get_power()");

	if(p_ctx == NULL || p_state == NULL) {
		EMGD_ERROR("DP:dp_get_power()=> Error: Null Pointer passed in");
		return PD_ERR_NULL_PTR;
	}

	/* Return the power state saved in the context */
	*p_state = p_ctx->power_state;

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	dp_save is called to save the default state of registers

	Parameters	:	p_context: Pointer to port driver allocated context structure
					pp_state : Returs a pointer to list of dp registers
					terminated with PD_REG_LIST_END.
					flags	: Not used

	Remarks     :	dp_save does not save any registers.

	Returns		:	PD_SUCCESS
	------------------------------------------------------------------------- */
int dp_save(void *p_context, void **pp_state, unsigned long flags)
{

	EMGD_DEBUG("dp: dp_save()");

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	dp_restore is called to restore the registers which were
					save previously via a call to dp_save

	Parameters	:	p_context: Pointer to port driver allocated context structure
					p_state	: List of dp registers
					flags	: Not used

	Remarks     :

	Returns     :	PD_SUCCESS
	------------------------------------------------------------------------- */
int dp_restore(void *p_context, void *p_state, unsigned long flags)
{

	EMGD_DEBUG("dp: dp_restore()");

	return PD_SUCCESS;
}


/*	============================================================================
	Function	:	dp_validate

	Parameters	:	cookie

	Remarks 	:	dp_Valite returns the cookie it received as an argument

	Returns 	:	cookie
	------------------------------------------------------------------------- */
unsigned long dp_validate(unsigned long cookie)
{
	EMGD_DEBUG("dp: dp_validate()");
	return cookie;
}


/*	============================================================================
	Function	:	dp_close is the last function to be called in the port
					driver

	Parameters	:	p_context: Pointer to port driver allocated context structure

	Remarks 	:

	Returns 	:	PD_SUCCESS
	------------------------------------------------------------------------- */
int dp_close(void *p_context)
{
#ifndef CONFIG_MICRO
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_close()");

	if (p_ctx) {
		if (p_ctx->p_attr_table != NULL) {
			pd_free(p_ctx->p_attr_table);
			p_ctx->p_attr_table = NULL;
			p_ctx->num_attrs	 = 0;
		}
		if (p_ctx->timing_table) {
			pd_free(p_ctx->timing_table);
			p_ctx->timing_table = NULL;
		}
		pd_free(p_ctx);
		g_dp_drv.num_devices--;
	}
#endif
	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_get_port_status is called to get the status of the
					display

	Parameters	:	p_context: Pointer to port driver allocated context structure
					port_status : Returns display type and connection state

	Returns 	:	PD_SUCCESS or PD_ERR_XXX
	------------------------------------------------------------------------- */
int dp_get_port_status(void *p_context, pd_port_status_t *port_status)
{
	/*FIXME: This is never tested.*/

	unsigned char edid_reg;
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_get_port_status()");

	port_status->display_type = PD_DISPLAY_DP_INT;
	port_status->connected    = PD_DISP_STATUS_DETACHED;
	
	/* We need to detect if the display is attched here.
	   Using read edid to check this */
	if(dp_read_ddc_reg(p_ctx, 0x0, &edid_reg)) {
		port_status->connected = PD_DISP_STATUS_ATTACHED;
		EMGD_DEBUG("dp: dp_get_port_status() dp is attached.");
	}

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;

}

/*	============================================================================
	Function	:	dp_get_attrs

	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned long dp_get_attrs(dp_device_context_t *p_ctx,   pd_attr_t *p_attr)
{
	int i;
	unsigned long attr_size = 0;
	unsigned long num_attr = 0;
	EMGD_TRACE_ENTER;
	
	attr_size = ARRAY_SIZE(edp_attrs_data) + ARRAY_SIZE(dp_attrs_data);
	/* Return number of attributes if list is NULL */
	if (p_attr == NULL) {
		return attr_size;
	}

	for (i = 0; i < ARRAY_SIZE(edp_attrs_data); i++) {
		pd_attr_t *p_attr_cur = &p_attr[num_attr];

		p_attr_cur->id   = edp_attrs_data[i].id;
		p_attr_cur->type = edp_attrs_data[i].type;
		p_attr_cur->default_value = edp_attrs_data[i].default_value;
		p_attr_cur->current_value = edp_attrs_data[i].current_value;

		pd_strcpy(p_attr_cur->name, edp_attrs_data[i].name);
		EMGD_DEBUG ("%ld: name=%s", num_attr, p_attr_cur->name);
		num_attr++;
	}

	for (i = 0; i < ARRAY_SIZE(dp_attrs_data); i++) {
		pd_attr_t *p_attr_cur = &p_attr[num_attr];

		p_attr_cur->id   = dp_attrs_data[i].id;
		p_attr_cur->type = dp_attrs_data[i].type;
		p_attr_cur->default_value = dp_attrs_data[i].default_value;
		p_attr_cur->current_value = dp_attrs_data[i].current_value;

		pd_strcpy(p_attr_cur->name, dp_attrs_data[i].name);
		EMGD_DEBUG ("%ld: name=%s", num_attr, p_attr_cur->name);
		num_attr++;	
	}

	EMGD_TRACE_EXIT;
	return attr_size;
}

/*	============================================================================
	Function	:	dp_init_attribute_table

	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */
int dp_init_attribute_table(dp_device_context_t *p_ctx)
{
	unsigned long num_general_attrs;
	unsigned char *p_table;

	EMGD_TRACE_ENTER;
	num_general_attrs = dp_get_attrs(p_ctx, NULL);
	p_ctx->num_attrs = num_general_attrs;

	p_ctx->p_attr_table = pd_malloc((p_ctx->num_attrs + 1) * sizeof(pd_attr_t));
	if (p_ctx->p_attr_table == NULL) {

		EMGD_ERROR("dp: Error ! sdvo_init_attribute_table: "
			"pd_malloc(p_attr_table) failed");

		p_ctx->num_attrs = 0;
		return PD_ERR_NOMEM;
	}

	pd_memset(p_ctx->p_attr_table, 0, (p_ctx->num_attrs + 1) * sizeof(pd_attr_t));
	p_table = (unsigned char *)p_ctx->p_attr_table;

	dp_get_attrs(p_ctx, (pd_attr_t *)p_table);

	EMGD_TRACE_EXIT;

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_read_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char dp_read_mmio_reg(dp_device_context_t *p_ctx, unsigned long reg,
						mmio_reg_t *p_Value)
{
	pd_reg_t reg_list[2];


	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
			reg_list, PD_REG_MIO)) {
		EMGD_DEBUG ("Failed reading register.");
		return FALSE;
	} else {
		*p_Value = (mmio_reg_t)reg_list[0].value;
		/*EMGD_DEBUG("dp : R : 0x%02lx, 0x%02lx", reg, *p_Value);*/
		return TRUE;
	}
}

/*	============================================================================
	Function	:	dp_write_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char dp_write_mmio_reg(dp_device_context_t *p_ctx, unsigned long reg,
						mmio_reg_t p_Value)
{
	pd_reg_t reg_list[2];

	reg_list[0].reg = reg;
	reg_list[0].value = p_Value;

	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->write_regs(p_ctx->p_callback->callback_context, reg_list,
			PD_REG_MIO)) {

		EMGD_DEBUG ("Failed writing register.");
		return FALSE;

	}else {

		return TRUE;
	}
}

/*	============================================================================
	Function	:	dp_write_mmio_reg is called to for i2c read operation
					Adopted from SDVO port driver based on bit and bit number
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

unsigned char dp_write_mmio_reg_bit(dp_device_context_t *p_ctx,
	unsigned long reg, unsigned char input, unsigned char bit_loc)
{
	pd_reg_t reg_list[2];
	mmio_reg_t p_Value;

	reg_list[0].reg = reg;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
		reg_list, PD_REG_MIO)) {
		EMGD_DEBUG ("Failed reading regs.");
		return FALSE;
	} else {
		p_Value = (mmio_reg_t)reg_list[0].value;

		if(input){
			p_Value |= (bit_loc);
		}else{
			p_Value &= (~bit_loc);
		}

		reg_list[0].reg		= reg;
		reg_list[0].value	= p_Value;
		reg_list[1].reg		= PD_REG_LIST_END;

		if (p_ctx->p_callback->write_regs(p_ctx->p_callback->callback_context, reg_list,
				PD_REG_I2C)) {
			EMGD_DEBUG ("Failed writing regs");
			return FALSE;
		}else {
			return TRUE;
		}
	}
}

/*	============================================================================
	Function	:	dp_read_i2c_reg is called to for i2c read operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

int dp_read_i2c_reg(dp_device_context_t *p_ctx, unsigned char offset,
						i2c_reg_t *p_Value)
{
	pd_reg_t reg_list[2];
	reg_list[0].reg = offset;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
		reg_list, PD_REG_I2C)) {
		return FALSE;
	} else {
		*p_Value = (i2c_reg_t)reg_list[0].value;
		EMGD_DEBUG("dp : R : 0x%02x, 0x%02x", offset, *p_Value);
		return TRUE;
	}
}

/*	============================================================================
	Function	:	dp_write_i2c_reg is called to for i2c write operation
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */
unsigned char dp_write_i2c_reg(dp_device_context_t *p_ctx, i2c_reg_t offset,
	i2c_reg_t value)
{
	pd_reg_t reg_list[2];

	EMGD_DEBUG("dp : W : 0x%02x, 0x%02x", offset, value);

	reg_list[0].reg = offset;
	reg_list[0].value = value;

	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->write_regs(p_ctx->p_callback->callback_context, reg_list,
			PD_REG_I2C)) {

		return FALSE;

	}else {

		return TRUE;
	}
}

/*	============================================================================
	Function	:	dp_read_ddc_reg is called to for i2c read operation for ddc
					Adopted from SDVO port driver
	Parameters	:

	Returns 	:
	------------------------------------------------------------------------- */

int dp_read_ddc_reg(dp_device_context_t *p_ctx, unsigned char offset,
						i2c_reg_t *p_Value)
{
	pd_reg_t reg_list[2];
	reg_list[0].reg = offset;
	reg_list[1].reg = PD_REG_LIST_END;

	if (p_ctx->p_callback->read_regs(p_ctx->p_callback->callback_context,
		reg_list, PD_REG_DDC)) {
		return FALSE;
	} else {
		*p_Value = (i2c_reg_t)reg_list[0].value;
		EMGD_DEBUG("dp : R : 0x%02x, 0x%02x", offset, *p_Value);
		return TRUE;
	}
}

/*	============================================================================
	Function	:	dp_audio_characteristic

	Parameters	:

	Remarks 	:	Set audio characteristic to be used in ELD calcilation.
					TODO: Make sure this is recalled if hotplug is detected

	Returns 	:
	------------------------------------------------------------------------- */
int dp_audio_characteristic(dp_device_context_t *p_ctx)
{
	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_audio_characteristic()");

	if(!(p_ctx)){
		EMGD_DEBUG("dp: dp_audio_characteristic()->context is null");
		return PD_ERR_INTERNAL;
	}
	if((!p_ctx->p_callback) || (!p_ctx->p_callback->eld)){
		EMGD_DEBUG("dp: dp_audio_characteristic()->callback or eld is null");
		return PD_ERR_INTERNAL;
	}
	if(*(p_ctx->p_callback->eld) == NULL){
		return PD_ERR_NULL_PTR;
	}

	(*p_ctx->p_callback->eld)->audio_flag |= PD_AUDIO_CHAR_AVAIL;
	/* Source DPG codes */
	(*p_ctx->p_callback->eld)->NPL = 13;
	(*p_ctx->p_callback->eld)->K0 = 30;
	(*p_ctx->p_callback->eld)->K1 = 74;

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_configure is called to program AVI infoframes, SPD
					infoframes, ELD data for audio, colorimetry and pixel replication.

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_mode: Point to current video output display mode

	Remarks 	:

	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int dp_configure(dp_device_context_t *p_ctx)
{
	unsigned long aud_cntl_st_reg=0, udi_if_ctrl_reg=0;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_configure()");

	/* Just return success since set mode should still work */
	if((p_ctx->p_callback->eld) == NULL){
		EMGD_ERROR("ELD not available");
		return PD_SUCCESS;
	}
	if(*(p_ctx->p_callback->eld) == NULL){
		EMGD_ERROR("ELD not available");
		return PD_SUCCESS;
	}
	if(dp_audio_characteristic(p_ctx) != PD_SUCCESS){
		EMGD_ERROR("Audio not available");
		return PD_SUCCESS;
	}

	/* Build and send ELD Info Frames */
	if(dp_send_eld(p_ctx) != PD_SUCCESS){
		EMGD_ERROR("Fail to write ELD to transmitter");
	}

	/* Build and send AVI Info Frames */
	if (dp_avi_info_frame(p_ctx) != PD_SUCCESS) {
		EMGD_ERROR("Fail to write AVI infoframes to transmitter");
	}

#ifndef CONFIG_MICRO
	/* Build and send SPD Info Frames */
	if (dp_spd_info_frame(p_ctx) != PD_SUCCESS) {
		EMGD_ERROR("Fail to write SPD Infoframes to transmitter");
	}
#endif

	/* Enable DIP */
	/* Read the UDI Infoframe control register */
	dp_read_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_ctl, &udi_if_ctrl_reg);
	/* Enable graphics island data packet */
	udi_if_ctrl_reg |= BIT31;
	/* Enable DIP type */
	udi_if_ctrl_reg |= (BIT21|BIT24);

	/* Select port */
	if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
		udi_if_ctrl_reg &= ~BIT30;
		udi_if_ctrl_reg |= BIT29;
	}else if(p_ctx->p_callback->port_num == DP_PORT_C_NUMBER){
		udi_if_ctrl_reg |= BIT30;
		udi_if_ctrl_reg &= ~BIT29;
	}else{
		EMGD_ERROR("Unsupported port number for internal DP");
		return PD_ERR_INTERNAL;
	}
	dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

	/* Enable ELD */
	/* Read the audio control state register */
	dp_read_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_cntl_st, &aud_cntl_st_reg);
	/* Set ELD valid to valid */
	aud_cntl_st_reg |= BIT14;
	dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_send_eld

	Parameters	:

	Remarks 	:	Builds eld structure and write it into SDVO ELD buffers

	Returns 	:
	------------------------------------------------------------------------- */
int dp_send_eld(dp_device_context_t *p_ctx)
{
	unsigned long aud_cntl_st_reg=0, aud_dpw_dpedid_reg=0;
	unsigned long data_to_write=0, offset=0;
	unsigned char *eld_data = NULL;
	unsigned long i;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_send_eld()");

	if(!((*p_ctx->p_callback->eld)->audio_flag & ELD_AVAIL)){
		EMGD_DEBUG("Eld not available");
		return PD_ERR_UNSUCCESSFUL;
	}

	/* ELD 1.0, CEA version retrieve from callback */
	(*p_ctx->p_callback->eld)->cea_ver = 0;
	(*p_ctx->p_callback->eld)->eld_ver = 1;

	/* Capability Flags */
	(*p_ctx->p_callback->eld)->repeater = 0;
	(*p_ctx->p_callback->eld)->hdcp = 0;
#ifdef CONFIG_MICRO
	(*p_ctx->p_callback->eld)->_44ms = 0;
#endif

	/* We do not provide Monitor length*/
	(*p_ctx->p_callback->eld)->mnl = 0;

#ifdef CONFIG_MICRO
	(*p_ctx->p_callback->eld)->sadc = 0;
#endif

	/* Read the audio control state register */
	dp_read_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_cntl_st, &aud_cntl_st_reg);

	/* Set ELD valid to invalid */
	dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	/* Reset ELD access address for HW */
	aud_cntl_st_reg &= ~(BIT5|BIT6|BIT7|BIT8);
	 dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_cntl_st, aud_cntl_st_reg);

	/* Data to write is max possible HW ELD buffer size */
	offset = 0;
	eld_data = (*p_ctx->p_callback->eld)->eld_ptr;
	data_to_write = (aud_cntl_st_reg >> 9) & 0x1F;

	/* Write ELD to register AUD_DPW_DPEDID */
	while (data_to_write > 0) {
		if (data_to_write >= 4) {
			/* Note: it is assumed that the reciever of the buffer is also
			* following little-endian format and the buffer is arranged in
			* little endian format */
			aud_dpw_dpedid_reg = MAKE_DWORD(eld_data[offset],
				eld_data[offset+1], eld_data[offset+2], eld_data[offset+3]);

			offset +=4;
			data_to_write -= 4;

		} else {
			unsigned char buffer[4];
			pd_memset(&buffer[0],0,4*sizeof(unsigned char));
			for(i=0;i<data_to_write;i++) {
				buffer[i] = eld_data[offset++];
			}
			aud_dpw_dpedid_reg = MAKE_DWORD(buffer[0],
				buffer[1], buffer[2], buffer[3]);

			data_to_write = 0;
		}
			dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->aud_dpw_dpedid, aud_dpw_dpedid_reg);
	}

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_avi_info_frame is called to program AVI infoframes

	Parameters	:	p_context: Pointer to port driver allocated context
								structure
					p_mode: Point to current video output display mode

	Remarks 	:

	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int dp_avi_info_frame(dp_device_context_t *p_context)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	int status = PD_SUCCESS;
	unsigned long sum = 0;
	dp_avi_info_t	avi;
	int i;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_avi_info_frame()");

	/* Initialize AVI InfoFrame to no scan info, no bar info, no active format */
	/* mode rgb, active format same as picture format */
	/* no aspect ratio, no colorimetry, no scaling */
	pd_memset(&avi, 0, sizeof(dp_avi_info_t));
	avi.header.type = DP_AVI_INFO_TYPE;
	avi.header.version = 2;
	avi.header.length = DP_AVI_BUFFER_SIZE - sizeof(dp_info_header_t);

	/* Set Video ID Code */
	avi.video_id_code = (unsigned char)(*p_ctx->p_callback->eld)->video_code;

	/* Set aspect ratio */
	avi.pic_aspect_ratio = (unsigned char)(*p_ctx->p_callback->eld)->aspect_ratio;

	/* Set Colorimetry */
	avi.colorimetry = (unsigned char)(*p_ctx->p_callback->eld)->colorimetry;

	/* Calc checksum */
	for (i= 0; i < DP_AVI_BUFFER_SIZE; i++) {
		sum += avi.data[i];
	}
	avi.header.chksum = (unsigned char)(0x100 - (sum & 0xFF));

	/* Update register */
	status = dp_write_dip(p_ctx, avi.data, DP_AVI_BUFFER_SIZE, DP_DIP_AVI);
	EMGD_TRACE_EXIT;
	return status;
}

/*	============================================================================
	Function	:	dp_spd_info_frame is called to program SPD infoframes

	Parameters	:	p_context: Pointer to port driver allocated context
								structure

	Remarks     :

	Returns     :	int : Status of command execution
	------------------------------------------------------------------------- */
int dp_spd_info_frame(dp_device_context_t *p_context)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	int status = PD_SUCCESS;
	dp_spd_info_t	spd;
	unsigned long sum = 0;
	int i;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_spd_info_frame()");

	/* Initialize SPD InfoFrame to zero */
	pd_memset(&spd, 0, sizeof(dp_spd_info_t));
	spd.header.type = DP_SPD_INFO_TYPE;
	spd.header.version = 1;
	spd.header.length = DP_SPD_BUFFER_SIZE - sizeof(dp_info_header_t);

	pd_memcpy(spd.vendor_name, DP_VENDOR_NAME, DP_INTEL_VENDOR_NAME_SIZE);
	pd_memcpy(spd.product_desc, DP_VENDOR_DESCRIPTION, DP_IEGD_VENDOR_DESCRIPTION_SIZE);
	spd.source_device = DP_SPD_SOURCE_PC;

	/* Calc checksum */
	for (i= 0; i < DP_SPD_BUFFER_SIZE; i++) {
		sum += spd.data[i];
	}
	spd.header.chksum = (unsigned char)(0x100 - (sum & 0xFF));

	status = dp_write_dip(p_ctx, spd.data, DP_SPD_BUFFER_SIZE, DP_DIP_SPD);
	EMGD_TRACE_EXIT;
	return status;
}

/*	============================================================================
	Function	:	dp_write_dip

	Parameters	:

	Remarks 	:	Write DIP

	Returns 	:
	------------------------------------------------------------------------- */
int dp_write_dip(dp_device_context_t *p_ctx, unsigned char *input,
	unsigned long dip_len, dp_dip_type_t dip_type)
{
	unsigned char data_to_write = 0;
	unsigned long udi_if_ctrl_reg = 0, udi_vidpac_data_reg = 0;
	unsigned long i = 0;
	int count = 0;

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_write_dip()");

		/* To update the DIP buffer, UDI_IF_CTL[31] needs to be set */
		udi_if_ctrl_reg |= BIT31;

		/* Set enable mask for dip type to disable */
		switch (dip_type) {
		case DP_DIP_AVI:
			udi_if_ctrl_reg &= ~BIT21;
			break;
		case DP_DIP_VSD:
			udi_if_ctrl_reg &= ~BIT22;
			break;
		case DP_DIP_SPD:
			udi_if_ctrl_reg &= ~BIT24;
			break;
		default:
			return PD_ERR_UNSUCCESSFUL;
		}

		/* Make sure input DIP size is acceptable */
		if (dip_len > (((udi_if_ctrl_reg >> 8) & 0xF)*4)) {
		/*	return PD_ERR_UNSUCCESSFUL; Bug in Cantiga chipset */
		}

		dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

		/* Select port */
		if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
			udi_if_ctrl_reg &= ~BIT30;
			udi_if_ctrl_reg |= BIT29;
		} else if(p_ctx->p_callback->port_num == DP_PORT_C_NUMBER){
			udi_if_ctrl_reg |= BIT30;
			udi_if_ctrl_reg &= ~BIT29;
		} else{
			EMGD_ERROR("Unsupported port number for internal DP");
			return PD_ERR_INTERNAL;
		}

		/* Select buffer index for this specific DIP */
		udi_if_ctrl_reg &= ~(BIT19|BIT20);
		switch(dip_type) {
		case DP_DIP_AVI:
			break;
		case DP_DIP_VSD:
			udi_if_ctrl_reg |= BIT20;
			break;
		case DP_DIP_SPD:
			udi_if_ctrl_reg |= (BIT19|BIT20);
			break;
		default:
			return PD_ERR_UNSUCCESSFUL;
		}

		/* AVI, SPD, VSD - enable DIP transmission every vsync */
		udi_if_ctrl_reg &= ~BIT17;
		udi_if_ctrl_reg |= BIT16;

		/* set the DIP access address */
		udi_if_ctrl_reg &= ~(BIT0|BIT1|BIT2|BIT3);

		/* update buffer index, port, frequency and RAM access address */
		dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_ctl, udi_if_ctrl_reg);

		/* Write DIP data */
		count = 0;
		data_to_write = (unsigned char)dip_len;

		/*
			Write the first three bytes
			|      |ECC for header|            |             |             |
			| DW 0 |(Read Only)   |Header Byte1|Header Byte 2|Header Byte 3|
			|      |              |            |             |             |
		*/


		udi_vidpac_data_reg = MAKE_DWORD(input[count], input[count+1],
			input[count+2], (unsigned char)0);
		/* Update the buffer */
		dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_data, udi_vidpac_data_reg);

		/* Increment the buffer pointer to start adding the payload data */
		count += 3;
		data_to_write -= 3;
		while (data_to_write > 0) {
			/* TBD: Do we need to wait for previous write completion? */
			if (data_to_write >= 4) {
				udi_vidpac_data_reg = MAKE_DWORD(input[count], input[count+1],
					input[count+2], (unsigned char)0);

				data_to_write -= 4;
				count +=4;
			} else if(data_to_write > 0) {
				unsigned char buffer[4];
				pd_memset(&buffer[0],0,4*sizeof(unsigned char));
				for(i=0;i<data_to_write;i++) {
					buffer[i] = input[count++];
				}
				udi_vidpac_data_reg = MAKE_DWORD(buffer[0],
					buffer[1], buffer[2], buffer[3]);
				data_to_write = 0;
			}
				dp_write_mmio_reg(p_ctx, p_ctx->dp_audio_port_regs->video_dip_data, udi_vidpac_data_reg);
		}

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

int dp_write_dpio_register(dp_device_context_t *p_ctx, unsigned long reg, unsigned long value)
{
	unsigned long temp = 0;
	os_alarm_t timeout;

	/* Polling for busy bit */
	timeout = OS_SET_ALARM(100);
	do {
		dp_read_mmio_reg(p_ctx, SB_PACKET_REG, &temp);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout))) ;

	dp_write_mmio_reg(p_ctx, SB_ADDRESS_REG, reg);	/* DPIO SB Address */

	dp_write_mmio_reg(p_ctx, SB_DATA_REG, value); /* DPIO SB Data */

	/* Construct SB_Packet_REG
	 * SidebandRid = 0
	 * SidebandPort = 0x88 << 8
	 * SidebandByteEnable = 0xF << 4
	 * SidebandOpcode = 1 (WRITE) << 16
	 */
	dp_write_mmio_reg(p_ctx, SB_PACKET_REG, (0x0 << 24) | (0x1<<16) | (0x12<<8) | (0xF<<4) );  /* DPIO SB Packet */

	timeout = OS_SET_ALARM(100);
	do {
		dp_read_mmio_reg(p_ctx, SB_PACKET_REG, &temp);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)));

	return 1;
}

unsigned long dp_read_dpio_register(dp_device_context_t *p_ctx, unsigned long reg)
{
	unsigned long temp = 0, value = 0;
	os_alarm_t timeout;

	/* Polling for busy bit */
	timeout = OS_SET_ALARM(100);
	do {
		dp_read_mmio_reg(p_ctx, SB_PACKET_REG, &temp);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout)) );

	dp_write_mmio_reg(p_ctx, SB_ADDRESS_REG, reg);	/* DPIO SB Address */

	/* Construct SB_Packet_REG
	 * SidebandRid = 0
	 * SidebandPort = 0x88 << 8
	 * SidebandByteEnable = 0xF << 4
	 * SidebandOpcode = 0 (WRITE) << 16
	 */
	dp_write_mmio_reg(p_ctx, SB_PACKET_REG, (0x0 << 24)|(0x0 << 16)|(0x12 << 8)|(0xF<<4)  );  /* DPIO SB Packet */

	/* Polling for busy bit */
	timeout = OS_SET_ALARM(100);
	do {
		dp_read_mmio_reg(p_ctx, SB_PACKET_REG, &temp);
	}while((temp & 0x1) && (!OS_TEST_ALARM(timeout))) ;

	dp_read_mmio_reg(p_ctx, SB_DATA_REG, &value); 

	return value;
}

int dp_read_rx_capability(dp_device_context_t *p_ctx)
{
	unsigned long dpcd_rev = 0;
	unsigned long dpcd_max_rate = 0;
	unsigned long dpcd_max_count = 0;
	unsigned long dpcd_max_downspread = 0;

#ifdef DPCD_DEBUG
	unsigned long dpcd_rx_no = 0;
	unsigned long dpcd_downstream_present = 0;
	unsigned long dpcd_max_channel_coding = 0;
	unsigned long dpcd_downstream_port_count = 0;
#endif
	unsigned long dpcd_training_aux_interval = 0;
	unsigned long dpcd_edp_configuration_cap = 0;
	unsigned long dpcd_port0_cap0 = 0;

#ifdef DPCD_DEBUG
	unsigned long dpcd_i2c_control_cap = 0;
	unsigned long dpcd_port0_cap1 = 0;
	unsigned long dpcd_port1_cap0 = 0;
	unsigned long dpcd_port1_cap1 = 0;
	unsigned long edp_rev = 0;
	unsigned long edp_gen_cap_reg_1 = 0;
	unsigned long edp_backlight_adjustment_cap_reg = 0;
#endif

	EMGD_TRACE_ENTER;
	EMGD_DEBUG("dp: dp_read_rx_capability()");


	EMGD_DEBUG("dp: dp_read_rx_capability()");

	/* DPCD 0, DPCD Revision Number */
	dpcd_rev = dpcdRead(p_ctx, 0, DPCD_REV, 1);
	p_ctx->dpcd_version = dpcd_rev;
	EMGD_DEBUG("dp: DPCD_REV: 0x%02lx", dpcd_rev);

	if (dpcd_rev == 0){
		EMGD_DEBUG ("DPCD read error!");
		return PD_ERR_INTERNAL;
	}
	/* DPCD 1, Maximum Link Rate of Main Link lanes */
	dpcd_max_rate = dpcdRead(p_ctx, 0, DPCD_MAX_LINK_RATE, 1);
	p_ctx->dpcd_max_rate = dpcd_max_rate;
	EMGD_DEBUG("dp: DPCD_MAX_LINK_RATE: 0x%02lx", dpcd_max_rate);

	/* DPCD 2, Maximum number of lanes */
	dpcd_max_count = dpcdRead(p_ctx, 0, DPCD_MAX_LANE_COUNT, 1);
	p_ctx->dpcd_max_count = dpcd_max_count;

	if (dpcd_max_rate == 0 || dpcd_max_count == 0){
		EMGD_DEBUG ("DPCD read error!");
		return PD_ERR_INTERNAL;
	}

	EMGD_DEBUG("dp: DPCD_MAX_LANE_COUNT: 0x%02lx", dpcd_max_count);

	/* DPCD 3, Maximum downspread % (BIT0) and AUX Link Training Handshake Needed (BIT6) */
	dpcd_max_downspread = dpcdRead(p_ctx, 0, DPCD_MAX_DOWNSPREAD, 1);
	p_ctx->dpcd_max_spread = dpcd_max_downspread;
	EMGD_DEBUG("dp: DPCD_MAX_DOWNSPREAD: 0x%02lx", dpcd_max_downspread);

#ifdef DPCD_DEBUG
	/* DPCD 4, Number assigned to the RX Port; value = +1 */
	dpcd_rx_no = dpcdRead(p_ctx, 0, DPCD_NO_RX_PORT_COUNT, 1);
	EMGD_DEBUG("dp: DPCD_NO_RX_PORT_COUNT: 0x%02lx", dpcd_rx_no);

	/* DPCD 5, Is Downstream Port present? */
	dpcd_downstream_present = dpcdRead(p_ctx, 0, DPCD_DOWNSTREAM_PRESENT, 1);
	EMGD_DEBUG("dp: DPCD_DOWNSTREAM_PRESENT: 0x%02lx", dpcd_downstream_present);

	/* DPCD 6, Maximum Link Channel Coding */
	dpcd_max_channel_coding = dpcdRead(p_ctx, 0, DPCD_MAX_CHANNEL_CODING, 1);
	EMGD_DEBUG("dp: DPCD_MAX_CHANNEL_CODING: 0x%02lx", dpcd_max_channel_coding);

	/* DPCD 7, Number of Downstream Port Count*/
	dpcd_downstream_port_count = dpcdRead(p_ctx, 0, DPCD_DOWNSTREAM_PORT_COUNT, 1);
	EMGD_DEBUG("dp: DPCD_DOWNSTREAM_PORT_COUNT: 0x%02lx", dpcd_downstream_port_count);
#endif
	/* DPCD 8 & DPCD A, Local EDID Present*/
	dpcd_port0_cap0 = dpcdRead(p_ctx, 0, DPCD_RECEIVE_PORT_0_CAP_0, 1);
	p_ctx->edid_present = (dpcd_port0_cap0 & BIT(1))?1:0;
	EMGD_DEBUG("dp: DPCD_RECEIVE_PORT_0_CAP_0: 0x%02lx", dpcd_port0_cap0);

#ifdef DPCD_DEBUG
	dpcd_port1_cap0 = dpcdRead(p_ctx, 0, DPCD_RECEIVE_PORT_1_CAP_0, 1);
	EMGD_DEBUG("dp: DPCD_RECEIVE_PORT_1_CAP_0: 0x%02lx", dpcd_port1_cap0);

	/* DPCD 9 & DPCD B, Port Lanes Buffer Size; Buffer size = (value + 1)*32 per lane */
	dpcd_port0_cap1 = dpcdRead(p_ctx, 0, DPCD_RECEIVE_PORT_0_CAP_1, 1);
	dpcd_port1_cap1 = dpcdRead(p_ctx, 0, DPCD_RECEIVE_PORT_1_CAP_1, 1);
	EMGD_DEBUG("dp: DPCD_RECEIVE_PORT_0_CAP_1: 0x%02lx", dpcd_port0_cap1);
	EMGD_DEBUG("dp: DPCD_RECEIVE_PORT_1_CAP_1: 0x%02lx", dpcd_port1_cap1);

	/* DPCD C, I2C Speed Control Capability */
	/* NOTE: If DisplayPort Rx doesn't implement the Physical I2C Bus, value 0x00 */
	dpcd_i2c_control_cap = dpcdRead(p_ctx, 0, DPCD_I2C_CONTROL_CAP, 1);
	EMGD_DEBUG("dp: DPCD_I2C_CONTROL_CAP: 0x%02lx", dpcd_i2c_control_cap);
#endif

	/* DPCD D, eDP Configuration Capability(eDP or DP detection) */
	/* NOTE: This DPCD field is RESERVED in DP1.1a, while valid in DP1.2 and eDP1.3 onwards */
	/* This was tested on Dell U2410 Panel which is DP1.1a. */
	dpcd_edp_configuration_cap = dpcdRead(p_ctx, 0, DPCD_EDP_CONFIGURATION_CAP, 1);

	EMGD_DEBUG("dp: DPCD_EDP_CONFIGURATION_CAP: 0x%lx", dpcd_edp_configuration_cap);
	/* DPCD E, Link status / adjust read interval */
	dpcd_training_aux_interval = dpcdRead(p_ctx, 0, DPCD_TRAINING_AUX_RD_INTERVAL, 1);
	p_ctx->dpcd_rd_interval = dpcd_training_aux_interval;
	EMGD_DEBUG("dp: DPCD_TRAINING_AUX_RD_INTERVAL: 0x%02lx", dpcd_training_aux_interval);

	p_ctx->embedded_panel = (dpcd_edp_configuration_cap & 0xB) ? 1 : 0;

	/* UMG: LFP Panel will be using 6bpc and DFP will be using 8bpc */
	/*TODO: Need to read downstream bits per color.*/
	p_ctx->bits_per_color = 0; 
	EMGD_DEBUG ("DPCD BPC is 0x%X", p_ctx->bits_per_color);
	/*eDP AUX control registers*/
#ifdef DPCD_DEBUG
	edp_rev = dpcdRead(p_ctx, 0, 0x700, 1);
	EMGD_DEBUG("dp: EDP_REV: 0x%02lx", edp_rev);

	edp_gen_cap_reg_1 = dpcdRead(p_ctx, 0, 0x701, 1);
	EMGD_DEBUG("dp: EDP_GENERAL_CAPABILITY_REGISTER_1 : 0x%02lx", edp_gen_cap_reg_1);

	edp_backlight_adjustment_cap_reg = dpcdRead(p_ctx, 0, 0x702, 1);
	EMGD_DEBUG("dp: EDP_BACKLIGHT_ADJUSTMENT_CAPABILITY_REGISTER: 0x%02lx", edp_backlight_adjustment_cap_reg);
#endif
	EMGD_DEBUG ("EDP:@2 p_ctx->embedded_panel = %lu", p_ctx->embedded_panel );

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

int dp_dump_vswing_preemp_dpio(dp_device_context_t *p_ctx)
{
	unsigned long data;
	unsigned long dpio_reg = 0;
	unsigned long val_PDiv = 0;
	unsigned long val_Temp = 0;

	EMGD_TRACE_ENTER;
	data = 0x00;

	val_PDiv = dp_read_dpio_register(p_ctx, dpio_reg+0x48);

	if(p_ctx->control_reg == DP_CONTROL_REG_B){
		dpio_reg = 0x100;
	} else if(p_ctx->control_reg == DP_CONTROL_REG_C) {
		dpio_reg = 0x200;
	}

	val_Temp = dp_read_dpio_register(p_ctx, dpio_reg+0x24);

	switch(val_PDiv) {
	case 0x055338954: /* 400mV 0db */
		break;

	case 0x0554d8954: /* 600mV 0db */
		switch(val_Temp) {
		case 0x00004000:	/* 600mV 0db */
			data = (data | 0x01 << 8 | 0x00);
			break;
		case 0x00002000:	/* 400mV 3p5db */
			data = (data | 0x00 << 8 | 0x01);
			break;
		default:
			break;
		}
		break;

	case 0x055738954: /* 600mV 3p5db */
		data = (data | 0x01 << 8 | 0x01);
		break;

	case 0x055668954:
		switch(val_Temp) {
		case 0x00004000: /* 800mV 0db */
			data = (data | 0x02 << 8 | 0x00);
			break;
		case 0x00000000: /* 400mV 6db */
			data = (data | 0x00 << 8 | 0x02);
			break;
		default:
			break;
		}
		break;
	case 0x0559ac0d4:
		switch(val_Temp) {
		case 0x00004000: /* 1200mV 0db */
			data = (data | 0x03 << 8 | 0x00);
			break;
		case 0x00002000: /* 800mV 3p5db */
			data = (data | 0x02 << 8 | 0x01);
			break;
		case 0x00000000: /* 600mV 6db */
			data = (data | 0x01 << 8 | 0x02);
			break;
		case 0x00006000: /* 400mV 9p5db */
			data = (data | 0x00 << 8 | 0x03);
			break;
		default:
			break;
		}
		break;
	default: 
		break;
	}

	EMGD_DEBUG("dp: VSWING: 0x%02lx", (data >> 8));
	EMGD_DEBUG("dp: PREEMP: 0x%02lx", (data & 0xFF));

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*---------------------------------------------------------------------------*\
Function   : dp_link_training
Parameters : p_context: Pointer to port driver allocated context
              max_rate:
              flags
Remarks    :
Returns    :
\*---------------------------------------------------------------------------*/
int dp_link_training(void *p_context, pd_timing_t *p_mode,unsigned long max_rate,
					 unsigned long flags)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned long cr_done = 0;
	unsigned long eq_done = 0;
	unsigned long temp_reg = 0;
	unsigned long regvalue1;
	unsigned long regvalue2;
	unsigned long req_adj_same_level = 0;
	unsigned long cur_vswing  = voltage_0_4;
	unsigned long cur_preemp  = preemp_no;
	unsigned long prev_vswing = cur_vswing;
	unsigned long prev_preemp = cur_preemp;
	unsigned long max_vswing = 0;
	unsigned long max_preemp = 0;
	unsigned long redo_training = 0;
	unsigned char lane_cnt = 0;
	int ret_val = PD_ERR_UNSUCCESSFUL;
	unsigned long link_rate;
	

	EMGD_TRACE_ENTER;


	if (max_rate == DP_LINKBW_1_62_GBPS){
		link_rate = 162000;
	} else {
		link_rate = 270000;
	}
	/*FIXME: TODO: BIT(3) of 61204 never been written.*/

	lane_cnt = (unsigned char)(p_ctx->dpcd_max_count & 0x0F);

	dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
	/* Note: 
	 * 1) UMG's finding on the VB-ID Transmission during x1 and x2 mode 
	 * doesn't meet DP Spec 1.1, thus using a WA flag to set this 
	 * idle training pattern only needed. 
	 * 2) UMG found issue with DP2VGA dongle, where idle pattern on 
	 * mode other than x4 will cause corruption. Thus idle pattern is only
	 * set when mode is x4.
	 */
	if(lane_cnt == 0x4) {
		temp_reg &= ~(BIT(29)|BIT(28));
		temp_reg |= BIT(29);			/* Idle Training Pattern */
		dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);

		/* UMG: Delay for 17ms, or next vblank */
		mdelay(17);
	}

	/* Disable Port */
	temp_reg &= ~BIT(31);            
	dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);

	dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
	switch(p_ctx->dpcd_max_count & 0xF) {
		case 1: 
			temp_reg &= ~(BIT20|BIT19); /*x1*/
			break;
		case 2: 
			temp_reg &= ~(BIT20|BIT19); /*x2*/
			temp_reg |= BIT19;
			break;
		case 4:
			temp_reg |= (BIT20 | BIT19);  /*x4*/
			break;
		default:
			/* Error: Unknown Lane Count */
			break;
	}
	temp_reg |= BIT18; /* Enable Framing */
	temp_reg |= BIT31; /* Enable Port */
	temp_reg &= ~(BIT29|BIT28); /* Training pattern 1 */

	if(flags & PD_SET_MODE_PIPE_B) {
		temp_reg |= BIT30;
	}

	dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);


	if(p_ctx->pwr_method) {
		edp_panel_power(p_ctx, TRUE);
	}

	/* Setting Enhanced Framing bit in Sink once Panel Power is done and ready for AUX Transactions */
	dpcdWrite(p_ctx, 0, DPCD_LANE_COUNT_SET_ADDR, 1, p_ctx->dpcd_max_count);

	/* Training Pattern 1 */
	EMGD_DEBUG("dp: Training Pattern 1");
	dpcdWrite(p_ctx, 0, DPCD_LINK_BW_SET_ADDR, 1, max_rate);

	/* UMG: Setting Lane Count, this is duplicated because the enhanced framing and lane count is the same DPCD address */
	/*dpcdWrite(p_ctx, 0, DPCD_LANE_COUNT_SET_ADDR, 1, p_ctx->dpcd_max_count);*/

	/* No scramble: BIT5 = 1*/
	dpcdWrite(p_ctx,0, DPCD_TRAINING_PATTERN_SET_ADDR, 1, 0x21);

	/* DPIO write - 
	 * 0x58 - 0x00505313A
	 * 0x54 - 0x043406055
	 * 0x48 - 0x055338954
	 * 0x54 - 0xc3406055
	 * 0x2c - 0x1f030040
	 * 0x24 - 0x00004000
	 */
	/* Begins training with Level 0 VSwing and Level 0 Pre-emp */

	if (PD_SUCCESS != dp_set_vswing_preemphasis(p_ctx, cur_vswing,
		cur_preemp)) {
		EMGD_DEBUG("dp: Invalid parameter passed in!!!");
		return ret_val;
	}

	dp_set_training_level(p_ctx, cur_vswing, cur_preemp, max_vswing, max_preemp);

	cr_done = 0;

	/* Loop only 5 times according to the DP spec */
	while ((req_adj_same_level < 5) && (0 == max_vswing)) {

		prev_vswing = cur_vswing;
		prev_preemp = cur_preemp;
		
		if (redo_training) {
			redo_training = 0;
			/* Define current training levels */
			if (cur_vswing >= voltage_1_2) {
				max_vswing = 1;
			} else {
				max_vswing = 0;
			}
			if (cur_preemp >= preemp_9_5dB) {
				max_preemp = 1;
			} else {
				max_preemp = 0;
			}
		
			if (PD_SUCCESS != dp_set_vswing_preemphasis(p_ctx, cur_vswing,
				cur_preemp)) {
				EMGD_DEBUG("dp: Invalid parameter passed in!!!");
				break;
			}

			dp_set_training_level(p_ctx, cur_vswing, cur_preemp, max_vswing, max_preemp);
		}

		if(p_ctx->dpcd_rd_interval) {
			pd_usleep(rd_intervals[p_ctx->dpcd_rd_interval]);
		} else {
			/* 
			 * DP Spec 1.2 states that if dpcd_rd_interval is 0x00, 
			 * use 100us for CR and 400us for EQ Training
			 */
			pd_usleep(100);
		}

		/* Checking clock recovery */
		regvalue1 = dpcdRead(p_ctx, 0, DPCD_LANE0_1_STATUS_ADDR, 1);
		regvalue2 = dpcdRead(p_ctx, 0, DPCD_LANE2_3_STATUS_ADDR, 1);

		/* BIT0 and BIT4 = CR Done */
		if((p_ctx->dpcd_max_count & 0xF) == 0x1) {
			if ((regvalue1 & 0x1) == 0x1) {
				cr_done = 1;
			} else {
				cr_done = 0;
			}
		} else if((p_ctx->dpcd_max_count & 0xF)  > 1) {
			if ((regvalue1 & 0x11) == 0x11) {
				cr_done = 1;
			} else {
				cr_done = 0;
			}
			/* For 4 Lanes LT, check status for Lane 2 and Lane 3 as well */
			if((p_ctx->dpcd_max_count & 0xF) == 4) {
				if((regvalue2 & 0x11) == 0x11) {
					cr_done |= 1;
				} else {
					cr_done &= 0;
				}
			}
		}

		if(cr_done == 1) {
			EMGD_DEBUG("dp: CR SUCCESSFUL");
			/* Check the equalization */
			/* Note: max vswing and per-emphasis will be handled */
			if (PD_SUCCESS == dp_perform_equalization(p_ctx, cur_vswing,
				cur_preemp)) {
				eq_done = 1;
				/* Notify the sink not in training and enable scrambling */
				EMGD_DEBUG("dp: EQ SUCCESSFUL");
				EMGD_DEBUG("dp: Port:0x%lx Link Training Successful with %s",
					p_ctx->control_reg, (max_rate == 0xa)?"HBR":"LBR");

				/* Disable Training Pattern */
				/* BIT1:0 = 00; Not in Training */
				/* BIT(5) = 0; Scrambling Enable */
				dpcdWrite(p_ctx, 0, DPCD_TRAINING_PATTERN_SET_ADDR, 1, 0);

				/* Set the source idle training pattern */
				dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
				temp_reg &= ~(BIT(29)|BIT(28));
				temp_reg |= BIT(29);
				dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);
				
				/* Delay 170 ms */
				pd_usleep(17000);
				
				/* Set source training not in progress */
				dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
				temp_reg |= (BIT(29)|BIT(28));
				dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);

				break;
			} else {
				EMGD_DEBUG("dp: EQ training failed...Re-do withh lower rate!!!");
				break;
			}
		} else {
			/* Get the pre-emphasis and voltage swing level from the sink */
			if (PD_SUCCESS == dp_get_request_equalization(p_ctx, &cur_vswing, 
					&cur_preemp)) {
				if ((cur_vswing == prev_vswing) && (cur_preemp == prev_preemp)) {
					req_adj_same_level++;
				} else {
					req_adj_same_level = 0;
				}
				/* signal the redo training */
				redo_training = 1;
			} else {
				EMGD_DEBUG("dp: Training failed!!!");
			}
		}
	} /* while */

	if (!cr_done || !eq_done) {
		EMGD_DEBUG("dp: Training failed!!!");

		/* Set source training not in progress */
		dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
		temp_reg |= (BIT(29)|BIT(28));
		dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);

	} else {
		unsigned long ctrl_reg = 0x0;
	

		/* Turn of VDD after finish link training */
		if(p_ctx->embedded_panel || p_ctx->pwr_method) {
			edp_enable_vdd(p_ctx, FALSE);
		}

		/* After Link Training, reset the Training Patterns bit to Pattern 1(default) */
		dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &ctrl_reg);
		ctrl_reg &= ~(BIT28 | BIT29);
		ctrl_reg &= ~BIT7; /* enable scrambling */
		dp_write_mmio_reg(p_ctx, p_ctx->control_reg, ctrl_reg);

		EMGD_DEBUG("dp: 1 After Link training at 0x%02lx with 0x%02lx", p_ctx->control_reg, ctrl_reg);

		mdelay(17);
		/* Set the TP to Not in Training */
		ctrl_reg |= (BIT28 | BIT29);
		dp_write_mmio_reg(p_ctx, p_ctx->control_reg, ctrl_reg);
		EMGD_DEBUG("dp: 2 After Link training at 0x%02lx with 0x%02lx", p_ctx->control_reg, ctrl_reg);
		ret_val = PD_SUCCESS;
	}

	EMGD_TRACE_EXIT;
	return ret_val;
}

#define TransADataN1      0x60034
#define TransADPLinkM1    0x60040
#define TransADPLinkN1    0x60044

#define TransBDataM1      0x61030
#define TransBDataN1      0x61034
#define TransBDPLinkM1    0x61040
#define TransBDPLinkN1    0x61044
#define  TU_SIZE(x)             (((x)-1) << 25) /* default size 64 */

static void
intel_reduce_ratio(unsigned long *num, unsigned long *den)
{
    while (*num > 0xffffff || *den > 0xffffff) {
         *num >>= 1;
         *den >>= 1;
    }
}


static void
intel_dp_compute_m_n(int bpp,
             int nlanes,
             int pixel_clock,
             int link_clock,
			unsigned long *tu,
			unsigned long *gmch_m,
			unsigned long *gmch_n,
			unsigned long *link_m,
			unsigned long *link_n)	
              //struct intel_dp_m_n *m_n)
{
	EMGD_DEBUG ("bpp = %d\n nlanes = %d\n pixel_clock = %d\n link_clock = %d",
			bpp, nlanes,pixel_clock, link_clock);
	
    *tu = 64;
    *gmch_m = (pixel_clock * bpp) >> 3;
    *gmch_n = link_clock * nlanes;
    intel_reduce_ratio(gmch_m, gmch_n);
    *link_m = pixel_clock;
    *link_n = link_clock;
    intel_reduce_ratio(link_m, link_n);
}



int dp_program_mntu_regs(dp_device_context_t *p_ctx, pd_timing_t *p_mode, unsigned long flags, unsigned long rate, unsigned long bpc)
{
	unsigned long lane_count = 0;
	unsigned long pixel_hz = 0;
	/* UMG: LFP Panel will be using 6bpc and DFP will be using 8bpc */
	unsigned long bpp = 0 /*p_ctx->bits_per_color * 3*/;
	unsigned long link_m = 0, link_n = 0;
	unsigned long gmch_m = 0, gmch_n = 0;
	unsigned long tu = 63; /* Fixed value of (64 - 1) = 0x7e000000 */


	EMGD_TRACE_ENTER;
	
	/* edp_panel_fit (p_ctx); */

	switch ( bpc ){
		case IGD_DISPLAY_BPC_10:
			EMGD_DEBUG ("BPC = 10");
			bpp = 10 * 3;
			break;
		case IGD_DISPLAY_BPC_8:
			EMGD_DEBUG ("BPC = 8");
			bpp = 8 * 3;
			break;
		case IGD_DISPLAY_BPC_6:
			EMGD_DEBUG ("BPC = 6");
			bpp = 6 * 3;
			break;
		default:
			EMGD_ERROR("Bits_per_color not set!");

	}
	/* DP/eDP Specific Registers Programming */
	lane_count = p_ctx->dpcd_max_count & 0xF;
	pixel_hz = p_mode->dclk * 1000; /* dclk is in KHz */

	EMGD_DEBUG ("dp: dclk = %lu",  p_mode->dclk);
	EMGD_DEBUG ("dp: lane_count = %lu",  lane_count);
	EMGD_DEBUG ("dp: rate = %lu",  rate);
	EMGD_DEBUG ("dp: pixel_hz = %lu",  pixel_hz);

	
	intel_dp_compute_m_n(bpp,
            lane_count,
            pixel_hz/1000,
            rate,
            &tu,
            &gmch_m,
             &gmch_n,
             &link_m,
             &link_n);

	if(flags & PD_SET_MODE_PIPE_B) {
		dp_write_mmio_reg(p_ctx, TransBDataM1 , TU_SIZE(tu) | gmch_m );
		dp_write_mmio_reg(p_ctx, TransBDataN1 , gmch_n);
		dp_write_mmio_reg(p_ctx, TransBDPLinkM1 , link_m);
		dp_write_mmio_reg(p_ctx, TransBDPLinkN1 , link_n);	

	} else {
		dp_write_mmio_reg(p_ctx, TransADataM1 , TU_SIZE(tu) | gmch_m );
		dp_write_mmio_reg(p_ctx, TransADataN1 , gmch_n);
		dp_write_mmio_reg(p_ctx, TransADPLinkM1 , link_m);
		dp_write_mmio_reg(p_ctx, TransADPLinkN1 , link_n);	

	}


	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

/*-------------------------------------------------------------------------*\
 * dp_get_request_equalization
 *
 * return: pass - PD_SUCCESS
 *         fail - PD_ERR_UNSUCCESSFUL
 * NOTE: hard-coded to handle lane#0
\*-------------------------------------------------------------------------*/
static int dp_set_vswing_preemphasis(void *p_context, unsigned long vswing,
							  unsigned long preemphasis)
{
	dp_device_context_t *p_ctx = (dp_device_context_t*)p_context;
	unsigned long reg_index = 0xc000;

	EMGD_TRACE_ENTER;

	if ((vswing + preemphasis) > 3) {
		return PD_ERR_UNSUCCESSFUL;
	}

	if(p_ctx->p_callback->port_num == DP_PORT_B_NUMBER) {
		reg_index = 0x8200;
		EMGD_DEBUG("DP: setting vswing and preemphasis for DP Port B, with vswing = %lu, preemphasis = %lu, port_num = %u \n", 
			vswing, preemphasis, p_ctx->p_callback->port_num);
	} else if(p_ctx->p_callback->port_num == DP_PORT_C_NUMBER) {
		reg_index = 0x8400;
		EMGD_DEBUG("DP: setting vswing and preemphasis for DP Port C/EDP, with vswing = %lu, preemphasis = %lu port_num = %u \n", 
			vswing, preemphasis, p_ctx->p_callback->port_num);
	}

	//dp_write_dpio_register(p_ctx, reg_index+0x58, 0x0505313a);
	//dp_write_dpio_register(p_ctx, reg_index+0x54, 0x43406055);


	/* These are fixed values as used by both UMG and SV Team */
	if(preemphasis == 0 && vswing == 0) { /* 0dB, 0.4V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B405555);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x552AB83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00004000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);

	}

	if(preemphasis == 0 && vswing == 1) { /* 0dB, 0.6V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B404040);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5548B83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00004000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);

	}

	if(preemphasis == 0 && vswing == 2) { /* 0dB, 0.8V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B245555);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5560B83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00004000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
	
	}

	if(preemphasis == 0 && vswing == 3) { /* 0dB, 1.2V */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B405555);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5598DA3A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00004000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
		
	}

	if(preemphasis == 1 && vswing == 0) { /* 3.5dB, 0.4V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B404040);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5552B83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00002000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);

	}

	if(preemphasis == 1 && vswing == 1) { /* 3.5dB, 0.6V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B404848);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5580B83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00002000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
		
	}

	if(preemphasis == 1 && vswing == 2) { /* 3.5dB, 0.8V */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B404040);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x55ADDA3A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00002000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);

	}

	if(preemphasis == 2 && vswing == 0) { /* 6dB, 0.4V */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B305555);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x5570B83A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
		
	}

	if(preemphasis == 2 && vswing == 1) { /* 6dB, 0.6V */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x2B2B4040);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x55ADDA3A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
	
	}

	if(preemphasis == 3 && vswing == 0) { /* 9.5dB, 0.4V  */
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x00000000);
		dp_write_dpio_register(p_ctx, reg_index+0x90, 0x1B405555);
		dp_write_dpio_register(p_ctx, reg_index+0x88, 0x55ADDA3A);
		dp_write_dpio_register(p_ctx, reg_index+0x8C, 0x0C782040);
		dp_write_dpio_register(p_ctx, reg_index+0x2C, 0x00030000);
		dp_write_dpio_register(p_ctx, reg_index+0x24, 0x00006000);
		dp_write_dpio_register(p_ctx, reg_index+0x94, 0x80000000);
		
	}

	EMGD_TRACE_EXIT;

	return PD_SUCCESS;
}

#ifndef CONFIG_MICRO
/* works for both Write/Read operation 
 * retData will be updated with data if read.
 */
unsigned int Start_Xmit(dp_device_context_t *p_ctx, unsigned long portAddr, 
														unsigned long numByte, unsigned long *retData)
{
	unsigned long regVal=0;
	unsigned int counter;

	dp_read_mmio_reg(p_ctx, portAddr, &regVal);

	/* FIXME: clear the status one-by-one */
	if (regVal & 0x52000000) {
		regVal |= (regVal & 0x52000000);
		dp_write_mmio_reg(p_ctx, portAddr, regVal);
	}

	/*
	if (regVal & 0x12000000) {
		regVal = regVal | 0x12000000;
		dp_write_mmio_reg(p_ctx, portAddr, regVal);
	}*/

	for (counter = 0; counter < 6; counter++) {

			/* wait til busy bit to clear */
			do {
				dp_read_mmio_reg(p_ctx, portAddr, &regVal);
			} while (regVal & 0x80000000);

			regVal = regVal & 0xFE0FFFFF;	/* clear the message size field */
			regVal = regVal | (numByte << 20);
			regVal = regVal | 0x80000000;
			dp_write_mmio_reg(p_ctx, portAddr, regVal);
	
			/* Wait till transaction complete status */
			do {
				dp_read_mmio_reg(p_ctx, portAddr, &regVal);
			} while (!(regVal & 0x40000000));

			EMGD_DEBUG ("dp: Reading register portAddr = 0x%lX with value of 0x%lx", portAddr, regVal);

			if (!(regVal & 0x12000000)) {
				/* if we are here, then the xmit was done at least.
				** Now, lets check for the response/reply from SINK
				*/
				dp_read_mmio_reg(p_ctx, portAddr + 4, &regVal);

				/* The reply will be either AUX_NACK(0x01), AUX_DEFER (0x02), and AUX_ACK (0x00)
				** We only going to look for AUX_ACK for now.
				*/
				if ((regVal & 0x30000000) == 0x00000000) { /* AUX_ACK is 0x00 */
					*retData= regVal << 0x08;
					dp_read_mmio_reg(p_ctx, portAddr + 8, &regVal);
					
					regVal= (regVal >> 24) | *retData;
					regVal= (regVal >> 16) | (regVal << 16);
					*retData= ((regVal & 0xFF00FF00) >> 8) | ((regVal & 0x00FF00FF) << 8);
					return PD_SUCCESS;
				}
			}

			dp_read_mmio_reg(p_ctx, portAddr, &regVal);
			regVal= regVal | 0x52000000;
	
			EMGD_DEBUG ("dp: Writing register portAddr = 0x%lX with value of 0x%lx", portAddr, regVal);
			dp_write_mmio_reg(p_ctx, portAddr, regVal);
	}
	return 0;
}

unsigned int SendCommand_AUX(dp_device_context_t *p_ctx, dp_aux_port_t *aux)
{
	unsigned long regData = aux->data;
	unsigned long dp_cmd = (unsigned long)aux->dp_cmd;
	unsigned long regCmd;
	unsigned int	retVal;

	/* for now we are going to limit the payload to 4 byte for both read/write */
	if ((aux->numByte == 0) || (aux->numByte > 4)) {
		return PD_ERR_INTERNAL;
	}

	if ((dp_cmd & 0x01) == 0) { /* is it write command */
		regData= (aux->data >> 16) | (aux->data << 16);
		regData= ((regData & 0xFF00FF00) >> 8) | ((regData & 0x00FF00FF) << 8);
		dp_write_mmio_reg(p_ctx, aux->portAddr + 8, regData);

	}

	regCmd= ((unsigned long)(dp_cmd) << 28) | 
					(unsigned long)(aux->dp_addr << 8) | (aux->numByte - 1);
	dp_write_mmio_reg(p_ctx, aux->portAddr + 4, regCmd);

	/* this may be wait till command complete with status */
	if (dp_cmd & 0x0001) { /* AUX Read Command if 1 */
		retVal= Start_Xmit(p_ctx, aux->portAddr, (unsigned long)4, &aux->data);
	} else { /* AUX Write Command */
		retVal= Start_Xmit(p_ctx, aux->portAddr, (unsigned long)(aux->numByte+4), &aux->data);
	}

	return retVal;
}
#endif

unsigned long dpcdRead(dp_device_context_t *p_ctx, unsigned char ddi_cnt, unsigned long addr, unsigned char byte_no)
{
	dp_aux_port_t aux;
	unsigned int retVal;

#if defined(CONFIG_MICRO) && (CONFIG_VBIOS_INITTIME == 1)
	common_rom_data_t *common_data= get_common_data_ptr_offset();
#endif

	if(p_ctx->control_reg == DP_CONTROL_REG_B){
		aux.portAddr = DP_AUX_CH_CTL_B;
	} else if(p_ctx->control_reg == DP_CONTROL_REG_C){
		aux.portAddr = DP_AUX_CH_CTL_C;
	} else{
		EMGD_ERROR("Unknown Port for internal DP");
		return PD_ERR_INTERNAL;
	}

	if ((byte_no == 0) || (byte_no > 4)) {
		EMGD_ERROR("empty byte or overflow.");
		return PD_ERR_INTERNAL;
	}

	aux.numByte= byte_no;
	aux.dp_cmd= 0x9;				/* Display port Read */
	aux.dp_addr= addr;
	aux.data= 0;

#ifndef CONFIG_MICRO
	retVal= SendCommand_AUX(p_ctx, &aux);
#else
	#if defined(CONFIG_MICRO) && (CONFIG_VBIOS_INITTIME == 1)
		retVal= common_data->SendCommand_AUX(&aux);
	#else
		retVal= SendCommand_AUX(&aux);
	#endif	
#endif
	/* 
	** The retVal contains the status of operation that 
	** can't be pass to caller since caller expects the data as return value
	*/
	EMGD_DEBUG ("dp: return aux.data = %ld\n", aux.data);
	return aux.data;
}

int dpcdWrite(dp_device_context_t *p_ctx, unsigned char ddi_cnt,
				 unsigned short addr, unsigned char byte_no, unsigned long data)
{
	dp_aux_port_t aux;
	unsigned int retVal;

#if defined(CONFIG_MICRO) && (CONFIG_VBIOS_INITTIME == 1)
		common_rom_data_t *common_data= get_common_data_ptr_offset();
#endif

	if(p_ctx->control_reg == DP_CONTROL_REG_B){
		aux.portAddr = DP_AUX_CH_CTL_B;
	} else if(p_ctx->control_reg == DP_CONTROL_REG_C){
		aux.portAddr = DP_AUX_CH_CTL_C;
	} else{
		EMGD_ERROR("Unknown Port for internal DP");
		return PD_ERR_INTERNAL;
	}

	if ((byte_no == 0) || (byte_no > 4)) {
		return PD_ERR_INTERNAL;
	}

	aux.numByte= byte_no;
	aux.dp_cmd= 0x8;				/* Display port Read */
	aux.dp_addr= (unsigned long)addr;
	aux.data= data;

#ifndef CONFIG_MICRO
	retVal= SendCommand_AUX(p_ctx, &aux);
#else
	#if defined(CONFIG_MICRO) && (CONFIG_VBIOS_INITTIME == 1)
		retVal= common_data->SendCommand_AUX(&aux);
	#else
		retVal= SendCommand_AUX(&aux);
	#endif	
#endif

	/* 
	** The retVal contains the status of operation that 
	** can't be pass to caller since caller expects the data as return value
	*/
	
	return aux.data;
}



static void edp_panel_fit(dp_device_context_t *p_ctx)
{
	unsigned long panel_fit_reg=0;
	unsigned long src_ratio, dest_ratio;
	pd_attr_t      *dp_attr      = NULL; 
	unsigned long upscale = 0;	

	EMGD_TRACE_ENTER;
	dp_read_mmio_reg(p_ctx, PFIT_CONTROL, &panel_fit_reg);

#if 0
	if (p_ctx->current_mode->width != p_ctx->fp_width ||
		p_ctx->current_mode->height != p_ctx->fp_height) {
		/* Enable panel fitting */
		panel_fit_reg |= BIT(31);
	} else {
		/* Disable panel fitting */
		panel_fit_reg &= ~BIT(31);
	}
	/* This is VLV DP/eDP workaround for flickering issue. */
	panel_fit_reg |= BIT(31);
#endif

	/* Select the pipe */
	if (p_ctx->pipe & PD_SET_MODE_PIPE_B) {
		EMGD_DEBUG ("Panel fit for Pipe B");
		if (p_ctx->p_callback->port_num == DP_PORT_B_NUMBER){
			unsigned long temp_reg = 0;
			dp_read_mmio_reg(p_ctx, 0x64200, &temp_reg);
			if ((temp_reg & BIT31) && (!(temp_reg & BIT30)) &&
				(panel_fit_reg & BIT31) && (!(panel_fit_reg & BIT29))){
			
			 /* Pipe B panel fit only supported on port DP-C and not DP-B 
			  *	if it is already in use by Pipe A*/
				EMGD_DEBUG ("EMGD does not support panel fitter on Secondary Port B.");
				EMGD_TRACE_EXIT;
				return;
			}
		}
		panel_fit_reg |= BIT(29);   /* bits[30:29] = 01 for pipe B */
	} else {
		EMGD_DEBUG ("Panel fit for Pipe A");
		panel_fit_reg &= ~BIT(29);   /* bits[30:29] = 00 for pipe A */
	}
	panel_fit_reg &= ~BIT(30);

	/* This is VLV DP/eDP workaround for flickering issue. */
	panel_fit_reg |= BIT(31);

	panel_fit_reg &= ~(BIT28 | BIT27 | BIT26); /* Reset Scaling mode*/
	panel_fit_reg &= ~(BIT25 | BIT24 ); /* Reset filter coeff.*/

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_MAINTAIN_ASPECT_RATIO, 0);
	p_ctx->aspect_ratio = dp_attr->current_value; /* default is OFF */

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_TEXT_TUNING, 0);
	p_ctx->text_tune = dp_attr->current_value; /* default is FUZZY */

 	dp_attr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_PANEL_FIT, 0);
	upscale = dp_attr->current_value; /* default is FUZZY */

	if (upscale){
		if(p_ctx->current_mode->private.extn_ptr) {
			
			igd_timing_info_t *scaled_timings =
				(igd_timing_info_t *)p_ctx->current_mode->private.extn_ptr;

				EMGD_DEBUG ("Maintain aspect ratio = %hd \nText tune = %ld", 
				p_ctx->aspect_ratio,
				p_ctx->text_tune);
			EMGD_DEBUG ("Native width = %hd, Wanted width =%hd",
				p_ctx->native_dtd->width,
				scaled_timings->width);
			
				if (p_ctx->native_dtd &&
						((scaled_timings->width != p_ctx->native_dtd->width) ||
						(scaled_timings->height != p_ctx->native_dtd->height))){
				
					EMGD_DEBUG ("DP Display will be scaled.\n");
					/* Scaling mode:
					*    Default - Auto scaling src_ratio == dest_ratio
					*    Piller box scaling - src_ratio < dest_ratio
					*    Letter box scaling - src_ratio > dest_ratio */

					/* To make this work correctly, port driver shall know the
					* size of the framebuffer, not the src mode. Most of the
					* times the src mode is fb, but not all the cases.
					* User has an attribute to change
					*    1. Between Pillerbox and auto, and vice versa
					*                and
					*    2. Between Letterbox and auto, and vice versa.
					*/
					if (p_ctx->aspect_ratio) {
						EMGD_DEBUG("Scaling aspect ratio maintained.\n");
						src_ratio = (scaled_timings->width << 10)/
							(scaled_timings->height);
						dest_ratio = (p_ctx->native_dtd->width << 10)/
							(p_ctx->native_dtd->height);

						if (dest_ratio > src_ratio) {
							/* Pillarbox scaling */
							panel_fit_reg |= BIT(27);
						} else if (dest_ratio < src_ratio) {
							/* Letterbox scaling */
							panel_fit_reg |= BIT(27) | BIT(26);
						}
					}

					/* Filter coefficient select: pd_context->text_tune = 0,1,2 */
					if (p_ctx->text_tune <= 2){
						EMGD_DEBUG ("Tuning with tune = %ld", p_ctx->text_tune);
						panel_fit_reg |= (p_ctx->text_tune << 24);
					}
			}
		}
	}

	/*disabled panel fitter for the cases where its resolution greater or more then 2560x1600*/
	if(p_ctx->current_mode->width >= 2560 && p_ctx->current_mode->height >= 1080)
		panel_fit_reg &= ~BIT(31);

	dp_write_mmio_reg(p_ctx, PFIT_CONTROL, panel_fit_reg);
	EMGD_DEBUG("panel_fit_reg 0x61230 = 0x%lx", panel_fit_reg);
	EMGD_TRACE_EXIT;
}

/*-------------------------------------------------------------------------*\
 * dp_perform_equalization
 *
 * return: pass - PD_SUCCESS
 *         fail - PD_ERR_UNSUCCESSFUL
\*-------------------------------------------------------------------------*/
static int dp_perform_equalization(void *p_context,
		unsigned long vswing_level, unsigned long emphasis_level)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned long loop_count = 0;
	unsigned long temp_reg = 0;
	unsigned long regvalue1, regvalue2, regvalue3;
	unsigned long cr_done = 1;
	unsigned long eq_sym_lock_done = 0;
	unsigned long interlane_align_done = 0;
	unsigned long sink_vswing = 0;
	unsigned long sink_preemphasis = 0;
	unsigned long max_vswing = 0;
	unsigned long max_preemp = 0;
	unsigned char lane_cnt = 0;
	unsigned long ret_val = PD_ERR_UNSUCCESSFUL;

	EMGD_TRACE_ENTER;
	lane_cnt = (unsigned char)(p_ctx->dpcd_max_count & 0x0f);

	/* Training Pattern 2 for the source and the sink */
	EMGD_DEBUG("dp: Training Pattern 2");
	dp_read_mmio_reg(p_ctx, p_ctx->control_reg, &temp_reg);
	temp_reg &= ~BIT(29);
	temp_reg |= BIT(28);
	dp_write_mmio_reg(p_ctx, p_ctx->control_reg, temp_reg);

	temp_reg  = dpcdRead(p_ctx, 0, DPCD_TRAINING_PATTERN_SET_ADDR, 1);
	temp_reg &= ~BIT(0);
	temp_reg |= (BIT(1) | BIT(5));
	dpcdWrite(p_ctx, 0, DPCD_TRAINING_PATTERN_SET_ADDR, 1, temp_reg);

	pd_usleep(1000);

	/* Set the source vswing and pre-emphasis */
	/* 0x58 - 0x00505313A - DPIO 0x8258
	 * 0x54 - 0x043406055
	 * 0x48 - 0x055338954
	 * 0x54 - 0xc3406055
	 * 0x2c - 0x1f030040
	 * 0x24 - 0x00004000
	 */
	if (PD_SUCCESS != dp_set_vswing_preemphasis(p_ctx, vswing_level, 
		emphasis_level)) {
		return ret_val;
	}

	/* Disable pre-emphaisis in DPCD and update the same */
	if (vswing_level >= voltage_1_2) {
		max_vswing = 1;
	} else {
		max_vswing = 0;
	}
	if (emphasis_level >= preemp_9_5dB) {
		max_preemp = 1;
	} else {
		max_preemp = 0;
	}

	dp_set_training_level(p_ctx, vswing_level, emphasis_level, max_vswing, max_preemp);

	while (1) {
		interlane_align_done = 0;
		eq_sym_lock_done = 0;

		loop_count++;
		pd_usleep(400);	/* UMG's value */

		regvalue1 = dpcdRead(p_ctx, 0, DPCD_LANE0_1_STATUS_ADDR, 1);
		regvalue2 = dpcdRead(p_ctx, 0, DPCD_LANE2_3_STATUS_ADDR, 1);

		if((p_ctx->dpcd_max_count & 0xF) == 0x1) {
			if((regvalue1 & 0x7) == 0x7) {
				eq_sym_lock_done = 1;
			} else {
				EMGD_DEBUG("LANE0_1 Status: 0x%lx",regvalue1);
				EMGD_DEBUG("Loop Count: %ld", loop_count);
				eq_sym_lock_done = 0;
			}
		} else if ((p_ctx->dpcd_max_count & 0xF) > 1) {
			if((regvalue1 & 0x77) == 0x77) {
				eq_sym_lock_done = 1;
			} else {
				EMGD_DEBUG("LANE0_1 Status: 0x%lx",regvalue1);
				EMGD_DEBUG("Loop Count: %ld", loop_count);
				eq_sym_lock_done = 0;
			}

			if ((p_ctx->dpcd_max_count & 0xF) == 4) {
				if((regvalue2 & 0x77) == 0x77) {
					eq_sym_lock_done |= 1;
				} else {
					EMGD_DEBUG("LANE2_3 Status: 0x%lx",regvalue2);
					EMGD_DEBUG("Loop Count: %ld", loop_count);
					eq_sym_lock_done &= 0;
				}
			}
		}

		if (!cr_done) {
			/* Clear the lane status bits */
			dpcdWrite(p_ctx, 0, DPCD_LANE0_1_STATUS_ADDR, 1, 0);
			dpcdWrite(p_ctx, 0, DPCD_LANE2_3_STATUS_ADDR, 1, 0);
			EMGD_DEBUG("dp: Training Pattern #2 failed!!!");
			break;
		}

		regvalue3 = dpcdRead(p_ctx, 0, DPCD_LANE_ALIGN_STATUS_ADDR, 1);
		/* BIT0: interlane aligned done */
		if (regvalue3 & 0x1) {
			interlane_align_done = 1;
		}

		if (eq_sym_lock_done && interlane_align_done) {
			EMGD_DEBUG("dp: Training Pattern #2  A passes!!!");
			ret_val = PD_SUCCESS;
			break;
		} else {
			/* Maximum eq iteration is 5 */
			if (loop_count > 5) {
				EMGD_DEBUG("dp: Training Pattern #2  B failed!!!");
				break;
			}

			/* Getting the requested vswing and pre-emphasis values from SINK */
			if (PD_SUCCESS != dp_get_request_equalization(p_ctx, &sink_vswing, 
					&sink_preemphasis)) {
				EMGD_DEBUG("dp: Training Pattern #2 C failed!!!");
				return ret_val;
			}

			if ((sink_vswing + sink_preemphasis) > 3) {
				sink_vswing = 3 - sink_preemphasis;
			}

			/* Set the vswing and pre-emp for Tx */
			if (PD_SUCCESS != dp_set_vswing_preemphasis(p_ctx, sink_vswing, 
				sink_preemphasis)) {
				EMGD_DEBUG("dp: Training Pattern #2 D failed!!!");
				break;
			}

			if (sink_vswing >= voltage_1_2) {
				max_vswing = 1;
			} else {
				max_vswing = 0;
			}
			if (sink_preemphasis >= preemp_9_5dB) {
				max_preemp = 1;
			} else {
				max_preemp = 0;
			}

			dp_set_training_level(p_ctx, sink_vswing, sink_preemphasis, max_vswing, max_preemp);
		}
	}

	EMGD_TRACE_EXIT;
	return ret_val;
}

/*-------------------------------------------------------------------------*\
 * Function:	dp_get_request_equalization
 *
 * Return:		pass - PD_SUCCESS
 *				fail - PD_ERR_UNSUCCESSFUL
 *
 * Remarks:		(From UMG) If different drive settings were requested from
 *				multiple lanes, DisplayPort Tx may use the highest pre-emphasis
 *				and voltage swing from all the requested drive settings.
 *				this is due to the per-lane drive setting adjustability is 
 *				an OPTIONAL feature for DisplayPort Tx.
\*-------------------------------------------------------------------------*/
static int dp_get_request_equalization(void *p_context,
		unsigned long* ptr_vswing, unsigned long* ptr_preemp)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned long req_value0_1, req_value2_3;
	
	EMGD_TRACE_ENTER;
	req_value0_1 = dpcdRead(p_ctx, 0, DPCD_ADJUST_REQUEST_LANE0_1_ADDR, 1);
	req_value2_3 = dpcdRead(p_ctx, 0, DPCD_ADJUST_REQUEST_LANE0_1_ADDR, 1);
	*ptr_vswing = req_value0_1 & 0x3;
	*ptr_preemp = (req_value0_1 & 0xc) >> 2;

	/* Get the highest possible VSwing */
	if(*ptr_vswing <= (req_value0_1 & 0x30) >> 4) {
		*ptr_vswing = (req_value0_1 & 0x30) >> 4;
	}
	if(*ptr_vswing <= (req_value2_3 & 0x3)) {
		*ptr_vswing = (req_value2_3 & 0x3);
	}
	if(*ptr_vswing <= (req_value2_3 & 0x30) >> 4) {
		*ptr_vswing = (req_value2_3 & 0x30) >> 4;
	}

	/* Get the highest possible Pre-emphasis */
	if(*ptr_preemp <= (req_value0_1 & 0xc) >> 2) {
		*ptr_preemp = (req_value0_1 & 0xc) >> 2;
	}
	if(*ptr_preemp <= (req_value2_3 & 0xc) >> 2) {
		*ptr_preemp = (req_value2_3 & 0xc) >> 2;
	}
	if(*ptr_preemp <= (req_value2_3 & 0xc0) >> 6) {
		*ptr_preemp = (req_value2_3 & 0xc0) >> 6;
	}

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

static int dp_set_training_level(void *p_context, unsigned long vswing,
		unsigned long preemphasis, unsigned long max_vswing, unsigned long max_preemp)
{
	dp_device_context_t *p_ctx = (dp_device_context_t *)p_context;
	unsigned char lane_cnt = 0;
	unsigned long temp_reg;
	EMGD_TRACE_ENTER;
	lane_cnt = (unsigned char)(p_ctx->dpcd_max_count & 0x0F);

	/* 
	 * BIT1:0 vswing
	 * BIT2:  max vswing
	 * BIT4:3 preremp
	 * BIT5:  max preremp
	 */
	temp_reg = (vswing | max_vswing << 2 |
				preemphasis << 3 | max_preemp << 5);

	if(lane_cnt == 2) {
		temp_reg = ((temp_reg & 0xFF) | (temp_reg & 0xFF) << 8);
	} else if (lane_cnt == 4) {
		temp_reg = ((temp_reg & 0xFF) | (temp_reg & 0xFF) << 8 
			| (temp_reg & 0xFF) << 16 | (temp_reg & 0xFF) << 24);
	}

	dpcdWrite(p_ctx, 0, DPCD_TRAINING_LANE0_SET_ADDR, lane_cnt, temp_reg); 

	EMGD_TRACE_EXIT;
	return PD_SUCCESS;
}

static int dp_wait_vblank(dp_device_context_t *p_ctx, unsigned long flags)
{
	unsigned long pipe_ctrl_reg;
	unsigned long reg=0, i;

	EMGD_TRACE_ENTER;
	if(flags & PD_SET_MODE_PIPE_B) {
		pipe_ctrl_reg = PIPEB_CONF;
	} else {
		pipe_ctrl_reg = PIPEA_CONF;
	}

	/* If pipe is off then just return */
	dp_read_mmio_reg(p_ctx, pipe_ctrl_reg, &reg);
	if (!((1L << 31) & reg)) {

		return PD_ERR_UNSUCCESSFUL;
	}

	/*
	 * When VGA plane is on the normal wait for vblank won't work
	 * so just skip it.
	 */
	dp_read_mmio_reg(p_ctx, 0x71400, &reg);
	if (!(reg & 0x80000000)) {

		return PD_ERR_UNSUCCESSFUL;
	}

	/* Clear VBlank interrupt status on PIPE STATUS REGISTER (0x1c offset from PIPE CONF)*/
	dp_read_mmio_reg(p_ctx, pipe_ctrl_reg + 0x1c, &reg);
	reg |= BIT(1);
	dp_write_mmio_reg(p_ctx, pipe_ctrl_reg + 0x1c, reg);

	for (i = 0; i < 0x100000; i++) {

		dp_read_mmio_reg(p_ctx, pipe_ctrl_reg + 0x1c, &reg);

		if ((reg & BIT(1)) != 0) {

			EMGD_TRACE_EXIT;
			return PD_SUCCESS;
		}
	}

	EMGD_ERROR("Timeout waiting for VBLANK");
	return PD_ERR_UNSUCCESSFUL;
}

static void edp_panel_protect(dp_device_context_t *p_ctx, int protect)
{
	unsigned long reg = 0 ;
	unsigned long pipe_reg_offset = 0x0;

	EMGD_TRACE_ENTER;
	
	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}

	dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);

	if(FALSE == protect) {
		EMGD_DEBUG ("Turn off write protect.");
		reg = 0xABCD0000 | (reg & 0x0000FFFF);
	} else if(TRUE == protect) {
		EMGD_DEBUG ("Turn on write protect.");
		reg = reg & 0x0000FFFF;
	}

	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 4, reg);
	EMGD_TRACE_EXIT;
}

/*
 * eDP panel Vdd enable: [DevCDV]: Enabling this bit enables the panel 
 * vdd if the embedded panel is DisplayPort, as indicated in bits 31:30 
 * of the panel power on sequencing.  Software must enable this bit for 
 * eDP link training.  After eDP link training is done, software must 
 * disable it and let the normal panel power sequencing to take control.
 * 0 = eDP panel Vdd disabled
 * 1 = eDP panel Vdd enabled
 *
 */
static void edp_enable_vdd(dp_device_context_t *p_ctx, int vdd)
{
	unsigned long reg = 0;
	unsigned long pipe_reg_offset = 0x0;

	EMGD_TRACE_ENTER;

	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}
	
	dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);

	EMGD_DEBUG (" Original reg = %lx ", reg);

	if(FALSE == vdd) {
		EMGD_DEBUG("  OFF VDD");
		reg &= ~BIT(3);
	} else if(TRUE == vdd) {
		EMGD_DEBUG("  ON VDD");
		reg |= BIT(3);
	}

	EMGD_DEBUG (" Want to write reg = %lx ", reg);
	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 4, reg);

	/* dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);
	EMGD_DEBUG (" Written reg = %lx ", reg); */

	EMGD_TRACE_EXIT;
}

static void edp_panel_backlight(dp_device_context_t *p_ctx, int backlight)
{
	unsigned long reg = 0 ;
	unsigned long pipe_reg_offset = 0x0;

	EMGD_TRACE_ENTER;

	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}
	
	dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);

	EMGD_DEBUG (" Original reg = %lx ", reg);

	if(FALSE == backlight) {
		EMGD_DEBUG("  OFF BACKLIGHT");
		reg &= ~BIT(2);
	} else if(TRUE == backlight) {
		EMGD_DEBUG("  ON BACKLIGHT");
		reg |= BIT(2);
	}

	EMGD_DEBUG (" Want to write reg = %lx ", reg);
	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 4, reg);

	/* dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg); 
	EMGD_DEBUG (" Written reg = %lx ", reg); */

	EMGD_TRACE_EXIT;
}

static void edp_panel_power(dp_device_context_t *p_ctx, int power)
{
	unsigned long reg = 0 ;
	unsigned long pipe_reg_offset = 0x0;

	EMGD_TRACE_ENTER;	

	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}

	dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);

	EMGD_DEBUG (" Original reg = %lx ", reg);

	/* 
	 * BIT0: Power State Target
	 * BIT1: Power Down on Reset
	 * BIT2: Backlight Enable
	 * BIT3: eDP Panel VDD Enable
	 */
	if(TRUE == power) {
		reg |= BIT(0);
	} else if(FALSE == power) {
		reg &= ~BIT(0);
	}

	EMGD_DEBUG (" Want to write reg = %lx ", reg);
	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 4, reg);

	/* dp_read_mmio_reg(p_ctx, pipe_reg_offset + 4, &reg);
	EMGD_DEBUG (" Written reg = %lx ", reg); */

	if(TRUE == power) {
		int i = 0;
		/* UMG: EDP T1 T2 Stabling Time - 60ms */
		pd_usleep(60000);

		/*FIXME: why 100*/
		do {
			dp_read_mmio_reg(p_ctx, pipe_reg_offset, &reg);
			EMGD_DEBUG (" Status reg = %lx ", reg); 
			i++;
			if (i == 100){
				EMGD_ERROR("Time out Panel Power failed!");
			}
		}while((((reg & 0x30000000) == 0x1) || ((reg & 0x80000000)  == 0x0)) && (i<100));

	} else if(FALSE == power) {

		int i = 0;
		do {
			dp_read_mmio_reg(p_ctx, pipe_reg_offset, &reg);
			EMGD_DEBUG (" Status reg = %lx ", reg); 
			i++;
			if (i == 100){
				EMGD_ERROR("Time out Panel Power failed!");
			}
		}while((((reg & 0x30000000) != 0x0) || ((reg & 0x80000000)  == 0x1) || ((reg & 0x8000000) == 1)) && (i<100));
	}

	EMGD_TRACE_EXIT;
}

/*
 * (00)0-Reserved
 * (01)1-DisplayPort B
 * (10)2-DisplayPort C
 * (11)3-Reserved
 */
/*FIXME: eDP always tied to DisplayPortC on AlpineValley*/
static void edp_panel_port_select (dp_device_context_t *p_ctx, int select){
	
	unsigned long delay = 0;
	unsigned long pipe_reg_offset = 0x0;
	
	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
	}

	dp_read_mmio_reg(p_ctx, pipe_reg_offset + 0x8, &delay);
	EMGD_DEBUG ("Read Pipe_PP_ON_DELAYS 0x61208 reg = 0x%lX", delay);

	/* Panel Control Port Select 
	 * BIT 31:30 - 00 = Reserved
	 *           - 01 = eDP B
	 *           - 10 = eDP C
	 */
	/*FIXME: eDP always tied to DisplayPortC on AlpineValley*/
	switch (select){
	case 1:
		delay &= ~BIT(31);
		delay |= BIT(30); 	
		EMGD_DEBUG (" eDP B is selected with bit = 01 ");	
		break;
	case 2:
		delay |= BIT(31);
		delay &= ~(BIT(30)); 
		EMGD_DEBUG (" eDP C is selected with bit = 10 ");
		break;	
	default:
		delay &= ~(BIT(31));
		delay &= ~(BIT(30)); 
		EMGD_DEBUG (" NO eDP is selected with bit = 00 ");	
	}	
	EMGD_DEBUG ("Will write to Pipe_PP_ON_DELAYS 0x61208 reg = 0x%lX", delay);
	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 0x8, delay);
}

static int edp_panel_pwm (dp_device_context_t *p_ctx){

	pd_attr_t      *tattr      = NULL;  /* holds time delay b/ pwr transition */
	mmio_reg_t	ctrl_reg = 0;
	unsigned long delay = 0;
	unsigned long	preserve;
	unsigned int i = 0;
	unsigned long pipe_reg_offset;
	/* SETUP Panel Power Registers here as set power is called with D3 state first*/
	

	EMGD_DEBUG ("Setting power state for eDP panel.");

	if (!(p_ctx->pipe & PD_SET_MODE_PIPE_B)){
		EMGD_DEBUG ("PIPE A");
		pipe_reg_offset = 0x61200;
		if (p_ctx->pwm_status[0] == TRUE){
			return PD_SUCCESS;
		}
		p_ctx->pwm_status[0] = TRUE;
	} else {
		EMGD_DEBUG ("PIPE B");
		pipe_reg_offset = 0x61300;
		if (p_ctx->pwm_status[1] == TRUE){
			return PD_SUCCESS;
		}
		p_ctx->pwm_status[1] = TRUE;
	}

	EMGD_DEBUG("Num of attributes exist is: %ld", p_ctx->num_attrs);			

	/* eDP power sequence table */
	/* Program panel power up/down delays:  */
	for ( i = 0; i < 2; i++)
	{	
		/* If i = 0, IEGD T1 = T3 for eDP (Power Up Delay) 
		 * If i = 1, IEGD T4 = T10 for eDP (Power Down Delay) 
		 */

		tattr = pd_get_attr(p_ctx->p_attr_table,
		p_ctx->num_attrs, table_set_power[i].id1, 0); /*Ta = T1 or T4 @[28:16] */
	
		if (tattr == NULL )
		{
			EMGD_ERROR ("@A Attribute not found! Loop i = %d", i);
			return (PD_ERR_INVALID_POWER);
		}
		delay =((tattr->current_value * 10) << 16);

		/* If i = 0, IEGD T2 = T7 for eDP (Power On to Backlight Enable Delay) 
		 * If i = 1, IEGD T3 = T9 for eDP (Backlight Off to Power Down Delay) 
		 */

		tattr = pd_get_attr(p_ctx->p_attr_table,
				p_ctx->num_attrs, table_set_power[i].id2, 0); /*Tb= T2 or T3 @[12:0]*/

		if (tattr == NULL )
		{
			EMGD_ERROR ("@B Attribute not found!");
			return (PD_ERR_INVALID_POWER);
		}
		delay |= tattr->current_value * 10;

		/*preserve non changing/reserved bits.*/
		dp_read_mmio_reg( p_ctx, pipe_reg_offset + table_set_power[i].reg_offset, &ctrl_reg );

		EMGD_DEBUG ("eDP Power register read ctrl_reg = %lu",ctrl_reg );				

		preserve = 0xE000E000;		/*0xE==1110 */
		ctrl_reg &= preserve;
		delay |= ctrl_reg;

		EMGD_DEBUG ("eDP Power register write delay = %lu",delay );				

		dp_write_mmio_reg(p_ctx, pipe_reg_offset + table_set_power[i].reg_offset, delay);
	}

	/* IEGD T5 = T12 for eDP (Power Cycle Delay) 
	 * BIT4:0 - Power Cycle Delay (1 ms unit. Must "+1"). T12 + 1
	 * BIT31:8 - Reference divider: The value should be (100*Ref-in-MHz/2)-1.  
	 * The default value assumes the default value for the display core clock that is for a 200MHz reference value.
	 */
	tattr = pd_get_attr(p_ctx->p_attr_table,
				p_ctx->num_attrs, PD_ATTR_ID_FP_PWR_T5, 0);

	if (tattr == NULL )
	{
		EMGD_ERROR ("@C Attribute not found!");
		return (PD_ERR_INVALID_POWER);
	}
	delay = tattr->current_value;
	/*power cycle |   Reference Divider                          */ 
	delay = (delay/100+1) | ((((unsigned long)p_ctx->core_freq*100/2)-1)<<8);
	/*preserve non changing/reserved bits.*/
	dp_read_mmio_reg( p_ctx, pipe_reg_offset + 0x10, &ctrl_reg );
	EMGD_DEBUG ("eDP: Read 0x61210 = %lX", ctrl_reg);
	preserve = 0x000000e0;
	ctrl_reg &= preserve;
	delay |= ctrl_reg;
	/* delay = 0x138706; //from vbios
	 * delay = 0x270F04; */

	/*This should be based on eDP T12*/

	/* 31:8 -is based on Display Core Freq:
	 * 			233MHz 2D81h
	 * 			200MHz 270Fh
	 * 			133Mhz 19F9 
	 *
	 * 4:0 - is based on T12
	 */
	
	EMGD_DEBUG ("eDP: Write 0x61210 = %lX", delay);
	dp_write_mmio_reg(p_ctx, pipe_reg_offset + 0x10, delay);



	/* PWM is a method of controlling the backlight intensity.
	 * It is not method to turn on baclkight.
	 * We still need the PD method to turn on the backlight.
	 *
	 * This feature is for Pouslbo Only. We check that the user has set the
	 * inverter frequency. Default intensity, if not set, is 100%
	 *
	 * Due to the high amount of calculation, we want to only set this register
	 * if it has not been ser previously. The register could be
	 * "brought forward" from VBIOS.
	 */

	/* Inverter Frequency for PWM Backlight. */
 	tattr = pd_get_attr(p_ctx->p_attr_table,
                 p_ctx->num_attrs, PD_ATTR_ID_INVERTER_FREQ, 0);
	if (tattr == NULL ) {
		EMGD_ERROR ("@A Attribute not found! Loop i = %d", i);
		return (PD_ERR_NULL_PTR);
	}
	/* EMGD_DEBUG ("Inverter Freq Attr = %lu", tattr->current_value); */
	if ((tattr->current_value >= INVERTER_FREQ_MIN) 
			&& (tattr->current_value <= INVERTER_FREQ_MAX)) {
		p_ctx->inverter_freq = tattr->current_value; 
		EMGD_DEBUG ("Current Inverter Freq =%ld", p_ctx->inverter_freq);
	} else {
		p_ctx->inverter_freq = tattr->default_value; 
		EMGD_DEBUG ("Default Inverter Freq =%ld", p_ctx->inverter_freq);
	}
	if(p_ctx->inverter_freq != 0xFFFF) {
		unsigned long reg_value = 0;
		unsigned long percentage = 0;
		unsigned long calculation = 0;
		unsigned long blc_pwm_ctl2 = 0;
		unsigned long temp = 0;
		EMGD_DEBUG ("PWM register programming");	
		/* We first need to get the graphics frequency, which will be used to
 		 * calculate Backlight Modulation Frequency[BMF]. BMF will be used to
   		 * fill up the 15 MSB in the 0x61254 register
   		 *
   		 * The calculation for the Modulation Frequency field in the
   		 * BLC_PWM_CTL Register is:
         *
       	 *     Reference Clock Freq               1
       	 *     -----------------------   x    ------------------
      	 *            Divider                   PWM Freq in Hz
	     *
         * Note: GEN3.5/4 Reference Clock is render core clock, divider is 32
      	 *       GEN4.5   Reference Clock is HRAW Clock, divider is 32
	     *       GEN5     Reference Clock is HRAW Clock, divider is 128
  		 */
		/* Some system bios cannot take 64 bit data type. Using a more
	     * simplified calculation that is not too accurate if the inputs
	     * are not round numbers */
		 /* Basic Math reminder: PERIOD = 1 / FREQUENCY */
		calculation = p_ctx->inverter_freq * 0x80; /* 128 for Gen5 */
		calculation = (p_ctx->core_freq * PWM_FREQ_CALC_CONSTANT_2) /
						calculation;

   		/* FIXME: blm_legacy_mode has to be added to a CED, for now it's hardcoded to 0 due to the MT System */
		/* blc_pwm_ctl2 = (1L << 31) | (p_ctx->blm_legacy_mode << 30) | (1L << 29); */
		/* blc_pwm_ctl2 = 0x803D4110 | BIT(26) | BIT (25) | BIT(24); */ /*Hardcoded for VLV*/
		blc_pwm_ctl2 = 0x80000000; /* FIXME: Why? */
		EMGD_DEBUG("PWM 0x61250 blc_pwm_ctl2 = %lX ", blc_pwm_ctl2); 
   		/* bit 15:8 need to be written twice. */
		dp_write_mmio_reg(p_ctx, pipe_reg_offset + 0x50, (blc_pwm_ctl2 & (~0x0000FF00)));
		dp_read_mmio_reg(p_ctx, pipe_reg_offset + 0x50, &temp);
		dp_write_mmio_reg(p_ctx, pipe_reg_offset + 0x50, blc_pwm_ctl2);
       

	  	reg_value = (calculation & 0xFFFF)<<16;
  		/* If calculation exceed the bit-field size limitations, write as 0xFFFF */
	   	if( BLC_MAX_PWM_CTL_REG_FREQ_VALUE < calculation )
   		{
			EMGD_DEBUG ("PWM Calculation set to 0xFFFF or MAX PWM FREQ.");
      		calculation = BLC_MAX_PWM_CTL_REG_FREQ_VALUE;
	   	}

	   	percentage = ( p_ctx->pwm_intensity * (unsigned long)calculation);
   		reg_value |= (unsigned long)( percentage / (int)100 );
		/* reg_value = 0x3D091325; */ /*Hardcoded for VLV*/
		/* reg_value = 0x0f420f42; */

		/* 15:0 - Intensity, percentage of high.
		 * 31:16 - Frequency. */ 
		EMGD_DEBUG("PWM 0x61254 reg_value = %lX ", reg_value); 
	   	dp_write_mmio_reg( p_ctx, pipe_reg_offset + 0x54,  reg_value );

	   	/* set the flag so that we only do this function once */
		EMGD_DEBUG ("PWM register programming done.");
	}

	return PD_SUCCESS;
}

/*	============================================================================
	Function	:	dp_map_audio_video_dip_regs is called to get the current
				pipe in use for a port.

	Parameters	:	p_context: Pointer to port driver allocated context
				structure
	Remarks 	:
	Returns 	:	int : Status of command execution
	------------------------------------------------------------------------- */
int dp_map_audio_video_dip_regs(dp_device_context_t *p_ctx)
{
	int current_pipe = p_ctx->p_callback->current_pipe;

		EMGD_DEBUG("Current pipe: %d", current_pipe);

		/* It is either PIPE_A or PIPE_B for VLV */
		if (current_pipe == PIPE_A) {
			p_ctx->dp_audio_port_regs = &g_vlv_dp_audio_video_dip_regs_a;
		} else if (current_pipe == PIPE_B) {
			p_ctx->dp_audio_port_regs = &g_vlv_dp_audio_video_dip_regs_b;
		} else {
			EMGD_ERROR("Unsupported pipe number!");
			return PD_ERR_UNSUCCESSFUL;
		}

	return PD_SUCCESS;
}
