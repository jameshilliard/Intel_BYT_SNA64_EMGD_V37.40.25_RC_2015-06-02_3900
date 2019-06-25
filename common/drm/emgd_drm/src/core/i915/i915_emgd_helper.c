/*
 * Copyright © 2008,2013,2014 Intel Corporation
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
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <linux/version.h>
#include <drmP.h>
#include <i915_drm.h>

#include "i915_reg.h"
#include "intel_drv.h"
#include "intel_bios.h"
#include "intel_ringbuffer.h"

#include "drm_emgd_private.h"

int intel_pin_and_fence_fb_obj(struct drm_device *dev,
		struct drm_i915_gem_object *obj,
		struct intel_ring_buffer *pipelined)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 alignment;
	int ret;

	switch (obj->tiling_mode) {
	case I915_TILING_NONE:
		if (IS_BROADWATER(dev) || IS_CRESTLINE(dev))
				alignment = 128 * 1024;
		else if (INTEL_INFO(dev)->gen >= 4)
				alignment = 4 * 1024;
		else
				alignment = 64 * 1024;
		break;
	case I915_TILING_X:
		/* pin() will align the object as required by fence */
		alignment = 0;
		break;
	case I915_TILING_Y:
		/* FIXME: Is this true? */
		DRM_ERROR("Y tiled not allowed for scan out buffers\n");
		return -EINVAL;
	default:
		BUG();
	}

	dev_priv->mm.interruptible = false;
	ret = i915_gem_object_pin_to_display_plane(obj, alignment, pipelined);
	if (ret)
			goto err_interruptible;

	/* Install a fence for tiled scan-out. Pre-i965 always needs a
	 * fence, whereas 965+ only requires a fence if using
	 * framebuffer compression.  For simplicity, we always install
	 * a fence as the cost is not that onerous.
	 */
	ret = i915_gem_object_get_fence(obj);
	if (ret)
		goto err_unpin;

	i915_gem_object_pin_fence(obj);

	dev_priv->mm.interruptible = true;
	return 0;

err_unpin:
	i915_gem_object_unpin_from_display_plane(obj);
err_interruptible:
	dev_priv->mm.interruptible = true;
	return ret;
}

void notify_ring(struct drm_device *dev,
			struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
	intel_atomic_notify_ring(dev, ring);
#endif

	if (ring->obj == NULL)
		return;

	trace_i915_gem_request_complete(ring, ring->get_seqno(ring, false));

	wake_up_all(&ring->irq_queue);
	if (i915_enable_hangcheck) {
		mod_timer(&dev_priv->gpu_error.hangcheck_timer,
				round_jiffies_up(jiffies + DRM_I915_HANGCHECK_JIFFIES));
	}
}

static void i915_report_and_clear_eir(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t instdone[I915_NUM_INSTDONE_REG];
	u32 eir = I915_READ(EIR);
	int pipe, i;

	if (!eir)
		return;

	printk(KERN_ERR "render error detected, EIR: 0x%08x\n",
	       eir);

	i915_get_extra_instdone(dev, instdone);

	if (IS_G4X(dev)) {
		if (eir & (GM45_ERROR_MEM_PRIV | GM45_ERROR_CP_PRIV)) {
			u32 ipeir = I915_READ(IPEIR_I965);

			printk(KERN_ERR "  IPEIR: 0x%08x\n",
			       I915_READ(IPEIR_I965));
			printk(KERN_ERR "  IPEHR: 0x%08x\n",
					I915_READ(IPEHR_I965));
			for (i = 0; i < ARRAY_SIZE(instdone); i++)
				pr_err("  INSTDONE_%d: 0x%08x\n", i, instdone[i]);
			printk(KERN_ERR "  INSTPS: 0x%08x\n",
			       I915_READ(INSTPS));
			printk(KERN_ERR "  ACTHD: 0x%08x\n",
			       I915_READ(ACTHD_I965));
			I915_WRITE(IPEIR_I965, ipeir);
			POSTING_READ(IPEIR_I965);
		}
		if (eir & GM45_ERROR_PAGE_TABLE) {
			u32 pgtbl_err = I915_READ(PGTBL_ER);
			printk(KERN_ERR "page table error\n");
			printk(KERN_ERR "  PGTBL_ER: 0x%08x\n",
			       pgtbl_err);
			I915_WRITE(PGTBL_ER, pgtbl_err);
			POSTING_READ(PGTBL_ER);
		}
	}

	if (!IS_GEN2(dev)) {
		if (eir & I915_ERROR_PAGE_TABLE) {
			u32 pgtbl_err = I915_READ(PGTBL_ER);
			printk(KERN_ERR "page table error\n");
			printk(KERN_ERR "  PGTBL_ER: 0x%08x\n",
			       pgtbl_err);
			I915_WRITE(PGTBL_ER, pgtbl_err);
			POSTING_READ(PGTBL_ER);
		}
	}

	if (eir & I915_ERROR_MEMORY_REFRESH) {
		printk(KERN_ERR "memory refresh error:\n");
		for_each_pipe(pipe)
			printk(KERN_ERR "pipe %c stat: 0x%08x\n",
			       pipe_name(pipe), I915_READ(PIPESTAT(pipe)));
		/* pipestat has already been acked */
	}
	if (eir & I915_ERROR_INSTRUCTION) {
		printk(KERN_ERR "instruction error\n");
		printk(KERN_ERR "  INSTPM: 0x%08x\n",
		       I915_READ(INSTPM));
		for (i = 0; i < ARRAY_SIZE(instdone); i++)
			pr_err("  INSTDONE_%d: 0x%08x\n", i, instdone[i]);
		if (INTEL_INFO(dev)->gen < 4) {
			u32 ipeir = I915_READ(IPEIR);

			printk(KERN_ERR "  IPEIR: 0x%08x\n",
			       I915_READ(IPEIR));
			printk(KERN_ERR "  IPEHR: 0x%08x\n",
			       I915_READ(IPEHR));
			printk(KERN_ERR "  ACTHD: 0x%08x\n",
			       I915_READ(ACTHD));
			I915_WRITE(IPEIR, ipeir);
			POSTING_READ(IPEIR);
		} else {
			u32 ipeir = I915_READ(IPEIR_I965);

			printk(KERN_ERR "  IPEIR: 0x%08x\n",
			       I915_READ(IPEIR_I965));
			printk(KERN_ERR "  IPEHR: 0x%08x\n",
			       I915_READ(IPEHR_I965));
			printk(KERN_ERR "  INSTPS: 0x%08x\n",
			       I915_READ(INSTPS));
			printk(KERN_ERR "  ACTHD: 0x%08x\n",
			       I915_READ(ACTHD_I965));
			I915_WRITE(IPEIR_I965, ipeir);
			POSTING_READ(IPEIR_I965);
		}
	}

	I915_WRITE(EIR, eir);
	POSTING_READ(EIR);
	eir = I915_READ(EIR);
	if (eir) {
		/*
		 * some errors might have become stuck,
		 * mask them.
		 */
		DRM_ERROR("EIR stuck: 0x%08x, masking\n", eir);
		I915_WRITE(EMR, I915_READ(EMR) | eir);
		I915_WRITE(IIR, I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT);
	}
}

static void intel_display_handle_reset(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	/*
	 * Flips in the rings have been nuked by the reset,
	 * so complete all pending flips so that user space
	 * will get its events and not get stuck.
	 */
	 list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		emgd_crtc_t *emgd_crtc = container_of(crtc, emgd_crtc_t, base);

		if (emgd_crtc->flip_pending)
			atomic_inc(&emgd_crtc->unpin_work_count);

		intel_prepare_page_flip(dev, emgd_crtc->pipe);
		crtc_pageflip_handler(dev, emgd_crtc->pipe);
	}
}

static void i915_error_wake_up(struct drm_i915_private *dev_priv,
			       bool reset_completed)
{
	struct intel_ring_buffer *ring;
	int i;

	/*
	 * Notify all waiters for GPU completion events that reset state has
	 * been changed, and that they need to restart their wait after
	 * checking for potential errors (and bail out to drop locks if there is
	 * a gpu reset pending so that i915_error_work_func can acquire them).
	 */

	/* Wake up __wait_seqno, potentially holding dev->struct_mutex. */
	for_each_ring(ring, dev_priv, i)
		wake_up_all(&ring->irq_queue);

	/* Wake up intel_crtc_wait_for_pending_flips, holding crtc->mutex. */
		wake_up_all(&dev_priv->pending_flip_queue);

	/*
	 * Signal tasks blocked in i915_gem_wait_for_error that the pending
	 * reset state is cleared.
	 */
	if (reset_completed)
		wake_up_all(&dev_priv->gpu_error.reset_queue);
}

/**
 * i915_handle_error - handle an error interrupt
 * @dev: drm device
 *
 * Do some basic checking of regsiter state at error interrupt time and
 * dump it to the syslog.  Also call i915_capture_error_state() to make
 * sure we get a record and make it available in debugfs.  Fire a uevent
 * so userspace knows something bad happened (should trigger collection
 * of a ring dump etc.).
 */
void i915_handle_error(struct drm_device *dev, bool wedged)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	i915_capture_error_state(dev);
	i915_report_and_clear_eir(dev);

	if (wedged) {
		atomic_set(&dev_priv->gpu_error.reset_counter,
				I915_RESET_IN_PROGRESS_FLAG);
		atomic_set_mask(I915_RESET_IN_PROGRESS_FLAG,
				&dev_priv->gpu_error.reset_counter);

		/*
		 * Wakeup waiting processes so that the reset work function
		 * i915_error_work_func doesn't deadlock trying to grab various
		 * locks. By bumping the reset counter first, the woken
		 * processes will see a reset in progress and back off,
		 * releasing their locks and then wait for the reset completion.
		 * We must do this for _all_ gpu waiters that might hold locks
		 * that the reset work needs to acquire.
		 *
		 * Note: The wake_up serves as the required memory barrier to
		 * ensure that the waiters see the updated value of the reset
		 * counter atomic_t.

		 */
		i915_error_wake_up(dev_priv, false);

#ifdef KERNEL_HAS_ATOMIC_PAGE_FLIP
		intel_atomic_wedged(dev);
#endif
	}

	schedule_work(&dev_priv->gpu_error.work);
}

static u32
ring_last_seqno(struct intel_ring_buffer *ring)
{
	return list_entry(ring->request_list.prev,
			  struct drm_i915_gem_request, list)->seqno;
}

static bool
ring_idle(struct intel_ring_buffer *ring, u32 seqno)
{
	return (list_empty(&ring->request_list) ||
		i915_seqno_passed(seqno, ring_last_seqno(ring)));
}

static struct intel_ring_buffer *
semaphore_waits_for(struct intel_ring_buffer *ring, u32 *seqno)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	u32 cmd, ipehr, acthd, acthd_min;

	ipehr = I915_READ(RING_IPEHR(ring->mmio_base));
	if ((ipehr & ~(0x3 << 16)) !=
	    (MI_SEMAPHORE_MBOX | MI_SEMAPHORE_COMPARE | MI_SEMAPHORE_REGISTER))
		return NULL;

	/* ACTHD is likely pointing to the dword after the actual command,
	 * so scan backwards until we find the MBOX.
	 */
	acthd = intel_ring_get_active_head(ring) & HEAD_ADDR;
	acthd_min = max((int)acthd - 3 * 4, 0);
	do {
		cmd = ioread32(ring->virtual_start + acthd);
		if (cmd == ipehr)
			break;

		acthd -= 4;
		if (acthd < acthd_min)
			return NULL;
	} while (1);

	*seqno = ioread32(ring->virtual_start+acthd+4)+1;
	return &dev_priv->ring[(ring->id + (((ipehr >> 17) & 1) + 1)) % 3];
}

static int semaphore_passed(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct intel_ring_buffer *signaller;
	u32 seqno, ctl;

	ring->hangcheck.deadlock = true;

	signaller = semaphore_waits_for(ring, &seqno);
	if (signaller == NULL || signaller->hangcheck.deadlock)
		return -1;

	/* cursory check for an unkickable deadlock */
	ctl = I915_READ_CTL(signaller);
	if (ctl & RING_WAIT_SEMAPHORE && semaphore_passed(signaller) < 0)
		return -1;

	return i915_seqno_passed(signaller->get_seqno(signaller, false), seqno);
}

static void semaphore_clear_deadlocks(struct drm_i915_private *dev_priv)
{
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		ring->hangcheck.deadlock = false;
}

static enum intel_ring_hangcheck_action
ring_stuck(struct intel_ring_buffer *ring, u32 acthd)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 tmp;

	if (ring->hangcheck.acthd != acthd)
		return HANGCHECK_ACTIVE;

	if (IS_GEN2(dev))
		return HANGCHECK_HUNG;

	/* Is the chip hanging on a WAIT_FOR_EVENT?
	 * If so we can simply poke the RB_WAIT bit
	 * and break the hang. This should work on
	 * all but the second generation chipsets.
	 */
	tmp = I915_READ_CTL(ring);
	if (tmp & RING_WAIT) {
		DRM_ERROR("Kicking stuck wait on %s\n",
			  ring->name);
		I915_WRITE_CTL(ring, tmp);
		return HANGCHECK_KICK;
	}

	if (INTEL_INFO(dev)->gen >= 6 && tmp & RING_WAIT_SEMAPHORE) {
		switch (semaphore_passed(ring)) {
		default:
			return HANGCHECK_HUNG;
		case 1:
			DRM_ERROR("Kicking stuck semaphore on %s\n",
				  ring->name);
			I915_WRITE_CTL(ring, tmp);
			return HANGCHECK_KICK;
		case 0:
			return HANGCHECK_WAIT;
		}
	}

	return HANGCHECK_HUNG;
}

/**
 * This is called when the chip hasn't reported back with completed
 * batchbuffers in a long time. We keep track per ring seqno progress and
 * if there are no progress, hangcheck score for that ring is increased.
 * Further, acthd is inspected to see if the ring is stuck. On stuck case
 * we kick the ring. If we see no progress on three subsequent calls
 * we assume chip is wedged and try to fix it by resetting the chip.
 */
/* #define RINGBUFFER_DEBUG */
void i915_hangcheck_elapsed(unsigned long data)
{
	struct drm_device *dev = (struct drm_device *)data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int i;
	int busy_count = 0, rings_hung = 0;
	bool stuck[I915_NUM_RINGS] = { 0 };
#define BUSY 1
#define KICK 5
#define HUNG 20
#define FIRE 30
#ifdef RINGBUFFER_DEBUG
	uint32_t tmp, start, head, tail;
	unsigned char * mmioptr;
#endif
	if (!i915_enable_hangcheck)
		return;

	for_each_ring(ring, dev_priv, i) {
		u32 seqno, acthd;
		bool busy = true;

		semaphore_clear_deadlocks(dev_priv);

		seqno = ring->get_seqno(ring, false);
		acthd = intel_ring_get_active_head(ring);

		if (ring->hangcheck.seqno == seqno) {
			if (ring_idle(ring, seqno)) {
				if (waitqueue_active(&ring->irq_queue)) {
					/* Issue a wake-up to catch stuck h/w. */
					if (!test_and_set_bit(ring->id,
						&dev_priv->gpu_error.missed_irq_rings)) {
						DRM_ERROR("Hangcheck timer elapsed... %s idle\n",
							  ring->name);
						wake_up_all(&ring->irq_queue);
					}
					/* Safeguard against driver failure */
					ring->hangcheck.score += HUNG;
				} else
					busy = false;
			} else {
				/* We always increment the hangcheck score
				 * if the ring is busy and still processing
				 * the same request, so that no single request
				 * can run indefinitely (such as a chain of
				 * batches). The only time we do not increment
				 * the hangcheck score on this ring, if this
				 * ring is in a legitimate wait for another
				 * ring. In that case the waiting ring is a
				 * victim and we want to be sure we catch the
				 * right culprit. Then every time we do kick
				 * the ring, add a small increment to the
				 * score so that we can catch a batch that is
				 * being repeatedly kicked and so responsible
				 * for stalling the machine.
				 */
				ring->hangcheck.action = ring_stuck(ring,
								    acthd);

				switch (ring->hangcheck.action) {
				case HANGCHECK_WAIT:
					break;
				case HANGCHECK_ACTIVE:
                    ring->hangcheck.score += BUSY;
					break;
				case HANGCHECK_KICK:
                    ring->hangcheck.score += KICK;
					break;
				case HANGCHECK_HUNG:
                    ring->hangcheck.score += HUNG;
					stuck[i] = true;
					break;
				}
			}
		} else {
			/* Gradually reduce the count so that we catch DoS
			 * attempts across multiple batches.
			 */
			if (ring->hangcheck.score > 0)
				ring->hangcheck.score--;
		}

		ring->hangcheck.seqno = seqno;
		ring->hangcheck.acthd = acthd;
		busy_count += busy;
	}

	for_each_ring(ring, dev_priv, i) {
		if (ring->hangcheck.score > FIRE) {
			rings_hung++;
			DRM_ERROR("%s: %s on %s 0x%x\n", ring->name,
				  stuck[i] ? "stuck" : "no progress",
				  stuck[i] ? "addr" : "seqno",
				  stuck[i] ? ring->hangcheck.acthd & HEAD_ADDR :
				  ring->hangcheck.seqno);
		}
	}

	if (rings_hung) {
#ifdef RINGBUFFER_DEBUG
			{
				/*
				0x020XX --> Render
				0x120XX --> Video
				0x220XX --> Blitter
				*/

				mmioptr = dev_priv->regs;
				tmp = *(unsigned long *)(mmioptr + 0x1820b0);
				DRM_ERROR("EIR for MASTER_ERROR = 0x%08x", tmp);

				DRM_ERROR("Dump for Blitter RingBuffer\n");
				DRM_ERROR("---------------------------\n");
				tmp = I915_READ(0x2203C);//RING_BUFFER_CTL
				DRM_ERROR("     -----> RING_BUFFER_CTL   = 0x%08x\n", tmp);
				start = I915_READ(0x22038);//RING_BUFFER_START
				DRM_ERROR("     -----> RING_BUFFER_START = 0x%08x\n", start);
				head = I915_READ(0x22034);//RING_BUFFER_HEAD
				DRM_ERROR("     -----> RING_BUFFER_HEAD = 0x%08x\n", head);
				head &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN HEAD = 0x%08x\n", head);
				tail = I915_READ(0x22030);//RING_BUFFER_TAIL
				DRM_ERROR("     -----> RING_BUFFER_TAIL = 0x%08x\n", tail);
				tail &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN TAIL = 0x%08x\n", tail);
				tmp = I915_READ(0x2206C);//INSTDONE
				DRM_ERROR("     ----->       INSTDONE   = 0x%08x\n", tmp);
				tmp = I915_READ(0x22064);//IPEIR
				DRM_ERROR("     ----->       IPEIR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x22068);//IPEHR
				DRM_ERROR("     ----->       IPEHR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x22074);//ACTHD
				DRM_ERROR("     ----->       ACTHD      = 0x%08x\n", tmp);
				tmp = I915_READ(0x220B0);//EIR
				DRM_ERROR("     ----->       EIR        = 0x%08x\n", tmp);
				tmp = I915_READ(0x22070);//INSTPS
				DRM_ERROR("     ----->       INSTPS     = 0x%08x\n", tmp);
				tmp = I915_READ(0x22078);//DMA_FADD
				DRM_ERROR("     ----->       DMA_FADD   = 0x%08x\n", tmp);

				DRM_ERROR("Dump for Render RingBuffer\n");
				DRM_ERROR("---------------------------\n");
				tmp = I915_READ(0x0203C);//RING_BUFFER_CTL
				DRM_ERROR("     -----> RING_BUFFER_CTL   = 0x%08x\n", tmp);
				start = I915_READ(0x02038);//RING_BUFFER_START
				DRM_ERROR("     -----> RING_BUFFER_START = 0x%08x\n", start);
				head = I915_READ(0x02034);//RING_BUFFER_HEAD
				DRM_ERROR("     -----> RING_BUFFER_HEAD = 0x%08x\n", head);
				head &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN HEAD = 0x%08x\n", head);
				tail = I915_READ(0x02030);//RING_BUFFER_TAIL
				DRM_ERROR("     -----> RING_BUFFER_TAIL = 0x%08x\n", tail);
				tail &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN TAIL = 0x%08x\n", tail);
				tmp = I915_READ(0x0206C);//INSTDONE
				DRM_ERROR("     ----->       INSTDONE   = 0x%08x\n", tmp);
				tmp = I915_READ(0x02064);//IPEIR
				DRM_ERROR("     ----->       IPEIR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x02068);//IPEHR
				DRM_ERROR("     ----->       IPEHR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x02074);//ACTHD
				DRM_ERROR("     ----->       ACTHD      = 0x%08x\n", tmp);
				tmp = I915_READ(0x020B0);//EIR
				DRM_ERROR("     ----->       EIR        = 0x%08x\n", tmp);
				tmp = *(unsigned long *)(mmioptr + 0x020b0);
				DRM_ERROR("     ----->       EIR        = 0x%08x\n", tmp);
				tmp = I915_READ(0x02070);//INSTPS
				DRM_ERROR("     ----->       INSTPS     = 0x%08x\n", tmp);
				tmp = I915_READ(0x02078);//DMA_FADD
				DRM_ERROR("     ----->       DMA_FADD   = 0x%08x\n", tmp);


				DRM_ERROR("Dump for Video RingBuffer\n");
				DRM_ERROR("---------------------------\n");
				tmp = I915_READ(0x1203C);//RING_BUFFER_CTL
				DRM_ERROR("     -----> RING_BUFFER_CTL   = 0x%08x\n", tmp);
				start = I915_READ(0x12038);//RING_BUFFER_START
				DRM_ERROR("     -----> RING_BUFFER_START = 0x%08x\n", start);
				head = I915_READ(0x12034);//RING_BUFFER_HEAD
				DRM_ERROR("     -----> RING_BUFFER_HEAD = 0x%08x\n", head);
				head &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN HEAD = 0x%08x\n", head);
				tail = I915_READ(0x12030);//RING_BUFFER_TAIL
				DRM_ERROR("     -----> RING_BUFFER_TAIL = 0x%08x\n", tail);
				tail &= 0x000FFFFF;
				DRM_ERROR("     ----->       CLEAN TAIL = 0x%08x\n", tail);
				tmp = I915_READ(0x1206C);//INSTDONE
				DRM_ERROR("     ----->       INSTDONE   = 0x%08x\n", tmp);
				tmp = I915_READ(0x12064);//IPEIR
				DRM_ERROR("     ----->       IPEIR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x12068);//IPEHR
				DRM_ERROR("     ----->       IPEHR      = 0x%08x\n", tmp);
				tmp = I915_READ(0x12074);//ACTHD
				DRM_ERROR("     ----->       ACTHD      = 0x%08x\n", tmp);
				tmp = I915_READ(0x120B0);//EIR
				DRM_ERROR("     ----->       EIR        = 0x%08x\n", tmp);
				tmp = I915_READ(0x12070);//INSTPS
				DRM_ERROR("     ----->       INSTPS     = 0x%08x\n", tmp);
				tmp = I915_READ(0x12078);//DMA_FADD
				DRM_ERROR("     ----->       DMA_FADD   = 0x%08x\n", tmp);

			}
#endif

		return i915_handle_error(dev, true);
	}

	if (busy_count)
		/* Reset timer case chip hangs without another request
		 * being added */
		mod_timer(&dev_priv->gpu_error.hangcheck_timer,
			  round_jiffies_up(jiffies +
					   DRM_I915_HANGCHECK_JIFFIES));
}

/**
 * i915_error_work_func - do process context error handling work
 * @work: work struct
 *
 * Fire an error uevent so userspace can see that a hang or error
 * was detected.
 */
void i915_error_work_func(struct work_struct *work)
{
	struct i915_gpu_error *error = container_of(work, struct i915_gpu_error,
			work);
	drm_i915_private_t *dev_priv = container_of(error, drm_i915_private_t,
			gpu_error);
	struct drm_device *dev = dev_priv->dev;
	char *error_event[] = { "ERROR=1", NULL };
	char *reset_event[] = { "RESET=1", NULL };
	char *reset_done_event[] = { "ERROR=0", NULL };
	int ret;

	kobject_uevent_env(&dev->primary->kdev.kobj, KOBJ_CHANGE, error_event);

	if (i915_reset_in_progress(error)) {
		DRM_DEBUG_DRIVER("resetting chip\n");

		/* We aren't calling an i915_reset, however there are things we need to
		 * do that are in there, so they are here.
		 */
		dev_priv->gpu_error.stop_rings = 0;

		kobject_uevent_env(&dev->primary->kdev.kobj, KOBJ_CHANGE, reset_event);
		
		ret = i915_reset(dev);
		intel_display_handle_reset(dev);
		
		if (ret == 0) {
			/*
			 * After all the gem state is reset, increment the reset
			 * counter and wake up everyone waiting for the reset to
			 * complete.
			 *
			 * Since unlock operations are a one-sided barrier only,
			 * we need to insert a barrier here to order any seqno
			 * updates before
			 * the counter increment.
			 */
			smp_mb__before_atomic_inc();
			atomic_inc(&dev_priv->gpu_error.reset_counter);

			kobject_uevent_env(&dev->primary->kdev.kobj,
						KOBJ_CHANGE, reset_done_event);
		} else {
			atomic_set(&error->reset_counter, I915_WEDGED);
		}

		/*
		 * Note: The wake_up also serves as a memory barrier so that
		 *  waiters see the update value of the reset counter atomic_t.
		 */
		i915_error_wake_up(dev_priv, true);
	}
}

void intel_unpin_fb_obj(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin_fence(obj);
	i915_gem_object_unpin_from_display_plane(obj);
}


void vlv_force_wake_get(struct drm_i915_private *dev_priv)
{
	int count;

	count = 0;

	/* Already awake? */
	if ((I915_READ(0x130094) & 0xa1) == 0xa1)
		return;

	I915_WRITE_NOTRACE(FORCEWAKE_VLV, 0xffffffff);
	POSTING_READ(FORCEWAKE_VLV);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK_VLV) & 1) == 0)
		udelay(10);
}

void vlv_force_wake_put(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_VLV, 0xffff0000);
	/* FIXME: confirm VLV behavior with Punit folks */
	POSTING_READ(FORCEWAKE_VLV);
}



/**
 * i915_reset - reset chip after a hang
 * @dev: drm device to reset
 *
 * Reset the chip.  Useful if a hang is detected. Returns zero on successful
 * reset or otherwise an error code.
 *
 * Procedure is fairly simple:
 *   - reset the chip using the reset reg
 *   - re-init context state
 *   - re-init hardware status page
 *   - re-init ring buffer
 *   - re-init interrupt state
 *   - re-init display
 */
int i915_reset(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool simulated;
	int ret;

	if (!i915_try_reset)
		return 0;

	mutex_lock(&dev->struct_mutex);

	i915_gem_reset(dev);

	simulated = dev_priv->gpu_error.stop_rings != 0;

	if (!simulated && get_seconds() - dev_priv->gpu_error.last_reset < 5) {
		DRM_ERROR("GPU hanging too fast, declaring wedged!\n");
		ret = -ENODEV;
	} else {
		ret = intel_gpu_reset(dev);

		/* Also reset the gpu hangman. */
		if (simulated) {
			DRM_INFO("Simulated gpu hang, resetting stop_rings\n");
			dev_priv->gpu_error.stop_rings = 0;
			if (ret == -ENODEV) {
				DRM_ERROR("Reset not implemented, but ignoring "
					  "error for simulated gpu hangs\n");
				ret = 0;
			}
		} else
			dev_priv->gpu_error.last_reset = get_seconds();
	}
	if (ret) {
		DRM_ERROR("Failed to reset chip.\n");
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	/* Ok, now get things going again... */

	/*
	 * Everything depends on having the GTT running, so we need to start
	 * there.  Fortunately we don't need to do this unless we reset the
	 * chip at a PCI level.
	 *
	 * Next we need to restore the context, but we don't use those
	 * yet either...
	 *
	 * Ring buffer needs to be re-initialized in the KMS case, or if X
	 * was running at the time of the reset (i.e. we weren't VT
	 * switched away).
	 */
	if (drm_core_check_feature(dev, DRIVER_MODESET) ||
			!dev_priv->ums.mm_suspended) {
		struct intel_ring_buffer *ring;
		int i;

		dev_priv->ums.mm_suspended = 0;

		i915_gem_init_swizzling(dev);

		for_each_ring(ring, dev_priv, i)
			ring->init(ring);

		i915_gem_context_init(dev);
		if (dev_priv->mm.aliasing_ppgtt) {
			ret = dev_priv->mm.aliasing_ppgtt->enable(dev);
			if (ret)
				i915_gem_cleanup_aliasing_ppgtt(dev);
		}

		/* Enable after with ppgtt support */
		/*
		if (dev_priv->mm.aliasing_ppgtt) {
			ret = dev_priv->mm.aliasing_ppgtt->enable(dev);
			if (ret)
				i915_gem_cleanup_aliasing_ppgtt(dev);
		}
		*/
		/*
		 * It would make sense to re-init all the other hw state, at
		 * least the rps/rc6/emon init done within modeset_init_hw. For
		 * some unknown reason, this blows up my ilk, so don't.
		 */

		mutex_unlock(&dev->struct_mutex);

		drm_irq_uninstall(dev);
		drm_irq_install(dev);
		/* EMGD doesn't have hotplug support yet 
 		 * Uncomment below only after add hotplug
 		 */
		/*	
 		intel_hpd_init(dev); 
		*/
	} else {
		mutex_unlock(&dev->struct_mutex);
	}

	return 0;
}

void vlv_turbo_up(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	/*
	 * This will lock the freq to the desired GPU freq when user
	 * write to either debugsfs/sysfs node of i915_max_freg
	 * eg: echo "792" > /sys/kernel/debug/dri/0/i915_max_freq
	 */
	if (dev_priv->rps.max_lock)
		return;

	/*
	 *  if we've retired everything and go idle,
	 *  but then we get a new submission before the rps.work
	 *  timer fires, we want to cancel that timer.
	 */
	cancel_delayed_work_sync(&dev_priv->rps.work);

	mutex_lock(&dev_priv->rps.hw_lock);
	if (dev_priv->rps.cur_delay < dev_priv->rps.max_delay)
		valleyview_set_rps(dev_priv->dev, dev_priv->rps.max_delay);
	mutex_unlock(&dev_priv->rps.hw_lock);
}

void vlv_turbo_down_work(struct work_struct *work)
{
	drm_i915_private_t *dev_priv = container_of(work, drm_i915_private_t,
						   rps.work.work);

	/*
	 * This will lock the freq to the desired GPU freq when user
	 * write to either debugsfs/sysfs node of i915_max_freg
	 * eg: echo "792" > /sys/kernel/debug/dri/0/i915_max_freq
	 */

	if (dev_priv->rps.max_lock)
		return;

	mutex_lock(&dev_priv->rps.hw_lock);
	if (dev_priv->rps.cur_delay > dev_priv->rps.rpe_delay)
		valleyview_set_rps(dev_priv->dev, dev_priv->rps.rpe_delay);
	mutex_unlock(&dev_priv->rps.hw_lock);
}


