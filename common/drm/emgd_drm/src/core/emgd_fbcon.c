/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_fbcon.c
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
 *  Framebuffer Console related functions.  This is the equivalent of
 *  what is in drm_fb_helper.c, a set of functions for configuring the
 *  framebuffer console. 
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.kms

#include <drmP.h>
#include <drm_crtc_helper.h>
#include <linux/vga_switcheroo.h>


#include "default_config.h"
#include "i915/i915_reg.h"
#include "i915/intel_bios.h"
#include "i915/intel_ringbuffer.h"
#include "i915/intel_drv.h"
#include "drm_emgd_private.h"
#include "emgd_drm.h"
#include <igd_core_structs.h>

#include <linux/version.h>
#include <linux/export.h>

#define USE_HELPERS 1 

/*------------------------------------------------------------------------------
 * Global Variables
 *------------------------------------------------------------------------------
 */
extern emgd_drm_config_t *config_drm;
extern int drm_emgd_width;
extern int drm_emgd_height;
extern int drm_emgd_depth;
extern int drm_emgd_bpp;
extern int drm_emgd_display_height;
extern int drm_emgd_display_width;
extern int drm_emgd_refresh;



/*------------------------------------------------------------------------------
 * Formal Declaration
 *------------------------------------------------------------------------------
 */
extern int emgd_framebuffer_init(struct drm_device *dev,
			emgd_framebuffer_t *emgd_fb,
			struct DRM_MODE_FB_CMD_TYPE *mode_cmd,
			struct drm_i915_gem_object *bo);

extern int convert_bpp_depth_to_drm_pixel_formal(unsigned int bpp,
		unsigned int depth);

extern int emgd_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
		struct drm_framebuffer *fb, int x, int y,
		enum mode_set_atomic state);


/* Sets up initial display configuration */
int emgd_fbcon_initial_config(emgd_fbdev_t *emgd_fbdev, int alloc_fb);


/*------------------------------------------------------------------------------
 * These are called by the framebuffer console
 *------------------------------------------------------------------------------
 */
static int  alloc_initial_fb(emgd_fbdev_t *emgd_fbdev);
static void fill_fix(emgd_fbdev_t *emgd_fbdev, struct fb_info *info);
static void fill_var(emgd_fbdev_t *emgd_fbdev, struct fb_info *info);



/*------------------------------------------------------------------------------
 * FBCON Functions
 * These are called by the framebuffer console
 *------------------------------------------------------------------------------
 */
static int emgd_fbcon_setcolreg(unsigned int regno,
			unsigned int red, unsigned int green, unsigned int blue,
			unsigned int transp, struct fb_info *info);
static int emgd_fbcon_check_var(struct fb_var_screeninfo *var,
			struct fb_info *info);
static int emgd_fbcon_set_par(struct fb_info *info);
static int emgd_fbcon_setcmap(struct fb_cmap *cmap, struct fb_info *info);
#if !USE_HELPERS
static int emgd_fbcon_pan_display(struct fb_var_screeninfo *var,
			struct fb_info *info);
static int emgd_fbcon_blank(int blank, struct fb_info *info);
#endif


/* This is called from within FBCON, the framebuffer console */
const struct fb_ops emgd_fb_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var   = emgd_fbcon_check_var,
	.fb_set_par     = emgd_fbcon_set_par,
	.fb_setcolreg   = emgd_fbcon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_setcmap     = emgd_fbcon_setcmap,
#if USE_HELPERS
	.fb_blank       = drm_fb_helper_blank,
	.fb_pan_display = drm_fb_helper_pan_display,
#else
	.fb_blank       = emgd_fbcon_blank,
	.fb_pan_display = emgd_fbcon_pan_display,
#endif
	/* .fb_debug_enter = drm_fb_helper_debug_enter, */
	/* .fb_debug_leave = drm_fb_helper_debug_leave, */
};



static void fill_fix(emgd_fbdev_t *emgd_fbdev, struct fb_info *info)
{
	struct drm_framebuffer *fb = &emgd_fbdev->emgd_fb->base;

	info->fix.type        = FB_TYPE_PACKED_PIXELS;
	info->fix.visual      = FB_VISUAL_TRUECOLOR;
	info->fix.mmio_start  = 0;
	info->fix.mmio_len    = 0;
	info->fix.type_aux    = 0;
	info->fix.xpanstep    = 1; /* doing it in hw */
	info->fix.ypanstep    = 1; /* doing it in hw */
	info->fix.ywrapstep   = 0;
	info->fix.accel       = FB_ACCEL_NONE;
	info->fix.type_aux    = 0;
	info->fix.line_length = fb->DRMFB_PITCH;
}



/**
 * fill_var
 *
 * Fills in the fb_info structure.  This function is called by alloc_init_fb,
 * and as such an actual mode would not have been set yet.  This means we
 * don't really know what "var.xres" and "var.yres" will be, so we have to
 * make the assumption that "config_drm->width" and "config_drm->height"
 * sepcify the resolution for the eventual mode.
 *
 * @param emgd_fbdev [IN]    FB device that contains the relevant information
 * @param info       [INOUT] fb_info structure to fill in
 *
 * @return 0 on success, an error code otherwise
 */
static void fill_var(emgd_fbdev_t *emgd_fbdev, struct fb_info *info)
{
	struct drm_framebuffer *fb = &emgd_fbdev->emgd_fb->base;


	/* Actual resolution for the mode.  We are assuming here that the
	 * mode requested through config_drm->width and config_drm->height can
	 * be set successfully */
	info->var.xres           = drm_emgd_display_width;
	info->var.yres           = drm_emgd_display_height;

	/* Size of the framebuffer */
	info->var.xres_virtual   = fb->width;
	info->var.yres_virtual   = fb->height;

	info->pseudo_palette     = emgd_fbdev->pseudo_palette;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.accel_flags    = FB_ACCELF_TEXT;
	info->var.xoffset        = 0;
	info->var.yoffset        = 0;
	info->var.activate       = FB_ACTIVATE_NOW;
	info->var.height         = -1;
	info->var.width          = -1;

	switch (fb->depth) {
	case 8:
		info->var.red.offset = 0;
		info->var.green.offset = 0;
		info->var.blue.offset = 0;
		info->var.red.length = 8; /* 8bit DAC */
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 15:
		info->var.red.offset = 10;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 5;
		info->var.blue.length = 5;
		info->var.transp.offset = 15;
		info->var.transp.length = 1;
		break;
	case 16:
		info->var.red.offset = 11;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 6;
		info->var.blue.length = 5;
		info->var.transp.offset = 0;
		break;
	case 24:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 24;
		info->var.transp.length = 8;
		break;
	default:
		break;
	}
}


/**
 * Allocate and initialize a DRM/emgd framebuffer resource based on
 * the initial configuration information.
 */
static int allocate_fb(struct drm_device *dev,
		drm_emgd_private_t *priv,
		emgd_framebuffer_t *fb,
		emgd_fb_config_t *fb_info,
		int *size)
{
	int width, height, depth, bpp;
	struct DRM_MODE_FB_CMD_TYPE mode_cmd;
	struct drm_i915_gem_object *obj;
	int ret;

	EMGD_TRACE_ENTER;

	if (fb == NULL) {
		EMGD_ERROR_EXIT("Allocation of framebuffer structure failed");
		return -1;
	}

	/*
	 * The config values may be zero, meaning that we use default values.
	 * The default values will be either values set via the module parameters
	 * or they may be the compiled in defaults.
	 */

	width  = (fb_info->width) ? fb_info->width : drm_emgd_width;
	height = (fb_info->height) ? fb_info->height : drm_emgd_height;
	depth  = (fb_info->depth) ? fb_info->depth : drm_emgd_depth;
	bpp    = (fb_info->bpp) ? fb_info->bpp : drm_emgd_bpp;

	*size = height * ALIGN(width * 4, 64);
	*size = ALIGN(*size, PAGE_SIZE); /* GEM demands all be page aligned */

	obj  = i915_gem_alloc_object(dev, *size);
	if (!obj) {
		EMGD_ERROR("Failed to allocate framebuffer");
		EMGD_TRACE_EXIT;
		return -1;
	}

	/*
	 * Create our initial console framebuffer using the values below.
	 */
	switch (bpp) {
		case  8: fb->igd_pixel_format = IGD_PF_ARGB8_INDEXED; break;
		case 16: fb->igd_pixel_format = IGD_PF_RGB16_565; break;
		case 24: fb->igd_pixel_format = IGD_PF_RGB24; break;
		case 32: fb->igd_pixel_format = IGD_PF_ARGB32; break;
		default: fb->igd_pixel_format = IGD_PF_ARGB32; break;
	}

	/* Initialize emgd_framebuffer_t */
	mode_cmd.DRMMODE_HANDLE = 0;
	/* OTC's driver makes this alignment, assume the same works here */
	mode_cmd.DRMFB_PITCH  = ALIGN(width * 4, 64);
	mode_cmd.width  = width;
	mode_cmd.height = height;

	mode_cmd.pixel_format = convert_bpp_depth_to_drm_pixel_formal(bpp, depth);


	ret = emgd_framebuffer_init(dev, fb, &mode_cmd, obj);
	if (ret) {
		EMGD_ERROR("Cant emgd_framebuffer_init!");
		EMGD_TRACE_EXIT;
		return -1;
	}

	EMGD_TRACE_EXIT;
	return 0;
}


/**
 * alloc_initial_fb
 *
 * This function creates a frame buffer using the config_drm->width and
 * config_drm->height.  The working assumption is this function will only be
 * called once at initialization time.  So the buffer allocated here is
 * for the console.
 *
 * @param emgd_fbdev [IN] Framebuffer device to allocate a buffer for.
 *
 * @return 0 on success, an error code otherwise
 */
static int alloc_initial_fb(emgd_fbdev_t *emgd_fbdev)
{
	struct fb_info      *info     = NULL;
	struct drm_device   *dev      = emgd_fbdev->priv->dev;
	struct device       *device   = &dev->pdev->dev;
	struct drm_fb_helper *fb_helper = &emgd_fbdev->helper;
	drm_emgd_private_t  *priv     = emgd_fbdev->priv;
	emgd_crtc_t         *emgd_crtc= NULL;
	igd_display_port_t  *port     = NULL;
	igd_context_t       *context;
	int                 size      = 0;
	int                 ret       = -EINVAL;
	int                 use_default_fb_res, i, fb_id;
	u32                 alignment = 0;

	EMGD_TRACE_ENTER;


	context = priv->context;

	/* Initialization.  Some of these are required for clean up code to
	 * work properly */
	memset(&priv->initfb_info, 0, sizeof(priv->initfb_info));
	memset(&priv->initfb_info2, 0, sizeof(priv->initfb_info2));
	emgd_fbdev->emgd_fb = NULL;

	/* Figure out what the size of the frame buffer will be.
	 *  - If the config file specifies a size, use that.
	 *  - If there's a native DTD, use the DTD active size.
	 *  - Else, use first EDID timing active size.
	 *  - Else, use compiled default.
	 */
	if((drm_emgd_width == -1) || (drm_emgd_height == -1)){
		/* It's not defined in the default_config.c, figure out timing size */
		use_default_fb_res = 0;

		/* Should this check if the crtc has a display width/height? */

		/* lets use the width and height from the native DTD of the primary
		 * display (old school EMGD ? the initial FB splash in clone still
		 * follows primary)?
		 */
		context->module_dispatch.dsp_get_display_crtc(0, &emgd_crtc);
		if (!emgd_crtc) {
			use_default_fb_res = 1;
		} else {
			context->module_dispatch.dsp_get_display_port(
					emgd_crtc->ports[0], &port, 0);
			/* cant use the 'PORT' macro at this point because upper DRM hasnt
			 * actually called us for a set mode operation just yet, which
			 * means no emgd_encoder is connected to this emgd_crtc yet.
			 * Is this a bug?!
			 */
			if(!port){
				use_default_fb_res = 1;
			} else {
				/* search for native dtd first - this would be user_dtd */
				if(port->fp_native_dtd){
					drm_emgd_width  = port->fp_native_dtd->width;
					drm_emgd_height = port->fp_native_dtd->height;
				} else {
					/* if not, search for edid dtd next */
					if(port->edid && port->edid->num_timings){
						for(i=0; i < port->edid->num_timings; ++i){
							if(config_drm->crtcs[0].display_width < port->edid->timings[i].width){
								drm_emgd_width  = port->edid->timings[i].width;
								drm_emgd_height = port->edid->timings[i].height;
							}
						}
					} else {
						use_default_fb_res = 1;
					}
				}
			}
		}

		if(use_default_fb_res){
			/*okay - we'll go for built in defaults */
			drm_emgd_width  = CONFIG_DEFAULT_WIDTH;
			drm_emgd_height = CONFIG_DEFAULT_HEIGHT;
		}
	}

	/*
	 * Make sure we have display width and height specified. If not
	 * default to the framebuffer width and height.
	 */
	if (drm_emgd_display_width == -1) {
		drm_emgd_display_width = drm_emgd_width;
	}
	if (drm_emgd_display_height == -1) {
		drm_emgd_display_height = drm_emgd_height;
	}

	mutex_lock(&dev->struct_mutex);	

	/* The initial frame buffer is the one attached to crtc 0 */
	fb_id = config_drm->crtcs[0].framebuffer_id;
	ret = allocate_fb(dev, priv, &priv->initfb_info,
			&config_drm->fbs[fb_id], &size);
	if (ret) {
		mutex_unlock(&dev->struct_mutex);	
		EMGD_ERROR("Cant allocate_fb for initial fb config! - bailing!");
		return -ENOMEM;
	}
	emgd_fbdev->emgd_fb = &priv->initfb_info;
	
	switch (priv->initfb_info.bo->tiling_mode) {
		default:
			EMGD_DEBUG("Invalid tiling mode specified! - assuming NONE");
		case I915_TILING_NONE:
			alignment = 4 * 1024;
			break;
		case I915_TILING_X:
			/* pin() will align the object as required by fence */
			alignment = 0;
			break;
		case I915_TILING_Y:
			EMGD_ERROR("Y tiled not allowed for scan out buffers! - continuing");
			break;
        }

	priv->mm.interruptible = false;
	ret = i915_gem_object_pin_to_display_plane(priv->initfb_info.bo, alignment, false);
	priv->mm.interruptible = true;
	if(ret) {
		mutex_unlock(&dev->struct_mutex);	
		EMGD_ERROR("Cant pin our first-initial-fb to display! - bailing!");
		drm_framebuffer_cleanup(&emgd_fbdev->emgd_fb->base);
		emgd_fbdev->emgd_fb = NULL;
		return -EBUSY;
	}

	/* Allocate fb_info
	 * OTC i915 uses function framebuffer_alloc() for (struct fb_info) allocation.
	 * But it is a bit slower than expected and actually this function do only
	 * memory allocation and ... assign correct pointer for fb_info->device.
	 * Field fb_info->device is important because it is used in
	 * register_framebuffer() function to check is pci_dev primary or not and
	 * to remove generic VGA frame buffer console after that */
	/*info = framebuffer_alloc(0, device);*/
	info = kzalloc(sizeof(struct fb_info), GFP_KERNEL);
	if (NULL == info) {
		EMGD_ERROR_EXIT("Allocation of fb_info failed");
		ret = -ENOMEM;
		goto err_unpin;
	}
	/*to assign right pointer*/
	info->device = device;

	info->par   = emgd_fbdev; /* Private data for all FBCON functions  */
	info->flags = FBINFO_DEFAULT; /* | FBINFO_CAN_FORCE_OUTPUT */
	info->fbops = (struct fb_ops*) &emgd_fb_ops;
	strcpy(info->fix.id, "emgdfb"); /* fix.id is 16 bytes long */

	priv->fbdev = info;

	EMGD_DEBUG("EMGD: Call fb_alloc_cmap()");
	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		EMGD_ERROR("%s: Can't allocate color map", info->fix.id);
		ret = -ENOMEM;
		goto err_unpin;
	}

	/*
	 * Does this need to be filled in and if so, with what?  Right now
	 * I'm trying to reuse the framebuffer that was already configured by
	 * the EMGD code.
	 *
	 * setup aperture base/size for vesafb takeover
	 */
	info->apertures = alloc_apertures(1);
	if (!info->apertures) {
		EMGD_ERROR("%s: Can't allocate apertures", info->fix.id);
		ret = -ENOMEM;
		goto err_unpin;
	}

	/* Here should be physical address instead of virtual
	 * Address will be used in register_framebuffer()
	 * to check is it required to remove generic VGA frame buffer or not*/
	info->apertures->ranges[0].base = priv->gtt.mappable_base;
	info->apertures->ranges[0].size =
		context->device_context.gatt_pages << PAGE_SHIFT;


	/*
	 * smem_start is the start of frame buffer mem (physical address), does
	 * that mean GTT or that it expects a physical contigous block in real
	 * memory?
	 *
	 * screen_base is a virtual address
	 */

	/* Set up framebuffer surface */
	EMGD_DEBUG("EMGD: Call pci_resource_start()");
	info->fix.smem_start = pci_resource_start(dev->pdev, 2) +
		i915_gem_obj_ggtt_offset(priv->initfb_info.bo);
	info->fix.smem_len   = size;

	/* Get kernel virtual memory address of framebuffer */
	EMGD_DEBUG("EMGD: Call mapping framebuffer");
	/*
	 * This needs to get a CPU virtual address of the frame buffer memory.
	 * There are two ways to do this.
	 *
	 * 1) Use vmap to map the page list.
	 * 2) Use ioremap_wc to create the mapping.
	 *
	 * Both seem to work.  Currently using the ioremap_wc() call to do
	 * the mapping.  The vmap call that also works is:
	 *
	 *	info->screen_base =
	 *		vmap(obj->pages, (size / PAGE_SIZE), VM_MAP, PAGE_KERNEL_UC_MINUS);
	*/

	/* info->screen_base =
		ioremap_wc(context->device_context.fb_adr + priv->initfb_info.bo->gtt_offset, size); */
	info->screen_base =
		ioremap_wc(priv->gtt.mappable_base + i915_gem_obj_ggtt_offset(priv->initfb_info.bo),
				size);

	if (!info->screen_base) {
		EMGD_ERROR("%s: Can't map framebuffer surface", info->fix.id);
		ret = -ENOSPC;
		goto err_unpin;
	}

	info->screen_size         = size;
	info->pixmap.size         = 64 * 1024;
	info->pixmap.buf_align    = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags        = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align   = 1;

	/* Initialize info->fix and info->var */
	fill_fix(emgd_fbdev, info);
	fill_var(emgd_fbdev, info);

	EMGD_DEBUG("Frame buffer %dx%d @ 0x%08lx",
			priv->initfb_info.base.width, priv->initfb_info.base.height,
			i915_gem_obj_ggtt_offset(priv->initfb_info.bo));

	mutex_unlock(&dev->struct_mutex);

	EMGD_DEBUG("EMGD: Call vga_switcheroo_client_fb_set()");
	vga_switcheroo_client_fb_set(dev->pdev, info);

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)
		fb_helper->fbdev = info;

	EMGD_TRACE_EXIT;
	return 0;

err_unpin:
	intel_unpin_fb_obj(priv->initfb_info.bo);
	mutex_unlock(&dev->struct_mutex);
	
	if (NULL != emgd_fbdev->emgd_fb) {
		drm_framebuffer_cleanup(&emgd_fbdev->emgd_fb->base);
		emgd_fbdev->emgd_fb = NULL;
	}
	memset(&priv->initfb_info, 0, sizeof(priv->initfb_info));
	if (NULL != info) {
		kfree(info);
		info = NULL;
	}

	return ret;
}



/**
 * emgd_fbcon_setcmap
 *
 * Sets color map for the framebuffer console device.  For now, we will set
 * both CRTC to the same color map, regardless of which display configuration
 * we are in.  There may be a case in the future where we will have to set
 * the color map for both CRTCs differently.
 *
 * We will also assume that we are dealing with FB_VISUAL_TRUECOLOR because
 * our alloc_initial_fb() function will only allocate framebuffer of this
 * type.
 *
 * @param cmap [IN] Input color map
 * @param info [IN] framebuffer to set
 *
 * @return 0 on success
 */
static int emgd_fbcon_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	emgd_fbdev_t      *emgd_fbdev = info->par;
	struct drm_device *dev        = emgd_fbdev->priv->dev;
	int                ret = 0;
	u16               *red, *green, *blue, *transp;
	u16                hred, hgreen, hblue, htransp;
	u32                new_value, mask;
	int                i, start_index;
	struct drm_crtc   *crtc;


	EMGD_TRACE_ENTER;
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
       drm_modeset_lock_all(dev);
#endif

	/* Set all the CRTCs to the same color map */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		red         = cmap->red;
		green       = cmap->green;
		blue        = cmap->blue;
		transp      = cmap->transp;
		start_index = cmap->start;

		for(i = 0; i < cmap->len; i++) {
			htransp = 0xffff;
			hred    = *red++;
			hgreen  = *green++;
			hblue   = *blue++;

			if (transp) {
				htransp = *transp++;
			}

			/* The palette only has 17 entries */
			if ((start_index - cmap->start) > 16) {
				ret = -EINVAL;
				break;
			}

			hred   >>= (16 - info->var.red.length);
			hgreen >>= (16 - info->var.green.length);
			hblue  >>= (16 - info->var.blue.length);

			new_value = (hred   << info->var.red.offset)   |
						(hgreen << info->var.green.offset) |
						(hblue  << info->var.blue.offset);

			if (info->var.transp.length > 0) {
				mask = (1 << info->var.transp.length) - 1;
				mask <<= info->var.transp.offset;
				new_value |= mask;
			}

			((u32 *) info->pseudo_palette)[start_index] = new_value;

			start_index++;
		}

		if (ret) {
			EMGD_ERROR("Invalid parameter.");
			goto out;
		}

		((struct drm_crtc_helper_funcs *)crtc->helper_private)->load_lut(crtc);
	}


	EMGD_TRACE_EXIT;
out:
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
       drm_modeset_unlock_all(dev);
#endif
	return ret;
}



/*
 * Currently, we are using the device indpendent DRM functions
 * for these.  If we need to do special processing, then uncomment
 * these functions and hook them into the function table.
 */
static int emgd_fbcon_check_var(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	int ret = 0;

	EMGD_TRACE_ENTER;



	EMGD_TRACE_EXIT;

	return ret;
}



static int emgd_fbcon_set_par(struct fb_info *info)
{
	int ret = 0;
	struct drm_fb_helper *fb_helper;
	struct drm_device *dev;
	struct drm_crtc *crtc;
	emgd_fbdev_t *emgd_fbdev;
	emgd_framebuffer_t *emgdfb;
	emgd_crtc_t *emgd_crtc;
	drm_emgd_private_t *priv;
	int i;

	EMGD_TRACE_ENTER;

	emgd_fbdev = info->par;
	priv = emgd_fbdev->priv;
	fb_helper = &emgd_fbdev->helper;
	dev = fb_helper->dev;

	mutex_lock(&dev->mode_config.mutex);
	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		emgdfb = PLANE(emgd_crtc)->fb_info;

		/*
		 * Since this will get called even on the initial framebuffer
		 * console initialization, we want to make sure this is only done
		 * when a VT switch happens.  If the current framebuffer is
		 * different from the console framebuffer, then this is a VT
		 * switch.
		 */
		if (emgdfb && (emgd_fbdev->emgd_fb->base.base.id != emgdfb->base.base.id)) {

			ret = emgd_fbcon_initial_config(priv->emgd_fbdev, 0);
			if (ret)
				EMGD_DEBUG("Failed to restore CRTC mode for fb console");

		}
	}
	mutex_unlock(&dev->mode_config.mutex);

	EMGD_TRACE_EXIT;

	return ret;
}



static int emgd_fbcon_setcolreg(unsigned int regno,
		unsigned int red, unsigned int green, unsigned int blue,
		unsigned int transp, struct fb_info *info)
{
	EMGD_TRACE_ENTER;

	EMGD_DEBUG("STUBED emgd_fbcon_setcolreg");

	EMGD_TRACE_EXIT;
	return 0;
}


/*
 * TODO: i915 driver use drm helper to do pan_display.
 * Maybe we dont need it after all. 
 * intel_fb.c:	.fb_pan_display = drm_fb_helper_pan_display,
 */
#if !USE_HELPERS
static int emgd_fbcon_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	return 0;
}
#endif

/*
 * Currently, we are using the device indpendent DRM functions
 * for these.  If we need to do special processing, then uncomment
 * these functions and hook them into the function table.
 */
#if !USE_HELPERS
static int emgd_fbcon_blank(int blank, struct fb_info *info)
{
	int ret = 0;
	struct drm_device *dev;
	struct drm_crtc *crtc = NULL;
	struct drm_encoder *encoder = NULL;
	struct drm_encoder_helper_funcs *encoder_funcs;
	emgd_fbdev_t *emgd_fbdev;

	EMGD_TRACE_ENTER;

	emgd_fbdev = info->par;
	dev	       = emgd_fbdev->priv->dev;

	switch(blank) {
	case FB_BLANK_UNBLANK:
		EMGD_DEBUG("Turn on Display");

		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {

			((struct drm_crtc_helper_funcs *)crtc->helper_private)->dpms(crtc,
				DRM_MODE_DPMS_ON);

			list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
				if(encoder->crtc == crtc) {
					encoder_funcs = encoder->helper_private;
					encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
				}
			}
		}
		break;
	case FB_BLANK_NORMAL:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		EMGD_DEBUG("Turn off Display");

		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
				if(encoder->crtc == crtc) {
					encoder_funcs = encoder->helper_private;
					encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
				}
			}

			((struct drm_crtc_helper_funcs *)crtc->helper_private)->dpms(crtc,
				DRM_MODE_DPMS_OFF);
		}
		break;
	default:
		EMGD_DEBUG("ERROR: Incorrect FB_BLANK value passed");
		break;
	}

	EMGD_TRACE_EXIT;

	return ret;
}
#endif


/*
 * emgd_fbcon_initial_config
 *
 * Uses configurations in config_drm to set the start-up configuration.
 * We will not support configurations that require more than one framebuffer
 * at DRM boot time.  The fallback configuration is SINGLE.
 *
 * @param emgd_fbdev (IN) Framebuffer device to initialize
 * @param alloc_fb   (IN) Allocate the initial framebuffer?
 *
 * @return 0 on success, an error code otherwise
 */
int emgd_fbcon_initial_config(emgd_fbdev_t *emgd_fbdev, int alloc_fb)
{
	int                   err;
	igd_context_t         *context;
	emgd_crtc_t           *emgd_crtc;
	emgd_encoder_t        *emgd_encoder;
	struct drm_crtc       *crtc         = NULL;
	struct drm_encoder    *encoder      = NULL;
	bool                   mode_set_ret = FALSE;
	struct drm_device     *dev          = emgd_fbdev->priv->dev;
	drm_emgd_private_t    *priv         = emgd_fbdev->priv;
	struct drm_display_mode mode, new_mode;
	int hdisplay = -1;
	int vdisplay = -1;
	int adjusted_hdisplay = -1;
	int adjusted_vdisplay = -1;
	bool get_mode_initial_fb = true;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_crtc_helper_funcs *crtc_funcs = NULL;
	bool                   new_mode_valid = false;
	struct drm_framebuffer *oldfb = NULL;
	struct drm_connector *connector;
	int i;
	int x = 0, y = 0;
	int size;
	int ret;
	unsigned long *dc;
	unsigned short port_pri = 0;
	unsigned short port_sec = 0;

	EMGD_TRACE_ENTER;

	context = ((drm_emgd_private_t *)dev->dev_private)->context;

	if (alloc_fb) {
		/* Allocate the initial framebuffer based on default_config.c settings */
		err = alloc_initial_fb(emgd_fbdev);
		if (err) {
			/*
			 * We failed to initial the initial framebuffer, so we're in
			 * trouble.  As a fallback, lets try to just use the standard
			 * DRM helper setup (which ignores default_config.c) to try to
			 * get *something* working.  If that fails too, we're so
			 * broken that we just need to bail out.
			 */
			EMGD_ERROR("Failed to create initial FB with default_config settings. "
				"(err=%d)", err);
			EMGD_ERROR("Falling back to DRM helper defaults.");
			err = drm_fb_helper_initial_config(&emgd_fbdev->helper, 32);
			if (err) {
				/* Hopeless.  Just give up. */
				return err;
			}
		}
	}

	/*
	 * Do some error checking here.  To continue past this point the
	 * display and frame buffer information needs to be available.
	 * Make sure it is.
	 */


	if (drm_emgd_display_width == -1) {
		drm_emgd_display_width =
			(drm_emgd_width) ? drm_emgd_width : CONFIG_DEFAULT_WIDTH;
	}
	if (drm_emgd_display_height == -1) {
		drm_emgd_display_height =
			(drm_emgd_height) ? drm_emgd_height : CONFIG_DEFAULT_HEIGHT;
	}

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct drm_connector_helper_funcs *connector_funcs =
			connector->helper_private;

		printk(KERN_INFO  "Connector %d is %s, encoder 0x%8lx\n",
				connector->base.id, drm_get_connector_name(connector), 
				(unsigned long) connector->encoder);
		if (!connector->encoder) {
			connector->encoder = connector_funcs->best_encoder(connector);
		}
	}

	if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
		/*
		 * Query the DC in concsole to check whether there is any
		 * secondary display which might changed via hotplug.
		 */
		err = context->drv_dispatch.query_dc(context, IGD_DISPLAY_CONFIG_EXTENDED, &dc, IGD_QUERY_DC_INIT);
		if (err == 0) {
			port_pri = IGD_DC_PRIMARY(*dc);
			port_sec = IGD_DC_SECONDARY(*dc);
		} else {
			/*
			 * If it is not extended display then query the
			 * DC to update the primary display as the primary
			 * port might changed via hotplug.
			 */
			err = context->drv_dispatch.query_dc(context, IGD_DISPLAY_CONFIG_SINGLE, &dc, IGD_QUERY_DC_INIT);
			if (err == 0) {
				port_pri = IGD_DC_PRIMARY(*dc);
			} else {
				/*
				 * In this case the driver will do no modeset as
				 * the port attached to crtc will be 0.
				 */
				EMGD_DEBUG("No Port Attached");
			}
		}
	}

	/* Attach the frame buffer to the CRTC(s)  */
	EMGD_DEBUG("Start attaching FB with all CRTCs");
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if(crtc != NULL) {
			emgd_crtc = container_of(crtc, emgd_crtc_t, base);

			/* Capture currently attach fb so we can unpin it */
			oldfb = crtc->fb;
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
			if (oldfb)
				drm_framebuffer_unreference(oldfb);
#endif

			crtc->fb = &emgd_fbdev->emgd_fb->base;
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
			if (crtc->fb)
				drm_framebuffer_reference(crtc->fb);
#endif
			if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)
				emgd_fbdev->helper.fb = crtc->fb;

			/* Set the basic parameters to look up the mode */
			memset(&new_mode, 0, sizeof(new_mode));
			memset(&mode, 0, sizeof(mode));

			if (emgd_crtc->pipe == 0) {
				if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)
					emgd_crtc->ports[0] = port_pri;
				EMGD_DEBUG("Setup mode for primary crtc PORT %d", emgd_crtc->ports[0]);

				mode.crtc_hdisplay = drm_emgd_display_width;
				mode.crtc_vdisplay = drm_emgd_display_height;
				mode.vrefresh      = drm_emgd_refresh;
				x                  = config_drm->crtcs[0].x_offset;
				y                  = config_drm->crtcs[0].y_offset;
			} else {
				if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)
					emgd_crtc->ports[0] = port_sec;
				EMGD_DEBUG("Setup mode for secondary crtc PORT %d", emgd_crtc->ports[0]);

				mode.crtc_hdisplay = (config_drm->crtcs[1].display_width) ?
					config_drm->crtcs[1].display_width : drm_emgd_display_width;
				mode.crtc_vdisplay = (config_drm->crtcs[1].display_height) ?
					config_drm->crtcs[1].display_height : drm_emgd_display_height;
				mode.vrefresh      = (config_drm->crtcs[1].refresh) ?
					config_drm->crtcs[1].refresh : drm_emgd_refresh;
				x                  = config_drm->crtcs[1].x_offset;
				y                  = config_drm->crtcs[1].y_offset;

				/*
				 * Above allocates a frame buffer for the "primary" display. This
				 * will be used by the fb console.
				 *
				 * What if the configuration specifies a different frame buffer
				 * for the "secondary" display(s)?  Those should be allocated
				 * here.
				 */
				if ((config_drm->crtcs[1].framebuffer_id !=
						config_drm->crtcs[0].framebuffer_id)) {
					if (alloc_fb) {
						EMGD_DEBUG("Explicit request for 2nd different framebuffer id by user");

						emgd_crtc->fbdev_fb = &priv->initfb_info2;
						ret = allocate_fb(dev, priv, emgd_crtc->fbdev_fb,
							&config_drm->fbs[config_drm->crtcs[1].framebuffer_id],
							&size);
						if (ret) {
							EMGD_ERROR("2nd framebuffer failed to allocate - reusing primary");
							/* Lets not bail - lets just use the original main primary 
							 * FB that the primary CRTC and fbdev console are using */
							/* Nothing to do since this line was done above as fallback */
								/* --> crtc->fb = &emgd_fbdev->emgd_fb->base; */
						} else  {
							crtc->fb = &emgd_crtc->fbdev_fb->base;
							if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG)
								emgd_fbdev->helper.fb = crtc->fb;
						}

						EMGD_DEBUG("Allocated the 2nd framebuffer and assinged into crtc");
					} else {
						/* Look at the top of this loop, all CRTCs gets assigned to
						 * main fbdev and during initial fbdev startup, VT switches
						 * and after we end X / weston, we end up with the 2nd CRTC pointing
						 * to the same FB as the 1st CRTC - i.e. the one we registered with
						 * fbdev. But should we consider ever having that 2nd CRTC with its
						 * own seperate splash screen statically stuck in embedded cases???
						 * If there is a need - then we just need to uncomment below - but
						 * that 2nd CRTC will now never show the console like the first one.
						 * NOTE: I dont think splash screent func fills up 2nd FB now.
						 */
						/* --->   crtc->fb = &priv->initfb_info2.base; <--- */
					}
				}
			}

			if (!crtc->fb)
				return -ENOMEM;
			if(mode.crtc_hdisplay <= crtc->fb->width) { /* CRTC <= FB */
                                if(mode.crtc_hdisplay + x > crtc->fb->width) { /* Check if offset creates FB out of bounds */
                                        x = crtc->fb->width - mode.crtc_hdisplay; /* Truncate offset to max allowable */
                                        EMGD_DEBUG("CRTC x-offset is out of bounds and will be set to max %d\n", x);
                                }
                        }
                        else { /* CRTC > FB */
                                if(x > crtc->fb->width - 8) { /* Offset has to have at least 8 pixel width of FB displayed */
                                        x = crtc->fb->width - 8; /* Truncate offset to max allowable */
                                        EMGD_DEBUG("CRTC offset for x is out of bounds and will be set to max %d\n", x);
                                }
                        }

                        if(mode.crtc_vdisplay <= crtc->fb->height) { /* Only check for offset out of bounds when CRTC <= FB */
                                if(mode.crtc_vdisplay + y > crtc->fb->height) { /* Check if offset creates FB out of bounds */
                                        y = crtc->fb->height - mode.crtc_vdisplay; /* Truncate offset to max allowable */
                                        EMGD_DEBUG("CRTC y offset is out of bounds and will be set to %d\n", y);
                                }
                        }
                        else { /* CRTC > FB */
                                if(y > crtc->fb->height - 8) { /* Offset has to have at least 8 lines of FB displayed */
                                        y = crtc->fb->height - 8; /* Truncate offset to max allowable */
                                        EMGD_DEBUG("CRTC offset for y is out of bounds and will be set to max %d\n", y);
                                }
                        }

			if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
				/*
				 * This is to break the kms link at the begninning
				 * to ensure that we have a clean state when performing
				 * hotplug
				 */

				list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
					connector->encoder->crtc = NULL;
				}

				list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
					encoder->crtc = NULL;
				}

			}
			/* Attach encoders to the proper crtc */
			list_for_each_entry(encoder, &dev->mode_config.encoder_list,
					head) {
				if(encoder != NULL) {
					emgd_encoder = container_of(encoder, emgd_encoder_t, base);

					/*
					 * DSP Alloc used the configuration information, included
					 * the DC to do the inital assigment of encoders to each
					 * CRTC. Look at that assignment and make sure the encoder
					 * points back to the CRTC.
					 */
					for (i = 0; i < IGD_MAX_PORTS; i++) {
					
						if (emgd_crtc->ports[i] ==
								emgd_encoder->igd_port->port_number) {
							emgd_encoder->base.crtc = crtc;

							EMGD_DEBUG("Found encoder %u for primary CRTC",
								emgd_encoder->igd_port->port_number);

							encoder_funcs = encoder->helper_private;
							if (!(err = encoder_funcs->mode_fixup(encoder, &mode,
									&new_mode))) {
								new_mode_valid = false;
							} else {
								new_mode_valid = true;
								new_mode.hdisplay     = new_mode.crtc_hdisplay;
								new_mode.htotal       = new_mode.crtc_htotal;
								new_mode.hsync_start  = new_mode.crtc_hsync_start;
								new_mode.hsync_end    = new_mode.crtc_hsync_end;
								new_mode.vdisplay     = new_mode.crtc_vdisplay;
								new_mode.vtotal       = new_mode.crtc_vtotal;
								new_mode.vsync_start  = new_mode.crtc_vsync_start;
								new_mode.vsync_end    = new_mode.crtc_vsync_end;
							}
						}
					}
				}
			}	

			if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
				list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
					connector->status =
						connector->funcs->detect(connector, false);
				}
			}

			/*
			 * We can't check for crtc_in_use until after we've hooked up the
			 * encoders above.  Ideally, we'd want to do all of this checking
			 * earlier, but that means re-factoring the whole function.
			 */
			if (config_drm->hal_params->display_flags & IGD_DISPLAY_HOTPLUG) {
				crtc->enabled = drm_helper_crtc_in_use(crtc);
				if (!crtc->enabled) {
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
					drm_framebuffer_unreference(crtc->fb);
#endif
					crtc->fb = NULL;
					goto alloc_fb;
				}
			}

			if (!new_mode_valid) {
				crtc_funcs = crtc->helper_private;
				if(!crtc_funcs->mode_fixup(crtc, &mode, &new_mode)) {
					memcpy(&new_mode, &mode, sizeof(mode));
				}
			}

			/*
			 * Some of the above configuration may be duplicated in the
			 * drm_crtc_helper_set_mode call.  It may be possible to optimize
			 * this a bit by not using the helper function.
			 */
			mode_set_ret = drm_crtc_helper_set_mode(crtc, &new_mode, x, y, oldfb);
			if (mode_set_ret == FALSE) {
				EMGD_ERROR("Failed to set mode on CRTC.");
			}

			/* save width/height of 1st crtc, to init fb console */
			if (get_mode_initial_fb) {
				get_mode_initial_fb = false;
				hdisplay = mode.crtc_hdisplay;
				vdisplay = mode.crtc_vdisplay;

				/* if  we cannot get valid width/height, try to fall back to pipe timing. 
				 * when no timing matched, assume there is a fall safe timing in pipe.
				*/
				if (!((new_mode.crtc_hdisplay > 0) && (new_mode.crtc_vdisplay > 0))) {
					igd_timing_info_t * pipe_timing = PIPE(emgd_crtc)->timing;
					EMGD_ERROR("No valid timing, fall back to pipe timing.");

					if (pipe_timing) {
						if ((pipe_timing->width > 0) && (pipe_timing->height > 0)) {
							adjusted_hdisplay = pipe_timing->width;
							adjusted_vdisplay = pipe_timing->height;
						}
					} else {
						EMGD_ERROR("Pipe timing fall back failed.");
						/* Try to go for built in defaults, instead leave it initialized -1. */
						adjusted_hdisplay = CONFIG_DEFAULT_WIDTH;
						adjusted_vdisplay = CONFIG_DEFAULT_HEIGHT;
					}
				} else {
					adjusted_hdisplay = new_mode.crtc_hdisplay;
					adjusted_vdisplay = new_mode.crtc_vdisplay;
				}
			}
		}
	}
	EMGD_DEBUG("DONE Attaching FB with all CRTCs");

	/* When doing centering, make sure the value var.xres and var.yres have to be
         * the same as the newly updated crtc before passing into register_framebuffer()*/
	/* Pass info of first crtc to register_framebuffer(), fb console use width/height of first crtc.
	   when there is more than 1 crtc, need to save timing for 1st crtc by hdisplay, vdisplay,
	   adjusted_hdisplay, and adjusted_vdisplay */
        if(drm_emgd_display_width > drm_emgd_width || drm_emgd_display_height > drm_emgd_height){  /*Check for centering*/
			if (hdisplay != adjusted_hdisplay || vdisplay != adjusted_vdisplay) {
	                    emgd_fbdev->priv->fbdev->var.xres = adjusted_hdisplay;
	                    emgd_fbdev->priv->fbdev->var.yres = adjusted_vdisplay;
			}
        }

alloc_fb:
	if (alloc_fb) {
		err = register_framebuffer(emgd_fbdev->priv->fbdev);

		EMGD_DEBUG("fb%d: %s framebuffer device",
			emgd_fbdev->priv->fbdev->node,
			emgd_fbdev->priv->fbdev->fix.id);
	}

	EMGD_TRACE_EXIT;
	return 0;
}

