/*
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 * Copyright (c) 2007-2008,2012,2013 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
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

#if !defined(_I915_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _I915_TRACE_H_


#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#include <drm/drmP.h>
#include "drm_emgd_private.h"
#include "intel_ringbuffer.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM i915
#define TRACE_SYSTEM_STRING __stringify(TRACE_SYSTEM)
#define TRACE_INCLUDE_FILE i915_trace

/* object tracking */

TRACE_EVENT(i915_atomic_evade,
	    TP_PROTO(u32 dsl_min, u32 dsl_max, u32 dsl),
	    TP_ARGS(dsl_min, dsl_max, dsl),

	    TP_STRUCT__entry(
			     __field(u32, dsl_min)
			     __field(u32, dsl_max)
			     __field(u32, dsl)
			     ),
	    TP_fast_assign(
			   __entry->dsl_min = dsl_min;
			   __entry->dsl_max = dsl_max;
			   __entry->dsl = dsl;
			   ),

	    TP_printk("dsl_min=%u dsl_max=%u dsl=%u", __entry->dsl_min, __entry->dsl_max, __entry->dsl)
);

TRACE_EVENT(i915_atomic_flush,
	    TP_PROTO(u32 dsl0, u32 dsl1, u32 dsl2),
	    TP_ARGS(dsl0, dsl1, dsl2),

	    TP_STRUCT__entry(
			     __field(u32, dsl0)
			     __field(u32, dsl1)
			     __field(u32, dsl2)
			     ),
	    TP_fast_assign(
			   __entry->dsl0 = dsl0;
			   __entry->dsl1 = dsl1;
			   __entry->dsl2 = dsl2;
			   ),

	    TP_printk("dsl(0)=%u dsl(1)=%u dsl(2)=%u", __entry->dsl0, __entry->dsl1, __entry->dsl2)
);


TRACE_EVENT(i915_atomic_flip,
	    TP_PROTO(bool sprite, int pipe, int action, u32 commit_surf, u32 commit_surflive, u32 surf, u32 surflive, u32 iir, u32 commit_dsl, u32 dsl, u32 flip_vbl_count, u32 vbl_count),
	    TP_ARGS(sprite, pipe, action, commit_surf, commit_surflive, surf, surflive, iir, commit_dsl, dsl, flip_vbl_count, vbl_count),

	    TP_STRUCT__entry(
			     __field(bool, sprite)
			     __field(int, pipe)
			     __field(int, action)
			     __field(u32, commit_surf)
			     __field(u32, commit_surflive)
			     __field(u32, surf)
			     __field(u32, surflive)
			     __field(u32, iir)
			     __field(u32, commit_dsl)
			     __field(u32, dsl)
			     __field(u32, flip_vbl_count)
			     __field(u32, vbl_count)
			     ),

	    TP_fast_assign(
			   __entry->sprite = sprite;
			   __entry->pipe = pipe;
			   __entry->action = action;
			   __entry->commit_surf = commit_surf;
			   __entry->commit_surflive = commit_surflive;
			   __entry->surf = surf;
			   __entry->surflive = surflive;
			   __entry->iir = iir;
			   __entry->commit_dsl = commit_dsl;
			   __entry->dsl = dsl;
			   __entry->flip_vbl_count = flip_vbl_count;
			   __entry->vbl_count = vbl_count;
			   ),

	    TP_printk(
		      "%s/%d %s commit_surf=%x commit_surflive=%x surf=%x surflive=%x iir=%x commit_dsl=%u dsl=%u flip_vbl_count=%u vbl_count=%u",
		      __entry->sprite ? "SPR" : "DSP", __entry->pipe,
		      __entry->action == 0 ? "new" :
		      __entry->action == 1 ? "flipped" :
		      __entry->action == 2 ? "not flipped" :
		      __entry->action == 3 ? "missed flipped" : "?",
		      __entry->commit_surf, __entry->commit_surflive,
		      __entry->surf, __entry->surflive,
		      __entry->iir,
		      __entry->commit_dsl, __entry->dsl,
		      __entry->flip_vbl_count, __entry->vbl_count
		      )
);

TRACE_EVENT(i915_flip_queue_len,
	    TP_PROTO(unsigned int queue_len),
	    TP_ARGS(queue_len),

	    TP_STRUCT__entry(
			     __field(u32, queue_len)
			     ),

	    TP_fast_assign(
			   __entry->queue_len = queue_len;
			   ),

	    TP_printk("queue_len=%u", __entry->queue_len)
);

TRACE_EVENT(i915_gem_object_pin_count,
	    TP_PROTO(struct drm_i915_gem_object *obj, u32 pin_count_pre, u32 pin_count_post),
	    TP_ARGS(obj, pin_count_pre, pin_count_post),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, pin_count_pre)
			     __field(u32, pin_count_post)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->pin_count_pre = pin_count_pre;
			   __entry->pin_count_post = pin_count_post;
			   ),

	    TP_printk("obj=%p, pin_count=%u->%u", __entry->obj, __entry->pin_count_pre, __entry->pin_count_post)
);
TRACE_EVENT(i915_gem_object_create,
	    TP_PROTO(struct drm_i915_gem_object *obj),
	    TP_ARGS(obj),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, size)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->size = obj->base.size;
			   ),

	    TP_printk("obj=%p, size=%u", __entry->obj, __entry->size)
);

TRACE_EVENT(i915_vma_bind,
	    TP_PROTO(struct i915_vma *vma, bool mappable),
	    TP_ARGS(vma, mappable),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(struct i915_address_space *, vm)
			     __field(u32, offset)
			     __field(u32, size)
			     __field(bool, mappable)
			     ),

	    TP_fast_assign(
			   __entry->obj = vma->obj;
			   __entry->vm = vma->vm;
			   __entry->offset = vma->node.start;
			   __entry->size = vma->node.size;
			   __entry->mappable = mappable;
			   ),

	    TP_printk("obj=%p, offset=%08x size=%x%s vm=%p",
		      __entry->obj, __entry->offset, __entry->size,
		      __entry->mappable ? ", mappable" : "",
		      __entry->vm)
);

TRACE_EVENT(i915_vma_unbind,
	    TP_PROTO(struct i915_vma *vma),
	    TP_ARGS(vma),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(struct i915_address_space *, vm)
			     __field(u32, offset)
			     __field(u32, size)
			     ),

	    TP_fast_assign(
			   __entry->obj = vma->obj;
			   __entry->vm = vma->vm;
			   __entry->offset = vma->node.start;
			   __entry->size = vma->node.size;
			   ),

	    TP_printk("obj=%p, offset=%08x size=%x vm=%p",
		      __entry->obj, __entry->offset, __entry->size, __entry->vm)
);

TRACE_EVENT(i915_gem_object_change_domain,
	    TP_PROTO(struct drm_i915_gem_object *obj, u32 old_read, u32 old_write),
	    TP_ARGS(obj, old_read, old_write),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, read_domains)
			     __field(u32, write_domain)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->read_domains = obj->base.read_domains | (old_read << 16);
			   __entry->write_domain = obj->base.write_domain | (old_write << 16);
			   ),

	    TP_printk("obj=%p, read=%02x=>%02x, write=%02x=>%02x",
		      __entry->obj,
		      __entry->read_domains >> 16,
		      __entry->read_domains & 0xffff,
		      __entry->write_domain >> 16,
		      __entry->write_domain & 0xffff)
);

TRACE_EVENT(i915_gem_object_pwrite,
	    TP_PROTO(struct drm_i915_gem_object *obj, u32 offset, u32 len),
	    TP_ARGS(obj, offset, len),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, offset)
			     __field(u32, len)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->offset = offset;
			   __entry->len = len;
			   ),

	    TP_printk("obj=%p, offset=%u, len=%u",
		      __entry->obj, __entry->offset, __entry->len)
);

TRACE_EVENT(i915_gem_object_pread,
	    TP_PROTO(struct drm_i915_gem_object *obj, u32 offset, u32 len),
	    TP_ARGS(obj, offset, len),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, offset)
			     __field(u32, len)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->offset = offset;
			   __entry->len = len;
			   ),

	    TP_printk("obj=%p, offset=%u, len=%u",
		      __entry->obj, __entry->offset, __entry->len)
);

TRACE_EVENT(i915_gem_object_fault,
	    TP_PROTO(struct drm_i915_gem_object *obj, u32 index, bool gtt, bool write),
	    TP_ARGS(obj, index, gtt, write),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     __field(u32, index)
			     __field(bool, gtt)
			     __field(bool, write)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   __entry->index = index;
			   __entry->gtt = gtt;
			   __entry->write = write;
			   ),

	    TP_printk("obj=%p, %s index=%u %s",
		      __entry->obj,
		      __entry->gtt ? "GTT" : "CPU",
		      __entry->index,
		      __entry->write ? ", writable" : "")
);

DECLARE_EVENT_CLASS(i915_gem_object,
	    TP_PROTO(struct drm_i915_gem_object *obj),
	    TP_ARGS(obj),

	    TP_STRUCT__entry(
			     __field(struct drm_i915_gem_object *, obj)
			     ),

	    TP_fast_assign(
			   __entry->obj = obj;
			   ),

	    TP_printk("obj=%p", __entry->obj)
);

DEFINE_EVENT(i915_gem_object, i915_gem_object_clflush,
	     TP_PROTO(struct drm_i915_gem_object *obj),
	     TP_ARGS(obj)
);

DEFINE_EVENT(i915_gem_object, i915_gem_object_destroy,
	    TP_PROTO(struct drm_i915_gem_object *obj),
	    TP_ARGS(obj)
);

TRACE_EVENT(i915_gem_evict,
	    TP_PROTO(struct drm_device *dev, u32 size, u32 align, bool mappable),
	    TP_ARGS(dev, size, align, mappable),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, size)
			     __field(u32, align)
			     __field(bool, mappable)
			    ),

	    TP_fast_assign(
			   __entry->dev = dev->primary->index;
			   __entry->size = size;
			   __entry->align = align;
			   __entry->mappable = mappable;
			  ),

	    TP_printk("dev=%d, size=%d, align=%d %s",
		      __entry->dev, __entry->size, __entry->align,
		      __entry->mappable ? ", mappable" : "")
);

TRACE_EVENT(i915_gem_evict_everything,
        TP_PROTO(struct drm_device *dev),
           TP_ARGS(dev),
	    TP_STRUCT__entry(
			     __field(u32, dev)
			    ),

	    TP_fast_assign(
			   __entry->dev = dev->primary->index;
			  ),
		TP_printk("dev=%d", __entry->dev)
);

TRACE_EVENT(i915_gem_evict_vm,
		TP_PROTO(struct i915_address_space *vm),
		TP_ARGS(vm),

		TP_STRUCT__entry(
			__field(struct i915_address_space *, vm)
			),

		TP_fast_assign(
			__entry->vm = vm;
			),

		TP_printk("dev=%d, vm=%p",
			__entry->vm->dev->primary->index, __entry->vm)
);

TRACE_EVENT(i915_gem_ring_sync_to,
		TP_PROTO(struct intel_ring_buffer *from,
		struct intel_ring_buffer *to,
		u32 seqno),
		TP_ARGS(from, to, seqno),

		TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, sync_from)
			     __field(u32, sync_to)
			     __field(u32, seqno)
			     ),

		TP_fast_assign(
			   __entry->dev = from->dev->primary->index;
			   __entry->sync_from = from->id;
			   __entry->sync_to = to->id;
			   __entry->seqno = seqno;
			   ),

		TP_printk("dev=%u, sync-from=%u, sync-to=%u, seqno=%u",
		      __entry->dev,
		      __entry->sync_from, __entry->sync_to,
		      __entry->seqno)
);

TRACE_EVENT(i915_gem_ring_dispatch,
		TP_PROTO(struct intel_ring_buffer *ring, u32 seqno, u32 flags),
		TP_ARGS(ring, seqno, flags),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, ring)
			     __field(u32, seqno)
			     __field(u32, flags)
			     ),

	    TP_fast_assign(
			   __entry->dev = ring->dev->primary->index;
			   __entry->ring = ring->id;
			   __entry->seqno = seqno;
			   __entry->flags = flags;
			   i915_trace_irq_get(ring, seqno);
			   ),

		TP_printk("dev=%u, ring=%u, seqno=%u, flags=%x",
			__entry->dev, __entry->ring, __entry->seqno, __entry->flags)

);

TRACE_EVENT(i915_gem_ring_flush,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 invalidate, u32 flush),
	    TP_ARGS(ring, invalidate, flush),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, ring)
			     __field(u32, invalidate)
			     __field(u32, flush)
			     ),

	    TP_fast_assign(
			   __entry->dev = ring->dev->primary->index;
			   __entry->ring = ring->id;
			   __entry->invalidate = invalidate;
			   __entry->flush = flush;
			   ),

	    TP_printk("dev=%u, ring=%x, invalidate=%04x, flush=%04x",
		      __entry->dev, __entry->ring,
		      __entry->invalidate, __entry->flush)
);

DECLARE_EVENT_CLASS(i915_gem_request,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, ring)
			     __field(u32, seqno)
			     ),

	    TP_fast_assign(
			   __entry->dev = ring->dev->primary->index;
			   __entry->ring = ring->id;
			   __entry->seqno = seqno;
			   ),

	    TP_printk("dev=%u, ring=%u, seqno=%u",
		      __entry->dev, __entry->ring, __entry->seqno)
);

DEFINE_EVENT(i915_gem_request, i915_gem_request_add,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno)
);

DEFINE_EVENT(i915_gem_request, i915_gem_request_complete,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno)
);

DEFINE_EVENT(i915_gem_request, i915_gem_request_retire,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno)
);

DEFINE_EVENT(i915_gem_request, i915_gem_request_wait_begin,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno)
);

DEFINE_EVENT(i915_gem_request, i915_gem_request_wait_end,
	    TP_PROTO(struct intel_ring_buffer *ring, u32 seqno),
	    TP_ARGS(ring, seqno)
);

DECLARE_EVENT_CLASS(i915_ring,
	    TP_PROTO(struct intel_ring_buffer *ring),
	    TP_ARGS(ring),

	    TP_STRUCT__entry(
			     __field(u32, dev)
			     __field(u32, ring)
			     ),

	    TP_fast_assign(
			   __entry->dev = ring->dev->primary->index;
			   __entry->ring = ring->id;
			   ),

	    TP_printk("dev=%u, ring=%u", __entry->dev, __entry->ring)
);

DEFINE_EVENT(i915_ring, i915_ring_wait_begin,
	    TP_PROTO(struct intel_ring_buffer *ring),
	    TP_ARGS(ring)
);

DEFINE_EVENT(i915_ring, i915_ring_wait_end,
	    TP_PROTO(struct intel_ring_buffer *ring),
	    TP_ARGS(ring)
);

TRACE_EVENT(i915_flip_request,
	    TP_PROTO(int plane, struct drm_i915_gem_object *obj),

	    TP_ARGS(plane, obj),

	    TP_STRUCT__entry(
		    __field(int, plane)
		    __field(struct drm_i915_gem_object *, obj)
		    ),

	    TP_fast_assign(
		    __entry->plane = plane;
		    __entry->obj = obj;
		    ),

	    TP_printk("plane=%d, obj=%p", __entry->plane, __entry->obj)
);

TRACE_EVENT(i915_flip_complete,
	    TP_PROTO(int plane, struct drm_i915_gem_object *obj),

	    TP_ARGS(plane, obj),

	    TP_STRUCT__entry(
		    __field(int, plane)
		    __field(struct drm_i915_gem_object *, obj)
		    ),

	    TP_fast_assign(
		    __entry->plane = plane;
		    __entry->obj = obj;
		    ),

	    TP_printk("plane=%d, obj=%p", __entry->plane, __entry->obj)
);

TRACE_EVENT(i915_reg_rw,
	TP_PROTO(bool write, u32 reg, u64 val, int len),

	TP_ARGS(write, reg, val, len),

	TP_STRUCT__entry(
		__field(u64, val)
		__field(u32, reg)
		__field(u16, write)
		__field(u16, len)
		),

	TP_fast_assign(
		__entry->val = (u64)val;
		__entry->reg = reg;
		__entry->write = write;
		__entry->len = len;
		),

	TP_printk("%s reg=0x%x, len=%d, val=(0x%x, 0x%x)",
		__entry->write ? "write" : "read",
		__entry->reg, __entry->len,
		(u32)(__entry->val & 0xffffffff),
		(u32)(__entry->val >> 32))
);

TRACE_EVENT(intel_gpu_freq_change,
	TP_PROTO(u32 freq),
	TP_ARGS(freq),

	TP_STRUCT__entry(
		__field(u32, freq)
		),

	TP_fast_assign(
		__entry->freq = freq;
		),

	TP_printk("new_freq=%u", __entry->freq)
);

#endif /* _I915_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./i915
#include <trace/define_trace.h>
