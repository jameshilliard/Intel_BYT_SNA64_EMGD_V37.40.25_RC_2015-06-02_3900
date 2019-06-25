/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_sysfs.c
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
 *  This implements a sysfs interface to the EMGD kernel module. The interface
 *  is used to control various unique aspects of the driver.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.sysfs

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <drm/i915_drm.h>

/* This should be in drm_emgd_private.h, not here */
#include "i915/i915_reg.h"

#include "drm_emgd_private.h"
#include "emgd_drv.h"
#include "emgd_drm.h"
#include "memory.h"
//#include <igd_core_structs.h>
#include <igd_ovl.h>
#include "splash_video.h"

#include <linux/version.h>
#include <linux/export.h>
#include <linux/module.h>


extern int emgd_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
		struct drm_framebuffer *fb, int x, int y, enum mode_set_atomic state);

unsigned int
get_zorder_control_values(unsigned long zorder_combined, int plane_id);


/*
 * The EMGD control kobject.  This object will hold the various control
 * and status attributes that the driver exposes.
 */
typedef struct emgd_obj {
	struct kobject kobj;
	drm_emgd_private_t *priv;
	union {
		int crtc_idx;
		emgd_framebuffer_t *fb;
		emgd_connector_t * connector;
		igd_ovl_plane_t * emgd_ovlplane;
		struct drm_i915_file_private * drmfilepriv;
	};
} emgd_obj_t;

#define to_emgd_obj(x) container_of(x, emgd_obj_t, kobj)


/*
 * Basic sysfs attribute definition.
 */
typedef struct emgd_attribute {
	struct attribute attr;
	ssize_t (*show)(emgd_obj_t *crtc, struct emgd_attribute *attr,
			char *buf);
	ssize_t (*store)(emgd_obj_t *crtc, struct emgd_attribute *attr,
			const char *buf, size_t count);
} emgd_attribute_t;

#define to_emgd_attribute(x) container_of(x, emgd_attribute_t, attr)

/*
 * Show function for emgd crtc attributes
 */
static ssize_t emgd_attr_show(struct kobject *obj,
		struct attribute *attr,
		char *buf)
{
	emgd_attribute_t *attribute;
	emgd_obj_t *emgd;

	attribute = to_emgd_attribute(attr);
	if(!attribute) {
		EMGD_ERROR("Attribute is invalid.");
		return -EIO;
	}

	emgd = to_emgd_obj(obj);
	if(!emgd) {
		EMGD_ERROR("EMGD object is invalid.");
		return -EIO;
	}

	/* make sure there's a show function assiciated with the attribute */
	if (!attribute->show) {
		return -EIO;
	}

	return attribute->show(emgd, attribute, buf);
}


/*
 * Store function for emgd crtc attributes
 */
static ssize_t emgd_attr_store(struct kobject *obj,
		struct attribute *attr,
		const char *buf,
		size_t count)
{
	emgd_attribute_t *attribute;
	emgd_obj_t *emgd;

	attribute = to_emgd_attribute(attr);
	if(!attribute) {
		EMGD_ERROR("Attribute is invalid.");
		return -EIO;
	}

	emgd = to_emgd_obj(obj);
	if(!emgd) {
		EMGD_ERROR("EMGD object is invalid.");
		return -EIO;
	}

	/* make sure there's a store function assiciated with the attribute */
	if (!attribute->store) {
		return -EIO;
	}

	return attribute->store(emgd, attribute, buf, count);
}


/*
 * The emgd sysfs_ops.
 *
 * These handle the read/write of emgd attributes.
 */
static const struct sysfs_ops emgd_sysfs_ops = {
	.show = emgd_attr_show,
	.store = emgd_attr_store,
};

/*
 * These are our driver's sysfs kobject wrappers.
 */
static emgd_obj_t *emgd_obj = NULL;
static emgd_obj_t *crtc_obj[4] = {NULL, NULL, NULL, NULL};
static emgd_obj_t *fbdir_obj = NULL;
static emgd_obj_t *info_obj = NULL;
static emgd_obj_t *freq_obj = NULL;


/*
 * The release function.  This is requred by the kernel and will free up
 * the resources used by our sysfs objects.
 */
static void emgd_obj_release(struct kobject *obj)
{
	emgd_obj_t *emgd = to_emgd_obj(obj);

	OS_FREE(emgd);
}

/*
 * The info function to show driver description name
 */
static ssize_t info_desc_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", DRIVER_DESC);
}

/*
 * The info function to show driver version
 */
static ssize_t info_version_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d.%d.%d\n", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCHLEVEL);
}

/*
 * The info function to show driver build date
 */
static ssize_t info_date_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", DRIVER_DATE);
}

/*
 * The info function to show chipset
 */
static ssize_t info_chipset_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	igd_init_info_t *info = emgd->priv->init_info;
	return scnprintf(buf, PAGE_SIZE, "%s\n", info->name);
}

/*
 *  The emgd cur freq show function is using to read current GPU frequency.
 *  Example: 
 *  	cat /sys/module/emgd/control/freq/i915_cur_freq
 */
static ssize_t emgd_cur_freq_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
			char *buf)
{
        drm_i915_private_t *dev_priv;
        struct drm_device *dev;
        int ret=0;
        int val=0;
	u32 pval;
        dev_priv = (drm_i915_private_t *)emgd->priv;
        dev = dev_priv->dev;
        if(!(IS_GEN6(dev) || IS_GEN7(dev)))
                return -ENODEV;

        ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
        if(ret)
                return ret;

	valleyview_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS, &pval);
        mutex_unlock(&dev_priv->rps.hw_lock);

	dev_priv->rps.cur_delay = pval >> 8;

        if(IS_VALLEYVIEW(dev))
                val = vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.cur_delay);
        else
                val = dev_priv->rps.cur_delay * GT_FREQUENCY_MULTIPLIER;

        return scnprintf(buf, PAGE_SIZE, "Current GPU freq: %d MHz\n", val);
}

/*
 *  The emgd min freq show function is using to read min GPU frequency.
 *  Example: 
 *  	cat /sys/module/emgd/control/freq/i915_min_freq
 */  
static ssize_t emgd_min_freq_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
                        char *buf)
{
        drm_i915_private_t *dev_priv;
        struct drm_device *dev;
        int ret=0;
        int val=0;
        dev_priv = (drm_i915_private_t *)emgd->priv;
        dev = dev_priv->dev;
        if(!(IS_GEN6(dev) || IS_GEN7(dev)))
                return -ENODEV;

        ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
        if(ret)
                return ret;
        if(IS_VALLEYVIEW(dev))
                val = vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.min_delay);
        else
                val = dev_priv->rps.min_delay * GT_FREQUENCY_MULTIPLIER;
        mutex_unlock(&dev_priv->rps.hw_lock);

        return scnprintf(buf, PAGE_SIZE, "Min GPU freq: %d MHz\n", val);
}


/*
 *  The emgd max freq show function is using to read max GPU frequency.
 *  Example: 
 *  	cat /sys/module/emgd/control/freq/i915_max_freq
 */
static ssize_t emgd_max_freq_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
			char *buf) 
{
        drm_i915_private_t *dev_priv;
        struct drm_device *dev;
	int ret=0;
	int val=0;
	dev_priv = (drm_i915_private_t *)emgd->priv;
	dev = dev_priv->dev;
	if(!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;
	
	ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
	if(ret)
		return ret;
	if(IS_VALLEYVIEW(dev))
		val = vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.max_delay);
	else
		val = dev_priv->rps.max_delay * GT_FREQUENCY_MULTIPLIER;
	mutex_unlock(&dev_priv->rps.hw_lock);
	
	return scnprintf(buf, PAGE_SIZE, "Max GPU freq: %d MHz\n", val);
}

/*
 *  The emgd max freq store function is using to set max GPU frequency.
 *  Example: 
 *  	echo "545" >  /sys/module/emgd/control/freq/i915_max_freq
 */
static ssize_t emgd_max_freq_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
			const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv;
	struct drm_device *dev;
	int ret=0;
	unsigned int value;
	
	dev_priv = (drm_i915_private_t *)emgd->priv;
	dev = dev_priv->dev;

	ret = kstrtou32(buf, 0, &value);
	if(ret)
		return ret;

	if(!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;

	ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
	if(ret)
		return ret;

	/*
 	 * Turbo will still be enabled, but won't go above the set value.
 	 */
	if(IS_VALLEYVIEW(dev)) {
		value = vlv_freq_opcode(dev_priv->mem_freq, value);
		dev_priv->rps.max_delay = value;
		dev_priv->rps.max_lock = 1;
		valleyview_set_rps(dev, value);
	} else {
		do_div(value, GT_FREQUENCY_MULTIPLIER);
		dev_priv->rps.max_delay = value;
		gen6_set_rps(dev, value);
	}

	mutex_unlock(&dev_priv->rps.hw_lock);

	return count;
}

#ifdef SPLASH_VIDEO_SUPPORT
/*
 * The splashvideo plane hold attribute store function
 *
 * Holds or releases the splash video plane.
 * Sysfs for splash video plane hold is NOT per CRTC
 *
 * Examples:
 *    echo "1" > /sys/module/emgd/control/crtc0/splash_video_plane_hold
 *    echo "0" > /sys/module/emgd/control/crtc0/splash_video_plane_hold
 *    cat /sys/module/emgd/control/crtc0/splash_video_plane_hold
 */
static ssize_t splashvideo_plane_hold_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv;
	int new_state;

        sscanf(buf, "%du", &new_state);

	dev_priv = (drm_i915_private_t *)emgd->priv;
	if (new_state != dev_priv->spvideo_holdplane) {
                if(new_state == 1){
                        dev_priv->spvideo_holdplane = 1;
                }else{
                        dev_priv->spvideo_holdplane = 0;
                }
        }

	return count;
}

/*
 * The splashvideo plane hold attribute show function
 *
 * Return the current splash video plane hold state to the caller.
 * Sysfs for splash video is NOT per CRTC.
 * State '0' means splash video plane hold is off
 * State '1' means splash video plane hold is on
 */
static ssize_t splashvideo_plane_hold_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	drm_i915_private_t *dev_priv;

	dev_priv = (drm_i915_private_t *)emgd->priv;
	return scnprintf(buf, PAGE_SIZE, "%d\n", dev_priv->spvideo_holdplane);
}
#endif

/*
 * The splashvideo attribute show function
 *
 * Return the current splash video state to the caller.
 * Sysfs for splash video is per CRTC.
 * State '0' means splash video is off
 * State '1' means splash video is on
 */
static ssize_t splashvideo_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				emgd->priv->spvideo_test_enabled);
	} else {
		EMGD_ERROR("Splash video query for invalid CRTC");
		return -EIO;
	}
}


/*
 * The splashvideo attribute store function
 *
 * Enables or disables the splash video that is configured in default_config.c
 * splashvideo binary to turn on and turn off the splash video
 *
 * Examples:
 *    echo "1" > /sys/module/emgd/control/crtc0/splash_video_enable
 *    echo "0" > /sys/module/emgd/control/crtc0/splash_video_enable
 *    cat /sys/module/emgd/control/crtc0/splash_video_enable
 */
static ssize_t splashvideo_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	int new_state;
	drm_i915_private_t *dev_priv;

	sscanf(buf, "%du", &new_state);

	dev_priv = (drm_i915_private_t *)emgd->priv;

	if (new_state != emgd->priv->spvideo_test_enabled) {
		if(new_state == 1){
			display_splash_video(dev_priv);
		} else{
			destroy_splash_video(dev_priv);
		}
	}

	return count;
}
static ssize_t emgd_ovlplane_fbblendovl_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	drm_i915_private_t *dev_priv;
	emgd_crtc_t *emgd_crtc;
	int ret = 0;

	EMGD_TRACE_ENTER;

	dev_priv = (drm_i915_private_t *)emgd->priv;

	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {

		emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]; /* ->freeze_crtc;*/
		ret = scnprintf(buf, PAGE_SIZE, "%u\n", emgd_crtc->fb_blend_ovl_on);
		EMGD_TRACE_EXIT;
		return ret;

	} else {
		EMGD_ERROR("fbblend show for invalid crtc.");
		EMGD_TRACE_EXIT;
		return -EIO;
	}
}
static ssize_t emgd_ovlplane_fbblendovl_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	unsigned long value;
	drm_i915_private_t *dev_priv;
	emgd_crtc_t *emgd_crtc;
	igd_context_t *context = NULL;
	u32 temp1, temp2, i;
	bool sprite_is_on = false, value_changed = false;


	EMGD_TRACE_ENTER;

	sscanf(buf, "%lu", &value);
	EMGD_DEBUG("value (from user) = 0x%lx", value);

	if ((value != 0) && (value != 1))
	{
		EMGD_ERROR("Invalid value for fb_blend_ovl!");
		EMGD_TRACE_EXIT;
		return -EIO;
	}

	dev_priv = (drm_i915_private_t *)emgd->priv;

	context = dev_priv->context;

	if ((emgd->crtc_idx >= 0) && (emgd->crtc_idx < emgd->priv->num_crtc)) 	{
		emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]; /*->freeze_crtc; */
		mutex_lock(&(dev_priv->dev->struct_mutex));
		if(emgd_crtc->fb_blend_ovl_on != value) {
			value_changed = true;
		}
		emgd_crtc->fb_blend_ovl_on = value;

		/* Lets see if we need to do an immediate register touch or not */
		if (emgd_crtc->igd_plane && value_changed) {

			/* first lets find out if a sprite is enabled */
			for (i = 0; i <dev_priv->num_ovlplanes; ++i) {
				if ( (dev_priv->ovl_planes[i].target_igd_pipe_ptr == emgd_crtc->igd_pipe) &&
					(dev_priv->ovl_planes[i].enabled) ) {
					sprite_is_on = true;
					break;
				}
			}
			if(sprite_is_on) {
				/* if value == 1, but sprite is off, then we only store it
				 * but dont actually trigger the display plane alpha enable,
				 * but if sprite is on, then lets just do it now */
	
				temp1 = I915_READ(emgd_crtc->igd_plane->plane_reg);
				temp2 = ((temp1 & 0x3C000000) >> 26);
				if(temp2 > 5)  {
					/* ((>> 16) > 5) are all the various pixel formats 
					 * that are more than 16bpp and have alpha versions*/
					if(emgd_crtc->fb_blend_ovl_on) {
						if(temp2==6) temp2 = 7;
						else if (temp2==8) temp2 = 9;
							else if (temp2==10) temp2 = 11;
						else if (temp2==12) temp2 = 13;
						else if (temp2==14) temp2 = 15;
					} else {
						if(temp2==7) temp2 = 6;
							else if (temp2==9) temp2 = 8;
						else if (temp2==11) temp2 = 10;
						else if (temp2==13) temp2 = 12;
						else if (temp2==15) temp2 = 14;
					}
					temp1 = ((temp1 & ~0x3C000000) | (temp2 << 26));
					I915_WRITE(emgd_crtc->igd_plane->plane_reg, temp1);
					I915_WRITE((emgd_crtc->igd_plane->plane_reg-4),
						I915_READ((emgd_crtc->igd_plane->plane_reg-4)));
				}
			}
			emgd_crtc_update_properties(&emgd_crtc->base,
					EMGD_CRTC_UPDATE_FB_BLEND_OVL_PROPERTY);
		}
		mutex_unlock(&(dev_priv->dev->struct_mutex));
	} else {
		EMGD_ERROR("fbblend store for invalid crtc.");
		return -EIO;
	}

	EMGD_TRACE_EXIT;

	return count;
}
static ssize_t  emgd_ovlplane_zorder_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	drm_i915_private_t *dev_priv;
	emgd_crtc_t *emgd_crtc;
	int ret = 0;

	EMGD_TRACE_ENTER;

	dev_priv = (drm_i915_private_t *)emgd->priv;

	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {

		emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]; /*->freeze_crtc;*/

		ret = scnprintf(buf, PAGE_SIZE, "0x%08lx\n", emgd_crtc->zorder);
		EMGD_TRACE_EXIT;
		return ret;
	} else {
		EMGD_TRACE_EXIT;
		EMGD_ERROR("zorder show for invalid crtc.");
		return -EIO;
	}


}
static ssize_t emgd_ovlplane_zorder_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	unsigned long value;
	drm_i915_private_t *dev_priv;
	emgd_crtc_t *emgd_crtc;

	EMGD_TRACE_ENTER;

	sscanf(buf, "%lx", &value);
	EMGD_DEBUG("value (from user) = 0x%lx", value);
	if ((value != 0x00010204) && (value != 0x00010402) &&
		(value != 0x00040102) && (value != 0x00040201) &&
		(value != 0x00020104) && (value != 0x00020401) )
	{
		EMGD_ERROR("Invalid value for ovl_zorder!");
		EMGD_TRACE_EXIT;
		return -EIO;
	}

	dev_priv = (drm_i915_private_t *)emgd->priv;

	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {

		emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]; /* ->freeze_crtc; */
		mutex_lock(&(dev_priv->dev->struct_mutex));
		emgd_crtc->zorder = value;
		emgd_crtc->sprite1->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite1->plane_id);
		emgd_crtc->sprite2->zorder_control_bits =
				get_zorder_control_values(emgd_crtc->zorder, emgd_crtc->sprite2->plane_id);

		emgd_crtc_update_properties(&emgd_crtc->base,
				EMGD_CRTC_UPDATE_ZORDER_PROPERTY);

		mutex_unlock(&(dev_priv->dev->struct_mutex));
	} else {
		EMGD_ERROR("zorder store for invalid crtc.");
		return -EIO;
	}

	EMGD_TRACE_EXIT;

	return count;

}

/*
 * The freeze attribute show function
 *
 * Return the current freeze framebuffer state to the caller. The
 * freeze state is a bitmask with each bit representing the state of
 * a crtc. For current hardware this means the state will likely only
 * be one of 0, 1, 2, or 3
 *
 * Should we display this in binary?
 */
static ssize_t freeze_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				emgd->priv->crtcs[emgd->crtc_idx]->freeze_state);
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}
}

/*
 * Lock the plane.
 *
 * This can be called from within the kernel module to lock the plane
 * on init. (useful for making sure the splash screen is not corrupted
 * during boot.
 */

ssize_t emgd_lock_plane(struct drm_device *dev, emgd_crtc_t *emgd_crtc)
{
	struct drm_framebuffer *fb = NULL;
	emgd_framebuffer_t fb_info;
	emgd_framebuffer_t *emgd_fb;
	igd_context_t *context;
	struct drm_i915_gem_object *bo;
	unsigned long size;
	unsigned char *static_fb, *current_fb;
	int ret;
	int align;

	context = ((drm_emgd_private_t *)dev->dev_private)->context;
	
	if (!emgd_crtc) {
		EMGD_ERROR("CRTC data structure invalid (crtc=%p).", emgd_crtc);
		return -EIO;
	}

	fb = emgd_crtc->base.fb;
	if (!fb) {
		EMGD_ERROR("CRTC data structure invalid (fb=%p).", fb);
		return -EIO;
	}

	emgd_fb = container_of(fb, emgd_framebuffer_t, base);
	if (!emgd_fb) {
		EMGD_ERROR("CRTC data structures invalid (emgdfb=%p).", emgd_fb);
		return -EIO;
	}

	mutex_lock(&dev->struct_mutex);

	/* Save current crtc / fb */
	emgd_crtc->freeze_crtc = emgd_crtc;
	emgd_crtc->freeze_fb = emgd_fb;
	emgd_crtc->freeze_pitch = fb->DRMFB_PITCH;

	size = ALIGN(emgd_crtc->freeze_pitch * fb->height, PAGE_SIZE);

	bo = i915_gem_alloc_object(dev, size);
	if (bo == NULL) {
		/* Failed to create bo */
		EMGD_ERROR("Failed to allocate temporary buffer object.");
		mutex_unlock(&dev->struct_mutex);
		return -EIO;
	}

	/* Pin to display */
	align = (bo->tiling_mode == I915_TILING_NONE) ? 4096 : 0;
	ret = i915_gem_object_pin_to_display_plane(bo, align, NULL);
	if (ret != 0) {
		/* Failed, don't lock plane */
		EMGD_ERROR("Failed to pin temporary buffer.");
		i915_gem_free_object(&bo->base);
		mutex_unlock(&dev->struct_mutex);
		return -EIO;
	}

	/* wait for rendering to finish on active fb */
	drm_gem_object_reference(&emgd_fb->bo->base);
	ret = i915_gem_object_finish_gpu(emgd_fb->bo);

	/*
	 * If the source framebuffer is tiled it needs to
	 * be fenced before copying??
	 *
	 * When vmapped, a tiled buffer is getting
	 * read out as tiled so the memcpy below makes a
	 * "tiled" copy.  Even though the temporary buffer
	 * flags don't indiate it's tiled, the contents really
	 * are. The freezed_tiled flag is being used to track
	 * this state.
	 *
	 * It would be more correct if the vmapped address
	 * provided the source data in a linear mode even
	 * when the buffer is tiled.
	 */
	if (emgd_fb->bo->tiling_mode != I915_TILING_NONE) {
		/*
		 * In theory, getting a fence for the buffer will
		 * allow us to read the buffer as if it was not
		 * tiled.  But this didn't seem to work.
		 *
		 * ret = i915_gem_object_get_fence(emgd_fb->bo);
		 */
		emgd_crtc->freezed_tiled = 1;
	} else {
		emgd_crtc->freezed_tiled = 0;
	}

	/* map both buffer objects and copy from current to temp */
	    current_fb =
			ioremap_wc(((drm_emgd_private_t *)dev->dev_private)->gtt.mappable_base + i915_gem_obj_ggtt_offset(emgd_fb->bo),
					size);

	if (!current_fb) {
		EMGD_ERROR("Failed to map current framebuffer.");
		i915_gem_object_unpin(bo);
		i915_gem_free_object(&bo->base);
		mutex_unlock(&dev->struct_mutex);
		return -EIO;
	}

	    static_fb =
			ioremap_wc(((drm_emgd_private_t *)dev->dev_private)->gtt.mappable_base + i915_gem_obj_ggtt_offset(bo),
							                size);

	if (!static_fb) {
		EMGD_ERROR("Failed to map temporary framebuffer.");
		iounmap(current_fb);
		i915_gem_object_unpin(bo);
		i915_gem_free_object(&bo->base);
		mutex_unlock(&dev->struct_mutex);
		return -EIO;
	}

	memcpy(static_fb, current_fb, size);
	iounmap(current_fb);
	iounmap(static_fb);
	drm_gem_object_unreference(&emgd_fb->bo->base);

	emgd_crtc->freeze_state = 1;
	emgd_crtc->freeze_bo = bo;
	emgd_crtc->freezed_pitch = fb->DRMFB_PITCH;
	emgd_crtc->unfreeze_xoffset = emgd_crtc->igd_plane->xoffset;
	emgd_crtc->unfreeze_yoffset = emgd_crtc->igd_plane->yoffset;

	/* Flip the plane to the static copy */
	/* For kernel >= 3.7, ioremap_wc is used to map the buffer through
	 * the Gfx Aperture. For tiled and fenced buffer, it will be read out
	 * as linear by memcpy because it was mapped through Gfx Aperture.
	 * This is different than vmap method used for kernel prior to 3.7,
	 * where the bo->pages are directly mapped with vmap.
	 * So in the case for ioremap_wc, the destination buffer will be
	 * linear and there is no tiling */
	bo->tiling_mode = I915_TILING_NONE;
	emgd_crtc->freezed_tiled = 0;
	fb_info.bo = bo;
	fb_info.base.DRMFB_PITCH  = fb->DRMFB_PITCH;
	fb_info.base.width  = fb->width;
	fb_info.base.height = fb->height;
	fb_info.igd_flags = emgd_fb->igd_flags;
	fb_info.igd_pixel_format = IGD_PF_ARGB32;

	context->mode_dispatch->kms_set_display_base(emgd_crtc, &fb_info);
	bo->tiling_mode = 0;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}


/*
 * The freeze attribute store function
 *
 * Set the panel lock state for a CRTC. The value used to represent the
 * state is binary (locked/unlocked).
 *
 * Examples:
 *    echo "1" > /sys/modules/emgd/control/crtc0/lock_panel
 *    echo "0" > /sys/modules/emgd/control/crtc0/lock_panel
 *    cat /sys/modules/emgd/control/crtc0/lock_panel
 */
static ssize_t freeze_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	int new_state;
	struct drm_device *dev;
	emgd_framebuffer_t *emgd_fb;
	emgd_crtc_t *emgd_crtc;
	igd_context_t *context;
	struct drm_i915_gem_object *bo;
	int ret;

	dev = emgd->priv->dev;
	context = emgd->priv->context;

	sscanf(buf, "%du", &new_state);

	if ((new_state != 0) && (new_state != 1)) {
		EMGD_ERROR("Invalid value for incoming state!");
		return -EIO;
	}

	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		/* Was the state really changed */
		if (new_state != emgd->priv->crtcs[emgd->crtc_idx]->freeze_state) {
			if (new_state == 0) {
				/*
				 * Unlock the plane and set it to last saved values.
				 *
				 * There's two possibities here.
				 *   1) A set_base_atomic was called to update the
				 *      base address and offset
				 *   2) A flip was called to update the base address.
				 */
				emgd->priv->crtcs[emgd->crtc_idx]->freeze_state = 0;
				mutex_lock(&dev->struct_mutex);

				emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]->freeze_crtc;
				emgd_fb = emgd->priv->crtcs[emgd->crtc_idx]->freeze_fb;

				if (!emgd_fb || !emgd_crtc) {
					EMGD_ERROR("CRTC data structures invalid"
						"(fb=%p, crtc=%p).", emgd_fb, emgd_crtc);
					mutex_unlock(&dev->struct_mutex);
					return -EIO;
				}

				emgd_crtc->igd_plane->xoffset = emgd_crtc->unfreeze_xoffset;
				emgd_crtc->igd_plane->yoffset = emgd_crtc->unfreeze_yoffset;

				/* emgd_fb could have changed while locked. Update fb_info */
				emgd_crtc->igd_plane->fb_info = emgd_fb;

				/* Call program plane */
				ret = context->mode_dispatch->kms_program_plane(emgd_crtc, TRUE);
				if (ret < 0) {
					mutex_unlock(&dev->struct_mutex);
					return ret;
				}

				context->mode_dispatch->kms_set_plane_pwr(emgd_crtc, TRUE, TRUE);

				/* unpin and free static copy bo */
				bo = emgd->priv->crtcs[emgd->crtc_idx]->freeze_bo;
				i915_gem_object_unpin(bo);
				drm_gem_object_unreference(&bo->base);

				mutex_unlock(&dev->struct_mutex);
			} else {
				/*
				 * Locking the plane.  To do this, we need to make
				 * a copy of the current framebuffer contents and
				 * flip to that copy.
				 */
				ret = emgd_lock_plane (dev, emgd->priv->crtcs[emgd->crtc_idx]);
				if (ret) {
					return ret;
				}
			}
		}
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}

	return count;
}

static ssize_t dpms_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				emgd->priv->crtcs[emgd->crtc_idx]->dpms_mode);
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}
}

static ssize_t mode_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		if ((emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe == NULL) ||
				(emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing == NULL)) {
			return scnprintf(buf, PAGE_SIZE, "No timing info available.\n");
		} else {
		return scnprintf(buf, PAGE_SIZE,
				"width:         %d\n"
				"height:        %d\n"
				"refresh:       %dHz\n"
				"dotclock:      %ld\n"
				"h-total:       %d\n"
				"h-blank start: %d\n"
				"h-blank end:   %d\n"
				"h-sync start:  %d\n"
				"h-sync end:    %d\n"
				"v-total:       %d\n"
				"v-blank start: %d\n"
				"v-blank end:   %d\n"
				"v-sync start:  %d\n"
				"v-sync end:    %d\n"
				"x offset:      %d\n"
				"y offset:      %d\n\n",
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->width,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->height,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->refresh,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->dclk,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->htotal,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->hblank_start,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->hblank_end,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->hsync_start,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->hsync_end,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->vtotal,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->vblank_start,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->vblank_end,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->vsync_start,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->vsync_end,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->x_offset,
				emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe->timing->y_offset
				);
		}
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}
}



/*
 * The vblank timestamp show function
 *
 * This function is using to show the last 4 set timestamp vblank value on the pipe.
 * It will show most latest timestamp first keep on the following.
 *
 * Examples:
 *    cat /sys/modules/emgd/control/crtc0/vblank_timestamp
 *    cat /sys/modules/emgd/control/crtc1/vblank_timestamp
 */

static ssize_t vblank_timestamp_show(emgd_obj_t *emgd, emgd_attribute_t *attr, 
		char *buf)
{	
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		if (emgd->priv->crtcs[emgd->crtc_idx]->igd_pipe == NULL){
			return scnprintf(buf, PAGE_SIZE, "No vblank timestamp info available.\n");
		} else {
			drm_i915_private_t *dev_priv;
			struct drm_device *dev;
			struct timeval vblank_timestamp[5];
			int i, tmp_position, tmp;
			bool updated = false;	
			dev_priv = (drm_i915_private_t *)emgd->priv;
			dev = dev_priv->dev;

			if(!(IS_GEN7(dev)))
				return -ENODEV;
			
			tmp = emgd->priv->crtcs[emgd->crtc_idx]->track_index;
			tmp_position = tmp - 1;

			for(i=0; i < 4 ; i++){
				if(tmp_position == -1){
					tmp_position = MAX_VBLANK_STORE - 1;
				}

				vblank_timestamp[i].tv_sec = 
					emgd->priv->crtcs[emgd->crtc_idx]->vblank_timestamp[tmp_position].tv_sec;
				vblank_timestamp[i].tv_usec = 
					emgd->priv->crtcs[emgd->crtc_idx]->vblank_timestamp[tmp_position].tv_usec;

				tmp_position -= 1;
			}
				

			if(tmp != emgd->priv->crtcs[emgd->crtc_idx]->track_index){
				tmp_position = emgd->priv->crtcs[emgd->crtc_idx]->track_index - 1;
				if(tmp_position == -1){
					tmp_position = MAX_VBLANK_STORE - 1;
				}
				vblank_timestamp[4] = emgd->priv->crtcs[emgd->crtc_idx]->vblank_timestamp[tmp_position];
				updated = true;
			} 
			
			if(updated == true){
                        	return scnprintf(buf, PAGE_SIZE,
                                	"%ld sec\n"
                                	"%ld usec\n\n"
                                	"%ld sec\n"
                                	"%ld usec\n\n"
                                	"%ld sec\n"
                                	"%ld usec\n\n"
                                	"%ld sec\n"
                                	"%ld usec\n",
                                	vblank_timestamp[4].tv_sec,
                                	vblank_timestamp[4].tv_usec,
                                	vblank_timestamp[0].tv_sec,
                                	vblank_timestamp[0].tv_usec,
                                	vblank_timestamp[1].tv_sec,
                                	vblank_timestamp[1].tv_usec,
                                	vblank_timestamp[2].tv_sec,
                                	vblank_timestamp[2].tv_usec);
			} else {
				return scnprintf(buf, PAGE_SIZE,
					"%ld sec\n"
					"%ld usec\n\n"
					"%ld sec\n"
					"%ld usec\n\n"
					"%ld sec\n"
					"%ld usec\n\n"
					"%ld sec\n"
					"%ld usec\n",
					vblank_timestamp[0].tv_sec,
					vblank_timestamp[0].tv_usec,
					vblank_timestamp[1].tv_sec,
					vblank_timestamp[1].tv_usec,
					vblank_timestamp[2].tv_sec,
					vblank_timestamp[2].tv_usec,
					vblank_timestamp[3].tv_sec,
					vblank_timestamp[3].tv_usec);
			}
		}
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}
}

static ssize_t property_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	unsigned long value;
	emgd_property_map_t *prop_map;
	emgd_connector_t *emgd_connector;
	emgd_encoder_t *emgd_encoder;
	emgd_crtc_t *emgd_crtc;
	igd_attr_t selected_attr;
	unsigned long port_number;
	unsigned long attr_id;

	if (count > 2 && buf[0] == '0' && buf[1] == 'x') {
		sscanf(&buf[2], "%lx", &value);
	} else {
		sscanf(buf, "%lu", &value);
	}

	emgd_connector = emgd->connector;
	if (!emgd_connector) {
		EMGD_ERROR("Connector data structure invalid.");
		return -EIO;
	}

	emgd_encoder   = emgd_connector->encoder;
	if (!emgd_encoder) {
		EMGD_ERROR("Encoder data structure invalid.");
		return -EIO;
	}

	port_number    = emgd_encoder->igd_port->port_number;

	emgd_crtc = container_of(emgd_encoder->base.crtc, emgd_crtc_t, base);
	if (!emgd_crtc) {
		EMGD_ERROR("CRTC data structure invalid (crtc=%p).", emgd_crtc);
		return -EIO;
	}

	if (CHECK_NAME(attr->attr.name, "brightness")) {
		attr_id = PD_ATTR_ID_FB_BRIGHTNESS;
	} else if(CHECK_NAME(attr->attr.name, "contrast")) {
		attr_id = PD_ATTR_ID_FB_CONTRAST;
	} else if(CHECK_NAME(attr->attr.name, "gamma")) {
		attr_id = PD_ATTR_ID_FB_GAMMA;
	} else {
		EMGD_ERROR("Unknown connector property %s", attr->attr.name);
		return -EIO;
	}

	list_for_each_entry(prop_map, &emgd_connector->property_map, head) {
		if (prop_map->attribute->id == attr_id) {
			struct drm_connector *connector = &emgd_connector->base;

			/*
			 * Set the drm connector property to the new value.
			 *
			 * connector properties were moved to an object model
			 * with kernel 3.8.
			 */
			drm_object_property_set_value(&connector->base,
					prop_map->property, (uint64_t)value);

			/*
			 * Set the EMGD port attribute to the new value and update
			 * the hardware palette registers.
			 */
			memcpy(&selected_attr, prop_map->attribute, sizeof(igd_attr_t));
			selected_attr.current_value = (unsigned long) value;
			selected_attr.flags |= PD_ATTR_FLAG_VALUE_CHANGED;

			emgd->priv->context->drv_dispatch.set_attrs(emgd->priv->context,
					emgd_crtc,
					port_number,
					1, /* Setting 1 attribute */
					&selected_attr);
			return count;
		}
	}
	return -EIO;
}

static ssize_t property_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	emgd_property_map_t *prop_map;
	emgd_connector_t *emgd_connector;
	unsigned long attr_id;

	emgd_connector = emgd->connector;
	if (!emgd_connector) {
		EMGD_ERROR("Connector data structure invalid.");
		return -EIO;
	}

	if (CHECK_NAME(attr->attr.name, "brightness")) {
		attr_id = PD_ATTR_ID_FB_BRIGHTNESS;
	} else if(CHECK_NAME(attr->attr.name, "contrast")) {
		attr_id = PD_ATTR_ID_FB_CONTRAST;
	} else if(CHECK_NAME(attr->attr.name, "gamma")) {
		attr_id = PD_ATTR_ID_FB_GAMMA;
	} else {
		EMGD_ERROR("Unknown connector property %s", attr->attr.name);
		return -EIO;
	}


	list_for_each_entry(prop_map, &emgd_connector->property_map, head) {
		if (prop_map->attribute->id == attr_id) {
			return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
					prop_map->attribute->current_value);
		}
	}

	EMGD_ERROR("%s property not found.", attr->attr.name);
	return -EIO;
}

static ssize_t ovl_attr_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	long value;
	emgd_crtc_t *emgd_crtc = NULL;
	igd_ovl_plane_t * emgd_ovlplane;
	int i;
	int update_gamma = 0;
	int update_ccorrect = 0;
	int update_pipeblend = 0;
	drm_emgd_private_t *dev_priv;

	sscanf(buf, "%li", &value);

	emgd_ovlplane = emgd->emgd_ovlplane;
	dev_priv = emgd->priv;

	if(emgd_ovlplane) {

		/* Search for a CRTC that this plane will sit on */
		if(!emgd_ovlplane->emgd_crtc) {
			for (i = 0; i < dev_priv->num_crtc; i++) {
				if(dev_priv->crtcs[i]->igd_pipe == (igd_display_pipe_t *)
						emgd_ovlplane->target_igd_pipe_ptr) {
					emgd_crtc = (emgd_crtc_t *)dev_priv->crtcs[i];
				}
			}

		} else {
			/* looks like the overlay is active anyway, program with this */
			emgd_crtc = (emgd_crtc_t *)emgd_ovlplane->emgd_crtc;
		}

		if(!emgd_crtc) {
			EMGD_ERROR("Cant find any crtc for this ovlplane!");
			return -EIO;
		}

		if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
			emgd_crtc = emgd->priv->crtcs[emgd->crtc_idx]->freeze_crtc;
			if(dev_priv->num_ovlplanes) {
				for(i=0; i< dev_priv->num_ovlplanes; ++i) {
					if(emgd_crtc->igd_pipe == (igd_display_pipe_t*)
						dev_priv->ovl_planes[i].target_igd_pipe_ptr) {
						emgd_ovlplane = &dev_priv->ovl_planes[i];
						break;
					}
				}
			}
		}

		if (CHECK_NAME(attr->attr.name, "brightness")) {
			if(IS_VALUE_VALID((short)value, MIN_BRIGHTNESS_YUV, MAX_BRIGHTNESS_YUV)) {
				emgd_ovlplane->ovl_info.color_correct.brightness =
									(short) value;
				update_ccorrect = 1;
			} else {
				EMGD_ERROR("Invalid value for brightness");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "contrast")) {
			if(IS_VALUE_VALID(value, MIN_CONTRAST_YUV, MAX_CONTRAST_YUV)) {
				emgd_ovlplane->ovl_info.color_correct.contrast =
								(unsigned short)value;
				update_ccorrect = 1;
			} else {
				EMGD_ERROR("Invalid value for contrast");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "saturation")) {
			if(IS_VALUE_VALID(value, MIN_SATURATION_YUV, MAX_SATURATION_YUV)) {
				emgd_ovlplane->ovl_info.color_correct.saturation =
								(unsigned short)value;
				update_ccorrect = 1;
			} else {
				EMGD_ERROR("Invalid value for saturation");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "hue")) {
			if(IS_VALUE_VALID(value, MIN_HUE_YUV, MAX_HUE_YUV)) {
				emgd_ovlplane->ovl_info.color_correct.hue =
								(unsigned short)value;
				update_ccorrect = 1;
			} else {
				EMGD_ERROR("Invalid value for hue");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "gamma_on")) {
			if(IS_VALUE_VALID(value, IGD_OVL_GAMMA_DISABLE, IGD_OVL_GAMMA_ENABLE)) {
				emgd_ovlplane->ovl_info.gamma.flags = value;
				update_gamma = 1;
			} else {
				EMGD_ERROR("Invalid value for enable/disable gamma");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "gamma_red")) {
			if(IS_VALUE_VALID(value, IGD_OVL_GAMMA_MIN, IGD_OVL_GAMMA_MAX)) {
				emgd_ovlplane->ovl_info.gamma.red = value;
				update_gamma = 1;
			} else {
				EMGD_ERROR("Invalid value for gamma red");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "gamma_green")) {
			if(IS_VALUE_VALID(value, IGD_OVL_GAMMA_MIN, IGD_OVL_GAMMA_MAX)) {
				emgd_ovlplane->ovl_info.gamma.green = value;
				update_gamma = 1;
			} else {
				EMGD_ERROR("Invalid value for gamma green");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "gamma_blue")) {
			if(IS_VALUE_VALID(value, IGD_OVL_GAMMA_MIN, IGD_OVL_GAMMA_MAX)) {
				emgd_ovlplane->ovl_info.gamma.blue = value;
				update_gamma = 1;
			} else {
				EMGD_ERROR("Invalid value for gamma blue");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "constalpha_on")) {
			if(IS_VALUE_VALID(value, CONSTALPHA_DISABLE, CONSTALPHA_ENABLE)) {
				emgd_ovlplane->ovl_info.pipeblend.has_const_alpha = value;
				update_pipeblend = 1;
			} else {
				EMGD_ERROR("Invalid value for enable/disable constalpha");
				return -EINVAL;
			}
		} else if(CHECK_NAME(attr->attr.name, "constalpha_val")) {
			if(IS_VALUE_VALID(value, CONSTALPHA_MIN, CONSTALPHA_MAX)) {
				emgd_ovlplane->ovl_info.pipeblend.const_alpha = value;
				update_pipeblend = 1;
			} else {
				EMGD_ERROR("Invalid value for constalpha");
				return -EINVAL;
			}
		} else {
			EMGD_ERROR("Set unknown ovlplane attr %s", attr->attr.name);
			return -EIO;
		}

		dev_priv->context->drv_dispatch.alter_ovl_attr(
				(igd_display_h) emgd_crtc, emgd_ovlplane->plane_id,
				&emgd_ovlplane->ovl_info.pipeblend, update_pipeblend, 0,
				&emgd_ovlplane->ovl_info.color_correct, update_ccorrect,
				&emgd_ovlplane->ovl_info.gamma, update_gamma);

		return count;
	}

	return -EIO;
}

static ssize_t ovl_attr_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	igd_ovl_plane_t * emgd_ovlplane;
	drm_emgd_private_t *dev_priv;

	emgd_ovlplane = emgd->emgd_ovlplane;
	dev_priv = emgd->priv;

	if(emgd_ovlplane) {

		if (CHECK_NAME(attr->attr.name, "brightness")) {
			return scnprintf(buf, PAGE_SIZE, "%d\n",
					emgd_ovlplane->ovl_info.color_correct.brightness);
		} else if(CHECK_NAME(attr->attr.name, "contrast")) {
			return scnprintf(buf, PAGE_SIZE, "%d\n",
					emgd_ovlplane->ovl_info.color_correct.contrast);
		} else if(CHECK_NAME(attr->attr.name, "saturation")) {
			return scnprintf(buf, PAGE_SIZE, "%d\n",
					emgd_ovlplane->ovl_info.color_correct.saturation);
		} else if(CHECK_NAME(attr->attr.name, "hue")) {
			return scnprintf(buf, PAGE_SIZE, "%d\n",
					emgd_ovlplane->ovl_info.color_correct.hue);
		} else if(CHECK_NAME(attr->attr.name, "gamma_on")) {
			return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
					emgd_ovlplane->ovl_info.gamma.flags);
		} else if(CHECK_NAME(attr->attr.name, "gamma_red")) {
			return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
					emgd_ovlplane->ovl_info.gamma.red);
		} else if(CHECK_NAME(attr->attr.name, "gamma_green")) {
			return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
					emgd_ovlplane->ovl_info.gamma.green);
		} else if(CHECK_NAME(attr->attr.name, "gamma_blue")) {
			return scnprintf(buf, PAGE_SIZE, "0x%08x\n",
					emgd_ovlplane->ovl_info.gamma.blue);
		} else if(CHECK_NAME(attr->attr.name, "constalpha_on")) {
			return scnprintf(buf, PAGE_SIZE, "%lu\n",
					emgd_ovlplane->ovl_info.pipeblend.has_const_alpha);
		} else if(CHECK_NAME(attr->attr.name, "constalpha_val")) {
			return scnprintf(buf, PAGE_SIZE, "0x%08lx\n",
					emgd_ovlplane->ovl_info.pipeblend.const_alpha);
		} else if(CHECK_NAME(attr->attr.name, "name")) {
			return scnprintf(buf, PAGE_SIZE, "%s\n",
					emgd_ovlplane->plane_name);
		} else {
			EMGD_ERROR("Get unknown ovlplane attr %s", attr->attr.name);
			return -EIO;
		}
	}

	EMGD_ERROR("%s Invalid emgd_ovlplane.", attr->attr.name);
	return -EIO;
}

/*
 * The palette_persistence attribute store and show function
 *
 * Set this attribute to 0 will honor the color setting from user
 * space for a particular CRTC when emgd_crtc_gamma_set is called.
 * Set to 1 or non-zero will ignore the color setting from user space,
 * the gamma, brightness and contrast port attributes set via sysfs
 * and default_config will not be overridden in the color palette.
 *
 * Examples:
 *    echo "1" > /sys/modules/emgd/control/crtc0/palette_persistence
 *    echo "0" > /sys/modules/emgd/control/crtc0/palette_persistence
 *    cat /sys/modules/emgd/control/crtc0/palette_persistence
 */
static ssize_t palette_persistence_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
		const char *buf, size_t count)
{
	int new_state;

	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		sscanf(buf, "%du", &new_state);
		if ((new_state != 0) && (new_state != 1)) {
			EMGD_ERROR("Invalid value for incoming state!");
			return -EIO;
		}
		emgd->priv->crtcs[emgd->crtc_idx]->palette_persistence = new_state;
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}

	return count;
}

static ssize_t palette_persistence_show(emgd_obj_t *emgd, emgd_attribute_t *attr,
		char *buf)
{
	if (emgd->crtc_idx >= 0 && emgd->crtc_idx < emgd->priv->num_crtc) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				emgd->priv->crtcs[emgd->crtc_idx]->palette_persistence);
	} else {
		EMGD_ERROR("crtc index %d is invalid.", emgd->crtc_idx);
		return -EIO;
	}
}

static ssize_t emgd_gemsched_shared_show(emgd_obj_t *emgd, emgd_attribute_t *attr, char *buf)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;

	if (CHECK_NAME(attr->attr.name, "period")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(dev_priv->scheduler.shared_period/1000));
	} else if(CHECK_NAME(attr->attr.name, "aereserve")) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)dev_priv->scheduler.shared_aereserve);
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(dev_priv->scheduler.shared_capacity/1000));
	}
	EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
	return -EIO;
}

static ssize_t emgd_gemsched_shared_store(emgd_obj_t *emgd, 
		emgd_attribute_t *attr, const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;
	uint32_t value;
	int ret;

	sscanf(buf, "%u", &value);
	
	if (mutex_lock_interruptible(&dev_priv->dev->struct_mutex)) {
		EMGD_ERROR("Cant lock-irq dev mutex");
		return -EIO;
	}
	if (CHECK_NAME(attr->attr.name, "period")) {
		if (dev_priv->scheduler.shared_period != (value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			dev_priv->scheduler.shared_period = (value*1000);
			EMGD_DEBUG("New shared period = %d\n", value);
		}
	} else if(CHECK_NAME(attr->attr.name, "aereserve")) {
		dev_priv->scheduler.shared_aereserve = value? true: false;
		EMGD_DEBUG("New shared AE reservation = %d\n", (value? 1:0));
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		if (dev_priv->scheduler.shared_capacity != (value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			dev_priv->scheduler.shared_capacity = (value*1000);
			EMGD_DEBUG("New shared capacity = %d\n", value);
		}
	} else {
		EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
		return -EIO;
	}
	mutex_unlock(&dev_priv->dev->struct_mutex);
	
	return count;
}

static ssize_t emgd_gemsched_video_show(emgd_obj_t *emgd, emgd_attribute_t *attr, char *buf)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;

	if (CHECK_NAME(attr->attr.name, "autoswitch")) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)dev_priv->scheduler.video_switch);
	} else if(CHECK_NAME(attr->attr.name, "initpriority")) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
			dev_priv->scheduler.video_priority);
	} else if(CHECK_NAME(attr->attr.name, "initperiod")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			dev_priv->scheduler.video_init_period);
	} else if(CHECK_NAME(attr->attr.name, "initcapacity")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			dev_priv->scheduler.video_init_capacity);
	}
	EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
	return -EIO;
}
static ssize_t emgd_gemsched_video_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
			const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;
	uint32_t value;

	sscanf(buf, "%u", &value);
	
	if (mutex_lock_interruptible(&dev_priv->dev->struct_mutex)) {
		EMGD_ERROR("Cant lock-irq dev mutex");
		return -EIO;
	}
	if (CHECK_NAME(attr->attr.name, "autoswitch")) {
		if (dev_priv->scheduler.video_switch != (bool)value) {
			dev_priv->scheduler.video_switch = (value > 0)? true: false;
			EMGD_DEBUG("New video switch = %d\n", (value > 0)? 1: 0);
		}
	} else if(CHECK_NAME(attr->attr.name, "initpriority")) {
		if (dev_priv->scheduler.video_priority != value) {
			dev_priv->scheduler.video_priority = value;
			EMGD_DEBUG("New video init priority= %d\n", value);
		}
	} else if(CHECK_NAME(attr->attr.name, "initperiod")) {
		if (dev_priv->scheduler.video_init_period != (value*1000)) {
			dev_priv->scheduler.video_init_period = (value*1000);
			EMGD_DEBUG("New video init period = %d\n", value);
		}
	} else if(CHECK_NAME(attr->attr.name, "initcapacity")) {
		if (dev_priv->scheduler.video_init_capacity != (value*1000)) {
			dev_priv->scheduler.video_init_capacity = (value*1000);
			EMGD_DEBUG("New video init capacity = %d\n", value);
		}
	} else {
		EMGD_ERROR("Unknown gem_sched video attr %s", attr->attr.name);
		return -EIO;
	}
	mutex_unlock(&dev_priv->dev->struct_mutex);
	
	return count;
}
static ssize_t emgd_gemsched_rogue_show(emgd_obj_t *emgd, emgd_attribute_t *attr, char *buf)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;

	if (CHECK_NAME(attr->attr.name, "autoswitch")) {
		return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)dev_priv->scheduler.rogue_switch);
	} else if(CHECK_NAME(attr->attr.name, "trigger")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(dev_priv->scheduler.rogue_trigger/1000));
	} else if(CHECK_NAME(attr->attr.name, "period")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(dev_priv->scheduler.rogue_period/1000));
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		return scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(dev_priv->scheduler.rogue_capacity/1000));
	}
	EMGD_ERROR("Unknown gem_sched rogue attr %s", attr->attr.name);
	return -EIO;
}
static ssize_t emgd_gemsched_rogue_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
			const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;
	struct drm_i915_file_private *rtemp = NULL;
	uint32_t value;
	int ret; bool idled=false;

	sscanf(buf, "%u", &value);
	
	if (mutex_lock_interruptible(&dev_priv->dev->struct_mutex)) {
		EMGD_ERROR("Cant lock-irq dev mutex");
		return -EIO;
	}
	if (CHECK_NAME(attr->attr.name, "autoswitch")) {
		if (dev_priv->scheduler.rogue_switch != (bool)value) {
			dev_priv->scheduler.rogue_switch = (value > 0)? true: false;
			EMGD_DEBUG("New rogue switch = %d\n", (value > 0)? 1: 0);
			if (!dev_priv->scheduler.rogue_switch) { 
				list_for_each_entry(rtemp, 
					&dev_priv->i915_client_list, client_link){
					if (rtemp->mm.sched_priority == SHARED_ROGUE_PRIORITY){
						spin_lock(&rtemp->mm.lock);
						if (!idled) {
							ret = i915_gpu_idle(dev_priv->dev);
							dev_priv->scheduler.req_prty_cnts[SHARED_NORMAL_PRIORITY] += 
								dev_priv->scheduler.req_prty_cnts[SHARED_ROGUE_PRIORITY];
							dev_priv->scheduler.req_prty_cnts[SHARED_ROGUE_PRIORITY] = 0;
						}
						idled = true;
						rtemp->mm.sched_priority = SHARED_NORMAL_PRIORITY;
						EMGD_DEBUG("New priority for pid %d = %d\n", 
							   PID_T_FROM_DRM_FILE(rtemp->drm_file), SHARED_NORMAL_PRIORITY);
						spin_unlock(&rtemp->mm.lock);
					}
				}
			}
			
		}
	} else if(CHECK_NAME(attr->attr.name, "trigger")) {
		if (dev_priv->scheduler.rogue_trigger != value*1000) {
			dev_priv->scheduler.rogue_trigger = (value*1000);
			EMGD_DEBUG("New rogue trigger = %d\n", value);
		}
	} else if(CHECK_NAME(attr->attr.name, "period")) {
		if (dev_priv->scheduler.rogue_period != (value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			dev_priv->scheduler.rogue_period = (value*1000);
			EMGD_DEBUG("New rogue period = %d\n", value);
		}
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		if (dev_priv->scheduler.rogue_capacity != (value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			dev_priv->scheduler.rogue_capacity = (value*1000);
			EMGD_DEBUG("New rogue capacity = %d\n", value);
		}
	} else {
		EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
		return -EIO;
	}
	mutex_unlock(&dev_priv->dev->struct_mutex);
	
	return count;
}
static ssize_t emgd_gemsched_process_show(emgd_obj_t *emgd, emgd_attribute_t *attr, char *buf)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;
	struct drm_i915_file_private *temp = NULL;
	bool found = false;
	int sched_avgexectime;
	uint32_t capacity, period;
	int ret=0, i;
	struct tm time_spec;

	if (mutex_lock_interruptible(&dev_priv->dev->struct_mutex)) {
		EMGD_ERROR("Cant lock-irq dev mutex");
		return -EIO;
	}

	list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
		if (temp == emgd->drmfilepriv){
			found = true;
			spin_lock(&temp->mm.lock);
			break;
		}
	}

	mutex_unlock(&dev_priv->dev->struct_mutex);
	
	if (!found) {
		EMGD_ERROR("gem_sched process show cant find drmfilepriv");
		return -EIO;
	}
	if (SHARED_NORMAL_PRIORITY == temp->mm.sched_priority) {
		period      = dev_priv->scheduler.shared_period;
		capacity    = dev_priv->scheduler.shared_capacity;
	} else if (SHARED_ROGUE_PRIORITY == temp->mm.sched_priority) {
		period      = dev_priv->scheduler.rogue_period;
		capacity    = dev_priv->scheduler.rogue_capacity;
	} else {
		period      = temp->mm.sched_period;
		capacity    = temp->mm.sched_capacity;
	}
	
	if (CHECK_NAME(attr->attr.name, "priority")) {
		ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			temp->mm.sched_priority);
	} else if (CHECK_NAME(attr->attr.name, "period")) {
		ret = scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(period/1000));
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		ret = scnprintf(buf, PAGE_SIZE, "%06dms\n",
			(capacity/1000));
	} else if(CHECK_NAME(attr->attr.name, "stats")) {
		ret = scnprintf(buf, PAGE_SIZE, "Priority    = %d\n",
			temp->mm.sched_priority);
		ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "Period      = %06dms\n",
			(period/1000));
		ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "Capcity     = %06dms\n",
			(capacity/1000));
		ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "Q'd request = %d\n",
			temp->mm.outstanding_requests);
  		sched_avgexectime = 0;
		for (i=0; i<MOVING_AVG_RANGE ;++i) {
			sched_avgexectime += temp->mm.exec_times[i];
		}
		if (sched_avgexectime) 
			sched_avgexectime = (sched_avgexectime / MOVING_AVG_RANGE);
		ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "Avg Exectime = %06dus\n", 
			sched_avgexectime);
		
		ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "Submission Time:\n");	
		for (i=0; i<MOVING_AVG_RANGE; i++) {
			time_to_tm(temp->mm.submission_times[i].tv_sec, 0, &time_spec);
			
			if (ret < PAGE_SIZE-18)
				ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "\t%2d. %02d:%02d:%02d:%03ld\n", i, time_spec.tm_hour, time_spec.tm_min, time_spec.tm_sec, temp->mm.submission_times[i].tv_usec/1000);
			else
				ret += scnprintf(&buf[ret], (PAGE_SIZE-ret), "sysfs buffer full");
		}
		
				 
	} else {
	      EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
	}
	
	spin_unlock(&temp->mm.lock);
	return ret;
}
static ssize_t emgd_gemsched_process_store(emgd_obj_t *emgd, emgd_attribute_t *attr,
			const char *buf, size_t count)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)emgd->priv;
	struct drm_i915_file_private *temp = NULL;
	struct drm_i915_file_private *temp2 = NULL;
	bool found = false;
	uint32_t old, new_value;
	int ret;

	sscanf(buf, "%u", &new_value);
	
	if (mutex_lock_interruptible(&dev_priv->dev->struct_mutex)) {
		EMGD_ERROR("Cant lock-irq dev mutex");
		return -EIO;
	}

	list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
		if (temp == emgd->drmfilepriv){
			found = true;
			spin_lock(&temp->mm.lock);
			break;
		}
	}
	
	if (!found) {
		mutex_unlock(&dev_priv->dev->struct_mutex);
		EMGD_ERROR("gem_sched process store cant find drmfilepriv");
		return -EIO;
	}
	
	if (CHECK_NAME(attr->attr.name, "priority")) {
		if (temp->mm.sched_period != new_value) {
			if (temp->drm_file->is_master) {
				EMGD_DEBUG("Changing master priority prohibited!- override to '0'");
				spin_unlock(&temp->mm.lock);
				mutex_unlock(&dev_priv->dev->struct_mutex);
				return count;
			}
			if ( (new_value < SHARED_NORMAL_PRIORITY) && (new_value != 0)) {
				/* if its not highest or lowest-shared, then u cant share slots */
				list_for_each_entry(temp2, &dev_priv->i915_client_list, client_link) {
					if (temp != temp2 ) {
						if( temp2->mm.sched_priority == new_value) {
							EMGD_ERROR("Priority request is not sharable - bailing!");
							spin_unlock(&temp->mm.lock);
							mutex_unlock(&dev_priv->dev->struct_mutex);
							return count;
						}
					}
				}
			}
					
			old = temp->mm.sched_priority;
			if (old != new_value && new_value <= MAX_SCHED_PRIORITIES) {
				ret = i915_gpu_idle(dev_priv->dev);
				dev_priv->scheduler.req_prty_cnts[new_value] += dev_priv->scheduler.req_prty_cnts[old];
				dev_priv->scheduler.req_prty_cnts[old] = 0;
				temp->mm.sched_priority = new_value;
				EMGD_DEBUG("New priority for pid %d = %d\n", 
					PID_T_FROM_DRM_FILE(temp->drm_file), new_value);
			}
		}
	} else if (CHECK_NAME(attr->attr.name, "period")) {
		old = temp->mm.sched_period;
		if (old != (new_value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			temp->mm.sched_period = new_value*1000;
			EMGD_DEBUG("New period for pid %d = %d\n",
					PID_T_FROM_DRM_FILE(temp->drm_file), new_value);
		}
	} else if(CHECK_NAME(attr->attr.name, "capacity")) {
		old = temp->mm.sched_capacity;
		if (old != (new_value*1000)) {
			ret = i915_gpu_idle(dev_priv->dev);
			temp->mm.sched_capacity = new_value*1000;
			EMGD_DEBUG("New capacity for pid %d = %d\n",
					PID_T_FROM_DRM_FILE(temp->drm_file), new_value);
		}
	} else {
	      EMGD_ERROR("Unknown gem_sched shared attr %s", attr->attr.name);
	}
	
	spin_unlock(&temp->mm.lock);
	mutex_unlock(&dev_priv->dev->struct_mutex);
	return count;
}

static emgd_attribute_t emgd_max_freq_attribute = 
		__ATTR(i915_max_freq, 0664, emgd_max_freq_show, emgd_max_freq_store);
static emgd_attribute_t emgd_min_freq_attribute = 
		__ATTR(i915_min_freq, 0444, emgd_min_freq_show, NULL);
static emgd_attribute_t emgd_cur_freq_attribute = 
		__ATTR(i915_cur_freq, 0444, emgd_cur_freq_show, NULL);
static struct attribute *emgd_freq_attrs[]= {
	&emgd_max_freq_attribute.attr,
	&emgd_min_freq_attribute.attr,
	&emgd_cur_freq_attribute.attr,
	NULL,
};

#ifdef SPLASH_VIDEO_SUPPORT
static emgd_attribute_t emgd_splashvideo_plane_attribute =
		__ATTR(splash_video_plane_hold, 0664, splashvideo_plane_hold_show, splashvideo_plane_hold_store);
#endif
static emgd_attribute_t emgd_splashvideo_attribute =
		__ATTR(splash_video_enable, 0664, splashvideo_show, splashvideo_store);
static emgd_attribute_t emgd_ovlplane_zorder_attribute =
		__ATTR(ovl_zorder, 0664, emgd_ovlplane_zorder_show, emgd_ovlplane_zorder_store);
static emgd_attribute_t emgd_ovlplane_fbblendovl_attribute =
		__ATTR(fb_blend_ovl, 0664, emgd_ovlplane_fbblendovl_show, emgd_ovlplane_fbblendovl_store);

static emgd_attribute_t emgd_freeze_attribute =
		__ATTR(lock_plane, 0664, freeze_show, freeze_store);
static emgd_attribute_t emgd_palette_persistence_attribute =
		__ATTR(palette_persistence, 0664, palette_persistence_show, palette_persistence_store);
static emgd_attribute_t emgd_dpms_attribute =
		__ATTR(DPMS, 0444, dpms_show, NULL);
static emgd_attribute_t emgd_mode_attribute =
		__ATTR(current_mode, 0444, mode_show, NULL);
static emgd_attribute_t emgd_vblank_timestamp_attribute =
		__ATTR(vblank_timestamp, 0444, vblank_timestamp_show, NULL);


#define FB_ATTR(NAME, ACCESSOR) \
	static ssize_t NAME ## _show(emgd_obj_t *emgd, emgd_attribute_t *attr,\
		char *buf)                                                        \
	{                                                                     \
		if (!emgd->fb) {                                                  \
			EMGD_ERROR("Invalid framebuffer for " # NAME " read");        \
			return -EIO;                                                  \
		}                                                                 \
	                                                                      \
		return scnprintf(buf, PAGE_SIZE, "%d\n", emgd->fb->ACCESSOR);     \
	}                                                                     \
		                                                                  \
	static emgd_attribute_t emgd_ ## NAME ## _attribute =                 \
		__ATTR(NAME, 0444, NAME ## _show, NULL);

FB_ATTR(fb_width, base.width);
FB_ATTR(fb_height, base.height);
FB_ATTR(fb_pitch, base.DRMFB_PITCH);
FB_ATTR(fb_depth, base.depth);
FB_ATTR(fb_bpp, base.bits_per_pixel);
FB_ATTR(fb_pin_count, bo->pin_count);
FB_ATTR(fb_tiling, bo->tiling_mode);


static struct attribute *emgd_crtc_attrs[] = {
	&emgd_freeze_attribute.attr,
	&emgd_dpms_attribute.attr,
	&emgd_mode_attribute.attr,
#ifdef SPLASH_VIDEO_SUPPORT
	&emgd_splashvideo_plane_attribute.attr,
#endif
	&emgd_splashvideo_attribute.attr,
	&emgd_ovlplane_zorder_attribute.attr,
	&emgd_ovlplane_fbblendovl_attribute.attr,
	&emgd_palette_persistence_attribute.attr,
	&emgd_vblank_timestamp_attribute.attr,
	NULL,
};

static struct attribute *emgd_fb_attrs[] = {
	&emgd_fb_width_attribute.attr,
	&emgd_fb_height_attribute.attr,
	&emgd_fb_pitch_attribute.attr,
	&emgd_fb_depth_attribute.attr,
	&emgd_fb_bpp_attribute.attr,
	&emgd_fb_pin_count_attribute.attr,
	&emgd_fb_tiling_attribute.attr,
	NULL,
};

static emgd_attribute_t emgd_brightness_attribute =
		__ATTR(brightness, 0664, property_show, property_store);
static emgd_attribute_t emgd_contrast_attribute =
		__ATTR(contrast, 0664, property_show, property_store);
static emgd_attribute_t emgd_gamma_attribute =
		__ATTR(gamma, 0664, property_show, property_store);

static struct attribute *emgd_connector_attrs[] = {
	&emgd_brightness_attribute.attr,
	&emgd_contrast_attribute.attr,
	&emgd_gamma_attribute.attr,
	NULL,
};

/* overlay / sprite runtime attributes */
static emgd_attribute_t emgd_ovlname_attribute =
		__ATTR(name,       0444, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlbrightness_attribute =
		__ATTR(brightness, 0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlcontrast_attribute =
		__ATTR(contrast,   0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlsaturation_attribute =
		__ATTR(saturation, 0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlhue_attribute =
		__ATTR(hue,        0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlgamma_enable_attribute =
		__ATTR(gamma_on,   0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlgammaR_attribute =
		__ATTR(gamma_red,     0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlgammaG_attribute =
		__ATTR(gamma_green,     0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_ovlgammaB_attribute =
		__ATTR(gamma_blue,     0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_const_alpha_enable_attribute =
		__ATTR(constalpha_on,   0664, ovl_attr_show, ovl_attr_store);
static emgd_attribute_t emgd_const_alpha_value_attribute =
		__ATTR(constalpha_val,  0664, ovl_attr_show, ovl_attr_store);

static struct attribute **emgd_ovlplane_attrs_list = NULL;

/* Info attributes */
static emgd_attribute_t emgd_info_desc_attribute =
		__ATTR(description, 0444, info_desc_show, NULL);
static emgd_attribute_t emgd_info_version_attribute =
		__ATTR(version, 0444, info_version_show, NULL);
static emgd_attribute_t emgd_info_date_attribute =
		__ATTR(builddate, 0444, info_date_show, NULL);
static emgd_attribute_t emgd_info_chipset_attribute =
		__ATTR(chipset, 0444, info_chipset_show, NULL);

static struct attribute *emgd_info_attrs[] = {
	&emgd_info_desc_attribute.attr,
	&emgd_info_version_attribute.attr,
	&emgd_info_date_attribute.attr,
	&emgd_info_chipset_attribute.attr,
	NULL,
};

/****************************
 *       GEM Scheduler      *
 *  (Prioritized Rendering) *
 *       Attributes         *
 ****************************/
/* !!!!!! NOTE: All GEM Scheduler attributes is readible by *
 * everyone but writable only by root / admin - that is it! */

/* GEM Attributes for "Shared" slot - runtime knobs for this slot *
 * take note that "shared slot" is the default for all processees */
/* Will appear as "/sys/module/emgd/control/i915_scheduler/shared/blah" */
static emgd_attribute_t emgd_gemsched_shared_period_attribute =
	__ATTR(period,   0664, emgd_gemsched_shared_show, emgd_gemsched_shared_store);
static emgd_attribute_t emgd_gemsched_shared_capacity_attribute =
	__ATTR(capacity, 0664, emgd_gemsched_shared_show, emgd_gemsched_shared_store);
static emgd_attribute_t emgd_gemsched_shared_aereserve_attribute =
	__ATTR(aereserve, 0664, emgd_gemsched_shared_show, emgd_gemsched_shared_store);
static struct attribute *emgd_gemsched_shared_attrs[] = {
	&emgd_gemsched_shared_period_attribute.attr,
	&emgd_gemsched_shared_capacity_attribute.attr,
	&emgd_gemsched_shared_aereserve_attribute.attr,
	NULL,
};

/* GEM Attributes for "Video auto-promotion" feature - initial knobs at promotion point*/
/* Will appear as "/sys/module/emgd/control/i915_scheduler/video/blah" */
static emgd_attribute_t emgd_gemsched_video_autoswitch_attribute =
	__ATTR(autoswitch,   0664, emgd_gemsched_video_show, emgd_gemsched_video_store);
static emgd_attribute_t emgd_gemsched_video_initpriority_attribute =
	__ATTR(initpriority, 0664, emgd_gemsched_video_show, emgd_gemsched_video_store);
static emgd_attribute_t emgd_gemsched_video_initperiod_attribute =
	__ATTR(initperiod,   0664, emgd_gemsched_video_show, emgd_gemsched_video_store);
static emgd_attribute_t emgd_gemsched_video_initcapacity_attribute =
	__ATTR(initcapacity, 0664, emgd_gemsched_video_show, emgd_gemsched_video_store);
static struct attribute *emgd_gemsched_video_attrs[] = {
	&emgd_gemsched_video_autoswitch_attribute.attr,
	&emgd_gemsched_video_initpriority_attribute.attr,
	&emgd_gemsched_video_initperiod_attribute.attr,
	&emgd_gemsched_video_initcapacity_attribute.attr,
	NULL,
};

/* GEM Attributes for "Rogue auto-demotion" feature - runtime knobs for rogue slot
b * take note "rogue slot" is the shared by 1 or more processes that get demoted here */
/* Will appear as "/sys/module/emgd/control/i915_scheduler/rogue/blah" */
static emgd_attribute_t emgd_gemsched_rogue_autoswitch_attribute =
	__ATTR(autoswitch, 0664, emgd_gemsched_rogue_show, emgd_gemsched_rogue_store);
static emgd_attribute_t emgd_gemsched_rogue_trigger_attribute =
	__ATTR(trigger,    0664, emgd_gemsched_rogue_show, emgd_gemsched_rogue_store);
static emgd_attribute_t emgd_gemsched_rogue_period_attribute =
	__ATTR(period,     0664, emgd_gemsched_rogue_show, emgd_gemsched_rogue_store);
static emgd_attribute_t emgd_gemsched_rogue_capacity_attribute =
	__ATTR(capacity,   0664, emgd_gemsched_rogue_show, emgd_gemsched_rogue_store);
static struct attribute *emgd_gemsched_rogue_attrs[] = {
	&emgd_gemsched_rogue_autoswitch_attribute.attr,
	&emgd_gemsched_rogue_trigger_attribute.attr,
	&emgd_gemsched_rogue_period_attribute.attr,
	&emgd_gemsched_rogue_capacity_attribute.attr,
	NULL,
};

/* GEM Attributes for "Per-Process slots" - runtime knobs for all the normal processes
 * take note this sysfs will */
/* Will appear as "/sys/module/emgd/control/i915_scheduler/process/[proc-ID]/blah" * 
 * ---> where the "process-ID" symlink is created or destroyed at new clients open *
 *      or close new DRM file private handles with our driver                      */
static emgd_attribute_t emgd_gemsched_process_priority_attribute =
	__ATTR(priority, 0664, emgd_gemsched_process_show, emgd_gemsched_process_store);
static emgd_attribute_t emgd_gemsched_process_period_attribute =
	__ATTR(period,   0664, emgd_gemsched_process_show, emgd_gemsched_process_store);
static emgd_attribute_t emgd_gemsched_process_capacity_attribute =
	__ATTR(capacity, 0664, emgd_gemsched_process_show, emgd_gemsched_process_store);
static emgd_attribute_t emgd_gemsched_process_stats_attribute =
	__ATTR(stats,    0664, emgd_gemsched_process_show, NULL);
static struct attribute *emgd_gemsched_process_attrs[] = {
	&emgd_gemsched_process_priority_attribute.attr,
	&emgd_gemsched_process_period_attribute.attr,
	&emgd_gemsched_process_capacity_attribute.attr,
	&emgd_gemsched_process_stats_attribute.attr,
	NULL,
};

static struct kobj_type emgd_gemsched_shared_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_gemsched_shared_attrs,
};
static struct kobj_type emgd_gemsched_video_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_gemsched_video_attrs,
};
static struct kobj_type emgd_gemsched_rogue_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_gemsched_rogue_attrs,
};
static struct kobj_type emgd_gemsched_process_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_gemsched_process_attrs,
};

static struct kobj_type emgd_crtc_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_crtc_attrs,
};

static struct kobj_type emgd_fb_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_fb_attrs,
};

static struct kobj_type emgd_control_ktype = {
	.release = emgd_obj_release,
};

static struct kobj_type emgd_connector_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_connector_attrs,
};

static struct kobj_type emgd_ovlplane_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = NULL,
};

static struct kobj_type emgd_info_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_info_attrs,
};

static struct kobj_type emgd_freq_ktype = {
	.sysfs_ops = &emgd_sysfs_ops,
	.release = emgd_obj_release,
	.default_attrs = emgd_freq_attrs,
};


/*
 * Create the "control" directory in the emgd module tree.
 */
static int emgd_create_control(struct drm_device *dev)
{
	int ret;
	struct kobject *parent_kobj;

	if (!emgd_obj) {
		emgd_obj = OS_ZALLOC(sizeof(*emgd_obj));
		if (!emgd_obj) {
			EMGD_ERROR("Failed to allocate sysfs object");
			return false;
		}

		emgd_obj->priv = dev->dev_private;
		emgd_obj->crtc_idx = -1;


		/* Initialize the kobject that will be used to hold our control attrs */
		kobject_init(&emgd_obj->kobj, &emgd_control_ktype);

		/*
		 * Add the  emgd kobject to the tree.  Insert the "control" object
		 * under:
		 *    /sys/modules/emgd
		 */
#ifdef MODULE
		parent_kobj = &THIS_MODULE->mkobj.kobj;
#else
		parent_kobj = kset_find_obj(module_kset, "emgd");
		if (parent_kobj == NULL) {
			EMGD_ERROR("Unable to find emgd in /sys/modules.");
			return false;
		}
#endif

		ret = kobject_add(&emgd_obj->kobj, parent_kobj, "control");
		if (ret) {
			EMGD_ERROR("Failed to add sysfs control directory");
			kobject_put(&emgd_obj->kobj);
			return false;
		}

		/* Send out a notification that our kobject was created */
		kobject_uevent(&emgd_obj->kobj, KOBJ_ADD);
	}

	return true;
}

/*
 * Create the "info" directory in the emgd module tree.
 */
static int emgd_create_info(struct drm_device *dev)
{
	struct kobject *parent_kobj;
	int ret;

	if (!info_obj) {
		info_obj = OS_ZALLOC(sizeof(*info_obj));
		if (!info_obj) {
			EMGD_ERROR("Failed to allocate sysfs object");
			return false;
		}

		info_obj->priv = dev->dev_private;
		info_obj->crtc_idx = -1;


		/* Initialize the kobject that will be used to hold our control attrs */
		kobject_init(&info_obj->kobj, &emgd_info_ktype);

		/*
		 * Add the  emgd kobject to the tree.  Insert the "info" object
		 * under:
		 *    /sys/modules/emgd
		 */
#ifdef MODULE
		parent_kobj = &THIS_MODULE->mkobj.kobj;
#else
		parent_kobj = kset_find_obj(module_kset, "emgd");
		if (parent_kobj == NULL) {
			EMGD_ERROR("Unable to find emgd in /sys/modules.");
			return false;
		}
#endif

		ret = kobject_add(&info_obj->kobj, parent_kobj, "info");
		if (ret) {
			EMGD_ERROR("Failed to add sysfs info directory");
			kobject_put(&info_obj->kobj);
			return false;
		}

		/* Send out a notification that our kobject was created */
		kobject_uevent(&info_obj->kobj, KOBJ_ADD);
	}

	return true;
}


/*
 * emgd_sysfs_addfb()
 *
 * Adds a node under /sys/module/emgd/control/fb/ for a new DRM
 * framebuffer.
 */
void emgd_sysfs_addfb(emgd_framebuffer_t *emgdfb)
{
	drm_emgd_private_t *priv;
	int ret;

	/*Sanity check*/
	if (emgdfb == NULL ){
		EMGD_ERROR("Argument 'emgdfb' to emgd_sysfs_addfb is not sane.");
		return;
	}
	if (fbdir_obj == NULL){
		EMGD_ERROR("fbdir in emgd_sysfs_addfb is not sane.");
		return;
	}

	priv = emgdfb->base.dev->dev_private;
	emgdfb->sysfs_obj = OS_ZALLOC(sizeof(*emgdfb->sysfs_obj));

	if (emgdfb->sysfs_obj == NULL){
		EMGD_ERROR("Failed to allocate memory (OS_ZALLOC)  to emgdfb->sysfs_obj");
		return;
	}

	emgdfb->sysfs_obj->priv = priv;
	emgdfb->sysfs_obj->fb = emgdfb;

	kobject_init(&emgdfb->sysfs_obj->kobj, &emgd_fb_ktype);
	ret = kobject_add(&emgdfb->sysfs_obj->kobj, &fbdir_obj->kobj, "%d", emgdfb->base.base.id);
	if (ret) {
		EMGD_ERROR("Failed to add framebuffer %d node to sysfs", emgdfb->base.base.id);
		kobject_put(&emgdfb->sysfs_obj->kobj);
		return;
	}
	kobject_uevent(&emgdfb->sysfs_obj->kobj, KOBJ_ADD);
}


/*
 * emgd_sysfs_rmfb()
 *
 * Removes the framebuffer node from /sys/module/emgd/control/fb/ when the DRM
 * framebuffer is destroyed.
 */
void emgd_sysfs_rmfb(emgd_framebuffer_t *emgdfb)
{
	kobject_put(&emgdfb->sysfs_obj->kobj);
	emgdfb->sysfs_obj = NULL;
}


/*
 * emgd_sysfs_switch_crtc_fb()
 *
 * Switches the framebuffer symlink associated with a CRTC in the
 * sysfs control directory.
 */
void emgd_sysfs_switch_crtc_fb(emgd_crtc_t *emgdcrtc)
{
	emgd_framebuffer_t *emgdfb;
	int ret;

	/*
	 * Some modesets will happen in the driver before we initialize
	 * sysfs and create the kobj's for each CRTC.  Just ignore those
	 * modesets until we're setup.
	 */
	if (!emgdcrtc->sysfs_obj) {
		return;
	}

	/*
	 * Remove old "framebuffer" symlink from CRTC directory.  This should
	 * work fine, even if the symlink doesn't exist yet (i.e., first
	 * mode set when no previous framebuffer was attached).
	 */
	sysfs_remove_link(&emgdcrtc->sysfs_obj->kobj, "framebuffer");

	/* Add new framebuffer symlink for the fb we're switching to */
	if (emgdcrtc->base.fb) {
		emgdfb = container_of(emgdcrtc->base.fb, emgd_framebuffer_t, base);

		if (!emgdfb->sysfs_obj) {
			EMGD_ERROR("Framebuffer for mode switch has no sysfs node");
			return;
		}

		ret = sysfs_create_link(&emgdcrtc->sysfs_obj->kobj,
			&emgdfb->sysfs_obj->kobj, "framebuffer");
		if (ret) {
			EMGD_ERROR("Failed to create 'framebuffer' link for crtc in sysfs");
		}
	}
}



/*
 * Add a connector node to the sysfs tree
 */
void emgd_sysfs_connector_add(emgd_connector_t *emgd_connector)
{
	drm_emgd_private_t *priv = emgd_connector->base.dev->dev_private;
	int ret;


	if (!emgd_create_control(emgd_connector->base.dev)) {
		return;
	}

	emgd_connector->sysfs_obj = OS_ZALLOC(sizeof(*emgd_connector->sysfs_obj));
	emgd_connector->sysfs_obj->priv = priv;
	emgd_connector->sysfs_obj->connector = emgd_connector;

	kobject_init(&emgd_connector->sysfs_obj->kobj, &emgd_connector_ktype);
	ret = kobject_add(&emgd_connector->sysfs_obj->kobj, &emgd_obj->kobj, "%s",
			drm_get_connector_name(&emgd_connector->base));
	if (ret) {
		EMGD_ERROR("Failed to add connector %s node to sysfs",
				drm_get_connector_name(&emgd_connector->base));
		kobject_put(&emgd_connector->sysfs_obj->kobj);
		return;
	}
}

void emgd_sysfs_connector_remove(emgd_connector_t *emgd_connector)
{
	kobject_put(&emgd_connector->sysfs_obj->kobj);
	emgd_connector->sysfs_obj = NULL;
}



/*
 * Add a sprite / overlay plane node to the sysfs tree
 */
void emgd_sysfs_ovlplane_add(drm_emgd_private_t *priv,
		igd_ovl_plane_t * emgd_ovlplane)
{
	int ret;
	int i, num_ovl_attrs = 1; /* least 1 for the NULL*/

	if (!emgd_create_control(priv->dev)) {
		return;
	}

	if(!emgd_ovlplane_attrs_list){
		++num_ovl_attrs; /* read only for the plane name */

		if(emgd_ovlplane->num_gamma) {
			num_ovl_attrs += 4;
			/* 1 for enable, 3 for 24.8 fixed point per each channel */
		}
		if(emgd_ovlplane->has_ccorrect) {
			num_ovl_attrs += 4;
			/* brightness, saturation, contrast, hue*/
		}
		if(emgd_ovlplane->has_constalpha) {
			num_ovl_attrs += 2;
			/* constalpha_enable, constalpha_value */
		}
		emgd_ovlplane_attrs_list = OS_ZALLOC(sizeof(struct attribute *) *
				num_ovl_attrs);
		if(!emgd_ovlplane_attrs_list){
			EMGD_ERROR("Failed to allocate the emgd_ovlplane_attrs list!");
			return;
		}
	}

	i = 0;

	/* start with the name of the plane */
	emgd_ovlplane_attrs_list[i++] = &emgd_ovlname_attribute.attr;

	if(emgd_ovlplane->num_gamma) {
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlgamma_enable_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlgammaR_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlgammaG_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlgammaB_attribute.attr;
	}
	if(emgd_ovlplane->has_ccorrect) {
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlbrightness_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlcontrast_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlsaturation_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_ovlhue_attribute.attr;
	}
	if(emgd_ovlplane->has_constalpha) {
		emgd_ovlplane_attrs_list[i++] = &emgd_const_alpha_enable_attribute.attr;
		emgd_ovlplane_attrs_list[i++] = &emgd_const_alpha_value_attribute.attr;
	}
	emgd_ovlplane_attrs_list[i++] = NULL;
	emgd_ovlplane_ktype.default_attrs = emgd_ovlplane_attrs_list;

	emgd_ovlplane->sysfs_obj = OS_ZALLOC(sizeof(*emgd_ovlplane->sysfs_obj));
	emgd_ovlplane->sysfs_obj->priv = priv;
	emgd_ovlplane->sysfs_obj->emgd_ovlplane = emgd_ovlplane;

	kobject_init(&emgd_ovlplane->sysfs_obj->kobj, &emgd_ovlplane_ktype);
	ret = kobject_add(&emgd_ovlplane->sysfs_obj->kobj, &emgd_obj->kobj, "PlaneId%d",
			emgd_ovlplane->plane_id);
	if (ret) {
		EMGD_ERROR("Failed to add ovl plane id %d node to sysfs",
				emgd_ovlplane->plane_id);
		kobject_put(&emgd_ovlplane->sysfs_obj->kobj);
		return;
	}
}
/*
 * Remove a sprite / overlay plane node from the sysfs tree
 */
void emgd_sysfs_ovlplane_remove(igd_ovl_plane_t * emgd_ovlplane)
{
	kobject_put(&emgd_ovlplane->sysfs_obj->kobj);
	emgd_ovlplane->sysfs_obj = NULL;

	OS_FREE(emgd_ovlplane_attrs_list);
	emgd_ovlplane_attrs_list = NULL;
	emgd_ovlplane_ktype.default_attrs = NULL;
}

/*
 * emgd_sysfs_addgemsched()
 *
 * Adds a node under /sys/module/emgd/control/i915_scheduler/ during init
 */
void emgd_sysfs_addgemsched(drm_emgd_private_t *priv)
{
	int ret;

	/*Sanity check*/
	if (priv == NULL ){
		EMGD_ERROR("Argument 'priv' to emgd_sysfs_addgem is not sane.");
		return;
	}

	if (!priv->scheduler.gemsched_sysfs_obj) {
	  
		/* 1st  step - create the "i915_scheduler" fs dir node */
		priv->scheduler.gemsched_sysfs_obj = OS_ZALLOC(sizeof(*priv->scheduler.gemsched_sysfs_obj));
		if (priv->scheduler.gemsched_sysfs_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemsched_sysfs_obj");
			return;
		}

		priv->scheduler.gemsched_sysfs_obj->priv = priv;
		kobject_init(&priv->scheduler.gemsched_sysfs_obj->kobj, &emgd_control_ktype);
		ret = kobject_add(&priv->scheduler.gemsched_sysfs_obj->kobj, &emgd_obj->kobj, "i915_scheduler");
		if (ret) {
			EMGD_ERROR("Failed to add i915_scheduler node to sysfs");
			goto free_main_node;
		}
		kobject_uevent(&priv->scheduler.gemsched_sysfs_obj->kobj, KOBJ_ADD);
		
		/* 2nd step - create the "shared" fs dir sub-node */
		priv->scheduler.gemsched_sysfs_shared_obj = OS_ZALLOC(sizeof(*priv->scheduler.gemsched_sysfs_shared_obj));
		if (priv->scheduler.gemsched_sysfs_shared_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemsched_sysfs_shared_obj");
			goto free_main_node;
		}

		priv->scheduler.gemsched_sysfs_shared_obj->priv = priv;
		kobject_init(&priv->scheduler.gemsched_sysfs_shared_obj->kobj, &emgd_gemsched_shared_ktype);
		ret = kobject_add(&priv->scheduler.gemsched_sysfs_shared_obj->kobj, 
				&priv->scheduler.gemsched_sysfs_obj->kobj, "shared");
		if (ret) {
			EMGD_ERROR("Failed to add shared node to i915_scheduler sysfs");
			goto free_shared_node;
		}
		kobject_uevent(&priv->scheduler.gemsched_sysfs_shared_obj->kobj, KOBJ_ADD);

		/* 3rd step - create the "video" fs dir sub-node */
		priv->scheduler.gemsched_sysfs_video_obj = OS_ZALLOC(sizeof(*priv->scheduler.gemsched_sysfs_video_obj));
		if (priv->scheduler.gemsched_sysfs_video_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemsched_sysfs_video_obj");
			goto free_shared_node;
		}

		priv->scheduler.gemsched_sysfs_video_obj->priv = priv;
		kobject_init(&priv->scheduler.gemsched_sysfs_video_obj->kobj, &emgd_gemsched_video_ktype);
		ret = kobject_add(&priv->scheduler.gemsched_sysfs_video_obj->kobj, 
				&priv->scheduler.gemsched_sysfs_obj->kobj, "video");
		if (ret) {
			EMGD_ERROR("Failed to add video node to i915_scheduler sysfs");
			goto free_video_node;
		}
		kobject_uevent(&priv->scheduler.gemsched_sysfs_video_obj->kobj, KOBJ_ADD);

		/* 4th  step - create the "rogue" fs dir sub-node */
		priv->scheduler.gemsched_sysfs_rogue_obj = OS_ZALLOC(sizeof(*priv->scheduler.gemsched_sysfs_rogue_obj));
		if (priv->scheduler.gemsched_sysfs_rogue_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemsched_sysfs_rogue_obj");
			goto free_video_node;
		}

		priv->scheduler.gemsched_sysfs_rogue_obj->priv = priv;
		kobject_init(&priv->scheduler.gemsched_sysfs_rogue_obj->kobj, &emgd_gemsched_rogue_ktype);
		ret = kobject_add(&priv->scheduler.gemsched_sysfs_rogue_obj->kobj, 
				&priv->scheduler.gemsched_sysfs_obj->kobj, "rogue");
		if (ret) {
			EMGD_ERROR("Failed to add rogue node to i915_scheduler sysfs");
			goto free_rogue_node;
		}
		kobject_uevent(&priv->scheduler.gemsched_sysfs_rogue_obj->kobj, KOBJ_ADD);
		
		/* 5th step - create the "process" fs dir sub-node */
		/* note this wont have any attributes but will serve as     *
		 * top level dir for per-process-id 2nd layer dir sub-node  *
		 * that will be created and destroyed on the fly as clients *
		 * open and close private drm handles with our driver       */
		priv->scheduler.gemsched_sysfs_process_obj = OS_ZALLOC(sizeof(*priv->scheduler.gemsched_sysfs_process_obj));
		if (priv->scheduler.gemsched_sysfs_process_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemsched_sysfs_process_obj");
			goto free_rogue_node;
		}

		priv->scheduler.gemsched_sysfs_process_obj->priv = priv;
		kobject_init(&priv->scheduler.gemsched_sysfs_process_obj->kobj, &emgd_control_ktype);
		ret = kobject_add(&priv->scheduler.gemsched_sysfs_process_obj->kobj, 
				&priv->scheduler.gemsched_sysfs_obj->kobj, "process");
		if (ret) {
			EMGD_ERROR("Failed to add process node to i915_scheduler sysfs");
			goto free_process_node;
		}
		kobject_uevent(&priv->scheduler.gemsched_sysfs_process_obj->kobj, KOBJ_ADD);
		
	}
	
	return;
	
free_process_node:
	kobject_put(&priv->scheduler.gemsched_sysfs_process_obj->kobj);
	priv->scheduler.gemsched_sysfs_process_obj = NULL;
free_rogue_node:
	kobject_put(&priv->scheduler.gemsched_sysfs_rogue_obj->kobj);
	priv->scheduler.gemsched_sysfs_rogue_obj = NULL;
free_video_node:
	kobject_put(&priv->scheduler.gemsched_sysfs_video_obj->kobj);
	priv->scheduler.gemsched_sysfs_video_obj = NULL;
free_shared_node:
	kobject_put(&priv->scheduler.gemsched_sysfs_shared_obj->kobj);
	priv->scheduler.gemsched_sysfs_shared_obj = NULL;
free_main_node:
	kobject_put(&priv->scheduler.gemsched_sysfs_obj->kobj);
	priv->scheduler.gemsched_sysfs_obj = NULL;
	
	return;
}

void emgd_sysfs_gemsched_addclient(drm_emgd_private_t * priv, 
			struct drm_i915_file_private * client)
{
	int ret;
	struct task_struct *task;
	char comm[TASK_COMM_LEN] = "Unknown";
	
	/* This is N/A if gem_scheduler is off */
	if (!gem_scheduler)
		return;

	/*Sanity check*/
	if (priv == NULL || client == NULL){
		EMGD_ERROR("Argument 'priv' or 'client' to gemschedb_addclient is not sane.");
		return;
	}
	
	/* This will be a new subfolder under the existing gemsched_sysfs_process_obj
	 * sysfs node (will appear as "/sys/module/emgd/control/i915_scheduler/..
	 * ..process/[proc-ID]/blah"). So lets ensure the parent sysfs node is valid */
	if (priv->scheduler.gemsched_sysfs_process_obj) {
		/* Create this "process" fs dir sub-node based on the PID of this drm_file */
		client->mm.gemsched_sysfs_client_obj = 
			OS_ZALLOC(sizeof(*client->mm.gemsched_sysfs_client_obj));

		if (client->mm.gemsched_sysfs_client_obj == NULL){
			EMGD_ERROR("Failed to allocate memory gemched_sysfs_client_obj");
			return;
		}

		client->mm.gemsched_sysfs_client_obj->priv = priv;
		client->mm.gemsched_sysfs_client_obj->drmfilepriv = client;
		task = pid_task(find_vpid(PID_T_FROM_DRM_FILE(client->drm_file)), PIDTYPE_PID);
		if (task)
			get_task_comm(comm, task);

		kobject_init(&client->mm.gemsched_sysfs_client_obj->kobj, &emgd_gemsched_process_ktype);
		ret = kobject_add(&client->mm.gemsched_sysfs_client_obj->kobj, 
				&priv->scheduler.gemsched_sysfs_process_obj->kobj, "%d-%.16s",
				PID_T_FROM_DRM_FILE(client->drm_file), comm);
		if (ret) {
			EMGD_ERROR("Failed to add client node, PID=%d, to i915_scheduler sysfs", 
				   PID_T_FROM_DRM_FILE(client->drm_file));
			kobject_put(&client->mm.gemsched_sysfs_client_obj->kobj);
			client->mm.gemsched_sysfs_client_obj = NULL;
			return;
		}
		kobject_uevent(&client->mm.gemsched_sysfs_client_obj->kobj, KOBJ_ADD);
	}
}

void emgd_sysfs_gemsched_removeclient(drm_emgd_private_t *priv, 
			struct drm_i915_file_private * client)
{
	/* This is N/A if gem_scheduler is off */
	if (!gem_scheduler)
		return;
	/*Sanity check*/
	if (priv == NULL || client == NULL){
		EMGD_ERROR("Argument 'priv' or 'client' to gemsched_rmclient is not sane.");
		return;
	}
	
	/* We want to delete the existing subfolder under existing gemsched_sysfs_process_obj
	 * sysfs node (appeared as "/sys/module/emgd/control/i915_scheduler/..
	 * ..process/[proc-ID]/blah"). So lets ensure the parent sysfs node is valid */
	if (priv->scheduler.gemsched_sysfs_process_obj) {
		kobject_put(&client->mm.gemsched_sysfs_client_obj->kobj);
		client->mm.gemsched_sysfs_client_obj = NULL;
		return;
	}
}

/*
 * emgd_sysfs_rmgem()
 *
 * Removes the gem node from /sys/module/emgd/control/i915_scheduler/ during cleanup
 */
void emgd_sysfs_rmgemsched(drm_emgd_private_t *priv)
{

	/*Sanity check*/
	if (priv == NULL ){
		EMGD_ERROR("Argument 'priv' to emgd_sysfs_rmgem is not sane.");
		return;
	}
	
	if (priv->scheduler.gemsched_sysfs_process_obj) {
		kobject_put(&priv->scheduler.gemsched_sysfs_process_obj->kobj);
		priv->scheduler.gemsched_sysfs_process_obj = NULL;
	}
	if (priv->scheduler.gemsched_sysfs_rogue_obj) {
		kobject_put(&priv->scheduler.gemsched_sysfs_rogue_obj->kobj);
		priv->scheduler.gemsched_sysfs_rogue_obj = NULL;
	}
	if (priv->scheduler.gemsched_sysfs_video_obj) {
		kobject_put(&priv->scheduler.gemsched_sysfs_video_obj->kobj);
		priv->scheduler.gemsched_sysfs_video_obj = NULL;
	}
	if (priv->scheduler.gemsched_sysfs_shared_obj) {
		kobject_put(&priv->scheduler.gemsched_sysfs_shared_obj->kobj);
		priv->scheduler.gemsched_sysfs_shared_obj = NULL;
	}
	if (priv->scheduler.gemsched_sysfs_obj) {
		kobject_put(&priv->scheduler.gemsched_sysfs_obj->kobj);
		priv->scheduler.gemsched_sysfs_obj = NULL;
	}
}

/*
 * Called during driver load to initialize the sysfs control interface
 */
void emgd_init_sysfs(struct drm_device *dev)
{
	int ret;
	drm_emgd_private_t *priv = dev->dev_private;
	emgd_framebuffer_t *emgdfb;
	int i;

	if (!emgd_create_control(dev)) {
		return;
	}

	if (!emgd_create_info(dev)) {
		return;
	}

	/* Add i915_scheduler control directory to sysfs*/
	if (gem_scheduler) {
		emgd_sysfs_addgemsched(priv);
	}

	/* Add freq control directory to sysfs */
	freq_obj = OS_ZALLOC(sizeof(*freq_obj));
	if (freq_obj == NULL) {
		EMGD_ERROR("Failed to allocate memory for freq_obj.");
		return;
	}
	freq_obj->priv = priv;
	freq_obj->crtc_idx = -1;
	kobject_init(&freq_obj->kobj, &emgd_freq_ktype);
	ret = kobject_add(&freq_obj->kobj, &emgd_obj->kobj, "freq");
	if (ret) {
		EMGD_ERROR("Failed to add sysfs freq directory");
		kobject_put(&freq_obj->kobj);
	} 
	kobject_uevent(&freq_obj->kobj, KOBJ_ADD);
	

	/*
	 * Add a framebuffer directory kobject to the tree.  This will create
	 * a directory /sys/modules/emgd/control/fb.  We'll later populate this
	 * directory with nodes for each DRM framebuffer.
	 */
	fbdir_obj = OS_ZALLOC(sizeof(*fbdir_obj));
	if (fbdir_obj == NULL){
		EMGD_ERROR("Failed to allocate memory for fbdir_obj.");
		return;
	}

	fbdir_obj->priv = priv;
	fbdir_obj->crtc_idx = -1;
	kobject_init(&fbdir_obj->kobj, &emgd_control_ktype);
	ret = kobject_add(&fbdir_obj->kobj, &emgd_obj->kobj, "fb");
	if (ret) {
		EMGD_ERROR("Failed to create framebuffer sysfs directory");
		kobject_put(&fbdir_obj->kobj);
	} else {
		kobject_uevent(&fbdir_obj->kobj, KOBJ_ADD);
	}

	/* Create a sysfs directory for each CRTC */
	for (i = 0; i < priv->num_crtc; i++) {
		crtc_obj[i] = OS_ZALLOC(sizeof(emgd_obj_t));
		if (!crtc_obj[i]) {
			EMGD_ERROR("Failed to allocate sysfs object for CRTC");
			break;
		}
		crtc_obj[i]->priv = priv;
		crtc_obj[i]->crtc_idx = i;
		kobject_init(&crtc_obj[i]->kobj, &emgd_crtc_ktype);
		ret = kobject_add(&crtc_obj[i]->kobj, &emgd_obj->kobj, "crtc%d", i);
		if (ret) {
			EMGD_ERROR("Failed to add sysfs CRTC directory");
			kobject_put(&crtc_obj[i]->kobj);
			continue;
		}
		kobject_uevent(&crtc_obj[i]->kobj, KOBJ_ADD);

		/* Link sysfs node to CRTC structure */
		priv->crtcs[i]->sysfs_obj = crtc_obj[i];

		/* Create a "framebuffer" symlink to the CRTC's framebuffer node */
		if (priv->crtcs[i]->base.fb) {
			emgdfb = container_of(priv->crtcs[i]->base.fb, emgd_framebuffer_t,
				base);

			if (!emgdfb->sysfs_obj) {
				emgd_sysfs_addfb(emgdfb);
				if (emgdfb == &priv->initfb_info) {
					/*
					 * Symbolic link the initial framebuffer node to
					 * "initial"
					 */
					ret = sysfs_create_link(&fbdir_obj->kobj,
							&priv->initfb_info.sysfs_obj->kobj, "initial");
					if (ret) {
						EMGD_ERROR("Failed to create 'initial' framebuffer link"
								" in sysfs");
					}
				}
			}

			if (!emgdfb->sysfs_obj) {
				EMGD_ERROR("Framebuffer for CRTC has no sysfs node");
				return;
			}

			ret = sysfs_create_link(&crtc_obj[i]->kobj,
				&emgdfb->sysfs_obj->kobj, "framebuffer");
			if (ret) {
				EMGD_ERROR("Failed to create 'framebuffer' link for crtc in sysfs");
			}
		}
	}
}


/*
 *  Called by the driver unload to free up the kobject resources.
 */
void emgd_destroy_sysfs(struct drm_device *dev)
{
	drm_emgd_private_t *priv = dev->dev_private;
	emgd_framebuffer_t *emgdfb;
	int i;

	/* Remove framebuffer node for initial framebuffer */
	sysfs_remove_link(&fbdir_obj->kobj, "initial");

	/* Remove crtc directories from sysfs */
	for (i = 0; i < priv->num_crtc; i++) {
		if (crtc_obj[i]) {
			if (priv->crtcs[i]->base.fb) {
				emgdfb = container_of(priv->crtcs[i]->base.fb,
						emgd_framebuffer_t, base);
				if (emgdfb->sysfs_obj) {
					emgd_sysfs_rmfb(emgdfb);
				}
			}

			priv->crtcs[i]->sysfs_obj = NULL;
			kobject_put(&crtc_obj[i]->kobj);
		}
	}

	/* Remove fb directory from sysfs */
	if (fbdir_obj) {
		kobject_put(&fbdir_obj->kobj);
	}

	/* Remove i915_scheduler control directory from sysfs*/
	if (gem_scheduler) {
		emgd_sysfs_rmgemsched(priv);
	}

	/* Remove control directory from sysfs */
	if (emgd_obj) {
		kobject_put(&emgd_obj->kobj);
	}

	/* Remove info directory from sysfs */
	if (info_obj) {
		kobject_put(&info_obj->kobj);
	}

	/* Remove freq directory from sysfs */
	if (freq_obj) {
		kobject_put(&freq_obj->kobj);
	}

}
