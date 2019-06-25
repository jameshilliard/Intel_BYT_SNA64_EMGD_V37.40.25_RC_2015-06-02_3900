/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd_drv.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2015, Intel Corporation.
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
 *  The main part of the kernel module.  This part gets everything going and
 *  connected, and then the rest can function.
 *-----------------------------------------------------------------------------
 */

#define MODULE_NAME hal.drm

#include <linux/firmware.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <drm/drm_pciids.h>
#include <drm/drmP.h>
#include <linux/vga_switcheroo.h>
#include <linux/vgaarb.h>
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
#include <igd_core_structs.h>
#include "splash_screen.h"
#include "splash_video.h"
#include "intelpci.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/export.h>

#include <linux/backlight.h>
#include "kernel-compat.h"
/******************************************************************************
 * Formal Declarations
 *****************************************************************************/

/* GEM Declarations */
void i915_gem_load(struct drm_device *dev);

void i915_emgd_write_hws_pga(struct drm_device *dev);

int i915_emgd_init_phys_hws(struct drm_device *dev);


/* KMS Declarations */
extern void emgd_modeset_init(struct drm_device *dev, igd_param_t *params);
extern void emgd_modeset_destroy(struct drm_device *dev);
extern int emgd_fbdev_init(drm_emgd_private_t *priv);
int emgd_fbcon_initial_config(emgd_fbdev_t *emgd_fbdev, int alloc_fb);

extern int crtc_pageflip_handler(struct drm_device *dev, int crtcnum);
extern int sprite_pageflip_handler(struct drm_device *dev, int crtcnum);

int emgd_driver_suspend(struct drm_device *dev, pm_message_t state);
int emgd_driver_resume(struct drm_device *dev);
int emgd_driver_load(struct drm_device *dev, unsigned long flags);
int emgd_driver_unload(struct drm_device *dev);
void emgd_uncore_early_sanitize(struct drm_device *dev);

/* sysfs declarations */
void emgd_init_sysfs(struct drm_device *dev);
void emgd_destroy_sysfs(struct drm_device *dev);
void emgd_sysfs_gemsched_addclient(drm_emgd_private_t * priv, struct drm_i915_file_private * client);
void emgd_sysfs_gemsched_removeclient(drm_emgd_private_t * priv, struct drm_i915_file_private * client);


/* Overlay Declarations */
extern void emgd_overlay_planes_cleanup(struct drm_device *dev);

int emgd_drm_freeze(struct drm_device *dev);
int emgd_drm_restore(struct drm_device *dev);

/* PM declarations */
extern void intel_pm_init(struct drm_device *dev);

extern void intel_uncore_sanitize(struct drm_device *dev);
extern void intel_uncore_init(struct drm_device *dev);
extern void intel_uncore_reset(struct drm_device *dev);

/*Panel Backlight Related Functions*/
extern int blc_exit(struct drm_device *dev);

/* Hotplug */
#define I915_REENABLE_HOTPLUG_DELAY (2*60*1000)
#define HPD_STORM_DETECT_PERIOD 1000
#define HPD_STORM_THRESHOLD 5

static void display_port_enablement(struct drm_device *dev,
		 struct drm_connector *connector, igd_display_port_t *port,
		 emgd_encoder_t *emgd_encoder);
static void emgd_hotplug_work_func(struct work_struct *work);
static int emgd_hpd_irq_event(struct drm_device *dev, struct drm_connector *connector);

static void valleyview_hpd_irq_setup(struct drm_device *dev);
static void valleyview_hpd_init(struct drm_device *dev);
static void i915_reenable_hotplug_timer_func(unsigned long data);

static const u32 hpd_mask_i915[] = {
	[HPD_SDVO_B] = HDMIB_HOTPLUG_INT_EN,
	[HPD_DP_C] = DPC_HOTPLUG_INT_EN
};

/******************************************************************************
 * Global Variables
 *****************************************************************************/

static oal_callbacks_t interrupt_callbacks = {
};

extern emgd_drm_config_t default_config_drm;
extern emgd_drm_config_t *config_drm;

/* This must be defined whether debug or release build */
igd_debug_t emgd_debug_flag = {
	{
		CONFIG_DEBUG_FLAGS
	}
};
igd_debug_t *emgd_debug = &emgd_debug_flag;

#ifdef DEBUG_BUILD_TYPE

MODULE_PARM_DESC(debug_dsp, "Debug: dsp");
module_param_named(debug_dsp, emgd_debug_flag.hal.dsp, short, 0600);

MODULE_PARM_DESC(debug_mode, "Debug: mode");
module_param_named(debug_mode, emgd_debug_flag.hal.mode, short, 0600);

MODULE_PARM_DESC(debug_init, "Debug: init");
module_param_named(debug_init, emgd_debug_flag.hal.init, short, 0600);

MODULE_PARM_DESC(debug_overlay, "Debug: overlay");
module_param_named(debug_overlay, emgd_debug_flag.hal.overlay, short, 0600);

MODULE_PARM_DESC(debug_power, "Debug: power");
module_param_named(debug_power, emgd_debug_flag.hal.power, short, 0600);

MODULE_PARM_DESC(debug_state, "Debug: state");
module_param_named(debug_state, emgd_debug_flag.hal.state, short, 0600);

MODULE_PARM_DESC(debug_oal, "Debug: Kernel Mode Setting");
module_param_named(debug_oal, emgd_debug_flag.hal.kms, short, 0600);

MODULE_PARM_DESC(debug_dpd, "Debug: dpd");
module_param_named(debug_dpd, emgd_debug_flag.hal.dpd, short, 0600);

MODULE_PARM_DESC(debug_splash, "Debug: splash screen/video");
module_param_named(debug_splash, emgd_debug_flag.hal.splash, short, 0600);

MODULE_PARM_DESC(debug_trace, "Global Debug: trace");
module_param_named(debug_trace, emgd_debug_flag.hal.trace, short, 0600);

MODULE_PARM_DESC(debug_debug, "Global Debug: Debug with no associated module ");
module_param_named(debug_debug, emgd_debug_flag.hal.general, short, 0600);

#endif


static struct drm_driver driver;

/* Note: The following module paramter values are advertised to the root user,
 * via the files in the /sys/module/emgd/parameters directory (e.g. the "init"
 * file contains the value of the "init" module parameter), and so we keep
 * these values up to date.
 *
 * Note: The initial values are all set to -1, so that we can tell if the user
 * set them.
 *
 * Would it make sense to move all of the module parameters to an
 * include file so they don't clutter up the main driver file?
 */
int drm_emgd_portorder[IGD_MAX_PORTS] = {-1, -1, -1, -1, -1, -1, -1};
int drm_emgd_numports;
MODULE_PARM_DESC(portorder, "Display port order (e.g. \"4,2,0,0,0\")");
module_param_array_named(portorder, drm_emgd_portorder, int, &drm_emgd_numports,
	0600);

int drm_emgd_configid = -1;
MODULE_PARM_DESC(configid, "Which defined configuration number to use (e.g. "
	"\"1\")");
module_param_named(configid, drm_emgd_configid, int, 0600);

int drm_emgd_display_width = -1;
MODULE_PARM_DESC(display_width, "Display resolution's width (e.g. \"1024\")");
module_param_named(display_width, drm_emgd_display_width, int, 0600);

int drm_emgd_display_height = -1;
MODULE_PARM_DESC(display_height, "Display resolution's height (e.g. \"768\")");
module_param_named(display_height, drm_emgd_display_height, int, 0600);

int drm_emgd_refresh = -1;
MODULE_PARM_DESC(refresh, "Monitor refresh rate (e.g. 60, as in 60Hz)");
module_param_named(refresh, drm_emgd_refresh, int, 0600);

int drm_emgd_width = -1;
MODULE_PARM_DESC(width, "Framebuffer width (e.g. \"1024\")");
module_param_named(width, drm_emgd_width, int, 0600);

int drm_emgd_height = -1;
MODULE_PARM_DESC(height, "Framebuffer height (e.g. \"768\")");
module_param_named(height, drm_emgd_height, int, 0600);

int drm_emgd_depth = -1;
MODULE_PARM_DESC(depth, "The framebuffer color depth (e.g. 24, 32)");
module_param_named(depth, drm_emgd_depth, int, 0600);

int drm_emgd_bpp = -1;
MODULE_PARM_DESC(bpp, "The framebuffer bits per pixel (e.g. 8, 16, 24, 32)");
module_param_named(bpp, drm_emgd_bpp, int, 0600);


MODULE_PARM_DESC(init, "Whether to initialize the display at startup (1=yes, "
	"0=no)");

bool drm_emgd_lock_plane __read_mostly = false;
module_param_named(lock_plane, drm_emgd_lock_plane, bool, 0644);
MODULE_PARM_DESC(lock_plane, "Lock the primary frame buffer at startup. "
		"(default: false)");

int i915_semaphores __read_mostly = -1;
module_param_named(semaphores, i915_semaphores, int, 0600);
MODULE_PARM_DESC(semaphores,
		"Use semaphores for inter-ring sync (default: false)");

bool i915_enable_hangcheck __read_mostly = true;
module_param_named(enable_hangcheck, i915_enable_hangcheck, bool, 0644);
MODULE_PARM_DESC(enable_hangcheck,
		"Periodically check GPU activity for detecting hangs. "
		"WARNING: Disabling this can cause system wide hangs. "
		"(default: true)");

/* PPGTT causes a GPU hang when we run weston, disabling for now */
bool i915_enable_ppgtt __read_mostly = 0;
module_param_named(i915_enable_ppgtt, i915_enable_ppgtt, bool, 0600);
MODULE_PARM_DESC(i915_enable_ppgtt,
		"Enable PPGTT (default: true)");

char firmware[PATH_MAX] = "\0";
module_param_string(firmware, firmware, sizeof(firmware), 0644);
MODULE_PARM_DESC(firmware,
		"Use a specified firmware blob either from "
		"built-in data or /lib/firmware.");

bool i915_try_reset __read_mostly = true;
module_param_named(reset, i915_try_reset, bool, 0600);
MODULE_PARM_DESC(reset, "Attempt GPU resets (default: true)");

int i915_disable_power_well __read_mostly = 0;
module_param_named(disable_power_well, i915_disable_power_well, int, 0600);
MODULE_PARM_DESC(disable_power_well,
		"Disable the power well when possible (default: false)");

int gem_scheduler __read_mostly = 0;
module_param_named(gem_scheduler, gem_scheduler, int, 0644);
MODULE_PARM_DESC(gem_scheduler, "GPU workload scheduling. (default: off)");

unsigned int drm_async_gpu = 1;
#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
module_param_named(async_gpu, drm_async_gpu, int, 0600);
MODULE_PARM_DESC(async_gpu,
		"Turn On/Off Asynchronous GPU Wait while nuclear page flipping");
#endif

/*emgd (as i915 too) can't work without intel_agp module!*/
extern int intel_agp_enabled;

/**
 * The primary fb_info to use when the DRM module [re-]initializes the display.
 */
emgd_framebuffer_t *primary_fb_info;
/**
 * The secondary fb_info to use when the DRM module [re-]initializes the
 * display.
 */
emgd_framebuffer_t *secondary_fb_info;
/**
 * The display information to use when the DRM module [re-]initializes the
 * display.
 */
igd_display_info_t pt_info;

/**
 * Splash screen thread
 */
struct task_struct *emgd_splashscreen_thread;

/* Note: This macro is #define'd in "oal/os/memory.h" */
#ifdef INSTRUMENT_KERNEL_ALLOCS
os_allocd_mem *list_head = NULL;
os_allocd_mem *list_tail = NULL;
#endif

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
	.has_scheduler = 1,
	.has_overlay = 0,
};


static const struct intel_device_info intel_valleyview_d_info = {
	.gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_force_wake = 1,
	.is_valleyview = 1,
	.has_scheduler = 1,
	.has_llc = 0,
	.has_vebox = 0,
	.has_overlay = 0,
};


static const struct intel_device_info intel_valleyview_m_info = {
	.gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_force_wake = 1,
	.is_valleyview = 1,
	.has_scheduler = 1,
	.has_llc = 0,
	.has_vebox = 0,
	.has_overlay = 0,
};


static const struct intel_device_info ivybridge_d_info = {
	.is_ivybridge = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
	.has_scheduler = 1,
	.has_overlay = 0,
};

static struct pci_device_id pciidlist[] = {
	{0x8086, PCI_DEVICE_ID_VGA_SNB, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&intel_sandybridge_d_info},
	{0x8086, PCI_DEVICE_ID_VGA_IVB,  PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&ivybridge_d_info},
	{0x8086, PCI_DEVICE_ID_VGA_VLV,  PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&intel_valleyview_m_info},
	{0x8086, PCI_DEVICE_ID_VGA_VLV2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&intel_valleyview_d_info},
	{0x8086, PCI_DEVICE_ID_VGA_VLV3, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&intel_valleyview_d_info},
	{0x8086, PCI_DEVICE_ID_VGA_VLV4, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)&intel_valleyview_d_info},
	{0, 0, 0, 0, 0, 0, 0}
};

/*MODULE_DEVICE_TABLE(pci, pciidlist);*/

bool i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	if (i915_semaphores >= 0)
		return i915_semaphores;

	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6)
		return !intel_iommu_enabled;

	return 1;
}

/*
 * To use DRM_IOCTL_DEF, the first arg should be the local (zero based)
 * IOCTL number, not the global number.
 */
#define EMGD_IOCTL_DEF(ioctl, _func, _flags) \
	[DRM_IOCTL_NR(ioctl) - DRM_COMMAND_BASE] = \
		{.cmd = ioctl, .func = _func, .flags = _flags}

static const struct drm_ioctl_desc emgd_ioctl[] = {
	/*
	 * NOTE: The flag "DRM_MASTER" for the final parameter indicates an ioctl
	 * can only be used by the DRM master process.  In an X environment, the
	 * X server will be the master, in a Wayland environment, the Wayland
	 * compositor will be master, and if just running standalone GBM apps,
	 * they'll gain master.  For ioctl's that we want to run from an X
	 * client app or a Wayland client app, we instead use DRM_AUTH; these
	 * clients will get the DRM master to authenticate them, after which
	 * they'll be able to call the ioctl's.  Random programs that haven't
	 * authenticated with the DRM master won't be able to call them.
	 */

	/* i915 GEM and other i915 libdrm requirements */
	DRM_IOCTL_DEF_DRV(I915_GETPARAM, emgd_get_param, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(I915_GEM_INIT, i915_gem_init_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_EXECBUFFER, i915_gem_execbuffer, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_EXECBUFFER2, i915_gem_execbuffer2, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_PIN, i915_gem_pin_ioctl, DRM_AUTH|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_UNPIN, i915_gem_unpin_ioctl, DRM_AUTH|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_BUSY, i915_gem_busy_ioctl, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_THROTTLE, i915_gem_throttle_ioctl, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_ENTERVT, i915_gem_entervt_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_LEAVEVT, i915_gem_leavevt_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_CREATE, i915_gem_create_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_PREAD, i915_gem_pread_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_PWRITE, i915_gem_pwrite_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_MMAP, i915_gem_mmap_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_MMAP_GTT, i915_gem_mmap_gtt_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_SET_DOMAIN, i915_gem_set_domain_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_SW_FINISH, i915_gem_sw_finish_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_SET_TILING, i915_gem_set_tiling, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_GET_TILING, i915_gem_get_tiling, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_GET_APERTURE, i915_gem_get_aperture_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_MADVISE, i915_gem_madvise_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_WAIT, i915_gem_wait_ioctl, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_CONTEXT_CREATE, i915_gem_context_create_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_CONTEXT_DESTROY, i915_gem_context_destroy_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_SET_CACHING, i915_gem_set_caching_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GEM_GET_CACHING, i915_gem_get_caching_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_REG_READ, i915_reg_read_ioctl, DRM_UNLOCKED),
	
#ifdef SPLASH_VIDEO_SUPPORT /* *NOTE*: drm_mode_setplane() function symbol has to be exported in drm */
	/* For splash video to overlay without being master */
	DRM_IOCTL_DEF_DRV(IGD_SPVIDEO_INIT_BUFFERS, emgd_ioctl_init_spvideo, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(IGD_SPVIDEO_DISPLAY, emgd_ioctl_display_spvideo, DRM_CONTROL_ALLOW|DRM_UNLOCKED),
#endif

	/* the following are new private IOCTLs that are i915 device driver specific that complements the
	 * above new multi-overlay-plane IOCTLs because those ioctls dont handle the more detailed plane
	 * attributes like the color key - we'll be adding for ourselves other things too like the FB-Blend-Ovl
	 * or the Z-Order stuff.
	 */
	/* unfortunately the following 4 didnt make it upstream 
	 * - but will not be required once we get atomic-mode-set with crtc properties */
	DRM_IOCTL_DEF_DRV(IGD_MODE_SET_PIPEBLEND,emgd_ioctl_set_ovlplane_pipeblending, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(IGD_MODE_GET_PIPEBLEND,emgd_ioctl_get_ovlplane_pipeblending, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
#ifdef UNBLOCK_OVERLAY_KERNELCOVERAGE
	DRM_IOCTL_DEF_DRV(IGD_SET_SPRITE_COLORCORRECT,emgd_ioctl_set_ovlplane_colorcorrection, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(IGD_GET_SPRITE_COLORCORRECT,emgd_ioctl_get_ovlplane_colorcorrection, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
#endif
	/* these are existing i915 IOCTLs */
	DRM_IOCTL_DEF_DRV(I915_SET_SPRITE_COLORKEY, emgd_ioctl_set_ovlplane_colorkey, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GET_SPRITE_COLORKEY, emgd_ioctl_get_ovlplane_colorkey, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(I915_GET_PIPE_FROM_CRTC_ID, emgd_ioctl_get_pipe_from_crtc_id, DRM_CONTROL_ALLOW|DRM_UNLOCKED)
};

static int emgd_max_ioctl = DRM_ARRAY_SIZE(emgd_ioctl);


/*
 * NOTE: The next part of this file are EMGD-specific DRM functions, exported to
 * the generic DRM code via the drm_driver struct:
 */

/**
 * The driver handle for talking with the HAL, within the DRM/kernel code.
 *
 * This is a "real handle" as opposed to the "fake handle" in user-space.
 * Notice that there's only one handle, as the secondary device shares this
 * handle.
 */
static igd_driver_h drm_HAL_handle = NULL;

/**
 * This is the drm_HAL_handle cast to an igd_context_t.  It is cached for quick
 * access.
 */
static igd_context_t *drm_HAL_context = NULL;

/**
 * This is the dispatch table for the HAL.  It is cached for quick access.
 */
static igd_dispatch_t *drm_HAL_dispatch = NULL;

/*!
 * get_pre_driver_info
 *
 * Gets the mode information before it has been changed by the driver.
 * The expecation is that this is the mode set by the boot firmware.
 *
 * The two uses for this are:
 *   1) seamless handover of display configuration from firmware to driver.
 *   2) failsafe configuration if driver configuration is invalid.
 *
 * @param mode_context This is where fw_info is stored
 *
 * @return -IGD_INVAL on failure
 * @return 0 on success
 */
static int get_pre_driver_info(mode_context_t *mode_context)
{
	EMGD_TRACE_ENTER;
	EMGD_TRACE_EXIT;
	return 0;
}

/**
 * A helper function that prints the igd_param_t struct.
 *
 * @param params (IN) The the igd_param_t struct to print
 */
void emgd_print_params(igd_param_t *params)
{
	int i;

	EMGD_DEBUG("Values of params:");
	EMGD_DEBUG(" page_request = %lu = 0x%lx", params->page_request,
		params->page_request);
	EMGD_DEBUG(" max_fb_size = %lu = 0x%lx", params->max_fb_size,
		params->max_fb_size);
	EMGD_DEBUG(" preserve_regs = %u", params->preserve_regs);
	EMGD_DEBUG(" display_flags = %lu = 0x%lx", params->display_flags,
		params->display_flags);
	EMGD_DEBUG(" port_order:");
	for (i = 0 ; i < IGD_MAX_PORTS; i++) {
		EMGD_DEBUG("  port number %d = %lu", i, params->port_order[i]);
	}
	EMGD_DEBUG(" display_params:");
	for (i = 0 ; i < IGD_MAX_PORTS; i++) {
		int j;

		EMGD_DEBUG("  port_number = %lu", params->display_params[i].port_number);
		EMGD_DEBUG("   present_params = %lu = 0x%lx",
			params->display_params[i].present_params,
			params->display_params[i].present_params);
		EMGD_DEBUG("   flags = %lu = 0x%lx",
			params->display_params[i].flags,
			params->display_params[i].flags);
		EMGD_DEBUG("   edid_avail = %u = 0x%x",
			params->display_params[i].edid_avail,
			params->display_params[i].edid_avail);
		EMGD_DEBUG("   edid_not_avail = %u = 0x%x",
			params->display_params[i].edid_not_avail,
			params->display_params[i].edid_not_avail);
		EMGD_DEBUG("   ddc_gpio = %lu", params->display_params[i].ddc_gpio);
		EMGD_DEBUG("   ddc_speed = %lu", params->display_params[i].ddc_speed);
		EMGD_DEBUG("   ddc_dab = %lu", params->display_params[i].ddc_dab);
		EMGD_DEBUG("   i2c_gpio = %lu", params->display_params[i].i2c_gpio);
		EMGD_DEBUG("   i2c_speed = %lu", params->display_params[i].i2c_speed);
		EMGD_DEBUG("   i2c_dab = %lu", params->display_params[i].i2c_dab);
		EMGD_DEBUG("   fp_info.fp_width = %lu",
			params->display_params[i].fp_info.fp_width);
		EMGD_DEBUG("   fp_info.fp_height = %lu",
			params->display_params[i].fp_info.fp_height);
		EMGD_DEBUG("   fp_info.fp_pwr_method = %lu",
			params->display_params[i].fp_info.fp_pwr_method);
		EMGD_DEBUG("   fp_info.fp_pwr_t1 = %lu",
			params->display_params[i].fp_info.fp_pwr_t1);
		EMGD_DEBUG("   fp_info.fp_pwr_t2 = %lu",
			params->display_params[i].fp_info.fp_pwr_t2);
		EMGD_DEBUG("   fp_info.fp_pwr_t3 = %lu",
			params->display_params[i].fp_info.fp_pwr_t3);
		EMGD_DEBUG("   fp_info.fp_pwr_t4 = %lu",
			params->display_params[i].fp_info.fp_pwr_t4);
		EMGD_DEBUG("   fp_info.fp_pwr_t5 = %lu",
			params->display_params[i].fp_info.fp_pwr_t5);
		EMGD_DEBUG("   dtd_list:");
		EMGD_DEBUG("    num_dtds = %lu",
			params->display_params[i].dtd_list.num_dtds);
		for (j = 0 ; j < params->display_params[i].dtd_list.num_dtds; j++) {
			EMGD_DEBUG("   *dtd[%d].width = %u", j,
				params->display_params[i].dtd_list.dtd[j].width);
			EMGD_DEBUG("    dtd[%d].height = %u", j,
				params->display_params[i].dtd_list.dtd[j].height);
			EMGD_DEBUG("    dtd[%d].refresh = %u", j,
				params->display_params[i].dtd_list.dtd[j].refresh);
			EMGD_DEBUG("    dtd[%d].dclk = %lu = 0x%lx", j,
				params->display_params[i].dtd_list.dtd[j].dclk,
				params->display_params[i].dtd_list.dtd[j].dclk);
			EMGD_DEBUG("    dtd[%d].htotal = %u", j,
				params->display_params[i].dtd_list.dtd[j].htotal);
			EMGD_DEBUG("    dtd[%d].hblank_start = %u", j,
				params->display_params[i].dtd_list.dtd[j].hblank_start);
			EMGD_DEBUG("    dtd[%d].hblank_end = %u", j,
				params->display_params[i].dtd_list.dtd[j].hblank_end);
			EMGD_DEBUG("    dtd[%d].hsync_start = %u", j,
				params->display_params[i].dtd_list.dtd[j].hsync_start);
			EMGD_DEBUG("    dtd[%d].hsync_end = %u", j,
				params->display_params[i].dtd_list.dtd[j].hsync_end);
			EMGD_DEBUG("    dtd[%d].vtotal = %u", j,
				params->display_params[i].dtd_list.dtd[j].vtotal);
			EMGD_DEBUG("    dtd[%d].vblank_start = %u", j,
				params->display_params[i].dtd_list.dtd[j].vblank_start);
			EMGD_DEBUG("    dtd[%d].vblank_end = %u", j,
				params->display_params[i].dtd_list.dtd[j].vblank_end);
			EMGD_DEBUG("    dtd[%d].vsync_start = %u", j,
				params->display_params[i].dtd_list.dtd[j].vsync_start);
			EMGD_DEBUG("    dtd[%d].vsync_end = %u", j,
				params->display_params[i].dtd_list.dtd[j].vsync_end);
			EMGD_DEBUG("    dtd[%d].mode_number = %d", j,
				params->display_params[i].dtd_list.dtd[j].mode_number);
			EMGD_DEBUG("    dtd[%d].flags = %lu = 0x%lx", j,
				params->display_params[i].dtd_list.dtd[j].flags,
				params->display_params[i].dtd_list.dtd[j].flags);
			EMGD_DEBUG("    dtd[%d].x_offset = %u", j,
				params->display_params[i].dtd_list.dtd[j].x_offset);
			EMGD_DEBUG("    dtd[%d].y_offset = %u", j,
				params->display_params[i].dtd_list.dtd[j].y_offset);
			/*EMGD_DEBUG("    dtd[%d].pd_private_ptr = 0x%p", j,
				params->display_params[i].dtd_list.dtd[j].pd_private_ptr); */
			EMGD_DEBUG("    dtd[%d].private_ptr = 0x%p", j,
				params->display_params[i].dtd_list.dtd[j].private.extn_ptr);
		}
		EMGD_DEBUG("   attr_list:");
		EMGD_DEBUG("    num_attrs = %lu",
			params->display_params[i].attr_list.num_attrs);
		for (j = 0 ; j < params->display_params[i].attr_list.num_attrs; j++) {
			EMGD_DEBUG("    attr[%d].id = %lu = 0x%lx", j,
				params->display_params[i].attr_list.attr[j].id,
				params->display_params[i].attr_list.attr[j].id);
			EMGD_DEBUG("    attr[%d].value = %lu = 0x%lx", j,
				params->display_params[i].attr_list.attr[j].value,
				params->display_params[i].attr_list.attr[j].value);
		}
	}
	EMGD_DEBUG("   quickboot = %lu", params->quickboot);
	EMGD_DEBUG("   qb_seamless = %d", params->qb_seamless);
	EMGD_DEBUG("   qb_video_input = %lu", params->qb_video_input);
	EMGD_DEBUG("   qb_splash = %d", params->qb_splash);
	EMGD_DEBUG("   polling = %d", params->polling);
} /* emgd_print_params() */

static const struct cparams {
	u16 i;
	u16 t;
	u16 m;
	u16 c;
} cparams[] = {
	{ 1, 1333, 301, 28664 },
	{ 1, 1066, 294, 24460 },
	{ 1, 800, 294, 25192 },
	{ 0, 1333, 276, 27605 },
	{ 0, 1066, 276, 27605 },
	{ 0, 800, 231, 23784 },
};


/**
 * A helper function that starts, initializes and configures the HAL.  This
 * will be called during emgd_driver_load() if the display is to be initialized
 * at module load time.  Otherwise, this is deferred till the X driver loads
 * and calls emgd_driver_pre_init().
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h").
 * @param params (IN) The the igd_param_t struct to give to the HAL.
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_startup_hal(struct drm_device *dev, igd_param_t *params)
{
	int err = 0;

	EMGD_TRACE_ENTER;

	/* Initialize the various HAL modules: */
	err = igd_module_init(drm_HAL_handle, &drm_HAL_dispatch, params);
	if (err != 0) {
		return -EIO;
	}

	EMGD_TRACE_EXIT;

	return 0;
} /* emgd_startup_hal() */


/*
 * set vga decode state - true == enable VGA decode
 */
static int intel_modeset_vga_set_state(struct drm_device *dev, bool state)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u16 gmch_ctrl;

	pci_read_config_word(dev_priv->bridge_dev, INTEL_GMCH_CTRL, &gmch_ctrl);
	if (state)
		gmch_ctrl &= ~INTEL_GMCH_VGA_DISABLE;
	else
		gmch_ctrl |= INTEL_GMCH_VGA_DISABLE;
	pci_write_config_word(dev_priv->bridge_dev, INTEL_GMCH_CTRL, gmch_ctrl);
	return 0;
}


/*
 * True enables decode
 * False disables decode
 */
static unsigned int emgd_vga_set_decode(void *cookie, bool state)
{
	struct drm_device *dev = cookie;

	intel_modeset_vga_set_state(dev, state);
	if (state) {
		return VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM |
			VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
	} else {
		return VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
	}
}


static void emgd_switcheroo_set_state(struct pci_dev *pdev,
		enum vga_switcheroo_state state)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	pm_message_t pmm = { .event = PM_EVENT_SUSPEND };

	if (state == VGA_SWITCHEROO_ON) {
		EMGD_DEBUG("vga switcheroo: switched on");
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
		/* emgd resume handler doesn't set to D0 */
		pci_set_power_state(dev->pdev, PCI_D0);
		emgd_driver_resume(dev);
		dev->switch_power_state = DRM_SWITCH_POWER_ON;
	} else {
		EMGD_DEBUG("vga switcheroo: switched off");
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
		emgd_driver_suspend(dev, pmm);
		dev->switch_power_state = DRM_SWITCH_POWER_OFF;
	}
}


static bool emgd_switcheroo_can_switch(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	bool can_switch;

	spin_lock(&dev->count_lock);
	can_switch = (dev->open_count == 0);
	spin_unlock(&dev->count_lock);
	return can_switch;
}

static const struct vga_switcheroo_client_ops emgd_switcheroo_ops = {
	.set_gpu_state = emgd_switcheroo_set_state,
	.reprobe = NULL,
	.can_switch = emgd_switcheroo_can_switch,
};

/*
 * Merge any module parameters passed in at module load time with the
 * compiled in configuration structure.
 *
 * The passed in paramenter values will override the compilied in
 * values.
 */
static void emgd_merge_module_params(void) {

	/* Get the desired display "width": */
	if (-1 != drm_emgd_display_width) {
		/* Override the compile-time value with the module parameter: */
		EMGD_DEBUG("Using the \"display_width\" module parameter: \"%d\"",
						drm_emgd_display_width);
	} else {
		/* Use the compile-time value: */

		if (config_drm->crtcs[0].display_width) {
			drm_emgd_display_width = config_drm->crtcs[0].display_width;
			EMGD_DEBUG("Using the compile-time \"display_width\" value: \"%d\"",
						drm_emgd_display_width);
		}
	}

	/* Get the desired display "height": */
	if (-1 != drm_emgd_display_height) {
		/* Override the compile-time value with the module parameter: */
		EMGD_DEBUG("Using the \"display_height\" module parameter: \"%d\"",
						drm_emgd_display_height);
	} else {
		/* Use the compile-time value: */
		if (config_drm->crtcs[0].display_height) {
			drm_emgd_display_height = config_drm->crtcs[0].display_height;
			EMGD_DEBUG("Using the compile-time \"display_height\" value: \"%d\"",
						drm_emgd_display_height);
		}
	}

	/* Get the desired display "refresh": */
	if (-1 != drm_emgd_refresh) {
		/* Override the compile-time value with the module parameter: */
		EMGD_DEBUG("Using the \"refresh\" module parameter: \"%d\"",
						drm_emgd_refresh);
		/*
		config_drm->crtc_0->refresh = drm_emgd_refresh;
		config_drm->crtc_1->refresh = drm_emgd_refresh;
		*/
	} else {
		/* Use the compile-time value: */
		if (config_drm->crtcs[0].refresh) {
			drm_emgd_refresh = config_drm->crtcs[0].refresh;
			EMGD_DEBUG("Using the compile-time \"refresh\" value: \"%d\"",
						drm_emgd_refresh);
		}
	}

	/* Get the desired display "width": */
	if (-1 != drm_emgd_width) {
		/* Override the compile-time value with the module parameter: */
		EMGD_DEBUG("Using the \"width\" module parameter: \"%d\"",
						drm_emgd_width);
		/*
		config_drm->crtc_0->width = drm_emgd_width;
		config_drm->crtc_0->display_width = drm_emgd_width;
		config_drm->crtc_1->width = drm_emgd_width;
		config_drm->crtc_1->display_width = drm_emgd_width;
		*/
	} else {
		/* Use the compile-time value: */

		if (config_drm->fbs[config_drm->crtcs[0].framebuffer_id].width) {
			drm_emgd_width =
				config_drm->fbs[config_drm->crtcs[0].framebuffer_id].width;
			EMGD_DEBUG("Using the compile-time \"width\" value: \"%d\"",
						drm_emgd_width);
		}
	}

	/* Get the desired display "height": */
	if (-1 != drm_emgd_height) {
		/* Override the compile-time value with the module parameter: */
		EMGD_DEBUG("Using the \"height\" module parameter: \"%d\"",
						drm_emgd_height);
		/*
		config_drm->crtc_0->height = drm_emgd_height;
		config_drm->crtc_0->display_height = drm_emgd_height;
		config_drm->crtc_1->height = drm_emgd_height;
		config_drm->crtc_1->display_height = drm_emgd_height;
		*/
	} else {
		/* Use the compile-time value: */
		if (config_drm->fbs[config_drm->crtcs[0].framebuffer_id].height) {
			drm_emgd_height =
				config_drm->fbs[config_drm->crtcs[0].framebuffer_id].height;
			EMGD_DEBUG("Using the compile-time \"height\" value: \"%d\"",
						drm_emgd_height);
		}
	}

	/* Get the initial framebuffer depth */
	if (-1 != drm_emgd_depth) {
		/*
		config_drm->crtc_0->depth = drm_emgd_depth;
		config_drm->crtc_1->depth = drm_emgd_depth;
		*/
	} else {
		if (config_drm->fbs[config_drm->crtcs[0].framebuffer_id].depth) {
			drm_emgd_depth =
				config_drm->fbs[config_drm->crtcs[0].framebuffer_id].depth;
		}
	}

	/* Get the initial framebuffer bits per pixel */
	if (-1 != drm_emgd_bpp) {
		/*
		config_drm->crtc_0->bpp = drm_emgd_bpp;
		config_drm->crtc_1->bpp = drm_emgd_bpp;
		*/
	} else {
		if (config_drm->fbs[config_drm->crtcs[0].framebuffer_id].bpp) {
			drm_emgd_bpp =
				config_drm->fbs[config_drm->crtcs[0].framebuffer_id].bpp;
		}
	}

}

static void emgd_kick_out_firmware_fb(struct drm_i915_private *dev_priv)
{
        struct apertures_struct *ap;
        struct pci_dev *pdev = dev_priv->dev->pdev;
        bool primary;
	u32 gma_addr;

        ap = alloc_apertures(1);
        if (!ap)
                return;


	pci_read_config_dword(pdev, PCI_BASE_ADDRESS_2, &gma_addr);

        ap->ranges[0].base = gma_addr;/*dev_priv->dev->agp->base;*/
	ap->ranges[0].size = dev_priv->gtt.mappable_end - dev_priv->gtt.base.start;
        primary =
                pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;

        remove_conflicting_framebuffers(ap, "inteldrmfb", primary);
        remove_conflicting_framebuffers(ap, "VGA16", primary);
        remove_conflicting_framebuffers(ap, "VESA", primary);

        kfree(ap);
}


/**
 * A helper function that initializes the display, and potentially merges the
 * module parameters with the pre-compiled parameters.
 *
 */
int emgd_init_display(drm_emgd_private_t *priv,
		struct drm_device *dev,
		igd_param_t *params)
{
	struct drm_framebuffer *framebuffer = NULL;
	int                     ret;
	drm_i915_private_t     *dev_priv = NULL;

	EMGD_TRACE_ENTER;

	dev_priv = (drm_i915_private_t *)priv;

	emgd_merge_module_params();

	/* Initialise stolen first so that we may reserve preallocated
	 * objects for the BIOS to KMS transition.
	 */
	ret = i915_gem_init_stolen(dev);
	if (ret) {
		EMGD_ERROR("Failed i915_gem_init_stolen.");
		vga_switcheroo_unregister_client(dev->pdev);
		vga_client_register(dev->pdev, NULL, NULL, NULL);
		return ret;
	}

	ret = i915_gem_init(dev);
	if (ret) {
		EMGD_ERROR("Failed i915_gem_init.");
		i915_gem_cleanup_stolen(dev);
		vga_switcheroo_unregister_client(dev->pdev);
		vga_client_register(dev->pdev, NULL, NULL, NULL);
		return ret;
	}

	/*
	 * TODO: move the drm_mode_config_init from emgd_modeset_init
	 * to here in order to fix some of the interrupt mutex crashes
	 * upon rebooting couple of times / insmod for the first time
	 * (specifically when connecting VGA + HDMI or VGA + DP -
	 * based on validation recent testings).
	 *
	 * It is observed that interrupt get fired way before the mutex got
	 * initiallized. Not sure why we are getting interrupt in this case.
	 */
	drm_mode_config_init(dev); /* drm helper function */

	/*
	 * Configure interrupt support.
	 *
	 *  - First configure the dispatch tables and callbacks.
	 *  - Then configure the interupt support for the imported
	 *    i915 code.
	 */

	pci_enable_msi(dev->pdev);
	ret = interrupt_init(priv->init_info->device_id, dev_priv->regs,
					&interrupt_callbacks);

	dev->max_vblank_count = 0xffffffff; /* full 32 bit counter */
	dev->driver->driver_features |= DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED;


	ret = drm_irq_install(dev);
	if (ret) {
		mutex_lock(&dev->struct_mutex);
		i915_gem_cleanup_ringbuffer(dev);
		mutex_unlock(&dev->struct_mutex);
		i915_gem_cleanup_aliasing_ppgtt(dev);
		vga_switcheroo_unregister_client(dev->pdev);
		vga_client_register(dev->pdev, NULL, NULL, NULL);
		return ret;
	}


	/*
	 * If there's more than one VGA card installed, the there needs to
	 * be arbitrated access to the common VGA resources.
	 *
	 * Register our request to access those common resources.
	 */
	vga_client_register(dev->pdev, dev, NULL, emgd_vga_set_decode);

#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
	vga_switcheroo_register_client(dev->pdev, &emgd_switcheroo_ops, false);
#else
	vga_switcheroo_register_client(dev->pdev, &emgd_switcheroo_ops);
#endif

	/*************************************
	 * Initialize kernel mode setting functionality
	 *************************************/
	emgd_modeset_init(priv->dev, params);

	/* Initialize the framebuffer device */
	emgd_fbdev_init(priv);

	drm_kms_helper_poll_init(dev);
	valleyview_hpd_init(dev);

	/* Get Display context */
	drm_HAL_context->module_dispatch.dsp_get_dc(NULL,
									(igd_display_context_t **) &priv->primary,
									(igd_display_context_t **) &priv->secondary);

	/*************************************
	 * Initialize primary_fb_info
	 * get the framebuffer that was initialized as part of the
	 * framebuffer console device setup.  That has a reference to
	 * the emgd primary fb_info structure.  The fb_info is needed
	 * during the rest of the initialization:
	 *    - background color
	 *    - splash screen
	 *    - splash video
	 *************************************/
	framebuffer = list_entry(priv->dev->mode_config.fb_list.next,
					struct drm_framebuffer, head);
	if (!framebuffer) {
		EMGD_ERROR("Can't display splash screen/video as there is no fb.");
	} else {
		primary_fb_info = &priv->initfb_info;

		/*************************************
		 * Display a splash video if requested by user
		 *************************************/
		if(config_drm->sv_data->immediate) {
			/* Display a splash video */
			EMGD_DEBUG("Calling display_splash_video()\n");
			EMGD_ERROR("We havent passed in correct crtcs for splash_video()\n");
			/* FIXME: This should be moved to a separate kernel thread */
			display_splash_video(dev_priv);
		}
		/*************************************
		 * Display a splash screen if requested by user
		 *************************************/
		else {
		    /*
    		 * Set the initial contents of the frame buffer in a separate
    		 * thread. This allows the splash screen display to happen in
	    	 * parallel with any remaining driver initialization.
	    	 */
	    	 EMGD_DEBUG("Calling display_splash_screen()\n");
	    	 emgd_splashscreen_thread = kthread_run(display_splash_screen,
				(void *)priv, "EMGDSplashScreen");
		}
	}

	EMGD_TRACE_EXIT;
	return 0;
} /* emgd_init_display() */

/**
 * This function takes the firmware data that was returned to us and parses it
 * to get the config information.
 */
int load_firmware(const struct firmware *fw)
{
	int i, k;
	unsigned long iter = 0;
	int config_version;

	/* Check to make sure we are reading a binary blob that uses the same
	 * version.
	 */
	config_version = fw->data[iter];
	iter += sizeof(int);
	if (config_version != CONFIG_VERSION) {
		EMGD_ERROR("Config version not supported by this driver.");
		return -EINVAL;
	}

	/* Allocate space for our new config_drm. */
	config_drm = (emgd_drm_config_t *)kmalloc(sizeof(emgd_drm_config_t), GFP_KERNEL);
	if (!config_drm) {
		EMGD_ERROR("Could not allocate memory for config_drm.");
		config_drm = &default_config_drm;
		return -ENOMEM;
	}
	memset(config_drm, 0, sizeof(emgd_drm_config_t));

	/* Store our fw data pointer so we can free it when the driver closes. */
	config_drm->fw = fw;

	/* Go through reading data, and saving the appropriate pointers. */
	config_drm->num_crtcs = fw->data[iter];
	iter += sizeof(int);
	config_drm->crtcs = (emgd_crtc_config_t *)&fw->data[iter];
	iter += sizeof(emgd_crtc_config_t) * config_drm->num_crtcs;

	config_drm->num_fbs = fw->data[iter];
	iter += sizeof(int);
	config_drm->fbs = (emgd_fb_config_t *)&fw->data[iter];
	iter += sizeof(emgd_fb_config_t) * config_drm->num_fbs;

	config_drm->ovl_config = (emgd_ovl_config_t *)&fw->data[iter];
	iter += sizeof(emgd_ovl_config_t);

	config_drm->ss_data = kmalloc(sizeof(emgd_drm_splash_screen_t), GFP_KERNEL);
	if (!config_drm->ss_data) {
		EMGD_ERROR("Could not allocate memory for ss_data");
		kfree(config_drm);
		config_drm = &default_config_drm;
		return -ENOMEM;
	}
	memcpy(config_drm->ss_data, &fw->data[iter], sizeof(emgd_drm_splash_screen_t));
	iter += sizeof(emgd_drm_splash_screen_t);

	config_drm->sv_data = (emgd_drm_splash_video_t *)&fw->data[iter];
	iter += sizeof(emgd_drm_splash_video_t);

	config_drm->num_hal_params = fw->data[iter];
	iter += sizeof(int);
	config_drm->hal_params = kmalloc(sizeof(igd_param_t) * config_drm->num_hal_params, GFP_KERNEL);
	if (!config_drm->hal_params) {
		EMGD_ERROR("Could not allocate memory for hal_params");
		kfree(config_drm->ss_data);
		kfree(config_drm);
		config_drm = &default_config_drm;
		return -ENOMEM;
	}
	memset(config_drm->hal_params, 0, sizeof(igd_param_t) * config_drm->num_hal_params);

	memcpy(config_drm->hal_params, &fw->data[iter],
			sizeof(igd_param_t) * config_drm->num_hal_params);
	iter += sizeof(igd_param_t) * config_drm->num_hal_params;

	for (k=0; k<config_drm->num_hal_params; k++) {
		for (i=0; i<IGD_MAX_PORTS; i++) {
			if (config_drm->hal_params[k].display_params[i].dtd_list.num_dtds > 0 ) {
				config_drm->hal_params[k].display_params[i].dtd_list.dtd =
					(igd_display_info_t *)&fw->data[iter];
				iter += sizeof(igd_display_info_t) *
					config_drm->hal_params[k].display_params[i].dtd_list.num_dtds;
			}

			if (config_drm->hal_params[k].display_params[i].attr_list.num_attrs > 0) {
				config_drm->hal_params[k].display_params[i].attr_list.attr =
					(igd_param_attr_t *)&fw->data[iter];
				iter += sizeof(igd_param_attr_t) *
					config_drm->hal_params[k].display_params[i].attr_list.num_attrs;
			}
		}
	}

	if (config_drm->ss_data->image_size > 0) {
		config_drm->ss_data->image_data = (unsigned char *)&fw->data[iter];
	}

	return 0;
} /* load_firmware() */

/**
 * This is the drm_driver.load() function.  It is called when the DRM "loads"
 * (i.e. when our driver loads, it calls drm_init(), which eventually causes
 * this driver function to be called).
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param flags (IN) The last member of the pci_device_id struct that we
 * fill-in at the top of this file (e.g. CHIP_PSB_8108).
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_driver_load(struct drm_device *dev, unsigned long flags)
{
	int i, err = 0;
	igd_param_t *params;
	drm_emgd_private *priv = NULL;
	igd_init_info_t *init_info;
	drm_i915_private_t *dev_priv = NULL;
	int ret = 0;
	const struct firmware *fw;

	//EMGD_TRACE_ENTER;
	printk("emgd driver loading...\n");

	/*asm volatile("int3"); for debugger breaking at load time*/

	/* Get binary blob which contains driver config */
	config_drm = &default_config_drm;
	if (firmware[0] != '\0') {
		err = request_firmware(&fw, firmware, dev->dev);
		if (err) {
			EMGD_ERROR("Failed to load emgd firmware: %s", firmware);
		} else {
			if (load_firmware(fw)) {
				EMGD_ERROR("Failed to parse emgd firmware: %s", firmware);
			} else {
				EMGD_DEBUG("Loaded emgd firmware: %s", firmware);
			}
		}
	} else {
		EMGD_DEBUG("No emgd firmware specified.");
	}

	/**************************************************************************
	 *
	 * Get the compile-time/module-parameter "params" before initializing the
	 * HAL:
	 *
	 **************************************************************************/

	/*************************************
	 * Take into account the "configid" module parameter:
	 *************************************/
	if (config_drm->num_hal_params <= 0) {
		EMGD_ERROR("The compile-time configuration (in \"default_config.c\")"
			"contains no igd_param_t structures.  Please fix and recompile.");
		return -EINVAL;
	}
	if ((drm_emgd_configid == 0) ||
		(drm_emgd_configid > config_drm->num_hal_params)) {
		EMGD_ERROR("Module parameter \"configid\" contains invalid value "
			"%d.\nMust specify a compile-time configuration (in "
			"\"default_config.c\"),\nnumbered between 1 and %d.",
			drm_emgd_configid, config_drm->num_hal_params);
		return -EINVAL;
	}

	/* Obtain the user-configurable set of parameter values: */
	if (drm_emgd_configid < 1) {
		drm_emgd_configid = 1;
	}
	params = &config_drm->hal_params[drm_emgd_configid-1];


	/*************************************
	 * Take into account the "portorder" module parameter:
	 *************************************/
	err = 0;
	EMGD_DEBUG("Determining desired port order:");
	if (0 == drm_emgd_numports) {
		/* Set this to 1 so we use the compile-time value below: */
		err = 1;
		drm_emgd_numports = IGD_MAX_PORTS;
	} else if (drm_emgd_numports != IGD_MAX_PORTS) {
		EMGD_ERROR("Module parameter \"portorder\" specifies %d ports "
			"(must specify %d in a comma-separated list).",
			drm_emgd_numports, IGD_MAX_PORTS);
		drm_emgd_numports = IGD_MAX_PORTS;
		err = -EINVAL;
	}
	if (!err) {
		/* Validate each port within the module parameter: */
		for (i = 0 ; i < drm_emgd_numports ; i++) {
			if ((drm_emgd_portorder[i] < 0) ||
				(drm_emgd_portorder[i] > IGD_MAX_PORTS)) {
				EMGD_ERROR("Item %d in module parameter \"portorder\" "
					"contains invalid value %d (must be between 0 and 5).",
					i, drm_emgd_portorder[i]);
				err = -EINVAL;
			}
		}
	}
	if (!err) {
		/* Override the compile-time value with the module parameter: */
		for (i = 0 ; i < drm_emgd_numports ; i++) {
			params->port_order[i] = drm_emgd_portorder[i];
		}
		EMGD_DEBUG("Using the \"portorder\" module parameter: \"%d, %d, %d, "
			"%d, %d\"", drm_emgd_portorder[0], drm_emgd_portorder[1],
			drm_emgd_portorder[2], drm_emgd_portorder[3],
			drm_emgd_portorder[4]);
	} else {
		/* Use the compile-time value: */
		for (i = 0 ; i < drm_emgd_numports ; i++) {
			drm_emgd_portorder[i] = params->port_order[i];
		}
		EMGD_DEBUG("Using the compile-time \"portorder\" value: \"%d, %d, "
			"%d, %d, %d\"", drm_emgd_portorder[0], drm_emgd_portorder[1],
			drm_emgd_portorder[2], drm_emgd_portorder[3],
			drm_emgd_portorder[4]);
		err = 0;
	}


	emgd_print_params(params);


	/**************************************************************************
	 *
	 * Minimally initialize and configure the driver, deferring as much as
	 * possible until the X driver starts.:
	 *
	 **************************************************************************/

	/*
	 * In order for some early ioctls (e.g. emgd_get_chipset_info()) to work,
	 * we must do the following minimal initialization.
	 */
	EMGD_DEBUG("Calling igd_driver_init()");
	init_info = (igd_init_info_t *)OS_ALLOC(sizeof(igd_init_info_t));
	if (init_info == NULL){
		EMGD_ERROR_EXIT("Failed to allocate init_info.");
		return -ENOMEM;
	}
	drm_HAL_handle = igd_driver_init(init_info);
	if (drm_HAL_handle == NULL) {
		EMGD_ERROR_EXIT("igd_drive_init failed.");
		OS_FREE(init_info);
		return -ENOMEM;
	}

	/* Get the HAL context and give it to the ioctl-handling "bridge" code: */
	drm_HAL_context = (igd_context_t *) drm_HAL_handle;

	/* Save the drm dev pointer, it's needed by igd_module_init */
	drm_HAL_context->drm_dev = dev;

	/* Create the private structure */
	priv = OS_ALLOC(sizeof(drm_emgd_private));
	if (NULL == priv) {
		OS_FREE(init_info);
		return -ENOMEM;
	}
	OS_MEMSET(priv, 0, sizeof(drm_emgd_private));
	priv->context = drm_HAL_context;
	priv->init_info = init_info;
	dev->dev_private = priv;
	/* to set maximal_allocated_memory_size for GEM */
	if (0 != params->page_request) {
		priv->maximal_allocated_memory_size = params->page_request * 4096;
	}
	else {
		/* to set defaulf value if 0 (no limit) */
		priv->maximal_allocated_memory_size = 0;
	}

	/*
	 * Initialization of GEM and GEM related structures imported from
	 * i915 driver.
	 */
	dev_priv =  (drm_i915_private_t *) priv;
	dev_priv->dev = dev;
	dev_priv->info = (struct intel_device_info *) flags;

	/* Remove conflicting FB of primary device */
	emgd_kick_out_firmware_fb(dev_priv);

	/* Do basic driver initialization & configuration: */
	EMGD_DEBUG("Calling igd_driver_config()");
	err = igd_driver_config(drm_HAL_handle);
	if (err != 0) {
		OS_FREE(init_info);
		OS_FREE(priv);
		OS_FREE(drm_HAL_handle);
		return -EIO;
	}

	dev_priv->bridge_dev = pci_get_bus_and_slot(0, PCI_DEVFN(0,0));
	if (!dev_priv->bridge_dev) {
		EMGD_ERROR("PCI bridge device not found!");
		ret = -EIO;
	}

	/* enable GEM by default */
	/* in i915 map registers*/
	dev_priv->regs =
		((igd_context_t *)drm_HAL_handle)->device_context.virt_mmadr;

	/* Initialize and configure the driver now */
	err = emgd_startup_hal(dev, params);
	if (err != 0) {
		OS_FREE(init_info);
		OS_FREE(priv);
		OS_FREE(drm_HAL_handle);
		return err;
	}

	if (IS_HASWELL(dev) && (I915_READ(HSW_EDRAM_PRESENT) == 1)) {
		/* The docs do not explain exactly how the calculation can be
		 * made. It is somewhat guessable, but for now, it's always
		 * 128MB.
		 * NB: We can't write IDICR yet because we do not have gt funcs
		 * set up */
		dev_priv->ellc_size = 128;
		DRM_INFO("Found %zuMB of eLLC\n", dev_priv->ellc_size);
	}

	/* Setup GTT mappings */
	ret = i915_gem_gtt_init(dev);
	if (ret) {
		EMGD_ERROR("Failed to initialize GTT");
		pci_dev_put(dev_priv->bridge_dev);
		return ret;
	}
	dev_priv->gtt.mappable =
		io_mapping_create_wc(dev_priv->gtt.mappable_base,
				dev_priv->gtt.mappable_end);

	if (dev_priv->gtt.mappable == NULL) {
		ret = -EIO;
		ret = 1;
	}

		/**/
		/* Set up a WC MTRR for non-PAT systems.  This is more common than
		 * one would think, because the kernel disables PAT on first
		 * generation Core chips because WC PAT gets overridden by a UC
		 * MTRR if present.  Even if a UC MTRR isn't present.
		 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		dev_priv->mm.gtt_mtrr = mtrr_add(dev_priv->gtt.mappable_base,
				dev_priv->gtt.mappable_end,
				MTRR_TYPE_WRCOMB, 1);
		if (dev_priv->mm.gtt_mtrr < 0) {
			EMGD_ERROR("MTRR allocation failed.  Graphics "
					"performance may suffer.");
		}
#else
		dev_priv->mm.gtt_mtrr = arch_phys_wc_add(dev_priv->gtt.mappable_base,
				 dev_priv->gtt.mappable_end);
#endif
	/* The i915 workqueue is primarily used for batched retirement of
	 * requests (and thus managing bo) once the task has been completed
	 * by the GPU. i915_gem_retire_requests() is called directly when we
	 * need high-priority retirement, such as waiting for an explicit
	 * bo.
	 *
	 * It is also used for periodic low-priority events, such as
	 * idle-timers and recording error state.
	 *
	 * All tasks on the workqueue are expected to acquire the dev mutex
	 * so there is no point in running more than one instance of the
	 * workqueue at any time: max_active = 1 and NON_REENTRANT.
	 */
	dev_priv->wq = alloc_workqueue("i915",
			WQ_UNBOUND | WQ_NON_REENTRANT,
			1);
	if (dev_priv->wq == NULL) {
		EMGD_ERROR("Failed to create our workqueue.");
		ret = -ENOMEM;
		/*goto out_mtrrfree;*/
	}

	intel_pm_init(dev);
	intel_uncore_init(dev);

	/*Initialize the GEM module*/
	if(!INTEL_INFO(dev)->has_scheduler) {
		gem_scheduler = 0;
	}
	INIT_LIST_HEAD(&dev_priv->i915_client_list);
	priv->has_gem = 1;
	i915_gem_load(dev);

	/* Initialize i915 private structure */
	spin_lock_init(&dev_priv->irq_lock);
	spin_lock_init(&dev_priv->gpu_error.lock);
	spin_lock_init(&dev_priv->mm.object_stat_lock);

	mutex_init(&dev_priv->rps.hw_lock);

	setup_timer(&dev_priv->gpu_error.hangcheck_timer, i915_hangcheck_elapsed,
			(unsigned long) dev);

	/*
	 * If firmware seamless is enabled, this will query the hardware
	 * plane, pipe, and port registers for the current values and modify
	 * the configuration to match.
	 */
	if (params->qb_seamless) {
		get_pre_driver_info(&(drm_HAL_context->mode_context));
	}

	/* Initialize the display */
	ret = emgd_init_display(priv, dev, params);
	if (ret < 0) {
		EMGD_ERROR("failed to init modeset\n");
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)
		if (dev_priv->mm.inactive_shrinker.scan_objects) {
#else
		if (dev_priv->mm.inactive_shrinker.shrink) {
#endif
			unregister_shrinker(&dev_priv->mm.inactive_shrinker);
		}
		destroy_workqueue(dev_priv->wq);
		/* Free the GTT mapping */
		io_mapping_free(dev_priv->gtt.mappable);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		if (dev_priv->mm.gtt_mtrr >= 0) {
			/* mtrr_del the GMM_ADDR aperture we allocated earlier */
			mtrr_del(dev_priv->mm.gtt_mtrr, dev_priv->gtt.mappable_base,
					dev_priv->gtt.mappable_end);
			dev_priv->mm.gtt_mtrr = -1;
		}
#else
		arch_phys_wc_del(dev_priv->mm.gtt_mtrr);
#endif
		dev_priv->gtt.base.cleanup(&dev_priv->gtt.base);
		OS_FREE(priv);
		return ret;
	}

	/* Initialize lock plane controls to unlocked and the fb offsets to 0*/
	for (i = 0; i < priv->num_crtc; i++) {
		priv->crtcs[i]->freeze_state = 0;
		priv->crtcs[i]->unfreeze_xoffset = 0;
		priv->crtcs[i]->unfreeze_yoffset = 0;
	}

	/* Create sysfs interface object tree */
	emgd_init_sysfs(dev);


#ifdef DEBUG_BUILD_TYPE
	/*
	 * Turn on KMS debug messages in the general DRM module if our OAL
	 * messages are on and it's a debug driver.
	 */
	if (emgd_debug->hal.drm) {
		drm_debug |= DRM_UT_KMS;
	}
#endif

	/* call to clear out old GT FIFO errors */
	emgd_uncore_early_sanitize(dev);

#ifdef MSVDX_SUPPORT
	if (HAS_VED(dev)) {
		ret = vlv_setup_ved(dev);
		if (ret < 0) {
			DRM_ERROR("failed to setup VED bridge: %d\n", ret);
		}
	}
#endif

	//EMGD_TRACE_EXIT;
	printk("emgd driver loaded!\n");

	return 0;
} /* emgd_driver_load() */

void emgd_uncore_early_sanitize(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
       
	if (IS_GEN6(dev) || IS_GEN7(dev))
		I915_WRITE(GTFIFODBG,I915_READ(GTFIFODBG));
}

/*
 * Equivalent of i915 GETPARAM ioctl.  We only implement the parameters
 * that actually matter to us for GEM usage (although we may be able
 * to use this ioctl interface to replace some of our custom EMGD
 * ioctl's as well in the future).
 */
int emgd_get_param(struct drm_device *dev, void *arg,
	struct drm_file *file_priv)
{
	drm_emgd_private *priv = dev->dev_private;
	drm_i915_getparam_t *param = arg;
	int retval;

	switch (param->param) {
	case I915_PARAM_CHIPSET_ID:
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
		retval = dev->pdev->device;
#else
		retval = dev->pci_device;
#endif
		break;
	case I915_PARAM_HAS_PAGEFLIPPING:
	case I915_PARAM_HAS_GEM:
	case I915_PARAM_HAS_EXECBUF2:
	case I915_PARAM_HAS_BSD:
	case I915_PARAM_HAS_BLT:
	case I915_PARAM_HAS_RELAXED_FENCING:
	case I915_PARAM_HAS_COHERENT_RINGS:
	case I915_PARAM_HAS_EXEC_CONSTANTS:
	case I915_PARAM_HAS_RELAXED_DELTA:
	case I915_PARAM_HAS_PRIME_VMAP_FLUSH:
		retval = 1;
		break;
	case I915_PARAM_HAS_OVERLAY:
		retval = HAS_OVERLAY(dev);
		break;
	case I915_PARAM_HAS_SECURE_BATCHES:
		retval = capable(CAP_SYS_ADMIN);
		break;
	case I915_PARAM_HAS_PINNED_BATCHES:
		retval = 1;
		break;
    case I915_PARAM_HAS_EXEC_HANDLE_LUT:
        retval = 1;
        break;
	case I915_PARAM_HAS_GEN7_SOL_RESET:
		retval = 1;
		break;
	case I915_PARAM_HAS_LLC:
		retval = HAS_LLC(dev);
		break;
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
	case I915_PARAM_HAS_WT:
		retval = HAS_WT(dev);
		break;
#endif
	case I915_PARAM_HAS_ALIASING_PPGTT:
		retval = priv->mm.aliasing_ppgtt ? 1 : 0;
		break;
	case I915_PARAM_HAS_VEBOX:
		retval = HAS_VEBOX(dev);
		break;
	case I915_PARAM_NUM_FENCES_AVAIL:
		retval = priv->num_fence_regs - priv->fence_reg_start;
		break;
	case I915_PARAM_HAS_MULTIPLANE_DRM:
		retval = 1;
		break;
	case 19 /*I915_PARAM_HAS_WAIT_TIMEOUT*/:
		retval = 0;
		break;
	case I915_PARAM_HAS_EXEC_NO_RELOC:
		retval = 1;
		break;
	default:
		EMGD_ERROR("Invalid parameter passed to GETPARAM ioctl: %d",
			param->param);
		return -EINVAL;
	}

	/* Copy return value to userspace storage */
	if (DRM_COPY_TO_USER(param->value, &retval, sizeof(retval))) {
		EMGD_ERROR("Failed to copy GETPARAM data to userspace");
		return -EFAULT;
	}

	return 0;
}



/**
 * This is the drm_driver.unload() function.  It is called when the DRM
 * "unloads."  That is, drm_put_dev() (in "drm_stub.c"), which is called by
 * drm_exit() (in "drm_drv.c"), calls this function.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_driver_unload(struct drm_device *dev)
{
	drm_emgd_private *priv = dev->dev_private;
	drm_i915_private_t *dev_priv =  (drm_i915_private_t *)priv;
	int ret;

	EMGD_TRACE_ENTER;

#ifdef MSVDX_SUPPORT
	if (HAS_VED(dev)) {
		vlv_teardown_ved(dev);
	}
#endif

	/* Unregister backlight driver if it still registered.*/
	blc_exit(dev);

	/* Clean up for overlay module */
	emgd_overlay_planes_cleanup(dev);

	/* Shrinker uninitialization. This was registered during i915_gem_load()*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)
	if (dev_priv->mm.inactive_shrinker.scan_objects)
#else
	if (dev_priv->mm.inactive_shrinker.shrink)
#endif
		unregister_shrinker(&dev_priv->mm.inactive_shrinker);

	mutex_lock(&dev->struct_mutex);
	/* idle the hardware */
	ret = i915_gpu_idle(dev);
	if (ret) {
		EMGD_ERROR("Failed to idle hardware on DRM unload");
	}
	i915_gem_retire_requests(dev);
	mutex_unlock(&dev->struct_mutex);

	/* Cancel the retire work handler, which should be idle now. */
	cancel_delayed_work_sync(&dev_priv->mm.retire_work);

	/* Free the GTT mapping */
	io_mapping_free(dev_priv->gtt.mappable);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	if (dev_priv->mm.gtt_mtrr >= 0) {
		mtrr_del(dev_priv->mm.gtt_mtrr, dev_priv->gtt.mappable_base,
				dev_priv->gtt.mappable_end);
		dev_priv->mm.gtt_mtrr = -1;
	}
#else
	arch_phys_wc_del(dev_priv->mm.gtt_mtrr);
#endif
	/* Disable the irq before mode object teardown, for
	 * the irq might enqueue unpin/hotplug work. */
	drm_irq_uninstall(dev);

	del_timer_sync(&dev_priv->hotplug_reenable_timer);
	drm_kms_helper_poll_fini(dev);

	/* Destroy sysfs interface object tree */
	emgd_destroy_sysfs(dev);
	mutex_lock(&dev->struct_mutex);
	/* shutdown modesetting */
	emgd_modeset_destroy(dev);
	mutex_unlock(&dev->struct_mutex);
	vga_switcheroo_unregister_client(dev->pdev);
	vga_client_register(dev->pdev, NULL, NULL, NULL);

	list_del(&dev_priv->gtt.base.global_link);
	WARN_ON(!list_empty(&dev_priv->vm_list));

	/* Free error state after interrupts are fully disabled. */
	del_timer_sync(&dev_priv->gpu_error.hangcheck_timer);
	cancel_work_sync(&dev_priv->gpu_error.work);

	if (dev->pdev->msi_enabled) {
		pci_disable_msi(dev->pdev);
	}

	/* flush any delayed tasks or pending work */
	flush_workqueue(dev_priv->wq);
	mutex_lock(&dev->struct_mutex);
	i915_gem_free_all_phys_object(dev);

	i915_gem_cleanup_ringbuffer(dev);

	i915_gem_context_fini(dev);
	mutex_unlock(&dev->struct_mutex);
	i915_gem_cleanup_aliasing_ppgtt(dev);
	i915_gem_cleanup_stolen(dev);

	if (!I915_NEED_GFX_HWS(dev)) {
		i915_emgd_free_hws(dev);
	}

	destroy_workqueue(dev_priv->wq);
	if (dev_priv->slab)
		kmem_cache_destroy(dev_priv->slab);\

	dev_priv->gtt.base.cleanup(&dev_priv->gtt.base);
	igd_driver_shutdown_hal(drm_HAL_handle);

	/*
	 * Lastly, free up overlay related memory allocations in drm_emgd_private
	 * Kernels with DRM plane support have plane cleanup code that gets kicked
	 * off automatically when KMS is torn down, however we need to free these
	 * here because we allocated all the planes at once.
	 * drm_mode_config_cleanup goes through and frees each plane individually.
	 * We used to free them all when we hit the last plane. However, in several
	 * cases we never hit that plane, and I suspect there are cases when we hit
	 * the last plane in the array out of order.  This resolves all those
	 * issues. We also have to wait till after the call to
	 * igd_driver_shutdown_hal as that resets the pipes planes and ports.
	 */
	if(priv->ovl_planes) {
		kfree(priv->ovl_planes);
		priv->ovl_planes = NULL;
	}
	if(priv->ovl_drmformats){
		kfree(priv->ovl_drmformats);
		priv->ovl_drmformats = NULL;
	}

	igd_driver_shutdown(drm_HAL_handle);

	OS_FREE(priv->init_info);
	OS_FREE(priv);
	dev->dev_private = NULL;

	/* Note: This macro is #define'd in "oal/os/memory.h" */
#ifdef INSTRUMENT_KERNEL_ALLOCS
	emgd_report_unfreed_memory();
#endif

	if (config_drm->fw) {
		kfree(config_drm->hal_params);
		kfree(config_drm->ss_data);
		release_firmware(config_drm->fw);
		kfree(config_drm);
	}

	EMGD_TRACE_EXIT;

	return 0;
} /* emgd_driver_unload() */


/**
 * This is the drm_driver.open() function.  It is called when a user-space
 * process opens the DRM device file.  The DRM drm_open() (in "drm_fops.c")
 * calls drm_open_helper(), which calls this function.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param file (IN) DRM's file private data struct (in "drmP.h")
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_driver_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *filepriv;
	drm_emgd_private *dev_priv = dev->dev_private;
	int i;

	EMGD_TRACE_ENTER;

	filepriv = OS_ALLOC(sizeof(*filepriv));
	if (!filepriv) {
		EMGD_ERROR("Failed to allocate DRM file private");
		return -ENOMEM;
	}

	file->driver_priv  = filepriv;
	filepriv->drm_file = file;

	spin_lock_init(&filepriv->mm.lock);
	INIT_LIST_HEAD(&filepriv->mm.request_list);
	INIT_LIST_HEAD(&filepriv->client_link);
	filepriv->mm.outstanding_requests = 0;
	if (file->is_master)
		filepriv->mm.sched_priority       = 0;
		 /* master should always be zero - else u'll get dead locks or 
		  * inverted-prioritiy impact on scheduler when doing client to
		  *  master flip-chain swapping */
	else
		filepriv->mm.sched_priority       = SHARED_NORMAL_PRIORITY;
	filepriv->mm.sched_period         = INITIAL_SHARED_PERIOD; 
		/* start-up uses "shared" priority, so period = shared period in drv ctx */
	filepriv->mm.sched_capacity       = INITIAL_SHARED_CAPACITY;
		/* start-up uses "shared" priority, so capacity = shared capacity in drv ctx */
	filepriv->mm.sched_balance        = usecs_to_jiffies(INITIAL_SHARED_CAPACITY);
		/* start-up uses "shared" priority, so balance = shared balance in drv ctx */
	/* init time keeping variables to appropriate values */
	filepriv->mm.sched_last_refresh_jiffs = jiffies;
	for (i=0; i<MOVING_AVG_RANGE ;++i) {
		filepriv->mm.exec_times[i] = 0;
		filepriv->mm.submission_times[i].tv_sec = 0;
		filepriv->mm.submission_times[i].tv_usec = 0;
	}
	filepriv->mm.last_exec_slot    = 0;
	filepriv->mm.sched_avgexectime = 0;
	/* add this new client into our main driver's context for tracking */
	mutex_lock(&dev->struct_mutex);
	dev_priv->num_clients++;
	list_add(&filepriv->client_link, &dev_priv->i915_client_list);
	mutex_unlock(&dev->struct_mutex);

	emgd_sysfs_gemsched_addclient(dev_priv, filepriv);
	idr_init(&filepriv->context_idr);

	INIT_LIST_HEAD(&filepriv->pending_flips);
	EMGD_TRACE_EXIT;
	return 0;
} /* emgd_driver_open() */


/**
 * emgd_driver_lastclose()
 *
 * Handler that gets called when the last DRM client closes its DRM filehandle.
 * At this point we should be back to the initial system boot-up state.  If
 * we had a splash screen, we should re-display that now.  Otherwise we should
 * be back at the fb console (and we can use the DRM helper to make sure we get
 * back into the right state for that).
 */
void emgd_driver_lastclose(struct drm_device *dev)
{
	drm_emgd_private *priv = dev->dev_private;
	struct drm_crtc *crtc;
	emgd_crtc_t *emgd_crtc;
	igd_context_t *context = ((drm_emgd_private_t *)priv)->context;
	int ret;

	EMGD_TRACE_ENTER;

	/* Restore initial display state */
	ret = emgd_fbcon_initial_config(priv->emgd_fbdev, 0);
	if (ret) {
		EMGD_DEBUG("Failed to restore CRTC mode for fb console");
	}

	/*
	 * If we were running a display server that used the cursor plane, we
	 * should make sure it's turned off now (if the server died, it could
	 * leave the plane on when we return to the initial FB).
	 */
	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc = container_of(crtc, emgd_crtc_t, base);
		context->mode_dispatch->kms_program_cursor(emgd_crtc, FALSE);
	}
	mutex_unlock(&dev->mode_config.mutex);

	/* Do we have a splash screen to display? */
	if (config_drm->ss_data->width && config_drm->ss_data->height) {
		/*
		 * This may not be needed if the framebuffer contents were never
		 * changed by anything.
		 */
		display_splash_screen(priv);
	}

	vga_switcheroo_process_delayed_switch();

	i915_gem_lastclose(dev);
	EMGD_TRACE_EXIT;

} /* emgd_driver_lastclose() */


/**
 * This is the drm_driver.preclose() function.  It is called when a user-space
 * process closes/releases the DRM device file.  At the very start of DRM
 * drm_release() (in "drm_fops.c"), it calls this function, before it does any
 * real work (other than get the lock).
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param priv (IN) DRM's file private data struct (in "drmP.h")
 */
void emgd_driver_preclose(struct drm_device *dev, struct drm_file *priv)
{
	EMGD_TRACE_ENTER;

	i915_gem_context_close(dev, priv);
	i915_gem_release(dev, priv);
	emgd_sysfs_gemsched_removeclient(dev->dev_private, 
		(struct drm_i915_file_private *)priv->driver_priv);

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_atomic_free_events(dev, priv);
#endif

	EMGD_TRACE_EXIT;
} /* emgd_driver_preclose() */


/**
 * This is the drm_driver.postclose() function.  It is called when a user-space
 * process closes/releases the DRM device file.  At the end of DRM
 * drm_release() (in "drm_fops.c"), but before drm_lastclose is optionally
 * called, it calls this function.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param file (IN) DRM's file private data struct (in "drmP.h")
 */
void emgd_driver_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *filepriv = file->driver_priv;

	EMGD_TRACE_ENTER;

	if (filepriv) {
		OS_FREE(filepriv);
	}

	EMGD_TRACE_EXIT;
} /* emgd_driver_postclose() */


/**
 * This is the drm_driver.suspend() function.  It appears to support what the
 * Linux "pm.h" file calls "the legacy suspend framework."  The DRM
 * drm_class_suspend() (in "drm_sysfs.c", which implements the Linux
 * class.suspend() function defined in "device.h") calls this function if
 * conditions are met, such as drm_minor->type is DRM_MINOR_LEGACY and the
 * driver doesn't have the DRIVER_MODESET (i.e. KMS--Kernel Mode Setting)
 * feature set.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param state (IN) What power state to put the device in (in "pm.h")
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_driver_suspend(struct drm_device *dev, pm_message_t state)
{
	int ret;
	unsigned int pwr_state;
	drm_emgd_private *priv = dev->dev_private;

	EMGD_TRACE_ENTER;

	/* When the system is suspended, the X server does a VT switch, which saves
	 * the register state of the X server, and restores the console's register
	 * state.  This code saves the console's register state, so that after the
	 * system resumes, VT switches to the console can occur.
	 */
	EMGD_DEBUG("Saving the console's register state");
	priv->suspended_state =
		drm_HAL_context->module_dispatch.reg_alloc(drm_HAL_context,
			IGD_REG_SAVE_ALL);
	if (priv->suspended_state) {
		drm_HAL_context->module_dispatch.reg_save(drm_HAL_context,
			priv->suspended_state);
	}

	/* Map the pm_message_t event states to HAL states: */
	switch (state.event) {
	case PM_EVENT_PRETHAW:
	case PM_EVENT_FREEZE:
		pwr_state = IGD_POWERSTATE_D1;
		break;
	case PM_EVENT_HIBERNATE:
		pwr_state = IGD_POWERSTATE_D3;
		break;
	case PM_EVENT_SUSPEND:
	default:
		pwr_state = IGD_POWERSTATE_D2;
		break;
	} /* switch (state) */

	EMGD_DEBUG("Calling pwr_alter()");
	ret = drm_HAL_dispatch->pwr_alter(drm_HAL_handle, pwr_state);
	EMGD_DEBUG("pwr_alter() returned %d", ret);

	EMGD_TRACE_EXIT;
	return ret;

} /* emgd_driver_suspend() */


/**
 * This is the drm_driver.resume() function.  It appears to support what the
 * Linux "pm.h" file calls "the legacy suspend framework."  The DRM
 * drm_class_resume() (in "drm_sysfs.c", which implements the Linux
 * class.suspend() function defined in "device.h") calls this function if
 * conditions are met, such as drm_minor->type is DRM_MINOR_LEGACY and the
 * driver doesn't have the DRIVER_MODESET feature set.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return 0 on Success
 * @return <0 on Error
 */
int emgd_driver_resume(struct drm_device *dev)
{
	int ret;
	drm_emgd_private *priv = dev->dev_private;

	EMGD_TRACE_ENTER;

	/* call to clear out old GT FIFO errors */
	emgd_uncore_early_sanitize(dev);

	EMGD_DEBUG("Calling pwr_alter()");
	ret = drm_HAL_dispatch->pwr_alter(drm_HAL_handle, IGD_POWERSTATE_D0);
	EMGD_DEBUG("pwr_alter() returned %d", ret);

	/* Restore the register state of the console, so that after the X server is
	 * back up, VT switches to the console can occur.
	 */
	EMGD_DEBUG("Restoring the console's register state");
	if (priv->suspended_state) {
		drm_HAL_context->module_dispatch.reg_restore(drm_HAL_context,
			priv->suspended_state);
		drm_HAL_context->module_dispatch.reg_free(drm_HAL_context,
			priv->suspended_state);
		priv->suspended_state = NULL;
	}

	i915_gem_restore_fences(dev);

	EMGD_DEBUG("Returning %d", ret);
	EMGD_TRACE_EXIT;
	return ret;
} /* emgd_driver_resume() */


/**
 * This is the drm_driver.device_is_agp() function.  It is called as a part of
 * drm_init() (before the drm_driver.load() function), and is "typically used
 * to determine if a card is really attached to AGP or not" (see the Doxygen
 * comment for this function in "drmP.h").  It is actually called by the
 * inline'd DRM procedure, drm_device_is_agp() (in "drmP.h").
 *
 * All Intel integrated graphics are treated as AGP.  This function always
 * returns 1.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return 0 the graphics "card" is absolutely not AGP.
 * @return 1 the graphics "card" is AGP.
 * @return 2 the graphics "card" may or may not be AGP.
 */
int emgd_driver_device_is_agp(struct drm_device *dev)
{
	return 1;
} /* emgd_driver_device_is_agp() */



/**
 * This is the drm_driver.get_vblank_counter() function.  It is called to get
 * the raw hardware vblank counter.  There are 4 places within "drm_irq.c" that
 * call this function.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param crtc (IN) counter to fetch (really a pipe for our driver)
 *
 * @return raw vblank counter value
 */
u32 emgd_driver_get_vblank_counter(struct drm_device *dev, int crtc_select)
{
	struct drm_crtc *crtc;
	emgd_crtc_t     *cur_emgd_crtc, *selected_emgd_crtc = NULL;
	igd_context_t * context = ((drm_emgd_private_t *)dev->dev_private)->context;


	/*EMGD_TRACE_ENTER;*/

	/* Find the CRTC associated with the  */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		cur_emgd_crtc = container_of(crtc, emgd_crtc_t, base);

		if ((1 << crtc_select) == cur_emgd_crtc->crtc_id) {
			selected_emgd_crtc = cur_emgd_crtc;
			break;
		}
	}

	if (NULL == selected_emgd_crtc) {
		EMGD_ERROR_EXIT("Invalid CRTC selected.");
		return -EINVAL;
	}

	/*EMGD_TRACE_EXIT;*/

	return context->mode_dispatch->kms_get_vblank_counter(selected_emgd_crtc);
} /* emgd_driver_get_vblank_counter() */



/**
 * This is the drm_driver.enable_vblank() function.  It is called by
 * drm_vblank_get() (in "drm_irq.c") to enable vblank interrupt events.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param crtc (IN) which irq to enable (really a pipe for our driver)
 *
 * @return 0 on Success
 * @return <0 if the given crtc's vblank interrupt cannot be enabled
 */
int emgd_driver_enable_vblank(struct drm_device *dev, int crtc)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;
	interrupt_data_t int_mask;
	EMGD_DEBUG("Enabling vblank interrupts for CRTC %d", crtc);

	int_mask.irq_display = crtc? IGD_IRQ_DISP_VBLANK_B : IGD_IRQ_DISP_VBLANK_A;

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);
	mask_interrupt_helper(&int_mask);
	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);

	return 0;
} /* emgd_driver_enable_vblank() */


/**
 * This is the drm_driver.disable_vblank() function.  It is called by
 * vblank_disable_fn() (in "drm_irq.c") to disable vblank interrupt events.
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 * @param crtc (IN) which irq to disable (really a pipe for our driver)
 */
void emgd_driver_disable_vblank(struct drm_device *dev, int crtc)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;
	interrupt_data_t int_mask;

	EMGD_DEBUG("Disabling vblank interrupts for CRTC %d", crtc);

	int_mask.irq_display = crtc? IGD_IRQ_DISP_VBLANK_B : IGD_IRQ_DISP_VBLANK_A;

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);
	unmask_interrupt_helper(&int_mask);
	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);
} /* emgd_driver_disable_vblank() */

/**
 * This is the drm_driver.irq_preinstall() function.  It is called by
 * drm_irq_install() (in "drm_irq.c") before it installs this driver's IRQ
 * handler (i.e. emgd_driver_irq_handler()).
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 */
void emgd_driver_irq_preinstall(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	atomic_set(&dev_priv->irq_received, 0);

	INIT_WORK(&dev_priv->hotplug_work, emgd_hotplug_work_func);
	INIT_WORK(&dev_priv->gpu_error.work, i915_error_work_func);
	INIT_DELAYED_WORK(&dev_priv->rps.work, vlv_turbo_down_work);

	setup_timer(&dev_priv->hotplug_reenable_timer, i915_reenable_hotplug_timer_func,
								(unsigned long) dev_priv);

	interrupt_stop_irq();
} /* emgd_driver_irq_preinstall() */

static int emgd_hpd_irq_event(struct drm_device *dev, struct drm_connector *connector)
{
	enum drm_connector_status old_status;

	WARN_ON(!mutex_is_locked(&dev->mode_config.mutex));
	old_status = connector->status;

	connector->status = connector->funcs->detect(connector, false);
	EMGD_DEBUG("[CONNECTOR:%d:%s] status updated from %d to %d\n",
			connector->base.id,
			drm_get_connector_name(connector),
			old_status, connector->status);
	return (old_status != connector->status);
}

static void display_port_enablement(struct drm_device *dev,
		struct drm_connector *connector, igd_display_port_t *port,
		emgd_encoder_t *emgd_encoder)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long temp = 0;

	temp = I915_READ(port->port_reg);

	/*
	 * TODO: The real intention of this function is to disable display port
	 * contorl bit 31 upon plugging out. This is a workaround to avoid missing
	 * interrupt especially when swapping out from DP port. Having said this,
	 * without hooking up to the probe function and even this workaround,
	 * the issue stated above is not reproducible. However, we are
	 * re-enabling back BIT31 upon plugging in connector as bit 31 of the
	 * connector might be disabled here earlier.
	 */

	if (connector->status == connector_status_disconnected)
		I915_WRITE(port->port_reg, (temp & ~BIT31));
	else if (connector->status == connector_status_connected) {
		dev_priv->context->mode_dispatch->kms_program_port(emgd_encoder, IGD_DISPLAY_ENABLE);
		/* Also necessary to re-program pipe to retrain link when DP is hotplugged */
		if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
			struct drm_crtc *crtc = emgd_encoder->base.crtc;
			if (crtc) {
				emgd_crtc_t *emgd_crtc = to_emgd_crtc(crtc);
				EMGD_DEBUG("Reinitializing pipe for DP hotplug\n");
				dev_priv->context->mode_dispatch->kms_program_pipe(emgd_crtc);
			}
		}
	}
	
	/*
	 * For connector->status == unknown, we will not know what state it's.
	 * It might be either connected or disconnected state. If it's a
	 * connected state, disabling bit 31 will eventually turn off the
	 * display and vice versa. Hence, we are not going to touch bit 31
	 * for an unknown state.
	 */

}

static void emgd_hotplug_work_func(struct work_struct *work)
{
	drm_emgd_private_t *dev_priv = container_of(work, drm_emgd_private_t,
					hotplug_work);

	struct drm_device *dev = dev_priv->dev;
	struct drm_connector *connector;
	bool changed = false, hpd_disabled = false;
	emgd_connector_t *emgd_connector;
	emgd_encoder_t *emgd_encoder;
	igd_display_port_t *port;
	unsigned long irqflags;
	u32 hotplug_event_bits, hpd_event_bits;

	EMGD_TRACE_ENTER;

	BUG_ON(!dev);

	mutex_lock(&dev->mode_config.mutex);

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);
	hotplug_event_bits = dev_priv->hotplug_event_bits;
	hpd_event_bits = dev_priv->hpd_event_bits;
	dev_priv->hotplug_event_bits = 0;
	dev_priv->hpd_event_bits = 0;
	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		emgd_connector = container_of(connector, emgd_connector_t, base);
		emgd_encoder = emgd_connector->encoder;
		port = emgd_connector->encoder->igd_port;

		if (dev_priv->hpd_stats[emgd_encoder->hpd_pin].hpd_mark == HPD_MARK_DISABLED &&
			connector->polled == DRM_CONNECTOR_POLL_HPD) {
			EMGD_DEBUG("HPD interrupt storm detected on connector %s:\
				switching from hotplug detection to polling\n",\
				drm_get_connector_name(connector));
			dev_priv->hpd_stats[emgd_encoder->hpd_pin].hpd_mark = HPD_DISABLED;
			connector->polled = DRM_CONNECTOR_POLL_CONNECT
				| DRM_CONNECTOR_POLL_DISCONNECT;
			hpd_disabled = true;
		}

		if (hpd_event_bits & (1 << emgd_encoder->hpd_pin)) {
			if ((hotplug_event_bits & IGD_IRQ_HPD_CRT) &&
				port->port_number == IGD_PORT_TYPE_ANALOG) {
				if (emgd_hpd_irq_event(dev, connector)) {
					EMGD_DEBUG("[hpd detected] Port: %d\n", port->port_number);
					display_port_enablement(dev, connector, port, emgd_encoder);
					changed = true;
				}
			}

			if (hotplug_event_bits & IGD_IRQ_HPD_DIG_B) {
				if (port->port_number == IGD_PORT_TYPE_SDVOB ||
					port->port_number == IGD_PORT_TYPE_DPB) {
					if (emgd_hpd_irq_event(dev, connector)) {
						EMGD_DEBUG("[hpd detected] Port: %d\n", port->port_number);
						display_port_enablement(dev, connector, port, emgd_encoder);
						changed = true ;
					}
				}
			}

			if (hotplug_event_bits & IGD_IRQ_HPD_DIG_C) {
				if (port->port_number == IGD_PORT_TYPE_SDVOC ||
					port->port_number == IGD_PORT_TYPE_DPC) {
					if (emgd_hpd_irq_event(dev, connector)) {
						EMGD_DEBUG("[hpd detected] Port: %d\n", port->port_number);
						display_port_enablement(dev, connector, port, emgd_encoder);
						changed = true;
					}
				}
			}
		}
	}

	if (hpd_disabled) {
		drm_kms_helper_poll_enable(dev);
		mod_timer(&dev_priv->hotplug_reenable_timer,
		jiffies + msecs_to_jiffies(I915_REENABLE_HOTPLUG_DELAY));
	}

	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {

		EMGD_DEBUG("Call to drm_kms_helper_hotplug\n");
		drm_kms_helper_hotplug_event(dev);
	}

	EMGD_TRACE_EXIT;
}

/**
 * This is the drm_driver.irq_postinstall() function.  It is called by
 * drm_irq_install() (in "drm_irq.c") after it installs this driver's IRQ
 * handler (i.e. emgd_driver_irq_handler()).
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return 0 on Success
 * @return <0 if the given crtc's vblank interrupt cannot be enabled
 */
int emgd_driver_irq_postinstall(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u16 msid;

	DRM_INIT_WAITQUEUE(&dev_priv->ring[RCS].irq_queue);
	if (HAS_BSD(dev))
		DRM_INIT_WAITQUEUE(&dev_priv->ring[VCS].irq_queue);
	if (HAS_BLT(dev))
		DRM_INIT_WAITQUEUE(&dev_priv->ring[BCS].irq_queue);

	/* Hack for broken MSIs on VLV-DC */
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
	if ((IS_VALLEYVIEW(dev) && ((dev)->pdev->device == PCI_DEVICE_ID_VGA_VLV))) {
#else
	if ((IS_VALLEYVIEW(dev) && ((dev)->pci_device == PCI_DEVICE_ID_VGA_VLV))) {
#endif
		pci_write_config_dword(dev_priv->dev->pdev, 0x94, 0xfee00000);
		pci_read_config_word(dev->pdev, 0x98, &msid);
		msid &= 0xff; /* mask out delivery bits */
		msid |= (1<<14);
		pci_write_config_word(dev_priv->dev->pdev, 0x98, msid);
	}



	interrupt_start_irq();

	return 0;
} /* emgd_driver_irq_postinstall() */


/**
 * This is the drm_driver.irq_uninstall() function.  It is called by
 * drm_irq_install() (in "drm_irq.c") as a part of uninstalling this driver's
 * IRQ handler (i.e. emgd_driver_irq_handler()).
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 */
void emgd_driver_irq_uninstall(struct drm_device *dev)
{
	interrupt_stop_irq();
} /* emgd_driver_irq_uninstall() */



int emgd_driver_master_create(struct drm_device *dev, struct drm_master *master)
{
	struct drm_i915_master_private *master_priv;

	master_priv = kzalloc(sizeof(*master_priv), GFP_KERNEL);
	if (!master_priv) {
		return -ENOMEM;
	}

	master->driver_priv = master_priv;

	return 0;
}


void emgd_driver_master_destroy(struct drm_device *dev, struct drm_master *master)
{
	struct drm_i915_master_private *master_priv = master->driver_priv;

	if (!master_priv) {
		return;
	}

	kfree(master_priv);
	master->driver_priv = NULL;
}


/**
 * This is the drm_driver.irq_handler() function, and is a Linux IRQ interrupt
 * handler (see the irq_handler_t in the Linux header "interrupt.h").  It is
 * installed by drm_irq_install() (in "drm_irq.c") by calling request_irq()
 * (implemented in "interrupt.h") with this function as the 2nd parameter.  The
 * return type is an enum (see the Linux header "irqreturn.h").
 *
 * @param dev (IN) DRM per-device (e.g. one GMA) struct (in "drmP.h")
 *
 * @return IRQ_NONE if the interrupt was not from this device
 * @return IRQ_HANDLED if the interrupt was handled by this device
 * @return IRQ_WAKE_THREAD if this handler requests to wake the handler thread
 */
irqreturn_t emgd_driver_irq_handler(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *) arg;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret = IRQ_NONE;
	struct drm_i915_master_private *master_priv;
	interrupt_data_t int_data;
	struct timeval now;
	struct drm_crtc *crtc;
	emgd_crtc_t *emgd_crtc;
	int previous_track_index;
	unsigned long irqflags;
	int i;
	bool storm_detected = false;
#ifdef MSVDX_SUPPORT
	u32 iir;
#endif

	atomic_inc(&dev_priv->irq_received);

	if (isr_helper() == ISR_NOT_OURS) {
		return ret;
	}

	ret = IRQ_HANDLED;

	request_interrupt_info(&int_data);

	if (dev->primary->master) {
		master_priv = dev->primary->master->driver_priv;
		if (master_priv->sarea_priv)
			master_priv->sarea_priv->last_dispatch =
				READ_BREADCRUMB(dev_priv);
	}

	if (int_data.irq_graphtrans) {
		if (int_data.irq_graphtrans & IGD_IRQ_GT_RND)
			notify_ring(dev, &dev_priv->ring[RCS]);
		if (int_data.irq_graphtrans & IGD_IRQ_GT_VID)
			notify_ring(dev, &dev_priv->ring[VCS]);
		if (int_data.irq_graphtrans & IGD_IRQ_GT_BLT)
			notify_ring(dev, &dev_priv->ring[BCS]);
	}

	if (int_data.irq_display) {
		if (int_data.irq_display & IGD_IRQ_DISP_FLIP_A) {
			intel_prepare_page_flip(dev, 0);
			crtc_pageflip_handler(dev, 0);
		}
		if (int_data.irq_display & IGD_IRQ_DISP_FLIP_B) {
			intel_prepare_page_flip(dev, 1);
			crtc_pageflip_handler(dev, 1);
		}

		/*
		 * OTC has removed these 2 calls to drm_handle_vblank in their code
		 * Ref commit: c2fb7916927e989ea424e61ce5fe617e54878827
		 */
		if (int_data.irq_display & IGD_IRQ_DISP_VBLANK_A) {
			drm_handle_vblank(dev, 0);
			drm_vblank_count_and_time(dev,0, &now);
			
		#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
			intel_atomic_handle_vblank(dev, 0);
		#endif
 			list_for_each_entry(crtc, &dev->mode_config.crtc_list,head) {
                                emgd_crtc = container_of(crtc, emgd_crtc_t, base);
                                if(emgd_crtc->pipe == 0){
					previous_track_index = emgd_crtc->track_index - 1;
					if(previous_track_index == -1){
						previous_track_index = MAX_VBLANK_STORE - 1;							
					}
					if(!((emgd_crtc->vblank_timestamp[previous_track_index].tv_sec == now.tv_sec) &&
					     (emgd_crtc->vblank_timestamp[previous_track_index].tv_usec == now.tv_usec))) {
                                        	if(emgd_crtc->track_index == MAX_VBLANK_STORE)
                                                	emgd_crtc->track_index = 0;	
                                        	emgd_crtc->vblank_timestamp[emgd_crtc->track_index].tv_sec 
									= now.tv_sec;
                                        	emgd_crtc->vblank_timestamp[emgd_crtc->track_index].tv_usec 
									= now.tv_usec;
                                        	emgd_crtc->track_index += 1; // next position 
                                	}
				}
                        }
		}
		if (int_data.irq_display & IGD_IRQ_DISP_VBLANK_B) {
			drm_handle_vblank(dev, 1);
			drm_vblank_count_and_time(dev,1, &now);

		#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
			intel_atomic_handle_vblank(dev, 1);
		#endif
                        list_for_each_entry(crtc, &dev->mode_config.crtc_list,head) {
                                emgd_crtc = container_of(crtc, emgd_crtc_t, base);
                                if(emgd_crtc->pipe == 1){
                                        previous_track_index = emgd_crtc->track_index - 1;
                                        if(previous_track_index == -1){
                                                previous_track_index = MAX_VBLANK_STORE - 1;
                                        }

					if(!((emgd_crtc->vblank_timestamp[previous_track_index].tv_sec == now.tv_sec) &&
					     (emgd_crtc->vblank_timestamp[previous_track_index].tv_usec == now.tv_usec))) {
						if(emgd_crtc->track_index == MAX_VBLANK_STORE)
							emgd_crtc->track_index = 0;

						emgd_crtc->vblank_timestamp[emgd_crtc->track_index].tv_sec 
									= now.tv_sec;
						emgd_crtc->vblank_timestamp[emgd_crtc->track_index].tv_usec 
									= now.tv_usec;
						emgd_crtc->track_index += 1; // next position 
					}
                                }
                        }
			
		}
		if (int_data.irq_display & IGD_IRQ_SPRITE_FLIP_A) {
          	/* For Stereo 3D only handle on the interrupt
             * for sprite B and sprite D, which only send
             * back single event for pageflip for two sprites
         	 */
			EMGD_DEBUG("Sprite A flip complete detected");
		}
		if (int_data.irq_display & IGD_IRQ_SPRITE_FLIP_B) {
			sprite_pageflip_handler(dev, 0);
			EMGD_DEBUG("Sprite B flip complete detected");
		}
		if (int_data.irq_display & IGD_IRQ_SPRITE_FLIP_C) {
          	/* For Stereo 3D only handle on the interrupt
             * for sprite B and sprite D, which only send
             * back single event for pageflip for two sprites
         	 */
			EMGD_DEBUG("Sprite C flip complete detected");
		}
		if (int_data.irq_display & IGD_IRQ_SPRITE_FLIP_D) {
			sprite_pageflip_handler(dev, 1);
			EMGD_DEBUG("Sprite D flip complete detected");
		}
	}

	if (int_data.irq_hpd & IGD_IRQ_HPD_DIG_B ||
		int_data.irq_hpd & IGD_IRQ_HPD_DIG_C ||
		int_data.irq_hpd & IGD_IRQ_HPD_CRT) {

			spin_lock_irqsave(&dev_priv->irq_lock, irqflags);
			dev_priv->hotplug_event_bits = int_data.irq_hpd;

			/*
			 * All the pins are marked as "HPD_ENABLED" initially. If there is a storm detected
			 * all the pins will then marked as "HPD_MARK_DISABLED". At that point, at that
			 * point all the hotplug interrupts will be disabled and polling mechanism starts
			 * kicks in until we reenable back the interrupt mechanism (after a period of time).
			 */

			for (i = 0; i < HPD_NUM_PINS; i++) {
				if (dev_priv->hpd_stats[i].hpd_mark != HPD_ENABLED)
					continue;

				dev_priv->hpd_event_bits |= (1 << i);
				if (!time_in_range(jiffies, dev_priv->hpd_stats[i].hpd_last_jiffies,
					dev_priv->hpd_stats[i].hpd_last_jiffies
					+ msecs_to_jiffies(HPD_STORM_DETECT_PERIOD))) {
					dev_priv->hpd_stats[i].hpd_last_jiffies = jiffies;
					dev_priv->hpd_stats[i].hpd_cnt = 0;
					EMGD_DEBUG("[HPD interrupt] Received HPD interrupt on PIN %d - cnt: 0\n", i);
				} else if (dev_priv->hpd_stats[i].hpd_cnt > HPD_STORM_THRESHOLD) {
					dev_priv->hpd_event_bits &= ~(1 << i);
					storm_detected = true;
					dev_priv->hpd_stats[i].hpd_mark = HPD_MARK_DISABLED;
					EMGD_ERROR("[HPD interrupt] HPD interrupt storm detected on PIN %d\n", i);
				} else {
					dev_priv->hpd_stats[i].hpd_cnt++;
					EMGD_DEBUG("[HPD interrupt] Received HPD interrupt on PIN %d - cnt: %d\n", i,
					dev_priv->hpd_stats[i].hpd_cnt);
				}
			}

			if (storm_detected) {
				EMGD_DEBUG("[HPD interrupt] storm detected!!!");
				/* Disable all the hotplug interrupt */
				valleyview_hpd_irq_setup(dev);
			}

			spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);

			EMGD_DEBUG("[HPD interrupt] Call emgd_hotplug_work_func");
			schedule_work(&dev_priv->hotplug_work);
	}

#ifdef MSVDX_SUPPORT
	iir = I915_READ(VLV_IIR);
	if (iir & VLV_VED_BLOCK_INTERRUPT)
		vlv_ved_irq_handler(dev);
#endif

	clear_interrupt_cache();

	return ret;
} /* emgd_driver_irq_handler() */

static void valleyview_hpd_init(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	struct drm_connector *connector;
	int i;

	for (i = 0; i < HPD_NUM_PINS; i++) {
		dev_priv->hpd_stats[i].hpd_cnt = 0;
		dev_priv->hpd_stats[i].hpd_mark = HPD_ENABLED;
	}

	/*
	 * upon initializing, drm_kms_helper_poll_init we are able to swap in
	 * between polling and interrupt mechanisms. DRM_CONNECTOR_POLL_HPD is
	 * set if we want to use interrupt mechanism.
	 *
	 * However, if we notify the drm core to poll our connectors, we may need
	 * need to set DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT
	 * for all our connectors.
	 */

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		connector->polled = DRM_CONNECTOR_POLL_HPD;
	}
}

static void valleyview_hpd_irq_setup(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	struct drm_encoder *encoder;
	emgd_encoder_t *emgd_encoder;
	u32 hotplug_en, adpa;

	assert_spin_locked(&dev_priv->irq_lock);

	hotplug_en = I915_READ(PORT_HOTPLUG_EN);
	hotplug_en &= ~(HDMIB_HOTPLUG_INT_EN | DPC_HOTPLUG_INT_EN);

	adpa = I915_READ(ADPA);
	adpa &= ~(BIT23);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		emgd_encoder = container_of(encoder, emgd_encoder_t, base);
		if (dev_priv->hpd_stats[emgd_encoder->hpd_pin].hpd_mark == HPD_ENABLED) {
			hotplug_en |= hpd_mask_i915[emgd_encoder->hpd_pin];

			/*
			 * set BIT 23 for analog port control register (0x61100)
			 * in the case where we need to enable CRT hotplug Enable
			 */
			if (emgd_encoder->hpd_pin == HPD_CRT)
				adpa |= (BIT23);
		}
	}

	I915_WRITE(PORT_HOTPLUG_EN, hotplug_en);
	I915_WRITE(ADPA, adpa);
}

static void i915_reenable_hotplug_timer_func(unsigned long data)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *)data;
	struct drm_device *dev = dev_priv->dev;
	unsigned long irqflags;
	struct drm_connector *connector;
	emgd_connector_t *emgd_connector;
	emgd_encoder_t *emgd_encoder;
	int i;

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);

	/*
	 *  Over here, we will marked all the pins to be HPD_ENABLED and
	 *  and set back all the connectors to DRM_CONNECTOR_POLL_HPD
	 *  in order to use interupt mechanism.
	 */

	for (i = 0; i < HPD_NUM_PINS; i++) {

		if (dev_priv->hpd_stats[i].hpd_mark == HPD_ENABLED)
			continue;

		dev_priv->hpd_stats[i].hpd_mark = HPD_ENABLED;

		list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
			emgd_connector = container_of(connector, emgd_connector_t, base);
			emgd_encoder = emgd_connector->encoder;
			if (emgd_encoder->hpd_pin == i) {
				EMGD_DEBUG("Reenabling HPD on connector %s\n",
				drm_get_connector_name(connector));
				connector->polled = DRM_CONNECTOR_POLL_HPD;
			}
		}
	}

	valleyview_hpd_irq_setup(dev);
	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);
}

void i915_check_and_clear_faults(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int i;

	if (INTEL_INFO(dev)->gen < 6)
		return;

	for_each_ring(ring, dev_priv, i) {
		u32 fault_reg;
		fault_reg = I915_READ(RING_FAULT_REG(ring));
		if (fault_reg & RING_FAULT_VALID) {
					DRM_DEBUG_DRIVER("Unexpected fault\n"
					"\tAddr: 0x%08lx\\n"
					"\tAddress space: %s\n"
					"\tSource ID: %d\n"
					"\tType: %d\n",
					fault_reg & PAGE_MASK,
					fault_reg & RING_FAULT_GTTSEL_MASK ? "GGTT" : "PPGTT",
					RING_FAULT_SRCID(fault_reg),
					RING_FAULT_FAULT_TYPE(fault_reg));
			I915_WRITE(RING_FAULT_REG(ring),
				fault_reg & ~RING_FAULT_VALID);
		}
	}
	POSTING_READ(RING_FAULT_REG(&dev_priv->ring[RCS]));
}

static int emgd_pci_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	if (PCI_FUNC(pdev->devfn)) {
		return -ENODEV;
	}

	/*
	 * Name changed at some point in time.  2.6.35 uses drm_get_dev
	 * and 2.6.38 uses drm_get_pci_dev  Need to figure what kernel
	 * version this changed.
	 */
	return drm_get_pci_dev(pdev, ent, &driver);
}

static void emgd_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

static int emgd_pm_suspend(struct device *dev)
{
#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret;

	if(!drm_dev || !drm_dev->dev_private) {
		EMGD_ERROR("DRM not initialized, aborting freeze.");
		return -ENODEV;
	}

	if(drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	ret = emgd_drm_freeze(drm_dev);
	if(ret)
		return ret;
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return ret;
#else
	return 0;
#endif
}

static int emgd_pm_freeze(struct device *dev)
{
#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	if(!drm_dev || !drm_dev->dev_private) {
		EMGD_ERROR("DRM not initialized, aborting freeze.");
		return -ENODEV;
	}

	ret= emgd_drm_freeze(drm_dev);
	return ret;
#else
	return 0;
#endif
}

static int emgd_pm_thaw(struct device *dev)
{
#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev= pci_get_drvdata(pdev);

	if (drm_core_check_feature(drm_dev, DRIVER_MODESET)) {
		mutex_lock(&drm_dev->struct_mutex);
		i915_gem_restore_gtt_mappings(drm_dev);
		mutex_unlock(&drm_dev->struct_mutex);
	} else if (drm_core_check_feature(drm_dev, DRIVER_MODESET))
		i915_check_and_clear_faults(drm_dev);

	intel_uncore_sanitize(drm_dev);

	return emgd_drm_restore(drm_dev);
#else
	return 0;
#endif
}

static int emgd_pm_poweroff(struct device *dev)
{
#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev= pci_get_drvdata(pdev);

	return emgd_drm_freeze(drm_dev);
#else
	return 0;
#endif
}

static int emgd_pm_restore(struct device *dev)
{
#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	if (pci_enable_device(pdev)) {
		return -EIO;
	}

	pci_set_master(pdev);

	if (drm_core_check_feature(drm_dev, DRIVER_MODESET)) {
		mutex_lock(&drm_dev->struct_mutex);
		i915_gem_restore_gtt_mappings(drm_dev);
		mutex_unlock(&drm_dev->struct_mutex);
	}

	ret = emgd_drm_restore(drm_dev);
	if (ret) {
		return ret;
	}

	drm_kms_helper_poll_enable(drm_dev);

	return ret;
#else
	return 0;
#endif
}


#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
int emgd_drm_freeze(struct drm_device *dev)
{
	int ret=0;
	drm_emgd_private *priv = dev->dev_private;

	EMGD_TRACE_ENTER;

	intel_set_power_well(dev, true);

	drm_kms_helper_poll_disable(dev);

	pci_save_state(dev->pdev);

	/* If KMS is active, we do the leave vt stuff here */
	if(drm_core_check_feature(dev, DRIVER_MODESET)) {
		/* FIXME: Is it should be call GEM idle first,
		 *	  before call uninstall drm irq?
		 *	  In i915_gem_leavevt_ioctl func, it call drm_irq_uninstall first,
		 *	  then only call i915_gem_idle function.
		 *	  Question: Will it be causing problem if GEM not yet idle already
		 *	  uninstall the drm irq?
		 */
		mutex_lock(&dev->struct_mutex);
		ret = i915_gem_idle(dev);
		mutex_unlock(&dev->struct_mutex);
		if(ret) {
			EMGD_ERROR("GEM idle failed, resume might fail");
					return ret;
		}

		cancel_delayed_work_sync(&priv->rps.delayed_resume_work);

		drm_irq_uninstall(dev);
	}

	intel_disable_gt_powersave(dev);
	i915_gem_restore_gtt_mappings(dev);
	i915_gem_cleanup_stolen(dev);

	/* When the system is suspended, the X server does a VT switch, which saves
	 * the register state of the X server, and restores the console's register
	 * state.  This code saves the console's register state, so that after the
	 * system resumes, VT switches to the console can occur.
	 */
	EMGD_DEBUG("Saving the console's register state");
	priv->suspended_state =
		drm_HAL_context->module_dispatch.reg_alloc(drm_HAL_context,
			IGD_REG_SAVE_ALL);

	if (priv->suspended_state) {
		drm_HAL_context->module_dispatch.reg_save(drm_HAL_context,
			priv->suspended_state);
	}

	/* FIXME
	 *EMGD_DEBUG("Calling pwr_alter()");
	 *ret = drm_HAL_dispatch->pwr_alter(drm_HAL_handle, IGD_POWERSTATE_D1);
	 *EMGD_DEBUG("pwr_alter() returned %d", ret);
	 *EMGD_ERROR("slim50: over pwr_alter (ret %d)", ret);
	 */

	priv->modeset_on_lid=0;

	EMGD_TRACE_EXIT;
	return ret;

} /* emgd_drm_freeze() */

#else
int emgd_drm_freeze(struct drm_device *dev)
{
	return 0;
}
#endif




#ifdef CONFIG_POWER_MANAGEMENT_SUPPORT
int emgd_drm_restore(struct drm_device *dev)
{
	int ret = 0;
	drm_emgd_private *priv = dev->dev_private;

	EMGD_TRACE_ENTER;

	/* call to clear out old GT FIFO errors */
	emgd_uncore_early_sanitize(dev);

	/* Restore the register state of the console, so that after the X server is
	 * back up, VT switches to the console can occur.
	 */
	EMGD_DEBUG("Restoring the console's register state");
	if (priv->suspended_state) {
		drm_HAL_context->module_dispatch.reg_restore(drm_HAL_context,
			priv->suspended_state);

		drm_HAL_context->module_dispatch.reg_free(drm_HAL_context,
			priv->suspended_state);
		priv->suspended_state = NULL;
	}

	if(drm_core_check_feature(dev, DRIVER_MODESET)) {
		mutex_lock(&dev->struct_mutex);

		ret = i915_gem_init_hw(dev);
		mutex_unlock(&dev->struct_mutex);
		if (ret != 0) {
			EMGD_DEBUG("Failed initialize gem ringbuffer");
		}

		drm_mode_config_reset(dev);
		drm_irq_install(dev);
		emgd_power_init_hw(dev);

		drm_helper_resume_force_mode(dev);
	}

	priv->modeset_on_lid = 0;
	EMGD_TRACE_EXIT;
	return ret;
} /* emgd_drm_restore() */
#else
int emgd_drm_restore(struct drm_device *dev)
{
	return 0;
}
#endif


static const struct dev_pm_ops emgd_pm_ops = {
	.suspend = emgd_pm_suspend,
	.resume = emgd_pm_restore,
	.freeze = emgd_pm_freeze,
	.thaw = emgd_pm_thaw,
	.poweroff = emgd_pm_poweroff,
	.restore = emgd_pm_restore,
};

/*
 * NOTE: The remainder of this file is standard kernel module initialization
 * code:
 */


/*
 * Older kernels kept the PCI driver information directly in the
 * DRM driver structure.  Newer kernels (2.6.38-rc3 and beyond)
 * move it outside of the DRM driver structure and pass it to
 * drm_pci_init instead in order to help pave the way for
 * USB graphics devices.
 */
#define EMGD_PCI_DRIVER {    \
	.name     = DRIVER_NAME, \
	.id_table = pciidlist,   \
	.probe = emgd_pci_probe, \
	.remove = emgd_pci_remove, \
	.driver.pm = &emgd_pm_ops, \
}
static struct pci_driver emgd_pci_driver = EMGD_PCI_DRIVER;

static struct vm_operations_struct i915_gem_vm_ops = {
	.fault = i915_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

#ifdef CONFIG_COMPAT
#define EMGD_COMPAT_IOCTL .compat_ioctl = i915_compat_ioctl,
#else
#define EMGD_COMPAT_IOCTL
#endif

#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
#define EMGD_FOPS_COMPAT
#else
#define EMGD_FOPS_COMPAT .fasync = drm_fasync,
#endif

#define EMGD_FOPS { \
	.owner   = THIS_MODULE,        \
	.open    = drm_open,           \
	.release = drm_release,        \
	.unlocked_ioctl   = drm_ioctl, \
	.mmap = drm_gem_mmap,          \
	.poll    = drm_poll,           \
	EMGD_FOPS_COMPAT	       \
	.read    = drm_read,           \
	EMGD_COMPAT_IOCTL              \
	.llseek = noop_llseek,         \
}

/*
 * Kernel 3.3 and beyond pull driver fops out into a constant structure that
 * is simply referenced from the main driver structure.  Earlier kernels
 * define the fops inline.
 */
static const struct file_operations emgd_driver_fops = EMGD_FOPS;


/**
 * DRM Sub driver entry points
 */
static struct drm_driver driver = {
	.driver_features    = DRIVER_GEM | DRIVER_USE_AGP | DRIVER_REQUIRE_AGP
	                    | DRIVER_MODESET
                            | DRIVER_PRIME
                              ,
	.load               = emgd_driver_load,
	.unload             = emgd_driver_unload,
	.open               = emgd_driver_open,
	.lastclose          = emgd_driver_lastclose,
	.preclose           = emgd_driver_preclose,
	.postclose          = emgd_driver_postclose,
	.suspend            = emgd_driver_suspend,
	.resume             = emgd_driver_resume,
	.device_is_agp      = emgd_driver_device_is_agp,
	.get_vblank_counter = emgd_driver_get_vblank_counter,
	.enable_vblank      = emgd_driver_enable_vblank,
	.disable_vblank     = emgd_driver_disable_vblank,
	.irq_preinstall     = emgd_driver_irq_preinstall,
	.irq_postinstall    = emgd_driver_irq_postinstall,
	.irq_uninstall      = emgd_driver_irq_uninstall,
	.irq_handler        = emgd_driver_irq_handler,
	.master_create      = emgd_driver_master_create,
	.master_destroy     = emgd_driver_master_destroy,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_init       = i915_debugfs_init,
	.debugfs_cleanup    = i915_debugfs_cleanup,
#endif
#if (LTSI_VERSION < KERNEL_VERSION(3, 13, 0))
	.gem_init_object = i915_gem_init_object,
#endif
	.gem_free_object = i915_gem_free_object,
	.gem_vm_ops = &i915_gem_vm_ops,
	.dumb_create = i915_gem_dumb_create,
	.dumb_map_offset = i915_gem_mmap_gtt,
#if (LTSI_VERSION < KERNEL_VERSION(3, 10, 0))
	.dumb_destroy = i915_gem_dumb_destroy,
#else
	.dumb_destroy = drm_gem_dumb_destroy,
#endif
	.ioctls             = emgd_ioctl,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = i915_gem_prime_export,
	.gem_prime_import = i915_gem_prime_import,
	.fops                = &emgd_driver_fops,

	.name                = DRIVER_NAME,
	.desc                = DRIVER_DESC,
	.date                = DRIVER_DATE,
	.major               = DRIVER_MAJOR,
	.minor               = DRIVER_MINOR,
	.patchlevel          = DRIVER_PATCHLEVEL,
};

/**
 * Standard procedure to initialize this kernel module when it is loaded.
 */
static int __init emgd_init(void) {
	int ret;

	EMGD_TRACE_ENTER;

	driver.driver_features &= ~(DRIVER_USE_AGP | DRIVER_REQUIRE_AGP);

	driver.num_ioctls = emgd_max_ioctl;

	ret = drm_pci_init(&driver, &emgd_pci_driver);
	EMGD_TRACE_EXIT;
	return ret;
}

/**
 * Standard procedure to clean-up this kernel module before it exits & unloads.
 */
static void __exit emgd_exit(void) {
	EMGD_TRACE_ENTER;

	drm_pci_exit(&driver, &emgd_pci_driver);
	EMGD_TRACE_EXIT;
}


module_init(emgd_init);
module_exit(emgd_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
