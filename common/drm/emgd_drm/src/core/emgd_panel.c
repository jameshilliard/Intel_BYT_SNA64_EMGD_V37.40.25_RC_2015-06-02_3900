/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd_panel.c
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
 *  This is to support backlight driver for eDP, LVDS.
 *-----------------------------------------------------------------------------
 */

#include <linux/version.h>
#include <drm/drm.h>
#include <drmP.h>
#include <drm_crtc_helper.h>
#include <linux/backlight.h>

#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "i915/intel_drv.h"

#include "drm_emgd_private.h"
#include "igd_interrupt.h"
#include "default_config.h"
#include "emgd_drv.h"
#include "emgd_drm.h"
#include "memory.h"
#include "io.h"
#include "igd_core_structs.h"

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
/*-----------------------------------------------------------------------------
 * Backlight support has 4 funtions in core layer:
 * 	-blc_get_brightness
 * 	-blc_get_max_brightness
 * 	-blc_set_brightness
 * 	-blc_support_check
 * 	-blc_init
 * 	-blc_exit
 *---------------------------------------------------------------------------*/

/*
 * Return current brightness set in pipe register 
 */
static int blc_get_brightness(struct backlight_device *bd, int pipe)
{
	struct drm_device *dev = bl_get_data(bd);
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;
	emgd_crtc_t *emgd_crtc;
	int brightness = 0;
	struct drm_crtc *crtc;

	EMGD_TRACE_ENTER;	

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		if (emgd_crtc->pipe == pipe){
			brightness = context->mode_dispatch->kms_get_brightness(emgd_crtc );
			if ((brightness != 0) && (brightness != -IGD_ERROR_INVAL)){
				if ((brightness >= MIN_BRIGHTNESS) && (brightness <=MAX_BRIGHTNESS)){
					EMGD_TRACE_EXIT;
    				return brightness;
				}
			}
		}
	}
    return 0;
}

/*
 * Return maximum brightness set in pipe register 
 */
static int  blc_get_max_brightness (struct drm_device *dev, int pipe){
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;
	int max = 0;
	emgd_crtc_t *emgd_crtc;
	struct drm_crtc *crtc;

	EMGD_TRACE_ENTER;	

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		if (emgd_crtc->pipe == pipe){
			max = context->mode_dispatch->kms_get_max_brightness(emgd_crtc );
			if ((max != 0) && (max != -IGD_ERROR_INVAL)){
				EMGD_TRACE_EXIT;
    			return max;
			}
		}
	}
	if (max == 0) {
		EMGD_DEBUG("DP Panel PWM value is 0!\n");
		/* i915 does this, I believe which means that we should not
		* smash PWM control as firmware will take control of it. */
		return 1;
	}
 
	return 1;


}

/*
 * Set brightness in pipe register 
 */
static int blc_set_brightness (struct backlight_device *bd, int pipe){

	struct drm_device *dev = bl_get_data(bd);
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;
	int level = bd->props.brightness;
	int ret;
	emgd_crtc_t *emgd_crtc;
	struct drm_crtc *crtc;
    
	EMGD_TRACE_ENTER;	

    /* Percentage 1-100% being valid */
	if (level < MIN_BRIGHTNESS){
		level = MIN_BRIGHTNESS;
		EMGD_ERROR ("Invalid brightness level.");
	}
	if (level > MAX_BRIGHTNESS){
		level = MAX_BRIGHTNESS;
		EMGD_ERROR ("Invalid brightness level.");
	}


	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		if (emgd_crtc->pipe == pipe){
			EMGD_DEBUG ("Setting brightness for Pipe = %d, level = %d", 
				pipe, level);
			ret = context->mode_dispatch->kms_set_brightness( emgd_crtc, level );
		}
	}
	EMGD_TRACE_EXIT;
	return 0;
}

/* Wrapper for pipes. blc_xxx_a for Pipe A. */
static int blc_get_brightness_a(struct backlight_device *bd){
	return blc_get_brightness(bd, 0);
}

static int blc_set_brightness_a (struct backlight_device *bd){
	return blc_set_brightness(bd, 0);

}

/* Wrapper for pipes. blc_xxx_b for Pipe B. */
static int blc_get_brightness_b(struct backlight_device *bd){
	return blc_get_brightness(bd, 1);
}

static int blc_set_brightness_b (struct backlight_device *bd){
	return blc_set_brightness(bd, 1);

}



 
/*
 * Return 1 for backlight supported display, 0 otherwise. 
 */
static int  blc_support_check (struct drm_device *dev, int pipe, unsigned long *port_number){
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;
	emgd_crtc_t *emgd_crtc;
	struct drm_crtc *crtc;
	int ret = 0; 


	EMGD_TRACE_ENTER;	
	
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		if (emgd_crtc->pipe == pipe){
			ret = context->mode_dispatch->kms_blc_support(emgd_crtc, port_number );
			if ((ret != 0) && (ret != -IGD_ERROR_INVAL)){
				EMGD_TRACE_EXIT;
				return ret;
			}
		}
	} 

	EMGD_TRACE_EXIT;
	return 0;
}

/* Backlight control drive operations. */
static const struct backlight_ops display_blc_ops[2] = {
	{
		.update_status = blc_set_brightness_a,
		.get_brightness = blc_get_brightness_a,
		/* .check_fb */
	},
	{
		.update_status = blc_set_brightness_b,
		.get_brightness = blc_get_brightness_b,
		/* .check_fb */
	},

};

#endif /* CONFIG_BACKLIGHT_CLASS_DEVICE */

/*
 * Initilize backlight driver.
 */
int blc_init(struct drm_device *dev){
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct backlight_properties props[2] ;
	int i = 0;
	char dis_name[13];
	unsigned long port_number = 0;
	
	EMGD_TRACE_ENTER;	

	for (i = 0; i < 2; i++){
	
		if (blc_support_check(dev, i, &port_number) && 
			(dev_priv->backlight[i] == NULL)){		
	
			memset (&(props[i]), 0, sizeof ( struct backlight_properties));
			props[i].max_brightness = MAX_BRIGHTNESS;
			props[i].type = BACKLIGHT_PLATFORM;

			if (port_number == 7){
				strcpy (dis_name, "emgd_drv_dpc" );
			} else {
				strcpy (dis_name, "emgd_drv_dpb" );
			}
			
			dev_priv->backlight[i] = backlight_device_register(dis_name,
				NULL, (void *) dev,  &display_blc_ops[i], &(props[i]));	

			if (dev_priv->backlight[i] == NULL){
				return -1;
			}

			dev_priv->backlight[i]->props.brightness =
				blc_get_brightness(dev_priv->backlight[i], i);

			EMGD_DEBUG ("Current brightness is %d out of 100", 
					dev_priv->backlight[i]->props.brightness);

			dev_priv->backlight[i]->props.max_brightness =
				blc_get_max_brightness(dev, i);

			backlight_update_status (dev_priv->backlight[i]);
	
		}
	}

	EMGD_TRACE_EXIT;
#endif /* CONFIG_BACKLIGHT_CLASS_DEVICE */
	return 0;
}


/*
 * Cleaning up when exit.
 */
void blc_exit (struct drm_device *dev){
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i = 0;

	EMGD_TRACE_ENTER;	

	for (i = 0; i < 2; i++){

		if (dev_priv->backlight[i]) {
			backlight_update_status(dev_priv->backlight[i]);
			backlight_device_unregister(dev_priv->backlight[i]);
		}

		dev_priv->backlight[i] = NULL;
	}

	EMGD_TRACE_EXIT;
#endif /* CONFIG_BACKLIGHT_CLASS_DEVICE */
}
/* End backlight support functions. */
