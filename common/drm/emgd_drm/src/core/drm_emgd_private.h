/*
 *-----------------------------------------------------------------------------
 * Filename: drm_emgd_private.h
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
 *  This file contains the EMGD-specific drm_device.dev_private structure as
 *  well as definitions to help glue i915 GEM code to the EMGD DRM.
 *-----------------------------------------------------------------------------
 */
#ifndef _DRM_EMGD_PRIVATE_H_
#define _DRM_EMGD_PRIVATE_H_

#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <igd_core_structs.h>
#include <linux/io-mapping.h>
#include <drm/intel-gtt.h>

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
#include <drm/drm_flip.h>
#endif

#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/intel-iommu.h>
#include "i915/intel_bios.h"
#include "emgd_drm.h"
#include <uapi/drm/i915_drm.h>

/*
 * Something below needs defines from the i915_reg.h file, but including
 * it here causes the driver to fail.
 */
/* #include "i915/i915_reg.h" */

/* FIXME: This define should not be here.  Find a better place to put it */
#define PSB_GATT_RESOURCE   2

/*********************/
/* i915 declarations */
/*********************/

#define pipe_name(p) ((p) + 'A')
#define plane_name(p) ((p) + 'A')
enum port {
    PORT_A = 0,
    PORT_B,
    PORT_C,
    PORT_D,
    PORT_E,
    I915_MAX_PORTS
};
#define port_name(p) ((p) + 'A')

#define I915_GEM_GPU_DOMAINS	(~(I915_GEM_DOMAIN_CPU | I915_GEM_DOMAIN_GTT))

#define for_each_pipe(p) for ((p) = 0; (p) < dev_priv->num_pipe; (p)++)

#define WATCH_LISTS	0
#define WATCH_GTT      0

#define I915_GEM_PHYS_CURSOR_0 1
#define I915_GEM_PHYS_CURSOR_1 2
#define I915_GEM_PHYS_OVERLAY_REGS 3
#define I915_MAX_PHYS_OBJECT (I915_GEM_PHYS_OVERLAY_REGS)

struct drm_i915_gem_phys_object {
	int id;
	struct page **page_list;
	drm_dma_handle_t *handle;
	struct drm_i915_gem_object *cur_obj;
};

struct mem_block {
	struct mem_block *next;
	struct mem_block *prev;
	int start;
	int size;
	struct drm_file *file_priv; /* NULL: free, -1: heap, other: real files */
};

struct opregion_header;
struct opregion_acpi;
struct opregion_swsci;
struct opregion_asle;

struct intel_opregion {
	struct opregion_header *header;
	struct opregion_acpi *acpi;
	struct opregion_swsci *swsci;
	struct opregion_asle *asle;
	void *vbt;
	u32 __iomem *lid_state;
};
#define OPREGION_SIZE            (8*1024)

struct intel_overlay;
struct intel_overlay_error_state;

struct drm_i915_master_private {
	drm_local_map_t *sarea;
	struct _drm_i915_sarea *sarea_priv;
};
#define I915_FENCE_REG_NONE -1
#define I915_MAX_NUM_FENCES 16
/* 16 fences + sign bit for FENCE_REG_NONE */
#define I915_MAX_NUM_FENCE_BITS 5


struct drm_i915_fence_reg {
	struct list_head lru_list;
	struct drm_i915_gem_object *obj;
	int pin_count;
};

struct sdvo_device_mapping {
	u8 initialized;
	u8 dvo_port;
	u8 slave_addr;
	u8 dvo_wiring;
	u8 i2c_pin;
	u8 i2c_speed;
	u8 ddc_pin;
};

struct intel_display_error_state;

struct drm_i915_error_state {
        struct kref ref;
        u32 eir;
        u32 pgtbl_er;
        u32 ier;
        u32 ccid;
        u32 derrmr;
        u32 forcewake;
        bool waiting[I915_NUM_RINGS];
        u32 pipestat[I915_MAX_PIPES];
        u32 tail[I915_NUM_RINGS];
        u32 head[I915_NUM_RINGS];
        u32 ctl[I915_NUM_RINGS];
        u32 ipeir[I915_NUM_RINGS];
        u32 ipehr[I915_NUM_RINGS];
        u32 instdone[I915_NUM_RINGS];
        u32 acthd[I915_NUM_RINGS];
        u32 semaphore_mboxes[I915_NUM_RINGS][I915_NUM_RINGS - 1];
        u32 semaphore_seqno[I915_NUM_RINGS][I915_NUM_RINGS - 1];
        u32 rc_psmi[I915_NUM_RINGS]; /* sleep state */
        /* our own tracking of ring head and tail */
        u32 cpu_ring_head[I915_NUM_RINGS];
        u32 cpu_ring_tail[I915_NUM_RINGS];
        u32 error; /* gen6+ */
        u32 err_int; /* gen7 */
        u32 instpm[I915_NUM_RINGS];
        u32 instps[I915_NUM_RINGS];
        u32 extra_instdone[I915_NUM_INSTDONE_REG];
        u32 seqno[I915_NUM_RINGS];
        u64 bbaddr;                        
        u32 fault_reg[I915_NUM_RINGS];
        u32 done_reg;
        u32 faddr[I915_NUM_RINGS];
        u64 fence[I915_MAX_NUM_FENCES];
        struct timeval time;
	struct drm_i915_error_ring {
                struct drm_i915_error_object {
                        int page_count;
                        u32 gtt_offset;
                        u32 *pages[0];
                } *ringbuffer, *batchbuffer;
                struct drm_i915_error_request {
                        long jiffies;
                        u32 seqno;
                        u32 tail;
                } *requests;
                int num_requests;
        } ring[I915_NUM_RINGS];
        struct drm_i915_error_buffer {
                u32 size;
                u32 name;
                u32 rseqno, wseqno;
                u32 gtt_offset;
                u32 read_domains;
                u32 write_domain;
                s32 fence_reg:I915_MAX_NUM_FENCE_BITS;
                s32 pinned:2;
                u32 tiling:2;
                u32 dirty:1;
                u32 purgeable:1;
                s32 ring:4;
                u32 cache_level:2;
        } *active_bo, *pinned_bo;
        u32 active_bo_count, pinned_bo_count;
        struct intel_overlay_error_state *overlay;
        struct intel_display_error_state *display;
};

struct intel_device_info {
	u8 gen;
	u8 is_mobile : 1;
	u8 is_i85x : 1;
	u8 is_i915g : 1;
	u8 is_i945gm : 1;
	u8 is_g33 : 1;
	u8 need_gfx_hws : 1;
	u8 is_g4x : 1;
	u8 is_pineview : 1;
	u8 is_broadwater : 1;
	u8 is_crestline : 1;
	u8 is_ivybridge : 1;
	u8 is_valleyview:1;
	u8 has_force_wake:1;
	u8 is_haswell:1;
	u8 has_fbc : 1;
	u8 has_pipe_cxsr : 1;
	u8 has_hotplug : 1;
	u8 cursor_needs_physical : 1;
	u8 has_overlay : 1;
	u8 overlay_needs_physical : 1;
	u8 supports_tv : 1;
	u8 has_bsd_ring : 1;
	u8 has_blt_ring : 1;
	u8 has_llc:1;
	u8 has_scheduler:1;
	u8 has_vebox:1;
};

enum i915_cache_level {
	I915_CACHE_NONE = 0,
	I915_CACHE_LLC, /* also used for snoopable memory on non-LLC */
	I915_CACHE_L3_LLC, /* gen7+, L3 sits between the domain specifc
			      caches, eg sampler/render caches, and the
			      large Last-Level-Cache. LLC is coherent with
			      the CPU, but L3 is only visible to the GPU. */
	I915_CACHE_WT, /* hsw:gt3e WriteThrough for scanouts */
};

typedef uint32_t gen6_gtt_pte_t;

struct i915_address_space {
	struct drm_mm mm;
	struct drm_device *dev;
	struct list_head global_link;
	unsigned long start;            /* Start offset always 0 for dri2 */
	size_t total;           /* size addr space maps (ex. 2GB for ggtt) */

	struct {
		dma_addr_t addr;
		struct page *page;
	} scratch;

	/**
	 * List of objects currently involved in rendering.
	 *
	 * Includes buffers having the contents of their GPU caches
	 * flushed, not necessarily primitives. last_rendering_seqno
	 * represents when the rendering involved will be completed.
	 *
	 * A reference is held on the buffer while on this list.
	 */
	struct list_head active_list;
	/**
	 * LRU list of objects which are not in the ringbuffer and
	 * are ready to unbind, but are still in the GTT.
	 *
	 * last_rendering_seqno is 0 while an object is in this list.
	 *
	 * A reference is not held on the buffer while on this list,
	 * as merely being GTT-bound shouldn't prevent its being
	 * freed, and we'll pull it off the list in the free path.
	 */
	struct list_head inactive_list;

	/* FIXME: Need a more generic return type */
	gen6_gtt_pte_t (*pte_encode)(dma_addr_t addr,
			enum i915_cache_level level);
		void (*clear_range)(struct i915_address_space *vm,
				    unsigned int first_entry,
				    unsigned int num_entries);
		void (*insert_entries)(struct i915_address_space *vm,
				       struct sg_table *st,
				       unsigned int first_entry,
				       enum i915_cache_level cache_level);
		void (*cleanup)(struct i915_address_space *vm);
};


/* The Graphics Translation Table is the way in which GEN hardware translates a
* Graphics Virtual Address into a Physical Address. In addition to the normal
* collateral associated with any va->pa translations GEN hardware also has a
* portion of the GTT which can be mapped by the CPU and remain both coherent
* and correct (in cases like swizzling). That region is referred to as GMADR in
* the spec.
*/
struct i915_gtt {
	struct i915_address_space base;
	size_t stolen_size;             /* Total size of stolen memory */
	unsigned long mappable_end;     /* End offset that we can CPU map */
	struct io_mapping *mappable;    /* Mapping to our CPU mappable region */
	phys_addr_t mappable_base;      /* PA of our GMADR */

	/** "Graphics Stolen Memory" holds the global PTEs */
	void __iomem *gsm;
	bool do_idle_maps;

	/* global gtt ops */
	int (*gtt_probe)(struct drm_device *dev, size_t *gtt_total,
			 size_t *stolen, phys_addr_t *mappable_base,
			 unsigned long *mappable_end);
};

#define gtt_total_entries(gtt) ((gtt).base.total >> PAGE_SHIFT)

struct i915_hw_ppgtt {
	struct i915_address_space base;
	unsigned num_pd_entries;
	struct page **pt_pages;
	uint32_t pd_offset;
	dma_addr_t *pt_dma_addr;

	int (*enable)(struct drm_device *dev);
};

/* To make things as simple as possible (ie. no refcounting), a VMA's lifetime
 * will always be <= an objects lifetime. So object refcounting should cover us.
 */

struct i915_vma {
	struct drm_mm_node node;
	struct drm_i915_gem_object *obj;
	struct i915_address_space *vm;

	/** This object's place on the active/inactive lists */
	struct list_head mm_list;

	struct list_head vma_link; /* Link in the object's VMA list */

	/** This vma's place in the batchbuffer or on the eviction list */
	struct list_head exec_list;

	/**
	 * Used for performing relocations during execbuffer insertion.
	 */
	struct hlist_node exec_node;
	unsigned long exec_handle;
	struct drm_i915_gem_exec_object2 *exec_entry;

};

struct i915_ctx_hang_stats {
	/* This context had batch pending when hang was declared */
	unsigned batch_pending;

	/* This context had batch active when hang was declared */
	unsigned batch_active;

	/* Time when this context was last blamed for a GPU reset */
	unsigned long guilty_ts;

	/* This context is banned to submit more work */
	bool banned;
};

/* This must match up with the value previously used for execbuf2.rsvd1. */
#define DEFAULT_CONTEXT_ID 0
struct i915_hw_context {
	struct kref ref;
	int id;
	bool is_initialized;
	uint8_t remap_slice;
	struct drm_i915_file_private *file_priv;
	struct intel_ring_buffer *ring;
	struct drm_i915_gem_object *obj;
	struct i915_ctx_hang_stats hang_stats;

	struct list_head link;
};

struct i915_fbc {
	unsigned long size;
	unsigned int fb_id;
	enum plane plane;
	int y;

	struct drm_mm_node *compressed_fb;
	struct drm_mm_node *compressed_llb;

	struct intel_fbc_work {
		struct delayed_work work;
		struct drm_crtc *crtc;
		struct drm_framebuffer *fb;
		int interval;
	} *fbc_work;

	enum {
		FBC_NO_OUTPUT, /* no outputs enabled to compress */
		FBC_STOLEN_TOO_SMALL, /* not enough space for buffers */
		FBC_UNSUPPORTED_MODE, /* interlace or doublescanned mode */
		FBC_MODE_TOO_LARGE, /* mode too large for compression */
		FBC_BAD_PLANE, /* fbc not supported on plane */
		FBC_NOT_TILED, /* buffer not tiled */
		FBC_MULTIPLE_PIPES, /* more than one pipe active */
		FBC_MODULE_PARAM,
		FBC_CHIP_DEFAULT, /* disabled by default on this chip */
	} no_fbc_reason;
};

enum intel_pch {
	PCH_IBX,	/* Ibexpeak PCH */
	PCH_CPT,	/* Cougarpoint PCH */
};

#define QUIRK_PIPEA_FORCE (1<<0)
#define QUIRK_LVDS_SSC_DISABLE (1<<1)

struct intel_watermark_params {
	unsigned long fifo_size;
	unsigned long max_wm;
	unsigned long default_wm;
	unsigned long guard_size;
	unsigned long cacheline_size;
};

struct cxsr_latency {
	int is_desktop;
	int is_ddr3;
	unsigned long fsb_freq;
	unsigned long mem_freq;
	unsigned long display_sr;
	unsigned long display_hpll_disable;
	unsigned long cursor_sr;
	unsigned long cursor_hpll_disable;
};

struct drm_i915_display_funcs {
	void (*dpms)(struct drm_crtc *crtc, int mode);
	bool (*fbc_enabled)(struct drm_device *dev);
	void (*enable_fbc)(struct drm_crtc *crtc, unsigned long interval);
	void (*disable_fbc)(struct drm_device *dev);
	int (*get_display_clock_speed)(struct drm_device *dev);
	int (*get_fifo_size)(struct drm_device *dev, int plane);
	void (*update_wm)(struct drm_device *dev);
	void (*sanitize_pm)(struct drm_device *dev);
	void (*update_sprite_wm)(struct drm_device *dev, int pipe,
			uint32_t sprite_width, int pixel_size);
	void (*update_linetime_wm)(struct drm_device *dev, int pipe,
			struct drm_display_mode *mode);
	int (*crtc_mode_set)(struct drm_crtc *crtc,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			int x, int y,
			struct drm_framebuffer *old_fb);
	void (*off)(struct drm_crtc *crtc);
	void (*write_eld)(struct drm_connector *connector,
			struct drm_crtc *crtc);
	void (*fdi_link_train)(struct drm_crtc *crtc);
	void (*init_clock_gating)(struct drm_device *dev);
	int (*queue_flip)(struct drm_device *dev, struct drm_crtc *crtc,
			struct drm_framebuffer *fb,
			struct drm_i915_gem_object *obj);
	int (*update_plane)(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			int x, int y);
	/* clock updates for mode set */
	/* cursor updates */
	/* render clock increase/decrease */
	/* display clock increase/decrease */
	/* pll clock increase/decrease */
};

struct intel_uncore_funcs {
	void (*force_wake_get)(struct drm_i915_private *dev_priv);
	void (*force_wake_put)(struct drm_i915_private *dev_priv);
};

struct intel_uncore {
	spinlock_t lock; /** lock is also taken in irq contexts. */

	struct intel_uncore_funcs funcs;

	unsigned fifo_count;
	unsigned forcewake_count;
};

struct i915_ums_state {
	/**
	 * Flag if the X Server, and thus DRM, is not currently in
	 * control of the device.
	 *
	 * This is set between LeaveVT and EnterVT.  It needs to be
	 * replaced with a semaphore.  It also needs to be
	 * transitioned away from for kernel modesetting.
	 */
	int mm_suspended;
};

#define MAX_L3_SLICES 2
struct intel_l3_parity {
	u32 *remap_info[MAX_L3_SLICES];
	struct work_struct error_work;
	int which_slice;
};

enum sched_policy {
	SCHED_POLICY_PRT = 1,
	SCHED_POLICY_HT
};
enum resv_policy {
	RESV_POLICY_PE = 1,
	RESV_POLICY_AE
};

#define MAX_SCHED_PRIORITIES    100
	/* this cant be too big as its also an int array size */
#define SHARED_NORMAL_PRIORITY MAX_SCHED_PRIORITIES-1    
#define SHARED_ROGUE_PRIORITY MAX_SCHED_PRIORITIES    
#define INITIAL_SHARED_PERIOD    50000 /* usecs */
#define INITIAL_SHARED_CAPACITY  35000 /* usecs */
#define INITIAL_ROGUE_PERIOD  500000 /* usecs */
#define INITIAL_ROGUE_CAPACITY  1000 /* usecs */
#define INITIAL_ROGUE_TRIGGER 60000 /*usecs, trigger to send into rogue*/

struct drm_i915_error_state_buf {
	unsigned bytes;
	unsigned size;
	int err;
	u8 *buf;
	loff_t start;
	loff_t pos;
};

struct i915_error_state_file_priv {
	struct drm_device *dev;
	struct drm_i915_error_state *error;
};

struct i915_gpu_error {
	/* For hangcheck timer */
#define DRM_I915_HANGCHECK_PERIOD 1500 /* in ms */
#define DRM_I915_HANGCHECK_JIFFIES msecs_to_jiffies(DRM_I915_HANGCHECK_PERIOD)
	struct timer_list hangcheck_timer;

	/* For reset and error_state handling. */
	spinlock_t lock;
	/* Protected by the above dev->gpu_error.lock. */
	struct drm_i915_error_state *first_error;
	struct work_struct work;


	unsigned long missed_irq_rings;

	unsigned long last_reset;

	/**
	 * State variable controlling the reset flow
	 *
	 * Upper bits are for the reset counter.  This counter is used by the
	 * wait_seqno code to race-free noticed that a reset event happened and
	 * that it needs to restart the entire ioctl (since most likely the
	 * seqno it waited for won't ever signal anytime soon).
	 *
	 * This is important for lock-free wait paths, where no contended lock
	 * naturally enforces the correct ordering between the bail-out of the
	 * waiter and the gpu reset work code.
	 *
	 * Lowest bit controls the reset state machine: Set means a reset is in
	 * progress. This state will (presuming we don't have any bugs) decay
	 * into either unset (successful reset) or the special WEDGED value (hw
	 * terminally sour). All waiters on the reset_queue will be woken when
	 * that happens.
	 */
	atomic_t reset_counter;

	/**
	 * Special values/flags for reset_counter
	 *
	 * Note that the code relies on
	 *      I915_WEDGED & I915_RESET_IN_PROGRESS_FLAG
	 * being true.
	 */
#define I915_RESET_IN_PROGRESS_FLAG    1
#define I915_WEDGED                    0xffffffff

	/**
	 * Waitqueue to signal when the reset has completed. Used by clients
	 * that wait for dev_priv->mm.wedged to settle.
	 */
	wait_queue_head_t reset_queue;

	/* For gpu hang simulation. */
	unsigned int stop_rings;

	/* For missed irq/seqno simulation. */
	unsigned int test_irq_rings;
};


struct i915_gem_mm {
	/** Memory allocator for GTT stolen memory */
	struct drm_mm stolen;
	/** List of all objects in gtt_space. Used to restore gtt
	* mappings on resume */
	struct list_head bound_list;
	/**
	* List of objects which are not bound to the GTT (thus
	* are idle and not used by the GPU) but still have
	* (presumably uncached) pages still attached.
	*/
	struct list_head unbound_list;

	/** Usable portion of the GTT for GEM */
	unsigned long stolen_base; /* limited to low memory (32-bit) */

	int gtt_mtrr;

	/** PPGTT used for aliasing the PPGTT with the GTT */
	struct i915_hw_ppgtt *aliasing_ppgtt;

	struct shrinker inactive_shrinker;
	bool shrinker_no_lock_stealing;

	/** LRU list of objects with fence regs on them. */
	struct list_head fence_list;

	/**
	* We leave the user IRQ off as much as possible,
	* but this means that requests will finish and never
	* be retired once the system goes idle. Set a timer to
	* fire periodically while the ring is running. When it
	* fires, go retire requests.
	*/
	struct delayed_work retire_work;

	/**
	* Are we in a non-interruptible section of code like
	* modesetting?
	*/
	bool interruptible;

	/** Bit 6 swizzling required for X tiling */
	uint32_t bit_6_swizzle_x;
	/** Bit 6 swizzling required for Y tiling */
	uint32_t bit_6_swizzle_y;

	/* storage for physical objects */
	struct drm_i915_gem_phys_object *phys_objs[I915_MAX_PHYS_OBJECT];

	/* accounting, useful for userland debugging */
	spinlock_t object_stat_lock;
	size_t object_memory;
	u32 object_count;
};


/*typedef struct _drm_emgd_private {*/
typedef struct drm_i915_private {

	/*********************/
	/* i915 declarations */
	/*********************/
	struct drm_device *dev;
	struct kmem_cache *slab;

	const struct intel_device_info *info;
	unsigned int gtt_mappable_entries;
	unsigned int gtt_total_entries;

	size_t stolen_size;             /* Total size of stolen memory */
	int has_gem;
	int relative_constants_mode;

	void __iomem *regs;
	struct intel_uncore uncore;

	struct intel_gmbus {
		struct i2c_adapter adapter;
		struct i2c_adapter *force_bit;
		u32 reg0;
	} *gmbus;

	struct pci_dev *bridge_dev;
	struct intel_ring_buffer ring[I915_NUM_RINGS];
	uint32_t last_seqno, next_seqno;

	drm_dma_handle_t *status_page_dmah;
	uint32_t counter;
	drm_local_map_t hws_map;
	struct resource mch_res;

	unsigned int cpp;
	int back_offset;
	int front_offset;
	int current_page;
	int page_flipping;

	atomic_t irq_received;

	/* protects the irq masks */
	spinlock_t irq_lock;
	/** Cached value of IMR to avoid reads in updating the bitfield */
	u32 pipestat[2];
	u32 irq_mask;
	u32 gt_irq_mask;
	u32 pch_irq_mask;

	u32 hotplug_supported_mask;
	struct work_struct hotplug_work;
	u32 hotplug_event_bits;

	struct {
		unsigned long hpd_last_jiffies;
		int hpd_cnt;
		enum {
			HPD_ENABLED = 0,
			HPD_DISABLED = 1,
			HPD_MARK_DISABLED = 2
		} hpd_mark;
	} hpd_stats[HPD_NUM_PINS];
	u32 hpd_event_bits;
	struct timer_list hotplug_reenable_timer;

	int tex_lru_log_granularity;
	struct mem_block *agp_heap;
	unsigned int sr01, adpa, ppcr, dvob, dvoc, lvds;
	int num_pipe;

	struct i915_fbc fbc;
	struct intel_opregion opregion;

	/* overlay */
	struct intel_overlay *overlay;
	bool sprite_scaling_enabled;

	/* LVDS info */
	int backlight_level;  /* restore backlight to this value */
	bool backlight_enabled;
	struct drm_display_mode *panel_fixed_mode;
	struct drm_display_mode *lfp_lvds_vbt_mode; /* if any */
	struct drm_display_mode *sdvo_lvds_vbt_mode; /* if any */

	/* Feature bits from the VBIOS */
	unsigned int int_tv_support:1;
	unsigned int lvds_dither:1;
	unsigned int lvds_vbt:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	unsigned int fdi_rx_polarity_inverted:1;
	int lvds_ssc_freq;
	struct {
		int rate;
		int lanes;
		int preemphasis;
		int vswing;

		bool initialized;
		bool support;
		int bpp;
		struct edp_power_seq pps;
	} edp;
	bool no_aux_handshake;

	struct notifier_block lid_notifier;

	int crt_ddc_pin;
	struct drm_i915_fence_reg fence_regs[16]; /* assume 965 */
	int fence_reg_start; /* 4 if userland hasn't ioctl'd us yet */
	int num_fence_regs; /* 8 on pre-965, 16 otherwise */

	unsigned int fsb_freq, mem_freq, is_ddr3;

	/* Protected by dev->error_lock. */
	struct drm_i915_error_state *first_error;

	struct workqueue_struct *wq;

	/* Display functions */
	struct drm_i915_display_funcs display;

	/* PCH chipset type */
	enum intel_pch pch_type;

	unsigned long quirks;

	/* Register state */
	bool modeset_on_lid;
	u8 saveLBB;
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 saveDSPARB;
	u32 saveHWS;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveBCLRPAT_A;
	u32 saveTRANSACONF;
	u32 saveTRANS_HTOTAL_A;
	u32 saveTRANS_HBLANK_A;
	u32 saveTRANS_HSYNC_A;
	u32 saveTRANS_VTOTAL_A;
	u32 saveTRANS_VBLANK_A;
	u32 saveTRANS_VSYNC_A;
	u32 savePIPEASTAT;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPAADDR;
	u32 saveDSPASURF;
	u32 saveDSPATILEOFF;
	u32 savePFIT_PGM_RATIOS;
	u32 saveBLC_HIST_CTL;
	u32 saveBLC_PWM_CTL;
	u32 saveBLC_PWM_CTL2;
	u32 saveBLC_CPU_PWM_CTL;
	u32 saveBLC_CPU_PWM_CTL2;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveBCLRPAT_B;
	u32 saveTRANSBCONF;
	u32 saveTRANS_HTOTAL_B;
	u32 saveTRANS_HBLANK_B;
	u32 saveTRANS_HSYNC_B;
	u32 saveTRANS_VTOTAL_B;
	u32 saveTRANS_VBLANK_B;
	u32 saveTRANS_VSYNC_B;
	u32 savePIPEBSTAT;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBADDR;
	u32 saveDSPBSURF;
	u32 saveDSPBTILEOFF;
	u32 saveVGA0;
	u32 saveVGA1;
	u32 saveVGA_PD;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 savePP_ON_DELAYS;
	u32 savePP_OFF_DELAYS;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_DIVISOR;
	u32 savePFIT_CONTROL;
	u32 save_palette_a[256];
	u32 save_palette_b[256];
	u32 saveDPFC_CB_BASE;
	u32 saveFBC_CFB_BASE;
	u32 saveFBC_LL_BASE;
	u32 saveFBC_CONTROL;
	u32 saveFBC_CONTROL2;
	u32 saveIER;
	u32 saveIIR;
	u32 saveIMR;
	u32 saveDEIER;
	u32 saveDEIMR;
	u32 saveGTIER;
	u32 saveGTIMR;
	u32 saveFDI_RXA_IMR;
	u32 saveFDI_RXB_IMR;
	u32 saveCACHE_MODE_0;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF2[3];
	u8 saveMSR;
	u8 saveSR[8];
	u8 saveGR[25];
	u8 saveAR_INDEX;
	u8 saveAR[21];
	u8 saveDACMASK;
	u8 saveCR[37];
	uint64_t saveFENCE[16];
	u32 saveCURACNTR;
	u32 saveCURAPOS;
	u32 saveCURABASE;
	u32 saveCURBCNTR;
	u32 saveCURBPOS;
	u32 saveCURBBASE;
	u32 saveCURSIZE;
	u32 saveDP_B;
	u32 saveDP_C;
	u32 saveDP_D;
	u32 savePIPEA_GMCH_DATA_M;
	u32 savePIPEB_GMCH_DATA_M;
	u32 savePIPEA_GMCH_DATA_N;
	u32 savePIPEB_GMCH_DATA_N;
	u32 savePIPEA_DP_LINK_M;
	u32 savePIPEB_DP_LINK_M;
	u32 savePIPEA_DP_LINK_N;
	u32 savePIPEB_DP_LINK_N;
	u32 saveFDI_RXA_CTL;
	u32 saveFDI_TXA_CTL;
	u32 saveFDI_RXB_CTL;
	u32 saveFDI_TXB_CTL;
	u32 savePFA_CTL_1;
	u32 savePFB_CTL_1;
	u32 savePFA_WIN_SZ;
	u32 savePFB_WIN_SZ;
	u32 savePFA_WIN_POS;
	u32 savePFB_WIN_POS;
	u32 savePCH_DREF_CONTROL;
	u32 saveDISP_ARB_CTL;
	u32 savePIPEA_DATA_M1;
	u32 savePIPEA_DATA_N1;
	u32 savePIPEA_LINK_M1;
	u32 savePIPEA_LINK_N1;
	u32 savePIPEB_DATA_M1;
	u32 savePIPEB_DATA_N1;
	u32 savePIPEB_LINK_M1;
	u32 savePIPEB_LINK_N1;
	u32 saveMCHBAR_RENDER_STANDBY;
	u32 savePCH_PORT_HOTPLUG;

	struct list_head vm_list; /* Global list of all address spaces */
	struct i915_gtt gtt; /* VMA representing the global address space */
	struct i915_gem_mm mm;


	/* Old dri1 support infrastructure, beware the dragons ya fools entering
	 * here! */
	struct {
		unsigned allow_batchbuffer : 1;
	} dri1;

	/* Kernel Modesetting */

	struct sdvo_device_mapping sdvo_mappings[2];
	/* indicate whether the LVDS_BORDER should be enabled or not */
	unsigned int lvds_border_bits;
	/* Panel fitter placement and size for Ironlake+ */
	u32 pch_pf_pos, pch_pf_size;
	int panel_t3, panel_t12;

	struct drm_crtc *plane_to_crtc_mapping[2];
	struct drm_crtc *pipe_to_crtc_mapping[2];
	wait_queue_head_t pending_flip_queue;
	bool flip_pending_is_done;

	/* Reclocking support */
	bool render_reclock_avail;
	bool lvds_downclock_avail;
	/* indicates the reduced downclock for LVDS*/
	int lvds_downclock;
	u16 orig_clock;
	int child_dev_num;
	struct child_device_config *child_dev;
	struct drm_connector *int_lvds_connector;
	struct drm_connector *int_edp_connector;

	bool mchbar_need_disable;

	struct intel_l3_parity l3_parity;

	/* Cannot be determined by PCIID. You must always read a register. */
	size_t ellc_size;

	/* Old ums support infrastructure, same warning applies. */
	struct i915_ums_state ums;

	/* gen6+ rps state */
	struct {
		/* work and pm_iir are protected by dev_priv->irq_lock */
		struct delayed_work work;
		u32 pm_iir;

		/* On vlv we need to manually drop to Vmin with a delayed work. */
		struct delayed_work vlv_work;

		/* The below variables an all the rps hw state are protected by
		 * dev->struct mutext. */
		u8 cur_delay;
		u8 min_delay;
		u8 max_delay;
		u8 rpe_delay;
		u8 hw_max;

		/* Once using i915_max_freq to set the GPU frequency, GPU frequency
		will not change based on workload */
		u8 max_lock;

		struct delayed_work delayed_resume_work;

		/*
		 * Protects RPS/RC6 register access and PCU communication.
		 * Must be taken after struct_mutex if nested.
		 */
		struct mutex hw_lock;
	} rps;

	/* ilk-only ips/rps state. Everything in here is protected by the global
	 * mchdev_lock in intel_pm.c */
	struct {
		u8 cur_delay;
		u8 min_delay;
		u8 max_delay;
		u8 fmax;
		u8 fstart;

		u64 last_count1;
		unsigned long last_time1;
		unsigned long chipset_power;
		u64 last_count2;
		struct timespec last_time2;
		unsigned long gfx_power;
		u8 corr;

		int c_m;
		int r_t;

		struct drm_i915_gem_object *pwrctx;
		struct drm_i915_gem_object *renderctx;
	} ips;
	spinlock_t *mchdev_lock;


	struct drm_mm_node *vlv_pctx;

	struct i915_gpu_error gpu_error;

	/* list of fbdev register on this device */
	/*struct intel_fbdev *fbdev;*/

	struct backlight_device *backlight[2];

	struct drm_property *broadcast_rgb_property;
	struct drm_property *force_audio_property;

	bool hw_contexts_disabled;
    uint32_t hw_context_size;
	struct list_head context_list;


	/*********************/
	/* EMGD declarations */
	/*********************/

	/**
	 * Saved state of the console, when suspending (or hibernating) the system.
	 * emgd_driver_suspend() allocates this, and emgd_driver_resume() frees
	 * this.
	 */
	void *suspended_state;

	/** The context is set during the DRM module load function. */
	igd_context_t *context;

	/**
	 * The primary display handle is copied to here each time alter_displays()
	 * is called via an ioctl.
	 */
	emgd_crtc_t * primary;

	/**
	 * The secondary display handle is copied to here each time alter_displays()
	 * is called via an ioctl.
	 */
	emgd_crtc_t * secondary;

	/**
	 * Store the device information so it can be passed back to userspace
	 * callers via an ioctl.
	 */
	igd_init_info_t *init_info;

	struct fb_info         *fbdev;
	emgd_crtc_t            *crtcs[IGD_MAX_PIPES];
	int                     num_crtc;

	struct {
		struct drm_property *z_order_prop;
		struct drm_property *fb_blend_ovl_prop;
	} crtc_properties;

	emgd_fbdev_t           *emgd_fbdev;
	emgd_framebuffer_t      initfb_info;
	/* Below here is a second FB that we never register to the OS fbdevice/console
	 * its only used if default config requested a seperate FB - though it actually
	 * gets "set-mode" during init mode set, the fbdev does setup the modes again 
	 * for itself and thus this ends up lying around not being used - though i guess
	 * there will be future uses when we talk about splash screen at startup w/o FB
	 */
	emgd_framebuffer_t      initfb_info2;

	/* contextual information for overlay / sprite plane management */
	int                     num_ovlplanes;
	igd_ovl_plane_t        *ovl_planes;
	int                     num_ovldrmformats;
	uint32_t               *ovl_drmformats;

	/* overlay plane properties */
	struct {
		struct drm_property *brightness;
		struct drm_property *contrast;
		struct drm_property *saturation;
		struct drm_property *hue;
		struct drm_property *gamma_enable;
		struct drm_property *gamma_red;
		struct drm_property *gamma_green;
		struct drm_property *gamma_blue;
		struct drm_property *const_alpha;
	} ovl_properties;

	/* in conjunction, here is context info for splash video */
	emgd_framebuffer_t     *spvideo_fb;
	int                     spvideo_holdplane;
	int                     spvideo_num_enabled;
#ifdef SPLASH_VIDEO_SUPPORT
	__u32			spvideo_fb_ids[SPVIDEO_MAX_FB_SUPPORTED];
	__u32			spvideo_fbs_allocated;
#endif
	int                     spvideo_planeid[IGD_MAX_PIPES];
	int 			spvideo_test_enabled;

	/* This is how much memory driver can allocate
	 * it is defined via "page_request" field in igd_param_t structure,
	 * see default_config.c file, by default size is 256M
	 * driver return ENOMEM if total size of allocated memory more than this value*/
	size_t maximal_allocated_memory_size;


	int num_clients;
	/* Keep a list of all clients who have an open file */
	struct list_head i915_client_list;
			/* points to all this client's "file_private" 
			 * ptr that reps each drm_i915_file_private */
	/* GPU Scheduler settings:
	 * Scheduler Policy   : SCHED_POLICY_PRT vs SCHED_POLICY_HT
	 * Reservation Policy : RESV_POLICY_PE vs RESV_POLICY_AE
	 * Default Period for new proces
	 * Default Capacity for new process
	 * Period for all processes at shared priority 100
	 * Capacitly for all processes at shared  priority 100
	 */
	struct {
		struct spinlock sched_lock;
		/* Shared normal slot knobs */
		int shared_period;  /* NOTE: this is usec, sysfs converts to / from msec */
		int shared_capacity;/* NOTE: this is usec, sysfs converts to / from msec */
		bool shared_aereserve;/* TRUE = use AE for budget reservation, else use PE */
		/* Shared rogue slot and demotion trigger time knobs */
		bool rogue_switch;
		int rogue_trigger;  /* NOTE: this is usec, sysfs converts to / from msec */
		int rogue_period;  /* NOTE: this is usec, sysfs converts to / from msec */
		int rogue_capacity;/* NOTE: this is usec, sysfs converts to / from msec */
		/* Video auto-promotion trigger time knobs */
		bool video_switch;
		int video_priority;
		int video_init_period;
		int video_init_capacity;

		/* operational runtime context for shared-normal-slot */
		unsigned long shared_balance; /* is jiffies */
		unsigned long shared_last_refresh_jiffs; /* is jiffies */
		
		/* operational runtime context for shared-rogue-slot */
		unsigned long rogue_balance; /* is jiffies */
		unsigned long rogue_last_refresh_jiffs; /* is jiffies */
		
		int req_prty_cnts[MAX_SCHED_PRIORITIES+1];
		struct drm_i915_file_private * req_prty_fp[MAX_SCHED_PRIORITIES+1];
		
		/* Sysfs objects that are used during driver life */
		struct emgd_obj *gemsched_sysfs_obj;
		struct emgd_obj *gemsched_sysfs_shared_obj;
		struct emgd_obj *gemsched_sysfs_video_obj;
		struct emgd_obj *gemsched_sysfs_rogue_obj;
		struct emgd_obj *gemsched_sysfs_process_obj;

	} scheduler;



#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	struct drm_flip_driver flip_driver;

	struct {
		struct list_head list;
		spinlock_t lock;
		struct work_struct work;
		struct workqueue_struct *wq;
		unsigned int next_flip_seq;
		unsigned int queue_len;
	} flip;	
#endif

#ifdef MSVDX_SUPPORT
	/* necessary resource sharing with ved driver. */
	struct {
		struct platform_device *platdev;
		int	irq;
	} ved;
#endif

} drm_emgd_private_t,  drm_i915_private_t; /*, drm_emgd_private, drm_i915_private;*/

typedef struct drm_i915_private drm_emgd_private;

static inline struct drm_i915_private *to_i915(const struct drm_device *dev)
{
	return dev->dev_private;
}

/* Iterate over initialised rings */
#define for_each_ring(ring__, dev_priv__, i__) \
    for ((i__) = 0; (i__) < I915_NUM_RINGS; (i__)++) \
        if (((ring__) = &(dev_priv__)->ring[(i__)]), intel_ring_initialized((ring__)))

/*
 * Used to map drm properties to emgd attributes.
 */
typedef struct _prop_map {
	struct list_head head;
	struct drm_property *property;
	igd_attr_t *attribute;
} emgd_property_map_t;


struct drm_i915_gem_object_ops {
	    /* Interface between the GEM object and its backing storage.
		 * get_pages() is called once prior to the use of the associated set
		 * of pages before to binding them into the GTT, and put_pages() is
		 * called after we no longer need them. As we expect there to be
		 * associated cost with migrating pages between the backing storage
		 * and making them available for the GPU (e.g. clflush), we may hold
		 * onto the pages after they are no longer referenced by the GPU
		 * in case they may be used again shortly (for example migrating the
		 * pages to a different memory domain within the GTT). put_pages()
		 * will therefore most likely be called when the object itself is
		 * being released or under memory pressure (where we attempt to
		 * reap pages for the shrinker).
		 */
	    int (*get_pages)(struct drm_i915_gem_object *);
		    void (*put_pages)(struct drm_i915_gem_object *);
};

#define I915_GTT_OFFSET_NONE ((u32)-1)

struct drm_i915_gem_object {
	struct drm_gem_object base;

	const struct drm_i915_gem_object_ops *ops;

	/** List of VMAs backed by this object */
	struct list_head vma_list;

	/** Stolen memory for this object, instead of being backed by shmem. */
	struct drm_mm_node *stolen;
	struct list_head global_list;

	struct list_head ring_list;
	/** Used in execbuf to temporarily hold a ref */
	struct list_head obj_exec_link;

	/**
	 * This is set if the object is on the active lists (has pending
	 * rendering and so a non-zero seqno), and is not set if it i s on
	 * inactive (ready to be unbound) list.
	 */
	unsigned int active : 1;

	/**
	 * This is set if the object has been written to since last bound
	 * to the GTT
	 */
	unsigned int dirty : 1;

	/**
	 * Fence register bits (if any) for this object.  Will be set
	 * as needed when mapped into the GTT.
	 * Protected by dev->struct_mutex.
	 *
	 * Size: 4 bits for 16 fences + sign (for FENCE_REG_NONE)
	 */
	signed int fence_reg : 5;

	/**
	 * Advice: are the backing pages purgeable?
	 */
	unsigned int madv : 2;

	/**
	 * Current tiling mode for the object.
	 */
	unsigned int tiling_mode : 2;
	/**
	 * Whether the tiling parameters for the currently associated fence
	 * register have changed. Note that for the purposes of tracking
	 * tiling changes we also treat the unfenced register, the register
	 * slot that the object occupies whilst it executes a fenced
	 * command (such as BLT on gen2/3), as a "fence".
	 */
	unsigned int fence_dirty:1;

	/** How many users have pinned this object in GTT space. The following
	 * users can each hold at most one reference: pwrite/pread, pin_ioctl
	 * (via user_pin_count), execbuffer (objects are not allowed multiple
	 * times for the same batchbuffer), and the framebuffer code. When
	 * switching/pageflipping, the framebuffer code has at most two buffers
	 * pinned per crtc.
	 *
	 * In the worst case this is 1 + 1 + 1 + 2*2 = 7. That would fit into 3
	 * bits with absolutely no headroom. So use 4 bits. */
	unsigned int pin_count : 4;
#define DRM_I915_GEM_OBJECT_MAX_PIN_COUNT 0xf

	/**
	 * Is the object at the current location in the gtt mappable and
	 * fenceable? Used to avoid costly recalculations.
	 */
	unsigned int map_and_fenceable : 1;

	/**
	 * Whether the current gtt mapping needs to be mappable (and isn't just
	 * mappable by accident). Track pin and fault separate for a more
	 * accurate mappable working set.
	 */
	unsigned int fault_mappable : 1;
	unsigned int pin_mappable : 1;
	unsigned int pin_display:1;

	/*
	 * Is the GPU currently using a fence to access this buffer,
	 */
	unsigned int pending_fenced_gpu_access:1;
	unsigned int fenced_gpu_access:1;

	unsigned int cache_level:3;

	unsigned int has_aliasing_ppgtt_mapping:1;
	unsigned int has_global_gtt_mapping:1;

	/*
	 * Is the object to be mapped as read-only to the GPU
	 * Only honoured if hardware has relevant pte bit
	 */
	unsigned long gt_ro;
	unsigned long gt_old_ro;

	 unsigned int has_dma_mapping:1;

	struct sg_table *pages;
	int pages_pin_count;

	void *dma_buf_vmapping;
	int vmapping_count;

	struct intel_ring_buffer *ring;

	/** Breadcrumb of last rendering to the buffer. */
	uint32_t last_read_seqno;
	uint32_t last_write_seqno;
	/** Breadcrumb of last fenced GPU access to the buffer. */
	uint32_t last_fenced_seqno;

	/** Current tiling stride for the object, if it's tiled. */
	uint32_t stride;

	/** Record of address bit 17 of each page at last unbind. */
	unsigned long *bit_17;

	/** User space pin count and filp owning the pin */
	uint32_t user_pin_count;
	struct drm_file *pin_filp;

	/** for phy allocated objects */
	struct drm_i915_gem_phys_object *phys_obj;

	/**
	 * Number of crtcs where this object is currently the fb, but
	 * will be page flipped away on the next vblank.  When it
	 * reaches 0, dev_priv->pending_flip_queue will be woken up.
	 */
	atomic_t pending_flip;
};
#define to_gem_object(obj) (&((struct drm_i915_gem_object *)(obj))->base)

#define to_intel_bo(x) container_of(x, struct drm_i915_gem_object, base)

/**
 * Request queue structure.
 *
 * The request queue allows us to note sequence numbers that have been emitted
 * and may be associated with active buffers to be retired.
 *
 * By keeping this list, we can avoid having to do questionable
 * sequence-number comparisons on buffer last_rendering_seqnos, and associate
 * an emission time with seqnos for tracking how far ahead of the GPU we are.
 */
struct drm_i915_gem_request {
	/** On Which ring this request was generated */
	struct intel_ring_buffer *ring;

	/** GEM sequence number associated with this request. */
	uint32_t seqno;

	/** Position in ringbuffer specific timestamp data page */
	struct drm_i915_gem_exec_timestamp * timestamp;

	/** Position in the ringbuffer of the start of the request */
	u32 head;

	/** Postion in the ringbuffer of the end of the request */
	u32 tail;

	/** Context related to this request */
	struct i915_hw_context *ctx;

	/** Batch buffer related to this request if any */
	struct drm_i915_gem_object *batch_obj;

	/** Time at which this request was emitted, in jiffies. */
	unsigned long emitted_jiffies;

	/** global list entry for this request */
	struct list_head list;

	struct drm_i915_file_private *file_priv;
	/** file_priv list entry for this request */
	struct list_head client_request;
		/* referenced by drm_i915_file_private */

	/*submission time of the request*/
	struct timeval submission_time;
};

#define MOVING_AVG_RANGE     15
#define PID_T_FROM_DRM_FILE(file) pid_nr(file->pid)

struct drm_i915_file_private {
	struct drm_file *drm_file;
	struct {
		struct spinlock lock;
		struct list_head request_list; 
			/* points to all this client's "client_request" 
			 * ptr that reps each drm_i915_gem_request */
		/* scheduling variables */
		int outstanding_requests;
		int sched_priority;
		int sched_period;  /* NOTE: this is usec, sysfs converts to / from msec */
		int sched_capacity;/* NOTE: this is usec, sysfs converts to / from msec */
		unsigned long sched_balance; /* NOTE: this is jiffies */
		/* time keeping variables */
		unsigned long sched_last_refresh_jiffs; /* is jiffies */
		int exec_times[MOVING_AVG_RANGE]; /* is usecs */
		struct timeval submission_times[MOVING_AVG_RANGE];
		unsigned long last_exec_slot; /* index into exec_times array */
		int sched_avgexectime; /* is usecs*/	
	
		/* sys-fs object for runtime gem-scheduler nodes */
		struct emgd_obj *gemsched_sysfs_client_obj;
	} mm;
	struct list_head client_link;
			/*referenced by drm_i915_private's i915_client_list */
	struct idr context_idr;
	struct i915_ctx_hang_stats hang_stats;
	struct list_head pending_flips;
};

enum intel_chip_family {
	CHIP_I8XX = 0x01,
	CHIP_I9XX = 0x02,
	CHIP_I915 = 0x04,
	CHIP_I965 = 0x08,
};

#define INTEL_INFO(dev)        (to_i915(dev)->info)

#define IS_I85X(dev)		(INTEL_INFO(dev)->is_i85x)
#define IS_I915G(dev)		(INTEL_INFO(dev)->is_i915g)
#define IS_I945GM(dev)		(INTEL_INFO(dev)->is_i945gm)
#define IS_BROADWATER(dev)	(INTEL_INFO(dev)->is_broadwater)
#define IS_CRESTLINE(dev)	(INTEL_INFO(dev)->is_crestline)
#define IS_G4X(dev)		(INTEL_INFO(dev)->is_g4x)
#define IS_PINEVIEW(dev)	(INTEL_INFO(dev)->is_pineview)
#define IS_G33(dev)		(INTEL_INFO(dev)->is_g33)
#define IS_IVYBRIDGE(dev)	(INTEL_INFO(dev)->is_ivybridge)
#define IS_VALLEYVIEW(dev)	(INTEL_INFO(dev)->is_valleyview)
#define IS_HASWELL(dev)		(INTEL_INFO(dev)->is_haswell)
#define IS_MOBILE(dev)		(INTEL_INFO(dev)->is_mobile)
#if (LTSI_VERSION > KERNEL_VERSION(3, 10, 0))
#define IS_I830(dev)		((dev)->pdev->device == 0x3577)
#define IS_845G(dev)		((dev)->pdev->device == 0x2562)
#define IS_I865G(dev)		((dev)->pdev->device == 0x2572)
#define IS_I915GM(dev)		((dev)->pdev->device == 0x2592)
#define IS_I945G(dev)		((dev)->pdev->device == 0x2772)
#define IS_GM45(dev)		((dev)->pdev->device == 0x2A42)
#define IS_PINEVIEW_G(dev)	((dev)->pdev->device == 0xa001)
#define IS_PINEVIEW_M(dev)	((dev)->pdev->device == 0xa011)
#define IS_IRONLAKE_D(dev)	((dev)->pdev->device == 0x0042)
#define IS_IRONLAKE_M(dev)	((dev)->pdev->device == 0x0046)
#define IS_IVB_GT1(dev)		((dev)->pdev->device == 0x0156 || \
				(dev)->pdev->device == 0x0152 || \
				(dev)->pdev->device == 0x015a)
#define IS_SNB_GT1(dev)		((dev)->pdev->device == 0x0102 || \
				(dev)->pdev->device == 0x0106 || \
				(dev)->pdev->device == 0x010A)
#else
#define IS_I830(dev)		((dev)->pci_device == 0x3577)
#define IS_845G(dev)		((dev)->pci_device == 0x2562)
#define IS_I865G(dev)		((dev)->pci_device == 0x2572)
#define IS_I915GM(dev)		((dev)->pci_device == 0x2592)
#define IS_I945G(dev)		((dev)->pci_device == 0x2772)
#define IS_GM45(dev)		((dev)->pci_device == 0x2A42)
#define IS_PINEVIEW_G(dev)	((dev)->pci_device == 0xa001)
#define IS_PINEVIEW_M(dev)	((dev)->pci_device == 0xa011)
#define IS_IRONLAKE_D(dev)	((dev)->pci_device == 0x0042)
#define IS_IRONLAKE_M(dev)	((dev)->pci_device == 0x0046)
#define IS_IVB_GT1(dev)		((dev)->pci_device == 0x0156 || \
				(dev)->pci_device == 0x0152 || \
				(dev)->pci_device == 0x015a)
#define IS_SNB_GT1(dev)		((dev)->pci_device == 0x0102 || \
				(dev)->pci_device == 0x0106 || \
				(dev)->pci_device == 0x010A)
#endif

/*
 * The genX designation typically refers to the render engine, so render
 * capability related checks should use IS_GEN, while display and other checks
 * have their own (e.g. HAS_PCH_SPLIT for ILK+ display, IS_foo for particular
 * chips, etc.).
 */
#define IS_GEN2(dev)	(INTEL_INFO(dev)->gen == 2)
#define IS_GEN3(dev)	(INTEL_INFO(dev)->gen == 3)
#define IS_GEN4(dev)	(INTEL_INFO(dev)->gen == 4)
#define IS_GEN5(dev)	(INTEL_INFO(dev)->gen == 5)
#define IS_GEN6(dev)	(INTEL_INFO(dev)->gen == 6)
#define IS_GEN7(dev)	(INTEL_INFO(dev)->gen == 7)

#define HAS_BSD(dev)            (INTEL_INFO(dev)->has_bsd_ring)
#define HAS_BLT(dev)            (INTEL_INFO(dev)->has_blt_ring)
#define HAS_LLC(dev)            (INTEL_INFO(dev)->has_llc)
#define HAS_WT(dev)            (IS_HASWELL(dev) && to_i915(dev)->ellc_size)
#define HAS_VEBOX(dev)            (INTEL_INFO(dev)->has_vebox)
#define I915_NEED_GFX_HWS(dev)	(INTEL_INFO(dev)->need_gfx_hws)

#define HAS_HW_CONTEXTS(dev)   (INTEL_INFO(dev)->gen >= 6)
#define HAS_ALIASING_PPGTT(dev)        (INTEL_INFO(dev)->gen >=6)

#define HAS_OVERLAY(dev)		(INTEL_INFO(dev)->has_overlay)
#define OVERLAY_NEEDS_PHYSICAL(dev)	(INTEL_INFO(dev)->overlay_needs_physical)

/* Early gen2 have a totally busted CS tlb and require pinned batches. */
#define HAS_BROKEN_CS_TLB(dev)         (IS_I830(dev) || IS_845G(dev))

/* With the 945 and later, Y tiling got adjusted so that it was 32 128-byte
 * rows, which changed the alignment requirements and fence programming.
 */
#define HAS_128_BYTE_Y_TILING(dev) (!IS_GEN2(dev) && !(IS_I915G(dev) || \
						      IS_I915GM(dev)))
#define SUPPORTS_DIGITAL_OUTPUTS(dev)	((!IS_GEN2(dev) && !IS_PINEVIEW(dev)) || IS_VALLEYVIEW(dev))
#define SUPPORTS_INTEGRATED_HDMI(dev)	(IS_G4X(dev) || IS_GEN5(dev) || IS_VALLEYVIEW(dev) )
#define SUPPORTS_INTEGRATED_DP(dev)	(IS_G4X(dev) || IS_GEN5(dev) || IS_VALLEYVIEW(dev) )
#define SUPPORTS_EDP(dev)		(IS_IRONLAKE_M(dev))
#define SUPPORTS_TV(dev)		(INTEL_INFO(dev)->supports_tv)
#define I915_HAS_HOTPLUG(dev)		 (INTEL_INFO(dev)->has_hotplug)
/* dsparb controlled by hw only */
#define DSPARB_HWCONTROL(dev) (IS_G4X(dev) || IS_IRONLAKE(dev))

#define HAS_FW_BLC(dev) (INTEL_INFO(dev)->gen > 2)
#define HAS_PIPE_CXSR(dev) (INTEL_INFO(dev)->has_pipe_cxsr)
#define I915_HAS_FBC(dev) (INTEL_INFO(dev)->has_fbc)

#define HAS_PCH_SPLIT(dev) (IS_GEN5(dev) || IS_GEN6(dev) || IS_IVYBRIDGE(dev))
#define HAS_PIPE_CONTROL(dev) (INTEL_INFO(dev)->gen >= 5)

#define INTEL_PCH_TYPE(dev) (to_i915(dev)->pch_type)
#define HAS_PCH_CPT(dev) (INTEL_PCH_TYPE(dev) == PCH_CPT)
#define HAS_PCH_IBX(dev) (INTEL_PCH_TYPE(dev) == PCH_IBX)

#define HAS_POWER_WELL(dev)    (IS_HASWELL(dev))

#define HAS_FORCE_WAKE(dev) (INTEL_INFO(dev)->has_force_wake)

/* DPF == dynamic parity feature */
#define HAS_L3_DPF(dev) (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))
#define NUM_L3_SLICES(dev) (HAS_L3_DPF(dev))

#define GT_FREQUENCY_MULTIPLIER 50

/*
 * HAS_EXEC_NO_RELOC & HAS_EXEC_HANDLE_LUT become a standard I915 parameter
 * with kernel 3.9 and defined in uapi/drm/i915_drm.h
 */
#ifndef I915_PARAM_HAS_EXEC_NO_RELOC
#define I915_PARAM_HAS_EXEC_NO_RELOC  25
#endif

#define I915_PARAM_HAS_LLC              17


#ifndef I915_PARAM_HAS_EXEC_HANDLE_LUT
#define I915_PARAM_HAS_EXEC_HANDLE_LUT   26
#endif



#define I915_EXEC_NO_RELOC             (1<<11)

#ifndef __EXEC_OBJECT_UNKNOWN_FLAGS
#define EXEC_OBJECT_WRITE (1<<2)
#define __EXEC_OBJECT_UNKNOWN_FLAGS -(EXEC_OBJECT_WRITE<<1)
#endif

#ifndef EXEC_OBJECT_NEEDS_GTT
#define EXEC_OBJECT_NEEDS_GTT (1<<1)
#endif
/** Use the reloc.handle as an index into the exec object array rather
     * than as the per-file handle.
     */
#define I915_EXEC_HANDLE_LUT           (1<<12)

#define __I915_EXEC_UNKNOWN_FLAGS -(I915_EXEC_HANDLE_LUT<<1)

/*
 * HAS_VEBOX become a standard I915 parameter with kernel 3.11 and defined
 * in uapi/drm/i915_drm.h.
 */
#ifndef I915_PARAM_HAS_VEBOX
#define I915_PARAM_HAS_VEBOX           22
#endif




#include "i915/i915_trace.h"

/**
 * RC6 is a special power stage which allows the GPU to enter an very
 * low-voltage mode when idle, using down to 0V while at this stage.  This
 * stage is entered automatically when the GPU is idle when RC6 support is
 * enabled, and as soon as new workload arises GPU wakes up automatically as well.
 *
 * There are different RC6 modes available in Intel GPU, which differentiate
 * among each other with the latency required to enter and leave RC6 and
 * voltage consumed by the GPU in different states.
 *
 * The combination of the following flags define which states GPU is allowed
 * to enter, while RC6 is the normal RC6 state, RC6p is the deep RC6, and
 * RC6pp is deepest RC6. Their support by hardware varies according to the
 * GPU, BIOS, chipset and platform. RC6 is usually the safest one and the one
 * which brings the most power savings; deeper states save more power, but
 * require higher latency to switch to and wake up.
 */
#define INTEL_RC6_ENABLE            (1<<0)
#define INTEL_RC6p_ENABLE           (1<<1)
#define INTEL_RC6pp_ENABLE          (1<<2)


extern int i915_semaphores __read_mostly;
extern bool i915_enable_hangcheck __read_mostly;
extern bool i915_enable_ppgtt __read_mostly;
extern bool i915_try_reset __read_mostly;
extern int gem_scheduler __read_mostly;
extern int i915_disable_power_well __read_mostly;

int i915_emgd_init_phys_hws(struct drm_device *dev);
void i915_emgd_free_hws(struct drm_device *dev);
extern void i915_kernel_lost_context(struct drm_device * dev);

extern int i915_emit_box(struct drm_device *dev,
			 struct drm_clip_rect *box,
			 int DR1, int DR4);
extern int intel_gpu_reset(struct drm_device *dev);
extern int i915_reset(struct drm_device *dev);
#ifdef CONFIG_COMPAT
extern long i915_compat_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg);
#endif

/* i915_gem.c */
int i915_gem_init_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_create_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
int i915_gem_pread_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
int i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int i915_gem_execbuffer(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_execbuffer2(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int i915_gem_busy_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_get_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_set_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_throttle_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
int i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int i915_gem_leavevt_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int i915_gem_set_tiling(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_get_tiling(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int i915_gem_wait_ioctl(struct drm_device *dev, void *data,
						struct drm_file *file_priv);
void *i915_gem_object_alloc(struct drm_device *dev);
void i915_gem_object_free(struct drm_i915_gem_object *obj);
void i915_gem_load(struct drm_device *dev);
#if (LTSI_VERSION < KERNEL_VERSION(3, 13, 0))
int i915_gem_init_object(struct drm_gem_object *obj);
#endif
void i915_gem_object_init(struct drm_i915_gem_object *obj,
		const struct drm_i915_gem_object_ops *ops);

struct drm_i915_gem_object *i915_gem_alloc_object(struct drm_device *dev,
						  size_t size);
void i915_gem_free_object(struct drm_gem_object *obj);
void i915_gem_vma_destroy(struct i915_vma *vma);
int __must_check i915_gem_object_pin(struct drm_i915_gem_object *obj,
				     struct i915_address_space *vm,
				     uint32_t alignment,
					 bool map_and_fenceable,
                     bool nonblocking);
void i915_gem_object_unpin(struct drm_i915_gem_object *obj);
int __must_check i915_vma_unbind(struct i915_vma *vma);
int __must_check i915_gem_object_ggtt_unbind(struct drm_i915_gem_object *obj);
int i915_gem_object_put_pages(struct drm_i915_gem_object *obj);
void i915_gem_release_mmap(struct drm_i915_gem_object *obj);
void i915_gem_lastclose(struct drm_device *dev);

int __must_check i915_gem_object_get_pages(struct drm_i915_gem_object *obj);

static inline struct page *i915_gem_object_get_page(struct drm_i915_gem_object *obj, int n)
{
#if (LTSI_VERSION <= KERNEL_VERSION(3, 10, 0))
	struct scatterlist *sg = obj->pages->sgl;
	while (n >= SG_MAX_SINGLE_ALLOC) {
		sg = sg_chain_ptr(sg + SG_MAX_SINGLE_ALLOC - 1);
		n -= SG_MAX_SINGLE_ALLOC - 1;
	}
	return sg_page(sg+n);
#else
	struct sg_page_iter sg_iter;

	for_each_sg_page(obj->pages->sgl, &sg_iter, obj->pages->nents, n)
		return sg_page_iter_page(&sg_iter);
	return NULL;
#endif

}

static inline void i915_gem_object_pin_pages(struct drm_i915_gem_object *obj)
{
	BUG_ON(obj->pages == NULL);
	obj->pages_pin_count++;
}
static inline void i915_gem_object_unpin_pages(struct drm_i915_gem_object *obj)
{
	BUG_ON(obj->pages_pin_count == 0);
	obj->pages_pin_count--;
}

int __must_check i915_mutex_lock_interruptible(struct drm_device *dev);
int i915_gem_object_sync(struct drm_i915_gem_object *obj,
		struct intel_ring_buffer *to);
void i915_vma_move_to_active(struct i915_vma *vma,
			     struct intel_ring_buffer *ring);
int i915_gem_dumb_create(struct drm_file *file_priv,
			 struct drm_device *dev,
			 struct drm_mode_create_dumb *args);
int i915_gem_mmap_gtt(struct drm_file *file_priv, struct drm_device *dev,
		      uint32_t handle, uint64_t *offset);
#if (LTSI_VERSION < KERNEL_VERSION(3, 10, 0))
int i915_gem_dumb_destroy(struct drm_file *file_priv, struct drm_device *dev,
			  uint32_t handle);
#endif

static inline void __user *to_user_ptr(u64 address)
{
	return (void __user *)(uintptr_t)address;
}

/**
 * Returns true if seq1 is later than seq2.
 */
static inline bool
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return (int32_t)(seq1 - seq2) >= 0;
}

int __must_check i915_gem_get_seqno(struct drm_device *dev, u32 *seqno);
int __must_check i915_gem_set_seqno(struct drm_device *dev, u32 seqno);
int __must_check i915_gem_object_get_fence(struct drm_i915_gem_object *obj);
int __must_check i915_gem_object_put_fence(struct drm_i915_gem_object *obj);

static inline bool
i915_gem_object_pin_fence(struct drm_i915_gem_object *obj)
{
   if (obj->fence_reg != I915_FENCE_REG_NONE) {
       struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
       dev_priv->fence_regs[obj->fence_reg].pin_count++;
       return true;
   } else
   	   return false;
}

static inline void
i915_gem_object_unpin_fence(struct drm_i915_gem_object *obj)
{
   if (obj->fence_reg != I915_FENCE_REG_NONE) {
	   struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
	   dev_priv->fence_regs[obj->fence_reg].pin_count--;
   }
}

void i915_gem_retire_requests(struct drm_device *dev);
void i915_gem_retire_requests_ring(struct intel_ring_buffer *ring);
int __must_check i915_gem_check_wedge(struct i915_gpu_error *error,
                                     bool interruptible);
static inline bool i915_reset_in_progress(struct i915_gpu_error *error)
{
	return unlikely(atomic_read(&error->reset_counter)
			& I915_RESET_IN_PROGRESS_FLAG);
}

static inline bool i915_terminally_wedged(struct i915_gpu_error *error)
{
	return atomic_read(&error->reset_counter) == I915_WEDGED;
}

int i915_gem_check_olr(struct intel_ring_buffer *ring, u32 seqno);
int __must_check
i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj,
						bool readonly);
void i915_gem_reset(struct drm_device *dev);
bool i915_gem_clflush_object(struct drm_i915_gem_object *obj, bool force);
int __must_check i915_gem_object_set_domain(struct drm_i915_gem_object *obj,
					    uint32_t read_domains,
					    uint32_t write_domain);
int __must_check i915_gem_object_finish_gpu(struct drm_i915_gem_object *obj);
int __must_check i915_gem_init(struct drm_device *dev);
int __must_check i915_gem_init_hw(struct drm_device *dev);
int i915_gem_l3_remap(struct intel_ring_buffer *ring, int slice);
void i915_gem_init_swizzling(struct drm_device *dev);
void i915_gem_cleanup_ringbuffer(struct drm_device *dev);
int __must_check i915_gpu_idle(struct drm_device *dev);
int __must_check i915_gem_idle(struct drm_device *dev);
int __i915_add_request(struct intel_ring_buffer *ring,
		       struct drm_file *file,
			struct drm_i915_gem_object *batch_obj,
			u32 *seqno,
			struct drm_i915_gem_exec_timestamp *timestamp);
#define i915_add_request(ring, seqno, timestamp) \
	__i915_add_request(ring, NULL, NULL, seqno, timestamp);
int __must_check i915_wait_seqno(struct intel_ring_buffer *ring,
				   uint32_t seqno);
int i915_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
int __must_check
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj,
				  bool write);
int __must_check
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, bool write);
int __must_check
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
				     u32 alignment,
				     struct intel_ring_buffer *pipelined);
void i915_gem_object_unpin_from_display_plane(struct drm_i915_gem_object *obj);
int i915_gem_attach_phys_object(struct drm_device *dev,
				struct drm_i915_gem_object *obj,
				int id,
				int align);
void i915_gem_detach_phys_object(struct drm_device *dev,
				 struct drm_i915_gem_object *obj);
void i915_gem_free_all_phys_object(struct drm_device *dev);
void i915_gem_release(struct drm_device *dev, struct drm_file *file);

uint32_t
i915_gem_get_gtt_size(struct drm_device *dev, uint32_t size, int tiling_mode);
uint32_t
i915_gem_get_gtt_alignment(struct drm_device *dev, uint32_t size,
			  int tiling_mode, bool fenced);

int i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
				    enum i915_cache_level cache_level);

struct drm_gem_object *i915_gem_prime_import(struct drm_device *dev,
				struct dma_buf *dma_buf);

struct dma_buf *i915_gem_prime_export(struct drm_device *dev,
				struct drm_gem_object *gem_obj, int flags);

void i915_gem_restore_fences(struct drm_device *dev);

unsigned long i915_gem_obj_offset(struct drm_i915_gem_object *o,
				  struct i915_address_space *vm);
bool i915_gem_obj_bound_any(struct drm_i915_gem_object *o);
bool i915_gem_obj_bound(struct drm_i915_gem_object *o,
			struct i915_address_space *vm);
unsigned long i915_gem_obj_size(struct drm_i915_gem_object *o,
				struct i915_address_space *vm);
struct i915_vma *i915_gem_obj_to_vma(struct drm_i915_gem_object *obj,
				     struct i915_address_space *vm);
struct i915_vma *
i915_gem_obj_lookup_or_create_vma(struct drm_i915_gem_object *obj,
				  struct i915_address_space *vm);

struct i915_vma *i915_gem_obj_to_ggtt(struct drm_i915_gem_object *obj);

/* Some GGTT VM helpers */
#define obj_to_ggtt(obj) \
	(&((struct drm_i915_private *)(obj)->base.dev->dev_private)->gtt.base)
static inline bool i915_is_ggtt(struct i915_address_space *vm)
{
	struct i915_address_space *ggtt =
		&((struct drm_i915_private *)(vm)->dev->dev_private)->gtt.base;
	return vm == ggtt;
}

static inline bool i915_gem_obj_ggtt_bound(struct drm_i915_gem_object *obj)
{
	return i915_gem_obj_bound(obj, obj_to_ggtt(obj));
}

static inline unsigned long
i915_gem_obj_ggtt_offset(struct drm_i915_gem_object *obj)
{
	return i915_gem_obj_offset(obj, obj_to_ggtt(obj));
}

static inline unsigned long
i915_gem_obj_ggtt_size(struct drm_i915_gem_object *obj)
{
	return i915_gem_obj_size(obj, obj_to_ggtt(obj));
}

static inline int __must_check
i915_gem_obj_ggtt_pin(struct drm_i915_gem_object *obj,
		      uint32_t alignment,
		      bool map_and_fenceable,
		      bool nonblocking)
{
	return i915_gem_object_pin(obj, obj_to_ggtt(obj), alignment,
				   map_and_fenceable, nonblocking);
}

/* i915_gem_context.c */
void i915_gem_context_init(struct drm_device *dev);
void i915_gem_context_fini(struct drm_device *dev);
void i915_gem_context_close(struct drm_device *dev, struct drm_file *file);
int i915_switch_context(struct intel_ring_buffer *ring,
                       struct drm_file *file, int to_id);
void i915_gem_context_free(struct kref *ctx_ref);
static inline void i915_gem_context_reference(struct i915_hw_context *ctx)
{
	kref_get(&ctx->ref);
}

static inline void i915_gem_context_unreference(struct i915_hw_context *ctx)
{
	kref_put(&ctx->ref, i915_gem_context_free);
}
struct i915_ctx_hang_stats * __must_check
i915_gem_context_get_hang_stats(struct drm_device *dev,
				struct drm_file *file,
				u32 id);

int i915_gem_context_create_ioctl(struct drm_device *dev, void *data,
                                 struct drm_file *file);
int i915_gem_context_destroy_ioctl(struct drm_device *dev, void *data,
                                  struct drm_file *file);

/* i915_gem_gtt.c */
int __must_check i915_gem_init_aliasing_ppgtt(struct drm_device *dev);
void i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev);
void i915_ppgtt_bind_object(struct i915_hw_ppgtt *ppgtt,
		struct drm_i915_gem_object *obj,
		enum i915_cache_level cache_level);
void i915_ppgtt_unbind_object(struct i915_hw_ppgtt *ppgtt,
		struct drm_i915_gem_object *obj);

void i915_gem_restore_gtt_mappings(struct drm_device *dev);
int __must_check i915_gem_gtt_prepare_object(struct drm_i915_gem_object *obj);
void i915_gem_gtt_bind_object(struct drm_i915_gem_object *obj,
								enum i915_cache_level cache_level);
void i915_gem_gtt_unbind_object(struct drm_i915_gem_object *obj);
void i915_gem_gtt_finish_object(struct drm_i915_gem_object *obj);
void i915_gem_init_global_gtt(struct drm_device *dev);
void i915_gem_setup_global_gtt(struct drm_device *dev, unsigned long start,
			       unsigned long mappable_end, unsigned long end);
int i915_gem_gtt_init(struct drm_device *dev);
static inline void i915_gem_chipset_flush(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		intel_gtt_chipset_flush();
}
extern void i915_check_and_clear_faults(struct drm_device *dev);

/* i915_gem_evict.c */
int __must_check i915_gem_evict_something(struct drm_device *dev,
					 struct i915_address_space *vm,
					 int min_size,
                                         unsigned alignment,
                                         unsigned cache_level,
                                         bool mappable,
                                         bool nonblock);
int i915_gem_evict_vm(struct i915_address_space *vm, bool do_idle);
int i915_gem_evict_everything(struct drm_device *dev);
/* i915_gem_stolen.c */
int i915_gem_init_stolen(struct drm_device *dev);
int i915_gem_stolen_setup_compression(struct drm_device *dev, int size);
void i915_gem_stolen_cleanup_compression(struct drm_device *dev);
void i915_gem_cleanup_stolen(struct drm_device *dev);
void i915_gem_stolen_cleanup_compression(struct drm_device *dev);
void i915_setup_pctx(struct drm_device *dev);
struct drm_i915_gem_object *
i915_gem_object_create_stolen(struct drm_device *dev, u32 size);
#if (LTSI_VERSION >= KERNEL_VERSION(3, 10, 0))
struct drm_i915_gem_object *
i915_gem_object_create_stolen_for_preallocated(struct drm_device *dev,
					      u32 stolen_offset,
					      u32 gtt_offset,
					      u32 size);
#endif
void i915_gem_object_release_stolen(struct drm_i915_gem_object *obj);

/* i915_gem_tiling.c */
static inline bool i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj->tiling_mode != I915_TILING_NONE;
}

void i915_gem_detect_bit_6_swizzle(struct drm_device *dev);
void i915_gem_object_do_bit_17_swizzle(struct drm_i915_gem_object *obj);
void i915_gem_object_save_bit_17_swizzle(struct drm_i915_gem_object *obj);

/* i915_gem_debug.c */
void i915_gem_dump_object(struct drm_i915_gem_object *obj, int len,
			  const char *where, uint32_t mark);

/* i915_debugfs.c */
int i915_debugfs_init(struct drm_minor *minor);
void i915_debugfs_cleanup(struct drm_minor *minor);

/* i915_gpu_error.c */
__printf(2, 3)
void i915_error_printf(struct drm_i915_error_state_buf *e, const char *f, ...);
int i915_error_state_to_str(struct drm_i915_error_state_buf *estr,
			    const struct i915_error_state_file_priv *error);
int i915_error_state_buf_init(struct drm_i915_error_state_buf *eb,
			      size_t count, loff_t pos);
static inline void i915_error_state_buf_release(
	struct drm_i915_error_state_buf *eb)
{
	kfree(eb->buf);
}
void i915_capture_error_state(struct drm_device *dev);
void i915_error_state_get(struct drm_device *dev,
			  struct i915_error_state_file_priv *error_priv);
void i915_error_state_put(struct i915_error_state_file_priv *error_priv);
void i915_destroy_error_state(struct drm_device *dev);

void i915_get_extra_instdone(struct drm_device *dev, uint32_t *instdone);
const char *i915_cache_level_str(int type);

/* i915_emgd_helper.c */
void i915_hangcheck_elapsed(unsigned long data);
void notify_ring(struct drm_device *dev, struct intel_ring_buffer *ring);
void i915_error_work_func(struct work_struct *work);
void vlv_turbo_up(struct drm_device *dev);
void vlv_turbo_down_work(struct work_struct *work);
extern int crtc_pageflip_handler(struct drm_device *dev, int crtcnum);

/* intel_pm.c */
extern void gen6_gt_check_fifodbg(struct drm_i915_private *dev_priv);
extern unsigned long i915_chipset_val(struct drm_i915_private *dev_priv);
extern unsigned long i915_mch_val(struct drm_i915_private *dev_priv);
extern unsigned long i915_gfx_val(struct drm_i915_private *dev_priv);
extern void i915_update_gfx_val(struct drm_i915_private *dev_priv);
extern void intel_init_clock_gating(struct drm_device *dev);
extern bool intel_using_power_well(struct drm_device *dev);
extern void intel_init_power_well(struct drm_device *dev);
extern void intel_set_power_well(struct drm_device *dev, bool enable);
extern void intel_enable_gt_powersave(struct drm_device *dev);
extern void intel_disable_gt_powersave(struct drm_device *dev);
int __gen6_gt_wait_for_fifo(struct drm_i915_private *dev_priv);
/* i915_emgd_irq.c */
void i915_handle_error(struct drm_device *dev, bool wedged);

/* modesetting */
extern void intel_disable_fbc(struct drm_device *dev);
extern void gen6_set_rps(struct drm_device *dev, u8 val);
extern void valleyview_set_rps(struct drm_device *dev, u8 val); 
extern void emgd_power_init_hw(struct drm_device *dev);

#if WATCH_LISTS
int i915_verify_lists(struct drm_device *dev);
#else
#define i915_verify_lists(dev) 0
#endif
void i915_gem_dump_object(struct drm_i915_gem_object *obj, int len,
			  const char *where, uint32_t mark);

/* intel_pm.c */

extern bool i915_semaphore_is_enabled(struct drm_device *dev);
int i915_reg_read_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);

/* On SNB platform, before reading ring registers forcewake bit
 * must be set to prevent GT core from power down and stale values being
 * returned.
 */
void gen6_gt_force_wake_get(struct drm_i915_private *dev_priv);
void gen6_gt_force_wake_put(struct drm_i915_private *dev_priv);

int sandybridge_pcode_read(struct drm_i915_private *dev_priv, u8 mbox, u32 *val);
int sandybridge_pcode_write(struct drm_i915_private *dev_priv, u8 mbox, u32 val);
int valleyview_punit_read(struct drm_i915_private *dev_priv, u8 addr, u32 *val);
int valleyview_punit_write(struct drm_i915_private *dev_priv, u8 addr, u32 val);
int valleyview_nc_read(struct drm_i915_private *dev_priv, u8 addr, u32 *val);
int vlv_gpu_freq(int ddr_freq, int val);
int vlv_freq_opcode(int ddr_freq, int val);

static bool IS_DISPLAYREG(u32 reg)
{
	/*
	 * This should make it easier to transition modules over to the
	 * new register block scheme, since we can do it incrementally.
	 */
	if (reg >= VLV_DISPLAY_BASE)
		return false;

	if (reg >= RENDER_RING_BASE &&
			reg < RENDER_RING_BASE + 0xff)
		return false;
	if (reg >= GEN6_BSD_RING_BASE &&
			reg < GEN6_BSD_RING_BASE + 0xff)
		return false;
	if (reg >= BLT_RING_BASE &&
			reg < BLT_RING_BASE + 0xff)
		return false;

	if (reg >= IPEIR_I965 &&
			reg < HWSTAM)
		return false;

	if(reg >= GTFIFODBG && reg <= GTFIFODBG+0xff)
		return false;

	if(reg >= GEN7_GT_GFX_RC6_LOCKED_TO_RPN &&
		reg <= GEN7_GT_GFX_RC6PP)
		return false;

	if(reg == RENDER_RC0_COUNTER)
		return false;

	if(reg == MEDIA_RC0_COUNTER)
		return false;

	if (reg == MI_MODE)
		return false;

	if (reg == GEN7_CXT_SIZE)
		return false;

	if (reg == GFX_MODE_GEN7)
		return false;
	
	if (reg == PGTBL_ER)
		return false;
	
	if (reg >= FENCE_REG_SANDYBRIDGE_0/*0x100000*/ &&
		reg <= GFX_FLSH_CNTL_GEN6 /*0x101008*/) {
		return false;
	}

	if (reg == RENDER_HWS_PGA_GEN7 ||
			reg == BSD_HWS_PGA_GEN7 ||
			reg == BLT_HWS_PGA_GEN7)
		return false;

	if (reg == GEN6_BSD_SLEEP_PSMI_CONTROL ||
			reg == GEN6_BSD_RNCID)
		return false;

	if (reg == GEN6_BLITTER_ECOSKPD)
		return false;

	if (reg >= GEN6_RPNSWREQ &&
			reg < GEN6_PMINTRMSK + 4)
		return false; 

	if (reg >= 0x4000c &&
			reg <= 0x4002c)
		return false;

	if (reg >= 0x4f000 &&
			reg <= 0x4f08f)
		return false;

	if (reg >= 0x4f100 &&
			reg <= 0x4f11f)
		return false;

	if (reg >= VLV_MASTER_IER &&
			reg <= GEN6_PMIER)
		return false;

	if (reg >= FENCE_REG_SANDYBRIDGE_0 &&
			reg < (FENCE_REG_SANDYBRIDGE_0 + (16*8)))
		return false;

	if (reg == FORCEWAKE_VLV ||
			reg == FORCEWAKE_ACK_VLV ||
			reg == 0x130090)
		return false;

	if (reg == GEN6_GDRST)
		return false;

	if(reg > 0x9400 && reg <= 0x9418){
		return false;
	}

        switch (reg) {
	       case _3D_CHICKEN3:
	       case IVB_CHICKEN3:
	       case GEN7_HALF_SLICE_CHICKEN1:
	       case GEN7_COMMON_SLICE_CHICKEN1:
	       case GEN7_L3CNTLREG1:
	       case GEN7_L3_CHICKEN_MODE_REGISTER:
	       case GEN7_ROW_CHICKEN2:
	       case GEN7_L3SQCREG4:
	       case GEN7_SQ_CHICKEN_MBCUNIT_CONFIG:
	       case GEN6_MBCTL:
	       case GEN6_UCGCTL2:
	       case GEN7_UCGCTL4:
	       case FORCEWAKE_MEDIA_VLV:
	       case FORCEWAKE_ACK_MEDIA_VLV:
	       case VLV_GTLC_PW_STATUS:
	              return false;
	       default:
	              break;
	}
	return true;
}


#define LP_RING(d) (&((struct drm_i915_private *)(d))->ring[RCS])

#define BEGIN_LP_RING(n) \
	intel_ring_begin(LP_RING(dev_priv), (n))

#define OUT_RING(x) \
	intel_ring_emit(LP_RING(dev_priv), x)

#define ADVANCE_LP_RING() \
	intel_ring_advance(LP_RING(dev_priv))

/**
 * Lock test for when it's just for synchronization of ring access.
 *
 * In that case, we don't need to do it when GEM is initialized as nobody else
 * has access to the ring.
 */
#define RING_LOCK_TEST_WITH_RETURN(dev, file) do {			\
	if (LP_RING(dev->dev_private)->obj == NULL)			\
		LOCK_TEST_WITH_RETURN(dev, file);			\
} while (0)

/* We give fast paths for the really cool registers */
#define NEEDS_FORCE_WAKE(dev_priv, reg) \
    ((HAS_FORCE_WAKE((dev_priv)->dev)) && \
     ((reg) < 0x40000) &&            \
     ((reg) != FORCEWAKE))

#define __i915_read(x, y) \
static inline u##x i915_read##x(struct drm_i915_private *dev_priv, u32 reg) { \
    u##x val = 0; \
    if (NEEDS_FORCE_WAKE((dev_priv), (reg))) { \
        unsigned long irqflags; \
        spin_lock_irqsave(&dev_priv->uncore.lock, irqflags); \
        if (dev_priv->uncore.forcewake_count == 0) \
            dev_priv->uncore.funcs.force_wake_get(dev_priv); \
        val = read##y(dev_priv->regs + reg); \
        if (dev_priv->uncore.forcewake_count == 0) \
            dev_priv->uncore.funcs.force_wake_put(dev_priv); \
        spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags); \
    } else if (IS_VALLEYVIEW(dev_priv->dev) && IS_DISPLAYREG(reg)) { \
        val = read##y(dev_priv->regs + reg + 0x180000);     \
    } else { \
        val = read##y(dev_priv->regs + reg); \
    } \
    trace_i915_reg_rw(false, reg, val, sizeof(val)); \
    return val; \
}

__i915_read(8, b)
__i915_read(16, w)
__i915_read(32, l)
__i915_read(64, q)
#undef __i915_read

#define __i915_write(x, y) \
static inline void i915_write##x(struct drm_i915_private *dev_priv, u32 reg, u##x val) { \
    u32 __fifo_ret = 0; \
    trace_i915_reg_rw(true, reg, val, sizeof(val)); \
    if (NEEDS_FORCE_WAKE((dev_priv), (reg))) { \
        __fifo_ret = __gen6_gt_wait_for_fifo(dev_priv); \
    } \
    if (IS_VALLEYVIEW(dev_priv->dev) && IS_DISPLAYREG(reg)) { \
        write##y(val, dev_priv->regs + reg + 0x180000);     \
    } else {                            \
        write##y(val, dev_priv->regs + reg);            \
    }                               \
    if (unlikely(__fifo_ret)) { \
        gen6_gt_check_fifodbg(dev_priv); \
    } \
}



__i915_write(8, b)
__i915_write(16, w)
__i915_write(32, l)
__i915_write(64, q)
#undef __i915_write

#define I915_READ8(reg)		i915_read8(dev_priv, (reg))
#define I915_WRITE8(reg, val)	i915_write8(dev_priv, (reg), (val))

#define I915_READ16(reg)	i915_read16(dev_priv, (reg))
#define I915_WRITE16(reg, val)	i915_write16(dev_priv, (reg), (val))
#define I915_READ16_NOTRACE(reg)	readw(dev_priv->regs + (reg))
#define I915_WRITE16_NOTRACE(reg, val)	writew(val, dev_priv->regs + (reg))

#define I915_READ(reg)		i915_read32(dev_priv, (reg))
#define I915_WRITE(reg, val)	i915_write32(dev_priv, (reg), (val))
#define I915_READ_NOTRACE(reg)		readl(dev_priv->regs + (reg))
#define I915_WRITE_NOTRACE(reg, val)	writel(val, dev_priv->regs + (reg))

#define I915_WRITE64(reg, val)	i915_write64(dev_priv, (reg), (val))
#define I915_READ64(reg)	i915_read64(dev_priv, (reg))

#define POSTING_READ(reg)	(void)I915_READ_NOTRACE(reg)
#define POSTING_READ16(reg)	(void)I915_READ16_NOTRACE(reg)



static inline unsigned long
timespec_to_jiffies_timeout(const struct timespec *value)
{
	unsigned long j = timespec_to_jiffies(value);

	return min_t(unsigned long, MAX_JIFFY_OFFSET, j + 1);
}

#ifdef MSVDX_SUPPORT
/* for ipvr */
#define HAS_VED(dev)		(INTEL_INFO(dev)->is_valleyview && IS_GEN7(dev))

int vlv_setup_ved(struct drm_device *dev);
void vlv_teardown_ved(struct drm_device *dev);
void vlv_ved_irq_handler(struct drm_device *dev);
#endif

#endif
