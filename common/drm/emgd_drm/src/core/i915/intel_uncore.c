/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "i915_reg.h"
#include "intel_drv.h"

#define FORCEWAKE_ACK_TIMEOUT_MS 2

static void __gen6_gt_wait_for_thread_c0(struct drm_i915_private *dev_priv)
{
	u32 gt_thread_status_mask;

	if (IS_HASWELL(dev_priv->dev))
		gt_thread_status_mask = GEN6_GT_THREAD_STATUS_CORE_MASK_HSW;
	else
		gt_thread_status_mask = GEN6_GT_THREAD_STATUS_CORE_MASK;

	/* w/a for a sporadic read returning 0 by waiting for the GT
	 * thread to wake up.
	 */
	if (wait_for_atomic_us((I915_READ_NOTRACE(GEN6_GT_THREAD_STATUS_REG) & gt_thread_status_mask) == 0, 500))
		DRM_ERROR("GT thread status wait timed out\n");
}

static void __gen6_gt_force_wake_reset(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE, 0);
	POSTING_READ(ECOBUS); /* something from same cacheline, but !FORCEWAKE */
}

static void __gen6_gt_force_wake_get(struct drm_i915_private *dev_priv)
{
	if (wait_for_atomic((I915_READ_NOTRACE(FORCEWAKE_ACK) & 1) == 0,
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for forcewake old ack to clear.\n");

	I915_WRITE_NOTRACE(FORCEWAKE, 1);
	POSTING_READ(ECOBUS); /* something from same cacheline, but !FORCEWAKE */

	if (wait_for_atomic((I915_READ_NOTRACE(FORCEWAKE_ACK) & 1),
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for forcewake to ack request.\n");

	/* WaRsForcewakeWaitTC0:snb */
	__gen6_gt_wait_for_thread_c0(dev_priv);
}

static void __gen6_gt_force_wake_mt_reset(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_MT, _MASKED_BIT_DISABLE(0xffff));
	/* something from same cacheline, but !FORCEWAKE_MT */
	POSTING_READ(ECOBUS);
}

static void __gen6_gt_force_wake_mt_get(struct drm_i915_private *dev_priv)
{
	u32 forcewake_ack;

	if (IS_HASWELL(dev_priv->dev))
		forcewake_ack = FORCEWAKE_ACK_HSW;
	else
		forcewake_ack = FORCEWAKE_MT_ACK;

	if (wait_for_atomic((I915_READ_NOTRACE(forcewake_ack) & FORCEWAKE_KERNEL) == 0,
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for forcewake old ack to clear.\n");

	I915_WRITE_NOTRACE(FORCEWAKE_MT, _MASKED_BIT_ENABLE(FORCEWAKE_KERNEL));
	/* something from same cacheline, but !FORCEWAKE_MT */
	POSTING_READ(ECOBUS);

	if (wait_for_atomic((I915_READ_NOTRACE(forcewake_ack) & FORCEWAKE_KERNEL),
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for forcewake to ack request.\n");

	/* WaRsForcewakeWaitTC0:ivb,hsw */
	__gen6_gt_wait_for_thread_c0(dev_priv);
}

void gen6_gt_check_fifodbg(struct drm_i915_private *dev_priv)
{
	u32 gtfifodbg;
	gtfifodbg = I915_READ_NOTRACE(GTFIFODBG);
	if (WARN(gtfifodbg & GT_FIFO_CPU_ERROR_MASK,
	     "MMIO read or write has been dropped %x\n", gtfifodbg))
		I915_WRITE_NOTRACE(GTFIFODBG, GT_FIFO_CPU_ERROR_MASK);
}

static void __gen6_gt_force_wake_put(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE, 0);
	/* something from same cacheline, but !FORCEWAKE */
	POSTING_READ(ECOBUS);
	gen6_gt_check_fifodbg(dev_priv);
}

static void __gen6_gt_force_wake_mt_put(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_MT, _MASKED_BIT_DISABLE(FORCEWAKE_KERNEL));
	/* something from same cacheline, but !FORCEWAKE_MT */
	POSTING_READ(ECOBUS);
	gen6_gt_check_fifodbg(dev_priv);
}

int __gen6_gt_wait_for_fifo(struct drm_i915_private *dev_priv)
{
	int ret = 0;

	if (dev_priv->uncore.fifo_count < GT_FIFO_NUM_RESERVED_ENTRIES) {
		int loop = 500;
		u32 fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		while (fifo <= GT_FIFO_NUM_RESERVED_ENTRIES && loop--) {
			udelay(10);
			fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		}
		if (WARN_ON(loop < 0 && fifo <= GT_FIFO_NUM_RESERVED_ENTRIES))
			++ret;
		dev_priv->uncore.fifo_count = fifo;
	}
	dev_priv->uncore.fifo_count--;

	return ret;
}

static void vlv_force_wake_reset(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_VLV, _MASKED_BIT_DISABLE(0xffff));
	/* something from same cacheline, but !FORCEWAKE_VLV */
	POSTING_READ(FORCEWAKE_ACK_VLV);
}

static void vlv_force_wake_get(struct drm_i915_private *dev_priv)
{
	unsigned long temp;

	if (wait_for_atomic((I915_READ_NOTRACE(FORCEWAKE_ACK_VLV) & FORCEWAKE_KERNEL) == 0,
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for forcewake old ack to clear.\n");

	temp = I915_READ(VLV_GTLC_WAKE_CTRL);
	temp |= VLV_GTLC_ALLOWWAKEREQ;
	I915_WRITE_NOTRACE(VLV_GTLC_WAKE_CTRL, temp);
	if (wait_for_atomic((I915_READ_NOTRACE(VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEACK),
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for GT to ack allow wake request.\n.");

	I915_WRITE_NOTRACE(FORCEWAKE_VLV, _MASKED_BIT_ENABLE(FORCEWAKE_KERNEL));
	I915_WRITE_NOTRACE(FORCEWAKE_MEDIA_VLV,
			   _MASKED_BIT_ENABLE(FORCEWAKE_KERNEL));

	if (wait_for_atomic((I915_READ_NOTRACE(FORCEWAKE_ACK_VLV) & FORCEWAKE_KERNEL),
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for GT to ack forcewake request.\n");

	if (wait_for_atomic((I915_READ_NOTRACE(FORCEWAKE_ACK_MEDIA_VLV) &
			     FORCEWAKE_KERNEL),
			    FORCEWAKE_ACK_TIMEOUT_MS))
		DRM_ERROR("Timed out waiting for media to ack forcewake request.\n");

	/* WaRsForcewakeWaitTC0:vlv */
	__gen6_gt_wait_for_thread_c0(dev_priv);
}

static void vlv_force_wake_put(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_VLV, _MASKED_BIT_DISABLE(FORCEWAKE_KERNEL));

	/*FIXME: clearing the media bit otherwise it will stay there as oxFFFE*/
	I915_WRITE(FORCEWAKE_MEDIA_VLV, 0xFFFF0000);
	/*I915_WRITE_NOTRACE(FORCEWAKE_MEDIA_VLV,_MASKED_BIT_DISABLE(FORCEWAKE_KERNEL));*/

	/* something from same cacheline, but !FORCEWAKE_VLV */
	POSTING_READ(FORCEWAKE_ACK_VLV);
	gen6_gt_check_fifodbg(dev_priv);
}

void intel_uncore_sanitize(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (IS_VALLEYVIEW(dev)) {
		vlv_force_wake_reset(dev_priv);
	} else if (INTEL_INFO(dev)->gen >= 6) {
		__gen6_gt_force_wake_reset(dev_priv);
		if (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))
			__gen6_gt_force_wake_mt_reset(dev_priv);
	}
}

void intel_uncore_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	spin_lock_init(&dev_priv->uncore.lock);

	intel_uncore_sanitize(dev);

	if (IS_VALLEYVIEW(dev)) {
		dev_priv->uncore.funcs.force_wake_get = vlv_force_wake_get;
		dev_priv->uncore.funcs.force_wake_put = vlv_force_wake_put;
	} else if (IS_IVYBRIDGE(dev) || IS_HASWELL(dev)) {
		dev_priv->uncore.funcs.force_wake_get = __gen6_gt_force_wake_mt_get;
		dev_priv->uncore.funcs.force_wake_put = __gen6_gt_force_wake_mt_put;
	} else if (IS_GEN6(dev)) {
		dev_priv->uncore.funcs.force_wake_get =
			__gen6_gt_force_wake_get;
		dev_priv->uncore.funcs.force_wake_put =
			__gen6_gt_force_wake_put;
	}
}

/*
 * Generally this is called implicitly by the register read function. However,
 * if some sequence requires the GT to not power down then this function should
 * be called at the beginning of the sequence followed by a call to
 * gen6_gt_force_wake_put() at the end of the sequence.
 */
void gen6_gt_force_wake_get(struct drm_i915_private *dev_priv)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->uncore.lock, irqflags);
	if (dev_priv->uncore.forcewake_count++ == 0)
		dev_priv->uncore.funcs.force_wake_get(dev_priv);
	spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags);
}

/*
 * see gen6_gt_force_wake_get()
 */
void gen6_gt_force_wake_put(struct drm_i915_private *dev_priv)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->uncore.lock, irqflags);
	if (--dev_priv->uncore.forcewake_count == 0)
		dev_priv->uncore.funcs.force_wake_put(dev_priv);
	spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags);
}

static const struct register_whitelist {
	uint64_t offset;
	uint32_t size;
	uint32_t gen_bitmask; /* support gens, 0x10 for 4, 0x30 for 4 and 5, etc. */
} whitelist[] = {
	{ RING_TIMESTAMP(RENDER_RING_BASE), 8, 0xF0 },
};

int i915_reg_read_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_reg_read *reg = data;
	struct register_whitelist const *entry = whitelist;
	int i;

	for (i = 0; i < ARRAY_SIZE(whitelist); i++, entry++) {
		if (entry->offset == reg->offset &&
		    (1 << INTEL_INFO(dev)->gen & entry->gen_bitmask))
			break;
	}

	if (i == ARRAY_SIZE(whitelist))
		return -EINVAL;

	switch (entry->size) {
	case 8:
		reg->val = I915_READ64(reg->offset);
		break;
	case 4:
		reg->val = I915_READ(reg->offset);
		break;
	case 2:
		reg->val = I915_READ16(reg->offset);
		break;
	case 1:
		reg->val = I915_READ8(reg->offset);
		break;
	default:
		WARN_ON(1);
		return -EINVAL;
	}

	return 0;
}

static int gen6_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int	ret;
	unsigned long irqflags;

	/* Hold uncore.lock across reset to prevent any register access
	 * with forcewake not set correctly
	 */
	spin_lock_irqsave(&dev_priv->uncore.lock, irqflags);

	/* Reset the chip */

	/* GEN6_GDRST is not in the gt power well, no need to check
	 * for fifo space for the write or forcewake the chip for
	 * the read
	 */
	I915_WRITE_NOTRACE(GEN6_GDRST, GEN6_GRDOM_FULL);

	/* Spin waiting for the device to ack the reset request */
	ret = wait_for((I915_READ_NOTRACE(GEN6_GDRST) & GEN6_GRDOM_FULL) == 0, 500);

	/* If reset with a user forcewake, try to restore, otherwise turn it off */
	if (dev_priv->uncore.forcewake_count)
		dev_priv->uncore.funcs.force_wake_get(dev_priv);
	else
		dev_priv->uncore.funcs.force_wake_put(dev_priv);

	/* Restore fifo count */
	dev_priv->uncore.fifo_count = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);

	spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags);
	return ret;
}

int intel_gpu_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = -ENODEV;

	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6: return gen6_do_reset(dev);
		break;
	}

	/* Also reset the gpu hangman. */
	if (dev_priv->gpu_error.stop_rings) {
		DRM_DEBUG("Simulated gpu hang, resetting stop_rings\n");
		dev_priv->gpu_error.stop_rings = 0;
		if (ret == -ENODEV) {
			DRM_ERROR("Reset not implemented, but ignoring "
				  "error for simulated gpu hangs\n");
			ret = 0;
		}
	}

	return ret;
}
