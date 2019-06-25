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
 *    Keith Packard <keithp@keithp.com>
 *
 */

#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/export.h>
#include "drmP.h"
#include "i915_reg.h"
#include "drm_emgd_private.h"
#include "intel_ringbuffer.h"
#include "i915_drm.h"
#include "intel_drv.h"

#define DRM_I915_RING_DEBUG 1


#if defined(CONFIG_DEBUG_FS)

enum {
	ACTIVE_LIST,
	INACTIVE_LIST,
	PINNED_LIST,
};

static const char *yesno(int v)
{
	return v ? "yes" : "no";
}

static int i915_capabilities(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	const struct intel_device_info *info = INTEL_INFO(dev);

	seq_printf(m, "gen: %d\n", info->gen);
	seq_printf(m, "pch: %d\n", INTEL_PCH_TYPE(dev));
#define B(x) seq_printf(m, #x ": %s\n", yesno(info->x))
	B(is_mobile);
	B(is_i85x);
	B(is_i915g);
	B(is_i945gm);
	B(is_g33);
	B(need_gfx_hws);
	B(is_g4x);
	B(is_pineview);
	B(is_broadwater);
	B(is_crestline);
	B(has_fbc);
	B(has_pipe_cxsr);
	B(has_hotplug);
	B(cursor_needs_physical);
	B(has_overlay);
	B(overlay_needs_physical);
	B(supports_tv);
	B(has_bsd_ring);
	B(has_blt_ring);
	B(has_llc);
	B(has_vebox);
#undef B

	return 0;
}

static const char *get_pin_flag(struct drm_i915_gem_object *obj)
{
	if (obj->user_pin_count > 0)
		return "P";
	else if (obj->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct drm_i915_gem_object *obj)
{
	switch (obj->tiling_mode) {
	default:
	case I915_TILING_NONE: return " ";
	case I915_TILING_X: return "X";
	case I915_TILING_Y: return "Y";
	}
}

static void
describe_obj(struct seq_file *m, struct drm_i915_gem_object *obj)
{
	seq_printf(m, "%p: %s%s %8zdKiB %04x %04x %d %d %d%s%s%s",
		   &obj->base,
		   get_pin_flag(obj),
		   get_tiling_flag(obj),
		   obj->base.size / 1024,
		   obj->base.read_domains,
		   obj->base.write_domain,
		   obj->last_read_seqno,
		   obj->last_write_seqno,
		   obj->last_fenced_seqno,
		   i915_cache_level_str(obj->cache_level),
		   obj->dirty ? " dirty" : "",
		   obj->madv == I915_MADV_DONTNEED ? " purgeable" : "");
	if (obj->base.name)
		seq_printf(m, " (name: %d)", obj->base.name);
	if (obj->pin_display)
		seq_printf(m, " (display)");
	if (obj->fence_reg != I915_FENCE_REG_NONE)
		seq_printf(m, " (fence: %d)", obj->fence_reg);
	if (i915_gem_obj_ggtt_bound(obj))
		seq_printf(m, " (gtt offset: %08lx, size: %08x)",
				i915_gem_obj_ggtt_offset(obj), (unsigned int)i915_gem_obj_ggtt_size(obj));
	if (obj->pin_mappable || obj->fault_mappable) {
		char s[3], *t = s;
		if (obj->pin_mappable)
			*t++ = 'p';
		if (obj->fault_mappable)
			*t++ = 'f';
		*t = '\0';
		seq_printf(m, " (%s mappable)", s);
	}
	if (obj->ring != NULL)
		seq_printf(m, " (%s)", obj->ring->name);
}

static void describe_ctx(struct seq_file *m, struct i915_hw_context *ctx)
{
	seq_putc(m, ctx->is_initialized ? 'I' : 'i');
	seq_putc(m, ctx->remap_slice ? 'R' : 'r');
	seq_putc(m, ' ');
}

static int i915_gem_object_list_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	uintptr_t list = (uintptr_t) node->info_ent->data;
	struct list_head *head;
	struct drm_device *dev = node->minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *vm = &dev_priv->gtt.base;
	struct i915_vma *vma;
	size_t total_obj_size, total_gtt_size;
	int count, ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	/* FIXME: the user of this interface might want more than just GGTT */
	switch (list) {
	case ACTIVE_LIST:
		seq_printf(m, "Active:\n");
		head = &vm->active_list;
		break;
	case INACTIVE_LIST:
		seq_printf(m, "Inactive:\n");
		head = &vm->inactive_list;
		break;
	default:
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	total_obj_size = total_gtt_size = count = 0;
	list_for_each_entry(vma, head, mm_list) {
		seq_printf(m, "   ");
		describe_obj(m, vma->obj);
		seq_printf(m, "\n");
		total_obj_size += vma->obj->base.size;
		total_gtt_size += vma->node.size;
		count++;
	}
	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "Total %d objects, %zu bytes, %zu GTT size\n",
		   count, total_obj_size, total_gtt_size);
	return 0;
}

#define count_objects(list, member) do { \
	list_for_each_entry(obj, list, member) { \
		size += i915_gem_obj_ggtt_size(obj); \
		++count; \
		if (obj->map_and_fenceable) { \
				mappable_size += i915_gem_obj_ggtt_size(obj); \
				++mappable_count; \
		} \
	} \
} while (0)

#define count_vmas(list, member) do { \
	list_for_each_entry(vma, list, member) { \
		size += i915_gem_obj_ggtt_size(vma->obj); \
		++count; \
		if (vma->obj->map_and_fenceable) { \
			mappable_size += i915_gem_obj_ggtt_size(vma->obj); \
			++mappable_count; \
		} \
	} \
} while (0)

static int i915_gem_object_info(struct seq_file *m, void* data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *vm = &dev_priv->gtt.base;
	u32 count, mappable_count;
	size_t size, mappable_size;
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	seq_printf(m, "%u objects, %zu bytes\n",
		   dev_priv->mm.object_count,
		   dev_priv->mm.object_memory);

    size = count = 0;
	list_for_each_entry(obj, &dev_priv->mm.unbound_list, global_list)
               size += obj->base.size, ++count;
    seq_printf(m, "%u unbound objects, %zu bytes\n", count, size);


	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.bound_list, global_list);
	seq_printf(m, "%u [%u] objects, %zu [%zu] bytes in gtt\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_vmas(&vm->active_list, mm_list);
	seq_printf(m, "  %u [%u] active objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_vmas(&vm->inactive_list, mm_list);
	seq_printf(m, "  %u [%u] inactive objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	list_for_each_entry(obj, &dev_priv->mm.bound_list, global_list) {
		if (obj->fault_mappable) {
			size += i915_gem_obj_ggtt_size(obj);
			++count;
		}
		if (obj->pin_mappable) {
			mappable_size += i915_gem_obj_ggtt_size(obj);
			++mappable_count;
		}
	}
	seq_printf(m, "%u pinned mappable objects, %zu bytes\n",
		   mappable_count, mappable_size);
	seq_printf(m, "%u fault mappable objects, %zu bytes\n",
		   count, size);

	seq_printf(m, "%zu [%lu] gtt total\n",
		   dev_priv->gtt.base.total,
		   dev_priv->gtt.mappable_end - dev_priv->gtt.base.start);

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_gem_gtt_info(struct seq_file *m, void* data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	uintptr_t list = (uintptr_t) node->info_ent->data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	size_t total_obj_size, total_gtt_size;
	int count, ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	total_obj_size = total_gtt_size = count = 0;
	list_for_each_entry(obj, &dev_priv->mm.bound_list, global_list) {
		if (list == PINNED_LIST && obj->pin_count == 0)
			continue;

		seq_printf(m, "   ");
		describe_obj(m, obj);
		seq_printf(m, "\n");
		total_obj_size += obj->base.size;
		total_gtt_size += i915_gem_obj_ggtt_size(obj);
		count++;
	}

	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "Total %d objects, %zu bytes, %zu GTT size\n",
		   count, total_obj_size, total_gtt_size);

	return 0;
}

static int i915_gem_pageflip_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	unsigned long flags;
	struct drm_crtc *crtc = NULL;
	struct intel_unpin_work *work;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if(crtc != NULL) {
			intel_crtc *intel_crtc = to_intel_crtc(crtc);
			const char pipe = pipe_name(intel_crtc->pipe);
			const char plane = plane_name(intel_crtc->plane);

			spin_lock_irqsave(&dev->event_lock, flags);
			work = intel_crtc->unpin_work;
			if (work == NULL) {
				seq_printf(m, "No flip due on pipe %c (plane %c)\n",
					   pipe, plane);
			} else {
				if (!work->pending) {
					seq_printf(m, "Flip queued on pipe %c (plane %c)\n",
						   pipe, plane);
				} else {
					seq_printf(m, "Flip pending (waiting for vsync) on pipe %c (plane %c)\n",
						   pipe, plane);
				}
				if (work->enable_stall_check)
					seq_printf(m, "Stall check enabled, ");
				else
					seq_printf(m, "Stall check waiting for page flip ioctl, ");
				seq_printf(m, "%d prepares\n", work->pending);

				if (work->old_fb_obj) {
					struct drm_i915_gem_object *obj = work->old_fb_obj;
					if (obj)
						seq_printf(m, "Old framebuffer gtt_offset 0x%08lx\n",
								i915_gem_obj_ggtt_offset(obj));
				}
				if (work->pending_flip_obj) {
					struct drm_i915_gem_object *obj = work->pending_flip_obj;
					if (obj)
						seq_printf(m, "New framebuffer gtt_offset 0x%08lx\n",
								i915_gem_obj_ggtt_offset(obj));
				}
			}
			spin_unlock_irqrestore(&dev->event_lock, flags);
		}
	}

	return 0;
}

static int i915_gem_request_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_request *gem_request;
	int ret, count;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	count = 0;
	if (!list_empty(&dev_priv->ring[RCS].request_list)) {
		seq_printf(m, "Render requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->ring[RCS].request_list,
				    list) {
			seq_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	if (!list_empty(&dev_priv->ring[VCS].request_list)) {
		seq_printf(m, "BSD requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->ring[VCS].request_list,
				    list) {
			seq_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	if (!list_empty(&dev_priv->ring[BCS].request_list)) {
		seq_printf(m, "BLT requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->ring[BCS].request_list,
				    list) {
			seq_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	mutex_unlock(&dev->struct_mutex);

	if (count == 0)
		seq_printf(m, "No requests\n");

	return 0;
}

static void i915_ring_seqno_info(struct seq_file *m,
				 struct intel_ring_buffer *ring)
{
	if (ring->get_seqno) {
		seq_printf(m, "Current sequence (%s): %u\n",
			   ring->name, ring->get_seqno(ring, false));
	}
}

static int i915_gem_seqno_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret, i;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	for (i = 0; i < I915_NUM_RINGS; i++)
		i915_ring_seqno_info(m, &dev_priv->ring[i]);

	mutex_unlock(&dev->struct_mutex);

	return 0;
}


static int i915_interrupt_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret, i, pipe;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	if (IS_VALLEYVIEW(dev)) {
		seq_printf(m, "Display IER:\t%08x\n",
			   I915_READ(VLV_IER));
		seq_printf(m, "Display IIR:\t%08x\n",
			   I915_READ(VLV_IIR));
		seq_printf(m, "Display IIR_RW:\t%08x\n",
			   I915_READ(VLV_IIR_RW));
		seq_printf(m, "Display IMR:\t%08x\n",
			   I915_READ(VLV_IMR));
		for_each_pipe(pipe)
			seq_printf(m, "Pipe %c stat:\t%08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));

		seq_printf(m, "Master IER:\t%08x\n",
			   I915_READ(VLV_MASTER_IER));

		seq_printf(m, "Render IER:\t%08x\n",
			   I915_READ(GTIER));
		seq_printf(m, "Render IIR:\t%08x\n",
			   I915_READ(GTIIR));
		seq_printf(m, "Render IMR:\t%08x\n",
			   I915_READ(GTIMR));

		seq_printf(m, "PM IER:\t\t%08x\n",
			   I915_READ(GEN6_PMIER));
		seq_printf(m, "PM IIR:\t\t%08x\n",
			   I915_READ(GEN6_PMIIR));
		seq_printf(m, "PM IMR:\t\t%08x\n",
			   I915_READ(GEN6_PMIMR));

		seq_printf(m, "Port hotplug:\t%08x\n",
			   I915_READ(PORT_HOTPLUG_EN));
		seq_printf(m, "DPFLIPSTAT:\t%08x\n",
			   I915_READ(VLV_DPFLIPSTAT));
		seq_printf(m, "DPINVGTT:\t%08x\n",
			   I915_READ(DPINVGTT));

	} else if (!HAS_PCH_SPLIT(dev)) {
		seq_printf(m, "Interrupt enable:    %08x\n",
			   I915_READ(IER));
		seq_printf(m, "Interrupt identity:  %08x\n",
			   I915_READ(IIR));
		seq_printf(m, "Interrupt mask:      %08x\n",
			   I915_READ(IMR));
		for_each_pipe(pipe)
			seq_printf(m, "Pipe %c stat:         %08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));
	} else {
		seq_printf(m, "North Display Interrupt enable:		%08x\n",
			   I915_READ(DEIER));
		seq_printf(m, "North Display Interrupt identity:	%08x\n",
			   I915_READ(DEIIR));
		seq_printf(m, "North Display Interrupt mask:		%08x\n",
			   I915_READ(DEIMR));
		seq_printf(m, "South Display Interrupt enable:		%08x\n",
			   I915_READ(SDEIER));
		seq_printf(m, "South Display Interrupt identity:	%08x\n",
			   I915_READ(SDEIIR));
		seq_printf(m, "South Display Interrupt mask:		%08x\n",
			   I915_READ(SDEIMR));
		seq_printf(m, "Graphics Interrupt enable:		%08x\n",
			   I915_READ(GTIER));
		seq_printf(m, "Graphics Interrupt identity:		%08x\n",
			   I915_READ(GTIIR));
		seq_printf(m, "Graphics Interrupt mask:		%08x\n",
			   I915_READ(GTIMR));
	}
	seq_printf(m, "Interrupts received: %d\n",
		   atomic_read(&dev_priv->irq_received));
	for (i = 0; i < I915_NUM_RINGS; i++) {
		if (IS_GEN6(dev) || IS_GEN7(dev)) {
			seq_printf(m, "Graphics Interrupt mask (%s):	%08x\n",
				   dev_priv->ring[i].name,
				   I915_READ_IMR(&dev_priv->ring[i]));
		}
		i915_ring_seqno_info(m, &dev_priv->ring[i]);
	}
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_gem_fence_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i, ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	seq_printf(m, "Reserved fences = %d\n", dev_priv->fence_reg_start);
	seq_printf(m, "Total fences = %d\n", dev_priv->num_fence_regs);
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_gem_object *obj = dev_priv->fence_regs[i].obj;

		seq_printf(m, "Fence %d, pin count = %d, object = ",
				i, dev_priv->fence_regs[i].pin_count);

		if (obj == NULL)
			seq_printf(m, "unused");
		else
			describe_obj(m, obj);
		seq_printf(m, "\n");
	}

	mutex_unlock(&dev->struct_mutex);
	return 0;
}

static int i915_hws_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	const volatile u32 __iomem *hws;
	int i;

	ring = &dev_priv->ring[(uintptr_t)node->info_ent->data];
	hws = (volatile u32 __iomem *)ring->status_page.page_addr;
	if (hws == NULL)
		return 0;

	for (i = 0; i < 4096 / sizeof(u32) / 4; i += 4) {
		seq_printf(m, "0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   i * 4,
			   hws[i], hws[i + 1], hws[i + 2], hws[i + 3]);
	}
	return 0;
}

static void i915_dump_object(struct seq_file *m,
			     struct io_mapping *mapping,
			     struct drm_i915_gem_object *obj)
{
	int page, page_count, i;

	page_count = obj->base.size / PAGE_SIZE;
	for (page = 0; page < page_count; page++) {
		u32 *mem = io_mapping_map_wc(mapping,
					     i915_gem_obj_ggtt_offset(obj) + page * PAGE_SIZE);
		for (i = 0; i < PAGE_SIZE; i += 4)
			seq_printf(m, "%08x :  %08x\n", i, mem[i / 4]);
		io_mapping_unmap(mem);
	}
}

static int i915_batchbuffer_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *vm = &dev_priv->gtt.base;
	struct i915_vma *vma;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	list_for_each_entry(vma, &vm->active_list, mm_list) {
		if (vma->obj->base.read_domains &
			I915_GEM_DOMAIN_COMMAND) {
			seq_printf(m, "--- gtt_offset = 0x%0lx\n",
				i915_gem_obj_ggtt_offset(vma->obj));
			i915_dump_object(m, dev_priv->gtt.mappable, vma->obj);
		}
	}

	mutex_unlock(&dev->struct_mutex);
	return 0;
}

static int i915_ringbuffer_data(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	ring = &dev_priv->ring[(uintptr_t)node->info_ent->data];
	if (!ring->obj) {
		seq_printf(m, "No ringbuffer setup\n");
	} else {
		const u8 __iomem *virt = ring->virtual_start;
		uint32_t off;

		for (off = 0; off < ring->size; off += 4) {
			uint32_t *ptr = (uint32_t *)(virt + off);
			seq_printf(m, "%08x :  %08x\n", off, *ptr);
		}
	}
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_ringbuffer_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret;

	ring = &dev_priv->ring[(uintptr_t)node->info_ent->data];
	if (ring->size == 0)
		return 0;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	seq_printf(m, "Ring %s:\n", ring->name);
	seq_printf(m, "  Head :    %08x\n", I915_READ_HEAD(ring) & HEAD_ADDR);
	seq_printf(m, "  Tail :    %08x\n", I915_READ_TAIL(ring) & TAIL_ADDR);
	seq_printf(m, "  Size :    %08x\n", ring->size);
	seq_printf(m, "  Active :  %08x\n", intel_ring_get_active_head(ring));
	seq_printf(m, "  NOPID :   %08x\n", I915_READ_NOPID(ring));
	if (IS_GEN6(dev) || IS_GEN7(dev)) {
		seq_printf(m, "  Sync 0 :   %08x\n", I915_READ_SYNC_0(ring));
		seq_printf(m, "  Sync 1 :   %08x\n", I915_READ_SYNC_1(ring));
	}
	seq_printf(m, "  Control : %08x\n", I915_READ_CTL(ring));
	seq_printf(m, "  Start :   %08x\n", I915_READ_START(ring));

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_error_state_open(struct inode *inode, struct file *file)
{
        struct drm_device *dev = inode->i_private;
        struct i915_error_state_file_priv *error_priv;

        error_priv = kzalloc(sizeof(*error_priv), GFP_KERNEL);
        if (!error_priv)
                return -ENOMEM;

        error_priv->dev = dev;

	i915_error_state_get(dev, error_priv);

	file->private_data = error_priv;

	return 0;
}

static int i915_error_state_release(struct inode *inode, struct file *file)
{

	struct i915_error_state_file_priv *error_priv = file->private_data;

	i915_error_state_put(error_priv);
        kfree(error_priv);

	return 0;
}

static ssize_t i915_error_state_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *pos)
{
	struct i915_error_state_file_priv *error_priv = file->private_data;
	struct drm_i915_error_state_buf error_str;
	loff_t tmp_pos = 0;
	ssize_t ret_count = 0;
	int ret;

	ret = i915_error_state_buf_init(&error_str, count, *pos);
	if (ret)
		return ret;

	ret = i915_error_state_to_str(&error_str, error_priv);
	if (ret)
		goto out;

	ret_count = simple_read_from_buffer(userbuf, count, &tmp_pos,
					    error_str.buf,
					    error_str.bytes);

	if (ret_count < 0)
		ret = ret_count;
	else
		*pos = error_str.start + ret_count;
out:
	i915_error_state_buf_release(&error_str);
	return ret ?: ret_count;
}

static const struct file_operations i915_error_state_fops = {
        .owner = THIS_MODULE,
        .open = i915_error_state_open,
	.read = i915_error_state_read,
        //.write = i915_error_state_write,
        .llseek = default_llseek,
        .release = i915_error_state_release,
};

static int
i915_debugfs_common_open(struct inode *inode,
						 struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static int
i915_next_seqno_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	*val = dev_priv->next_seqno;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int
i915_next_seqno_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	ret = i915_gem_set_seqno(dev, val);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_next_seqno_fops,
		i915_next_seqno_get, i915_next_seqno_set,
		"next_seqno :  0x%llx\n");

static int i915_rstdby_delays(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u16 crstanddelay;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	crstanddelay = I915_READ16(CRSTANDVID);

	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "w/ctx: %d, w/o ctx: %d\n", (crstanddelay >> 8) & 0x3f, (crstanddelay & 0x3f));

	return 0;
}

static int i915_cur_delayinfo(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;
	u32 pval;

	if (IS_GEN5(dev)) {
		u16 rgvswctl = I915_READ16(MEMSWCTL);
		u16 rgvstat = I915_READ16(MEMSTAT_ILK);

		seq_printf(m, "Requested P-state: %d\n", (rgvswctl >> 8) & 0xf);
		seq_printf(m, "Requested VID: %d\n", rgvswctl & 0x3f);
		seq_printf(m, "Current VID: %d\n", (rgvstat & MEMSTAT_VID_MASK) >>
			   MEMSTAT_VID_SHIFT);
		seq_printf(m, "Current P-state: %d\n",
			   (rgvstat & MEMSTAT_PSTATE_MASK) >> MEMSTAT_PSTATE_SHIFT);
	} else if ((IS_GEN6(dev) || IS_GEN7(dev)) && !IS_VALLEYVIEW(dev)) {
		u32 gt_perf_status = I915_READ(GEN6_GT_PERF_STATUS);
		u32 rp_state_limits = I915_READ(GEN6_RP_STATE_LIMITS);
		u32 rp_state_cap = I915_READ(GEN6_RP_STATE_CAP);
		u32 rpstat;
		u32 rpupei, rpcurup, rpprevup;
		u32 rpdownei, rpcurdown, rpprevdown;
		int max_freq;

		/* RPSTAT1 is in the GT power well */
		ret = mutex_lock_interruptible(&dev->struct_mutex);
		if (ret)
			return ret;

		gen6_gt_force_wake_get(dev_priv);

		rpstat = I915_READ(GEN6_RPSTAT1);
		rpupei = I915_READ(GEN6_RP_CUR_UP_EI);
		rpcurup = I915_READ(GEN6_RP_CUR_UP);
		rpprevup = I915_READ(GEN6_RP_PREV_UP);
		rpdownei = I915_READ(GEN6_RP_CUR_DOWN_EI);
		rpcurdown = I915_READ(GEN6_RP_CUR_DOWN);
		rpprevdown = I915_READ(GEN6_RP_PREV_DOWN);

		gen6_gt_force_wake_put(dev_priv);
		mutex_unlock(&dev->struct_mutex);

		seq_printf(m, "GT_PERF_STATUS: 0x%08x\n", gt_perf_status);
		seq_printf(m, "RPSTAT1: 0x%08x\n", rpstat);
		seq_printf(m, "Render p-state ratio: %d\n",
			   (gt_perf_status & 0xff00) >> 8);
		seq_printf(m, "Render p-state VID: %d\n",
			   gt_perf_status & 0xff);
		seq_printf(m, "Render p-state limit: %d\n",
			   rp_state_limits & 0xff);
		seq_printf(m, "CAGF: %dMHz\n", ((rpstat & GEN6_CAGF_MASK) >>
						GEN6_CAGF_SHIFT) * GT_FREQUENCY_MULTIPLIER);
		seq_printf(m, "RP CUR UP EI: %dus\n", rpupei &
			   GEN6_CURICONT_MASK);
		seq_printf(m, "RP CUR UP: %dus\n", rpcurup &
			   GEN6_CURBSYTAVG_MASK);
		seq_printf(m, "RP PREV UP: %dus\n", rpprevup &
			   GEN6_CURBSYTAVG_MASK);
		seq_printf(m, "RP CUR DOWN EI: %dus\n", rpdownei &
			   GEN6_CURIAVG_MASK);
		seq_printf(m, "RP CUR DOWN: %dus\n", rpcurdown &
			   GEN6_CURBSYTAVG_MASK);
		seq_printf(m, "RP PREV DOWN: %dus\n", rpprevdown &
			   GEN6_CURBSYTAVG_MASK);

		max_freq = (rp_state_cap & 0xff0000) >> 16;
		seq_printf(m, "Lowest (RPN) frequency: %dMHz\n",
			   max_freq * GT_FREQUENCY_MULTIPLIER);

		max_freq = (rp_state_cap & 0xff00) >> 8;
		seq_printf(m, "Nominal (RP1) frequency: %dMHz\n",
			   max_freq * GT_FREQUENCY_MULTIPLIER);

		max_freq = rp_state_cap & 0xff;
		seq_printf(m, "Max non-overclocked (RP0) frequency: %dMHz\n",
			   max_freq * GT_FREQUENCY_MULTIPLIER);

		seq_printf(m, "Max overclocked frequency: %dMHz\n",
				dev_priv->rps.hw_max * GT_FREQUENCY_MULTIPLIER);
	} else if (IS_VALLEYVIEW(dev)) {

		ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
		if (ret)
			return ret;

		valleyview_punit_read(dev_priv, PUNIT_REG_GPU_FREQ_STS, &pval);
		mutex_unlock(&dev_priv->rps.hw_lock);

		dev_priv->rps.cur_delay = pval >> 8;

		seq_printf(m, "DDR freq: %d MHz\n", dev_priv->mem_freq);

		seq_printf(m, "Max GPU freq: %d MHz\n",
				vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.max_delay));
		seq_printf(m, "Min GPU freq: %d MHz\n",
				vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.min_delay));
		seq_printf(m, "Current GPU freq: %d MHz\n",
				vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.cur_delay));

	} else {
		seq_printf(m, "no P-state info available\n");
	}

	return 0;
}

static int i915_delayfreq_table(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 delayfreq;
	int ret, i;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	for (i = 0; i < 16; i++) {
		delayfreq = I915_READ(PXVFREQ_BASE + i * 4);
		seq_printf(m, "P%02dVIDFREQ: 0x%08x (VID: %d)\n", i, delayfreq,
			   (delayfreq & PXVFREQ_PX_MASK) >> PXVFREQ_PX_SHIFT);
	}

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static inline int MAP_TO_MV(int map)
{
	return 1250 - (map * 25);
}

static int i915_inttoext_table(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 inttoext;
	int ret, i;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	for (i = 1; i <= 32; i++) {
		inttoext = I915_READ(INTTOEXT_BASE_ILK + i * 4);
		seq_printf(m, "INTTOEXT%02d: 0x%08x\n", i, inttoext);
	}

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int ironlake_drpc_info(struct seq_file *m)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 rgvmodectl, rstdbyctl;
	u16 crstandvid;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	rgvmodectl = I915_READ(MEMMODECTL);
	rstdbyctl = I915_READ(RSTDBYCTL);
	crstandvid = I915_READ16(CRSTANDVID);

	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "HD boost: %s\n", (rgvmodectl & MEMMODE_BOOST_EN) ?
		   "yes" : "no");
	seq_printf(m, "Boost freq: %d\n",
		   (rgvmodectl & MEMMODE_BOOST_FREQ_MASK) >>
		   MEMMODE_BOOST_FREQ_SHIFT);
	seq_printf(m, "HW control enabled: %s\n",
		   rgvmodectl & MEMMODE_HWIDLE_EN ? "yes" : "no");
	seq_printf(m, "SW control enabled: %s\n",
		   rgvmodectl & MEMMODE_SWMODE_EN ? "yes" : "no");
	seq_printf(m, "Gated voltage change: %s\n",
		   rgvmodectl & MEMMODE_RCLK_GATE ? "yes" : "no");
	seq_printf(m, "Starting frequency: P%d\n",
		   (rgvmodectl & MEMMODE_FSTART_MASK) >> MEMMODE_FSTART_SHIFT);
	seq_printf(m, "Max P-state: P%d\n",
		   (rgvmodectl & MEMMODE_FMAX_MASK) >> MEMMODE_FMAX_SHIFT);
	seq_printf(m, "Min P-state: P%d\n", (rgvmodectl & MEMMODE_FMIN_MASK));
	seq_printf(m, "RS1 VID: %d\n", (crstandvid & 0x3f));
	seq_printf(m, "RS2 VID: %d\n", ((crstandvid >> 8) & 0x3f));
	seq_printf(m, "Render standby enabled: %s\n",
		   (rstdbyctl & RCX_SW_EXIT) ? "no" : "yes");
	seq_printf(m, "Current RS state: ");
	switch (rstdbyctl & RSX_STATUS_MASK) {
	case RSX_STATUS_ON:
		seq_printf(m, "on\n");
		break;
	case RSX_STATUS_RC1:
		seq_printf(m, "RC1\n");
		break;
	case RSX_STATUS_RC1E:
		seq_printf(m, "RC1E\n");
		break;
	case RSX_STATUS_RS1:
		seq_printf(m, "RS1\n");
		break;
	case RSX_STATUS_RS2:
		seq_printf(m, "RS2 (RC6)\n");
		break;
	case RSX_STATUS_RS3:
		seq_printf(m, "RC3 (RC6+)\n");
		break;
	default:
		seq_printf(m, "unknown\n");
		break;
	}

	return 0;
}

static int gen6_drpc_info(struct seq_file *m)
{

	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 rpmodectl1, gt_core_status, rcctl1;
	unsigned forcewake_count;
	int count=0, ret;


	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	spin_lock_irq(&dev_priv->uncore.lock);
	forcewake_count = dev_priv->uncore.forcewake_count;
	spin_unlock_irq(&dev_priv->uncore.lock);

	if (forcewake_count) {
		seq_printf(m, "RC information inaccurate because somebody "
				"holds a forcewake reference \n");
	} else {
		/* NB: we cannot use forcewake, else we read the wrong values */
		while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1))
			udelay(10);
		seq_printf(m, "RC information accurate: %s\n", yesno(count < 51));
	}

	gt_core_status = readl(dev_priv->regs + GEN6_GT_CORE_STATUS);
	trace_i915_reg_rw(false, GEN6_GT_CORE_STATUS, gt_core_status, 4);

	rpmodectl1 = I915_READ(GEN6_RP_CONTROL);
	rcctl1 = I915_READ(GEN6_RC_CONTROL);
	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "Video Turbo Mode: %s\n",
			yesno(rpmodectl1 & GEN6_RP_MEDIA_TURBO));
	seq_printf(m, "HW control enabled: %s\n",
			yesno(rpmodectl1 & GEN6_RP_ENABLE));
	seq_printf(m, "SW control enabled: %s\n",
			yesno((rpmodectl1 & GEN6_RP_MEDIA_MODE_MASK) ==
				GEN6_RP_MEDIA_SW_MODE));
	seq_printf(m, "RC1e Enabled: %s\n",
			yesno(rcctl1 & GEN6_RC_CTL_RC1e_ENABLE));
	seq_printf(m, "RC6 Enabled: %s\n",
			yesno(rcctl1 & GEN6_RC_CTL_RC6_ENABLE));
	seq_printf(m, "Deep RC6 Enabled: %s\n",
			yesno(rcctl1 & GEN6_RC_CTL_RC6p_ENABLE));
	seq_printf(m, "Deepest RC6 Enabled: %s\n",
			yesno(rcctl1 & GEN6_RC_CTL_RC6pp_ENABLE));
	seq_printf(m, "Current RC state: ");
	switch (gt_core_status & GEN6_RCn_MASK) {
		case GEN6_RC0:
			if (gt_core_status & GEN6_CORE_CPD_STATE_MASK)
				seq_printf(m, "Core Power Down\n");
			else
				seq_printf(m, "on\n");
			break;
		case GEN6_RC3:
			seq_printf(m, "RC3\n");
			break;
		case GEN6_RC6:
			seq_printf(m, "RC6\n");
			break;
		case GEN6_RC7:
			seq_printf(m, "RC7\n");
			break;
		default:
			seq_printf(m, "Unknown\n");
			break;
	}

	seq_printf(m, "Core Power Down: %s\n",
			yesno(gt_core_status & GEN6_CORE_CPD_STATE_MASK));
	return 0;
}

static int i915_drpc_info(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;

	if (IS_GEN6(dev) || IS_GEN7(dev))
		return gen6_drpc_info(m);
	else
		return ironlake_drpc_info(m);
}

static int i915_fbc_status(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!I915_HAS_FBC(dev)) {
		seq_printf(m, "FBC unsupported on this chipset\n");
		return 0;
	}

	/* FBC not enabled in EMGD at the moment
	 * if (intel_fbc_enabled(dev)) {
	 *	seq_printf(m, "FBC enabled\n");
	 * } else {
	 */
		seq_printf(m, "FBC disabled: ");
		switch (dev_priv->fbc.no_fbc_reason) {
		case FBC_NO_OUTPUT:
			seq_printf(m, "no outputs");
			break;
		case FBC_STOLEN_TOO_SMALL:
			seq_printf(m, "not enough stolen memory");
			break;
		case FBC_UNSUPPORTED_MODE:
			seq_printf(m, "mode not supported");
			break;
		case FBC_MODE_TOO_LARGE:
			seq_printf(m, "mode too large");
			break;
		case FBC_BAD_PLANE:
			seq_printf(m, "FBC unsupported on plane");
			break;
		case FBC_NOT_TILED:
			seq_printf(m, "scanout buffer not tiled");
			break;
		case FBC_MULTIPLE_PIPES:
			seq_printf(m, "multiple pipes are enabled");
			break;
		case FBC_MODULE_PARAM:
			seq_printf(m, "disabled per module param (default off)");
			break;
		default:
			seq_printf(m, "unknown reason");
		}
		seq_printf(m, "\n");
	/* }  FBC not enabled in EMGD at the moment. */
	return 0;
}

static int i915_sr_status(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool sr_enabled = false;

	sr_enabled = I915_READ(WM1_LP_ILK) & WM1_LP_SR_EN;

	seq_printf(m, "self-refresh: %s\n",
		   sr_enabled ? "enabled" : "disabled");

	return 0;
}

static int i915_emon_status(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long temp, chipset, gfx;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	temp = i915_mch_val(dev_priv);
	chipset = i915_chipset_val(dev_priv);
	gfx = i915_gfx_val(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	seq_printf(m, "GMCH temp: %ld\n", temp);
	seq_printf(m, "Chipset power: %ld\n", chipset);
	seq_printf(m, "GFX power: %ld\n", gfx);
	seq_printf(m, "Total power: %ld\n", chipset + gfx);

	return 0;
}

static int i915_ring_freq_table(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;
	int gpu_freq, ia_freq;

	if (!(IS_GEN6(dev) || IS_GEN7(dev))) {
		seq_printf(m, "unsupported on this chipset\n");
		return 0;
	}

	ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
	if (ret)
		return ret;

	seq_printf(m, "GPU freq (MHz)\tEffective CPU freq (MHz)\tEffective Ring freq (MHz)\n");

	for (gpu_freq = dev_priv->ips.min_delay; gpu_freq <= dev_priv->ips.max_delay;
	     gpu_freq++) {
		ia_freq = gpu_freq;
		sandybridge_pcode_read(dev_priv,
				GEN6_PCODE_READ_MIN_FREQ_TABLE,
				&ia_freq);
		seq_printf(m, "%d\t\t%d\t\t\t\t%d\n",
				gpu_freq * GT_FREQUENCY_MULTIPLIER,
				((ia_freq >> 0) & 0xff) * 100,
				((ia_freq >> 8) & 0xff) * 100);
	}

	mutex_unlock(&dev_priv->rps.hw_lock);

	return 0;
}

static int i915_gfxec(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	seq_printf(m, "GFXEC: %ld\n", (unsigned long)I915_READ(0x112f4));

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_opregion(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_opregion *opregion = &dev_priv->opregion;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	if (opregion->header)
		seq_write(m, opregion->header, OPREGION_SIZE);

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_gem_framebuffer_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	emgd_fbdev_t *ifbdev;
	emgd_framebuffer_t *fb;
	int ret;

	ret = mutex_lock_interruptible(&dev->mode_config.mutex);
	if (ret)
		return ret;

	ifbdev = dev_priv->emgd_fbdev;
	fb = container_of(ifbdev->helper.fb, emgd_framebuffer_t, base);

	if (fb) {
		seq_printf(m, "fbcon size: %d x %d, depth %d, %d bpp, obj ",
				fb->base.width,
				fb->base.height,
				fb->base.depth,
				fb->base.bits_per_pixel);
		if (fb->bo) {
			describe_obj(m, fb->bo);
		}
		seq_printf(m, "\n");
	}
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
	mutex_lock(&dev->mode_config.fb_lock);
#endif
	list_for_each_entry(fb, &dev->mode_config.fb_list, base.head) {
		if (&fb->base == ifbdev->helper.fb)
			continue;

		seq_printf(m, "user size: %d x %d, depth %d, %d bpp, obj ",
			   fb->base.width,
			   fb->base.height,
			   fb->base.depth,
			   fb->base.bits_per_pixel);
		if (fb->bo) {
			describe_obj(m, fb->bo);
		}
		seq_printf(m, "\n");
	}
#if (LTSI_VERSION >= KERNEL_VERSION(3, 9, 0))
	mutex_unlock(&dev->mode_config.fb_lock);
#endif
	return 0;
}

static int i915_context_status(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	struct i915_hw_context *ctx;
	int ret, i;

	ret = mutex_lock_interruptible(&dev->mode_config.mutex);
	if (ret)
		return ret;

	if (dev_priv->ips.pwrctx) {
		seq_printf(m, "power context ");
		describe_obj(m, dev_priv->ips.pwrctx);
		seq_printf(m, "\n");
	}

	if (dev_priv->ips.renderctx) {
		seq_printf(m, "render context ");
		describe_obj(m, dev_priv->ips.renderctx);
		seq_printf(m, "\n");
	}

	list_for_each_entry(ctx, &dev_priv->context_list, link) {
		seq_printf(m, "HW context ");
		describe_ctx(m, ctx);
		for_each_ring(ring, dev_priv, i)
			if (ring->default_context == ctx)
				seq_printf(m,
					"(default context %s) ", ring->name);

		describe_obj(m, ctx->obj);
		seq_printf(m, "\n");
	}

	mutex_unlock(&dev->mode_config.mutex);

	return 0;
}

static int i915_gen6_forcewake_count_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned forcewake_count;

	spin_lock_irq(&dev_priv->uncore.lock);
	forcewake_count = dev_priv->uncore.forcewake_count;
	spin_unlock_irq(&dev_priv->uncore.lock);

	seq_printf(m, "forcewake count = %u\n", forcewake_count);

	return 0;
}

static int
i915_wedged_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	drm_i915_private_t *dev_priv = dev->dev_private;

	*val = atomic_read(&dev_priv->gpu_error.reset_counter);

	return 0;
}

static int
i915_wedged_set(void *data, u64 val)
{
	struct drm_device *dev = data;

	DRM_INFO("Manually setting wedged to %llu\n", val);
	i915_handle_error(dev, val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_wedged_fops,
		i915_wedged_get, i915_wedged_set,
		"wedged :  %llu\n");

static int
i915_ring_stop_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	drm_i915_private_t *dev_priv = dev->dev_private;

	*val = dev_priv->gpu_error.stop_rings;

	return 0;
}

static int
i915_ring_stop_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	DRM_DEBUG_DRIVER("Stopping rings 0x%08llx\n", val);

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	dev_priv->gpu_error.stop_rings = val;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_ring_stop_fops,
		i915_ring_stop_get, i915_ring_stop_set,
		"0x%08llx\n");

static int
i915_max_freq_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;

	ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
	if (ret)
		return ret;

	if (IS_VALLEYVIEW(dev))
		*val = vlv_gpu_freq(dev_priv->mem_freq, dev_priv->rps.max_delay);
	else
		*val = dev_priv->rps.max_delay * GT_FREQUENCY_MULTIPLIER;

	mutex_unlock(&dev_priv->rps.hw_lock);

	return 0;
}

static int
i915_max_freq_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;

	DRM_DEBUG_DRIVER("Manually setting max freq to %llu\n", val);

	ret = mutex_lock_interruptible(&dev_priv->rps.hw_lock);
	if (ret)
		return ret;

	/*
	 * Turbo will still be enabled, but won't go above the set value.
	 */
	if (IS_VALLEYVIEW(dev)) {
		val = vlv_freq_opcode(dev_priv->mem_freq, val);
		
	if (val < dev_priv->rps.hw_max)
		dev_priv->rps.max_delay = val;
	else
		dev_priv->rps.max_delay = dev_priv->rps.hw_max;

		dev_priv->rps.max_lock = 1;
		valleyview_set_rps(dev, val);
	} else {
		do_div(val, GT_FREQUENCY_MULTIPLIER);
		dev_priv->rps.max_delay = val;
		gen6_set_rps(dev, val);
	}
	mutex_unlock(&dev_priv->rps.hw_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_max_freq_fops,
		i915_max_freq_get, i915_max_freq_set,
		"max freq: %llu\n");

static int
i915_cache_sharing_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 snpcr;
	int ret;

	if (!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	snpcr = I915_READ(GEN6_MBCUNIT_SNPCR);
	mutex_unlock(&dev_priv->dev->struct_mutex);

	*val = (snpcr & GEN6_MBC_SNPCR_MASK) >> GEN6_MBC_SNPCR_SHIFT;

	return 0;
}

static int
i915_cache_sharing_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 snpcr;

	if (!(IS_GEN6(dev) || IS_GEN7(dev)))
		return -ENODEV;

	if (val > 3)
		return -EINVAL;

	DRM_DEBUG_DRIVER("Manually setting uncore sharing to %llu\n", val);

	/* Update the cache sharing policy here as well */
	snpcr = I915_READ(GEN6_MBCUNIT_SNPCR);
	snpcr &= ~GEN6_MBC_SNPCR_MASK;
	snpcr |= (val << GEN6_MBC_SNPCR_SHIFT);
	I915_WRITE(GEN6_MBCUNIT_SNPCR, snpcr);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_cache_sharing_fops,
		i915_cache_sharing_get, i915_cache_sharing_set,
		"%llu\n");

static ssize_t
i915_sched_read(struct file *filp,
		char __user *ubuf,
		size_t max,
		loff_t *ppos)
{
	struct drm_device *dev = filp->private_data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_file_private *temp = NULL;
	size_t len = 0;
	char *buf, *buf2;
	struct requests_struct{
		pid_t pid;
		char comm[TASK_COMM_LEN];
		int outstanding;
		int priority;
		int period;
		int capacity;
		int avgexectime;
	};
	struct requests_struct * requests = NULL;
	int entries, entry_size, i = 0, j = 0, ret = 0;
	unsigned long sched_avgexectime;
	struct task_struct *task;

	if (!gem_scheduler)
		return 0;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
		if(temp != NULL){
			i++;
		}
	}

	DRM_DEBUG_DRIVER("Num clients = %d\n", i);

	if (!i) {
		mutex_unlock(&dev->struct_mutex);
		entry_size = strlen("No clients found!\n"
				"General Settings -->\n"
				"\tShared_Period   = XXXXms\n"
				"\tShared Capacity = XXXXms\n") + 1;
		buf = drm_malloc_ab(1, entry_size);
		if (!buf) {
			DRM_DEBUG_DRIVER("No memory for buf - 1\n");
			return 0;
		}
		memset(buf, 0, entry_size);
		sprintf(buf, "No clients found!\n"
				"General Settings -->\n"
				"\tShared_Period   = %06dms\n"
				"\tShared Capacity = %06dms\n",
			dev_priv->scheduler.shared_period/1000, dev_priv->scheduler.shared_capacity/1000);
		len = strlen(buf);
		ret = simple_read_from_buffer(ubuf, max, ppos, buf, len);
		drm_free_large(buf);
		return ret;
	}

	requests = drm_malloc_ab(i, sizeof(struct requests_struct));
	if(!requests) {
		mutex_unlock(&dev->struct_mutex);
		DRM_DEBUG_DRIVER("No memory for requests - 1\n");
		return 0;
	};

	i = 0;
	list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
		struct drm_file *file = temp->drm_file;
		requests[i].pid = PID_T_FROM_DRM_FILE(file);
		task = pid_task (find_vpid(requests[i].pid), PIDTYPE_PID); 
		get_task_comm(requests[i].comm, task);
		spin_lock(&temp->mm.lock);
		requests[i].outstanding = temp->mm.outstanding_requests;
		requests[i].priority    = temp->mm.sched_priority;
		if (SHARED_NORMAL_PRIORITY == requests[i].priority) {
			requests[i].period      = dev_priv->scheduler.shared_period;
			requests[i].capacity    = dev_priv->scheduler.shared_capacity;
		} 
		else if (SHARED_ROGUE_PRIORITY == requests[i].priority) {
			requests[i].period      = dev_priv->scheduler.rogue_period;
			requests[i].capacity    = dev_priv->scheduler.rogue_capacity;
		}
		else {
			requests[i].period      = temp->mm.sched_period;
			requests[i].capacity    = temp->mm.sched_capacity;
		}
		sched_avgexectime = 0;
		for (j=0; j<MOVING_AVG_RANGE ;++j) {
			sched_avgexectime += temp->mm.exec_times[j];
		}
		if (sched_avgexectime) 
			sched_avgexectime = (sched_avgexectime / MOVING_AVG_RANGE);
		requests[i].avgexectime = sched_avgexectime; 
		spin_unlock(&temp->mm.lock);
		i++;
	}
	mutex_unlock(&dev->struct_mutex);

	entries = i;
	entry_size = strlen("PID 123456 ---> XXXXXXXXXXXXXXXX\n"
			 "\toutstanding = XXXXXX\n"
			 "\tpriority    = XXXXXX\n"
			 "\tperiod      = XXXXXXms\n"
			 "\tcapacity    = XXXXXXms\n"
			 "\tavgexectime = XXXXXXus\n") + 1;
	buf2 = drm_malloc_ab(entries, entry_size);
	memset(buf2, 0, entries * entry_size);
	for(i = 0; i < entries; i++) {
		len += sprintf(buf2 + len,
			"PID %06d ---> %.16s\n"
			"\toutstanding = %06d\n"
			"\tpriority    = %06d\n"
			"\tperiod      = %06dms\n"
			"\tcapacity    = %06dms\n"
			"\tavgexectime = %06dus\n",
			requests[i].pid,
			requests[i].comm,
			requests[i].outstanding,
			requests[i].priority,
			(requests[i].period/1000),
			(requests[i].capacity/1000),
			requests[i].avgexectime);
	}

	buf = kasprintf(GFP_KERNEL, "%s\n"
			"General Settings -->\n"
			"\tShared Period   = %06dms\n"
			"\tShared Capacity = %06dms\n"
			"\tRogue Period    = %06dms\n"
			"\tRogue Capacity  = %06dms\n",
			buf2, 
			dev_priv->scheduler.shared_period/1000, dev_priv->scheduler.shared_capacity/1000,
			(dev_priv->scheduler.rogue_switch)? dev_priv->scheduler.rogue_period/1000 : 0,
			(dev_priv->scheduler.rogue_switch)? dev_priv->scheduler.rogue_capacity/1000 : 0);

	len = strlen(buf);
	ret = simple_read_from_buffer(ubuf, max, ppos, buf, len);

	drm_free_large(buf2);
	kfree(buf);
	return ret;
}

static inline void
test_and_do_idle(struct drm_device *dev, bool *change)
{
	if (*change == false && i915_gpu_idle(dev))
		DRM_ERROR("Couldn't idle GPU before sched changes\n");

	*change = true;
}

#define CHANGE_PRIORITY  0x01
#define CHANGE_PERIOD    0x02
#define CHANGE_CAPACITY  0x04

static void
set_client_sched_parm(struct drm_device *dev, pid_t pid, uint32_t change_mask,
	      int new_value, bool *change)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_file_private *temp, *temp2;

	list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
		struct drm_file *file = temp->drm_file;
		if (PID_T_FROM_DRM_FILE(file) == pid) {
			int old = 0;
			if (change_mask & CHANGE_PRIORITY) {
				if (file->is_master) {
					EMGD_DEBUG("Changing master priority prohibited!- override to '0'");
					return;
				}
				if ( (new_value < SHARED_NORMAL_PRIORITY) && (new_value != 0)) {
					/* if its not highest or lowest-shared, then u cant share slots */
					list_for_each_entry(temp2, &dev_priv->i915_client_list, client_link) {
						if (temp != temp2 ) {
							if( temp2->mm.sched_priority == new_value) {
								EMGD_ERROR("Priority request is not sharable - bailing!");
								return;
							}
						}
					}
				}
					
				old = temp->mm.sched_priority;
				if (old != new_value && new_value <= MAX_SCHED_PRIORITIES) {
					test_and_do_idle(dev, change);
					dev_priv->scheduler.req_prty_cnts[new_value] += dev_priv->scheduler.req_prty_cnts[old];
					dev_priv->scheduler.req_prty_cnts[old] = 0;
					temp->mm.sched_priority = new_value;
					EMGD_DEBUG("New priority for pid "
						 "%d = %d\n", pid, new_value);
				}
			}
			if (change_mask & CHANGE_PERIOD) {
				old = temp->mm.sched_period;
				if (old != (new_value*1000)) {
					test_and_do_idle(dev, change);
					temp->mm.sched_period = new_value*1000;
					EMGD_DEBUG("New period for pid "
						 "%d = %d\n", pid, new_value);
				}
			}
			if (change_mask & CHANGE_CAPACITY) {
				old = temp->mm.sched_capacity;
				if (old != (new_value*1000)) {
					test_and_do_idle(dev, change);
					temp->mm.sched_capacity = new_value*1000;
					EMGD_DEBUG("New capacity for pid "
						 "%d = %d\n", pid, new_value);
				}
			}
			break;
		}
	}
}

static ssize_t
i915_sched_write(struct file *filp,
		 const char __user *ubuf,
		 size_t cnt,
		 loff_t *ppos)
{
	struct drm_device *dev = filp->private_data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	char *buf, *ptr;
	uint32_t temp;
	bool change = false;
	struct drm_i915_file_private *rtemp;

	if (!gem_scheduler)
		return -ENODEV;

	buf = drm_malloc_ab(cnt + 1, 1);
	if (!buf)
		return -E2BIG;

	if (copy_from_user(buf, ubuf, cnt))
		goto out;

	if (mutex_lock_interruptible(&dev->struct_mutex))
		goto out;

	buf[cnt] = 0;
	ptr = buf;

	/* 
	 * sharedt=[new shared period time msecs]
	 * sharedc=[new shared capacity time msecs;
	 * pidp=[pid]-[new priority]
	 * pidt=[pid]-[new period time msecs]
	 * pidc=[pid]-[new capacity time msecs]
	 */
	while ((ptr != NULL) && *ptr != 0) {
		if (CHECK_NAME(ptr, "sharedt=")) {
			temp = simple_strtoul(ptr + 8, NULL, 0);
			if (dev_priv->scheduler.shared_period != (temp*1000)) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.shared_period = (temp*1000);
				EMGD_DEBUG("New shared period = %d\n",
					 temp);
			}
		} else if (CHECK_NAME(ptr, "sharedc=")) {
			temp = simple_strtoul(ptr + 8, NULL, 0);
			if (dev_priv->scheduler.shared_capacity != (temp*1000)) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.shared_capacity = (temp*1000);
				EMGD_DEBUG("New shared capacity = %d\n",
					 temp);
			}
		} else if (CHECK_NAME(ptr, "roguet=")) {
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (dev_priv->scheduler.rogue_period != (temp*1000)) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.rogue_period = (temp*1000);
				EMGD_DEBUG("New rogue period = %d\n",
					 temp);
			}
		} else if (CHECK_NAME(ptr, "roguec=")) {
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (dev_priv->scheduler.rogue_capacity != (temp*1000)) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.rogue_capacity = (temp*1000);
				EMGD_DEBUG("New rogue capacity = %d\n",
					 temp);
			}
		} else if (CHECK_NAME(ptr, "roguel=")) {
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (dev_priv->scheduler.rogue_trigger != (temp*1000)) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.rogue_trigger = (temp*1000);
				EMGD_DEBUG("New rogue trigger = %d\n",
					temp);
			}
		} else if (CHECK_NAME(ptr, "rogueb=")) {
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (temp > 0){
				dev_priv->scheduler.rogue_switch = true; 
			}
			else { 
				dev_priv->scheduler.rogue_switch = false;
				list_for_each_entry(rtemp, &dev_priv->i915_client_list, client_link){
					struct drm_file *file = rtemp->drm_file;
					if (rtemp->mm.sched_priority == SHARED_ROGUE_PRIORITY){
						set_client_sched_parm(dev, PID_T_FROM_DRM_FILE(file), CHANGE_PRIORITY, SHARED_NORMAL_PRIORITY, &change);
					}
				}
			}
		} else if (CHECK_NAME(ptr, "videob=")) {
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (temp > 0){
				dev_priv->scheduler.video_switch = true;
			}
			else {
				dev_priv->scheduler.video_switch = false;
			}
		}  else if (CHECK_NAME(ptr, "videop=")) { 
			temp = simple_strtoul(ptr + 7, NULL, 0);
			if (dev_priv->scheduler.video_priority != temp) {
				test_and_do_idle(dev, &change);
				dev_priv->scheduler.video_priority = temp;
				EMGD_DEBUG("New default video priority = %d\n",
					temp);
			}
		} else if (CHECK_NAME(ptr, "pidp=")) {
			pid_t pid = simple_strtoul(ptr + 5, NULL, 10);
			ptr = strchr(ptr, '-');
			if (ptr != NULL && ptr[1] != 0) {
				temp = simple_strtoul(ptr+1, NULL, 10);
				set_client_sched_parm(dev, pid, CHANGE_PRIORITY, temp, &change);
			} else if (ptr == NULL)
				break;
		} else if (CHECK_NAME(ptr, "pidt=")) {
			pid_t pid = simple_strtoul(ptr + 5, NULL, 10);
			ptr = strchr(ptr, '-');
			if (ptr != NULL && ptr[1] != 0) {
				temp = simple_strtoul(ptr+1, NULL, 10);
				set_client_sched_parm(dev, pid, CHANGE_PERIOD, temp, &change);
			} else if (ptr == NULL)
				break;
		} else if (CHECK_NAME(ptr, "pidc=")) {
			pid_t pid = simple_strtoul(ptr + 5, NULL, 10);
			ptr = strchr(ptr, '-');
			if (ptr != NULL && ptr[1] != 0) {
				temp = simple_strtoul(ptr+1, NULL, 10);
				set_client_sched_parm(dev, pid, CHANGE_CAPACITY, temp, &change);
			} else if (ptr == NULL)
				break;
		}
		 else {
			/* Assume the whole string is busted, and bail */
			DRM_ERROR("Unable to parse command %s\n", ptr);
			break;
		}
		ptr = strchr(ptr, ',');
		if (ptr != NULL)
			ptr += strspn(ptr, ", \t\n");
	}

	mutex_unlock(&dev->struct_mutex);
out:
	drm_free_large(buf);
	return cnt;
}

#undef CHANGE_PRIORITY
#undef CHANGE_PERIOD
#undef CHANGE_CAPACITY

static const struct file_operations i915_scheduler_fops = {
	.owner = THIS_MODULE,
	.open = i915_debugfs_common_open,
	.read = i915_sched_read,
	.write = i915_sched_write,
};

/* As the drm_debugfs_init() routines are called before dev->dev_private is
 * allocated we need to hook into the minor for release. */
static int
drm_add_fake_info_node(struct drm_minor *minor,
		       struct dentry *ent,
		       const void *key)
{
	struct drm_info_node *node;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL) {
		debugfs_remove(ent);
		return -ENOMEM;
	}

	node->minor = minor;
	node->dent = ent;
	node->info_ent = (void *) key;

	mutex_lock(&minor->debugfs_lock);
	list_add(&node->list, &minor->debugfs_list);
	mutex_unlock(&minor->debugfs_lock);

	return 0;
}

static int i915_forcewake_open(struct inode *inode, struct file *file)
{
	struct drm_device *dev = inode->i_private;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;
	gen6_gt_force_wake_get(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int i915_forcewake_release(struct inode *inode, struct file *file)
{
	struct drm_device *dev = inode->i_private;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	/*
	 * It's bad that we can potentially hang userspace if struct_mutex gets
	 * forever stuck.  However, if we cannot acquire this lock it means that
	 * almost certainly the driver has hung, is not unload-able. Therefore
	 * hanging here is probably a minor inconvenience not to be seen my
	 * almost every user.
	 */
	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	gen6_gt_force_wake_put(dev_priv);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static const struct file_operations i915_forcewake_fops = {
	.owner = THIS_MODULE,
	.open = i915_forcewake_open,
	.release = i915_forcewake_release,
};

static int
i915_ring_missed_irq_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;

	*val = dev_priv->gpu_error.missed_irq_rings;
	return 0;
}

static int
i915_ring_missed_irq_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	/* Lock against concurrent debugfs callers */
	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;
	dev_priv->gpu_error.missed_irq_rings = val;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_ring_missed_irq_fops,
			i915_ring_missed_irq_get, i915_ring_missed_irq_set,
			"0x%08llx\n");

static int
i915_ring_test_irq_get(void *data, u64 *val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;

	*val = dev_priv->gpu_error.test_irq_rings;

	return 0;
}

static int
i915_ring_test_irq_set(void *data, u64 val)
{
	struct drm_device *dev = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	DRM_DEBUG_DRIVER("Masking interrupts on rings 0x%08llx\n", val);

	/* Lock against concurrent debugfs callers */
	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	dev_priv->gpu_error.test_irq_rings = val;
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i915_ring_test_irq_fops,
			i915_ring_test_irq_get, i915_ring_test_irq_set,
			"0x%08llx\n");

#define DROP_UNBOUND 0x1
#define DROP_BOUND 0x2
#define DROP_RETIRE 0x4
#define DROP_ACTIVE 0x8
#define DROP_ALL (DROP_UNBOUND | \
		DROP_BOUND | \
		DROP_RETIRE | \
		DROP_ACTIVE)

static ssize_t
i915_drop_caches_read(struct file *filp,
		     char __user *ubuf,
		     size_t max,
		     loff_t *ppos)
{
	char buf[20];
	int len;

	len = snprintf(buf, sizeof(buf), "0x%08x\n", DROP_ALL);
	if (len > sizeof(buf))
		len = sizeof(buf);

	return simple_read_from_buffer(ubuf, max, ppos, buf, len);
}

static ssize_t
i915_drop_caches_write(struct file *filp,
		const char __user *ubuf,
		size_t cnt,
		loff_t *ppos)
{
	struct drm_device *dev = filp->private_data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_address_space *vm;
	struct i915_vma *vma, *x;
	char buf[20];
	int val = 0, ret;

	if (cnt > 0) {
		if (cnt > sizeof(buf) - 1)
			return -EINVAL;

		if (copy_from_user(buf, ubuf, cnt))
			return -EFAULT;
		buf[cnt] = 0;

		val = simple_strtoul(buf, NULL, 0);
	}

	DRM_DEBUG_DRIVER("Dropping caches: 0x%08x\n", val);

	/* No need to check and wait for gpu resets, only libdrm auto-restarts
	 * on ioctls on -EAGAIN. */
	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	if (val & DROP_ACTIVE) {
		ret = i915_gpu_idle(dev);
		if (ret)
			goto unlock;
	}

	if (val & (DROP_RETIRE | DROP_ACTIVE))
		i915_gem_retire_requests(dev);

	if (val & DROP_BOUND) {
		list_for_each_entry(vm, &dev_priv->vm_list, global_link) {
			list_for_each_entry_safe(vma, x, &vm->inactive_list,
					mm_list) {
				if (vma->obj->pin_count)
					continue;

				ret = i915_vma_unbind(vma);
				if (ret)
					goto unlock;
			}
		}
	}

unlock:
	mutex_unlock(&dev->struct_mutex);

	return ret ?: cnt;
}

static const struct file_operations i915_drop_caches_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = i915_drop_caches_read,
	.write = i915_drop_caches_write,
	.llseek = default_llseek,
};

static int i915_debugfs_create(struct dentry *root,
                              struct drm_minor *minor,
                              const char *name,
                              const struct file_operations *fops)
{
	struct drm_device *dev = minor->dev;
	struct dentry *ent;

	ent = debugfs_create_file(name,
				  S_IRUGO | S_IWUSR,
				  root, dev,
				  fops);
	if (IS_ERR(ent))
		return PTR_ERR(ent);

	return drm_add_fake_info_node(minor, ent, fops);
}

static struct drm_info_list i915_debugfs_list[] = {
	{"i915_capabilities", i915_capabilities, 0},
	{"i915_gem_objects", i915_gem_object_info, 0},
	{"i915_gem_gtt", i915_gem_gtt_info, 0},
	{"i915_gem_pinned", i915_gem_gtt_info, 0, (void *) PINNED_LIST},
	{"i915_gem_active", i915_gem_object_list_info, 0, (void *) ACTIVE_LIST},
	{"i915_gem_inactive", i915_gem_object_list_info, 0, (void *) INACTIVE_LIST},
	{"i915_gem_pageflip", i915_gem_pageflip_info, 0},
	{"i915_gem_request", i915_gem_request_info, 0},
	{"i915_gem_seqno", i915_gem_seqno_info, 0},
	{"i915_gem_fence_regs", i915_gem_fence_regs_info, 0},
	{"i915_gem_interrupt", i915_interrupt_info, 0},
	{"i915_gem_hws", i915_hws_info, 0, (void *)RCS},
	{"i915_gem_hws_blt", i915_hws_info, 0, (void *)BCS},
	{"i915_gem_hws_bsd", i915_hws_info, 0, (void *)VCS},
	{"i915_ringbuffer_data", i915_ringbuffer_data, 0, (void *)RCS},
	{"i915_ringbuffer_info", i915_ringbuffer_info, 0, (void *)RCS},
	{"i915_bsd_ringbuffer_data", i915_ringbuffer_data, 0, (void *)VCS},
	{"i915_bsd_ringbuffer_info", i915_ringbuffer_info, 0, (void *)VCS},
	{"i915_blt_ringbuffer_data", i915_ringbuffer_data, 0, (void *)BCS},
	{"i915_blt_ringbuffer_info", i915_ringbuffer_info, 0, (void *)BCS},
	{"i915_batchbuffers", i915_batchbuffer_info, 0},
	{"i915_rstdby_delays", i915_rstdby_delays, 0},
	{"i915_cur_delayinfo", i915_cur_delayinfo, 0},
	{"i915_delayfreq_table", i915_delayfreq_table, 0},
	{"i915_inttoext_table", i915_inttoext_table, 0},
	{"i915_drpc_info", i915_drpc_info, 0},
	{"i915_emon_status", i915_emon_status, 0},
	{"i915_ring_freq_table", i915_ring_freq_table, 0},
	{"i915_gfxec", i915_gfxec, 0},
	{"i915_fbc_status", i915_fbc_status, 0},
	{"i915_sr_status", i915_sr_status, 0},
	{"i915_opregion", i915_opregion, 0},
	{"i915_gem_framebuffer", i915_gem_framebuffer_info, 0},
	{"i915_context_status", i915_context_status, 0},
	{"i915_gen6_forcewake_count", i915_gen6_forcewake_count_info, 0},
};
#define I915_DEBUGFS_ENTRIES ARRAY_SIZE(i915_debugfs_list)

struct i915_debugfs_files {
	const char *name;
	const struct file_operations *fops;
} i915_debugfs_files[] = {
	{"i915_wedged", &i915_wedged_fops},
	{"i915_max_freq", &i915_max_freq_fops},
	{"i915_forcewake_user", &i915_forcewake_fops},
	{"i915_scheduler", &i915_scheduler_fops},
	{"i915_cache_sharing", &i915_cache_sharing_fops},
	{"i915_ring_stop", &i915_ring_stop_fops},
	{"i915_ring_missed_irq", &i915_ring_missed_irq_fops},
	{"i915_ring_test_irq", &i915_ring_test_irq_fops},
	{"i915_gem_drop_caches", &i915_drop_caches_fops},
	{"i915_error_state", &i915_error_state_fops},
	{"i915_next_seqno", &i915_next_seqno_fops},
};

int i915_debugfs_init(struct drm_minor *minor)
{
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(i915_debugfs_files); i++) {
		ret = i915_debugfs_create(minor->debugfs_root, minor,
					  i915_debugfs_files[i].name,
					  i915_debugfs_files[i].fops);
		if (ret)
			return ret;
	}

	return drm_debugfs_create_files(i915_debugfs_list,
					I915_DEBUGFS_ENTRIES,
					minor->debugfs_root, minor);
}

void i915_debugfs_cleanup(struct drm_minor *minor)
{
	int i;

	drm_debugfs_remove_files(i915_debugfs_list,
				 I915_DEBUGFS_ENTRIES, minor);
	for (i = 0; i < ARRAY_SIZE(i915_debugfs_files); i++) {
		struct drm_info_list *info_list =
			(struct drm_info_list *) i915_debugfs_files[i].fops;

		drm_debugfs_remove_files(info_list, 1, minor);
	}
}

#endif /* CONFIG_DEBUG_FS */
