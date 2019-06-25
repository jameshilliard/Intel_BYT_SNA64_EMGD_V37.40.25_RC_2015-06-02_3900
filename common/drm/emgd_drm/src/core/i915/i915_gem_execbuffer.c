/*
 * Copyright © 2008,2010,2012-2014 Intel Corporation
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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#include <linux/version.h>
#include "drmP.h"
#include "i915_drm.h"
#include "i915_reg.h"
#include "intel_bios.h"
#include "intel_ringbuffer.h"
#include "drm_emgd_private.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include <linux/dma_remapping.h>

struct eb_vmas {
	struct list_head vmas;
	int and;
	union {
		struct i915_vma *lut[0];
		struct hlist_head buckets[0];
	};
};

static struct eb_vmas *
eb_create(struct drm_i915_gem_execbuffer2 *args, struct i915_address_space *vm)
{
	struct eb_vmas *eb = NULL;

	if (args->flags & I915_EXEC_HANDLE_LUT) {
		unsigned size = args->buffer_count;
		size *= sizeof(struct i915_vma *);
		size += sizeof(struct eb_vmas);
		eb = kmalloc(size, GFP_TEMPORARY | __GFP_NOWARN | __GFP_NORETRY);
	}

	if (eb == NULL) {
		unsigned size = args->buffer_count;
		unsigned count = PAGE_SIZE / sizeof(struct hlist_head) / 2;
		BUILD_BUG_ON_NOT_POWER_OF_2(PAGE_SIZE / sizeof(struct hlist_head));
		while (count > 2*size)
			count >>= 1;
		eb = kzalloc(count*sizeof(struct hlist_head) +
				sizeof(struct eb_vmas),
				GFP_TEMPORARY);
		if (eb == NULL)
			return eb;

		eb->and = count - 1;
	} else
		eb->and = -args->buffer_count;

	INIT_LIST_HEAD(&eb->vmas);
	  return eb;
}

static void
eb_reset(struct eb_vmas *eb)
{

	if (eb->and >= 0)
		memset(eb->buckets, 0, (eb->and+1)*sizeof(struct hlist_head));
}

static int
eb_lookup_vmas(struct eb_vmas *eb,
	       struct drm_i915_gem_exec_object2 *exec,
	       const struct drm_i915_gem_execbuffer2 *args,
	       struct i915_address_space *vm,
	       struct drm_file *file)
{
	struct drm_i915_gem_object *obj;
	struct list_head objects;
	int i, ret = 0;

	INIT_LIST_HEAD(&objects);
	spin_lock(&file->table_lock);
	/* Grab a reference to the object and release the lock so we can lookup
	 * or create the VMA without using GFP_ATOMIC */
	for (i = 0; i < args->buffer_count; i++) {
		obj = to_intel_bo(idr_find(&file->object_idr, exec[i].handle));
		if (obj == NULL) {
			spin_unlock(&file->table_lock);
			DRM_DEBUG("Invalid object handle %d at index %d\n",
					exec[i].handle, i);
			ret = -ENOENT;
			goto out;
		}

		if (!list_empty(&obj->obj_exec_link)) {
			spin_unlock(&file->table_lock);
			DRM_DEBUG("Object %p [handle %d, index %d] appears more than once in object list\n",
					obj, exec[i].handle, i);
			ret = -EINVAL;
			goto out;
		}

		drm_gem_object_reference(&obj->base);
		list_add_tail(&obj->obj_exec_link, &objects);
	}
	spin_unlock(&file->table_lock);

	i = 0;
	list_for_each_entry(obj, &objects, obj_exec_link) {
		struct i915_vma *vma;

		vma = i915_gem_obj_lookup_or_create_vma(obj, vm);
		if (IS_ERR(vma)) {
			/* XXX: We don't need an error path fro vma because if
			 * the vma was created just for this execbuf, object
			 * unreference should kill it off.*/
			DRM_DEBUG("Failed to lookup VMA\n");
			ret = PTR_ERR(vma);
			goto out;
		}

		list_add_tail(&vma->exec_list, &eb->vmas);

		vma->exec_entry = &exec[i];
		if (eb->and < 0) {
			eb->lut[i] = vma;
		} else {
			uint32_t handle = args->flags & I915_EXEC_HANDLE_LUT ? i : exec[i].handle;
			vma->exec_handle = handle;
			hlist_add_head(&vma->exec_node,
					&eb->buckets[handle & eb->and]);
		}
		++i;
	}


out:
	while (!list_empty(&objects)) {
		obj = list_first_entry(&objects,
				       struct drm_i915_gem_object,
				       obj_exec_link);
		list_del_init(&obj->obj_exec_link);
		if (ret)
			drm_gem_object_unreference(&obj->base);
	}
	return ret;
}

static struct i915_vma *eb_get_vma(struct eb_vmas *eb, unsigned long handle)
{
	if (eb->and < 0) {
		if (handle >= -eb->and)
			return NULL;
		return eb->lut[handle];
	} else {
		struct hlist_head *head;
		struct hlist_node *node;
		head = &eb->buckets[handle & eb->and];
		hlist_for_each(node, head) {
			struct i915_vma *vma;

			vma = hlist_entry(node, struct i915_vma, exec_node);
			if (vma->exec_handle == handle)
				return vma;
		}
		return NULL;
	}
}

static void eb_destroy(struct eb_vmas *eb)
{
	while (!list_empty(&eb->vmas)) {
		struct i915_vma *vma;

		vma = list_first_entry(&eb->vmas,
				      struct i915_vma,
				      exec_list);
		list_del_init(&vma->exec_list);
		drm_gem_object_unreference(&vma->obj->base);
	}
	kfree(eb);
}

static inline int use_cpu_reloc(struct drm_i915_gem_object *obj)
{
	return (HAS_LLC(obj->base.dev) ||
			obj->base.write_domain == I915_GEM_DOMAIN_CPU ||
			!obj->map_and_fenceable ||
			obj->cache_level != I915_CACHE_NONE);
}

static int
relocate_entry_cpu(struct drm_i915_gem_object *obj,
		   struct drm_i915_gem_relocation_entry *reloc)
{
	uint32_t page_offset = offset_in_page(reloc->offset);
	char *vaddr;
	int ret = -EINVAL;

	ret = i915_gem_object_set_to_cpu_domain(obj, true);
	if (ret)
		return ret;

	vaddr = kmap_atomic(i915_gem_object_get_page(obj,
				reloc->offset >> PAGE_SHIFT));
	*(uint32_t *)(vaddr + page_offset) = reloc->delta;
	kunmap_atomic(vaddr);

	return 0;
}

static int
relocate_entry_gtt(struct drm_i915_gem_object *obj,
		   struct drm_i915_gem_relocation_entry *reloc)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t __iomem *reloc_entry;
	void __iomem *reloc_page;
	int ret = -EINVAL;

	ret = i915_gem_object_set_to_gtt_domain(obj, true);
	if (ret)
		return ret;

	ret = i915_gem_object_put_fence(obj);
	if (ret)
		return ret;

	/* Map the page containing the relocation we're going to perform.  */
	reloc->offset += i915_gem_obj_ggtt_offset(obj);
	reloc_page = io_mapping_map_atomic_wc(dev_priv->gtt.mappable,
			reloc->offset & PAGE_MASK);
	reloc_entry = (uint32_t __iomem *)
		(reloc_page + offset_in_page(reloc->offset));
	iowrite32(reloc->delta, reloc_entry);
	io_mapping_unmap_atomic(reloc_page);

	return 0;
}

static int
i915_gem_execbuffer_relocate_entry(struct drm_i915_gem_object *obj,
				  struct eb_vmas *eb,
				  struct drm_i915_gem_relocation_entry *reloc,
				  struct i915_address_space *vm)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_gem_object *target_obj;
	struct drm_i915_gem_object *target_i915_obj;
	struct i915_vma *target_vma;
	uint32_t target_offset;
	int ret = -EINVAL;

	/* we've already hold a reference to all valid objects */
	target_vma = eb_get_vma(eb, reloc->target_handle);
	if (unlikely(target_vma == NULL))
		return -ENOENT;
	target_i915_obj = target_vma->obj;
	target_obj = &target_vma->obj->base;

	target_offset = i915_gem_obj_ggtt_offset(target_i915_obj);

	/* Sandybridge PPGTT errata: We need a global gtt mapping for MI and
	 * pipe_control writes because the gpu doesn't properly redirect them
	 * through the ppgtt for non_secure batchbuffers. */
	if (unlikely(IS_GEN6(dev) &&
				reloc->write_domain == I915_GEM_DOMAIN_INSTRUCTION &&
				!target_i915_obj->has_global_gtt_mapping)) {
		i915_gem_gtt_bind_object(target_i915_obj,
				target_i915_obj->cache_level);
	}

	/* Validate that the target is in a valid r/w GPU domain */
	if (unlikely(reloc->write_domain & (reloc->write_domain - 1))) {
		DRM_DEBUG("reloc with multiple write domains: "
			  "obj %p target %d offset %d "
			  "read %08x write %08x",
			  obj, reloc->target_handle,
			  (int) reloc->offset,
			  reloc->read_domains,
			  reloc->write_domain);
		return ret;
	}
	if (unlikely((reloc->write_domain | reloc->read_domains)
				& ~I915_GEM_GPU_DOMAINS)) {
		DRM_DEBUG("reloc with read/write non-GPU domains: "
			  "obj %p target %d offset %d "
			  "read %08x write %08x",
			  obj, reloc->target_handle,
			  (int) reloc->offset,
			  reloc->read_domains,
			  reloc->write_domain);
		return ret;
	}
	if (unlikely(reloc->write_domain && target_obj->pending_write_domain &&
		     reloc->write_domain != target_obj->pending_write_domain)) {
		DRM_DEBUG("Write domain conflict: "
			  "obj %p target %d offset %d "
			  "new %08x old %08x\n",
			  obj, reloc->target_handle,
			  (int) reloc->offset,
			  reloc->write_domain,
			  target_obj->pending_write_domain);
		return ret;
	}

	target_obj->pending_read_domains |= reloc->read_domains;
	target_obj->pending_write_domain |= reloc->write_domain;


	/* If the relocation already has the right value in it, no
	 * more work needs to be done.
	 */
	if (target_offset == reloc->presumed_offset)
		return 0;

	/* Check that the relocation address is valid... */
	if (unlikely(reloc->offset > obj->base.size - 4)) {
		DRM_DEBUG("Relocation beyond object bounds: "
			  "obj %p target %d offset %d size %d.\n",
			  obj, reloc->target_handle,
			  (int) reloc->offset,
			  (int) obj->base.size);
		return ret;
	}
	if (unlikely(reloc->offset & 3)) {
		DRM_DEBUG("Relocation not 4-byte aligned: "
			  "obj %p target %d offset %d.\n",
			  obj, reloc->target_handle,
			  (int) reloc->offset);
		return ret;
	}

	/* We can't wait for rendering with pagefaults disabled */
	if (obj->active && in_atomic())
		return -EFAULT;

	reloc->delta += target_offset;
	if (use_cpu_reloc(obj))
		ret = relocate_entry_cpu(obj, reloc);
	else
		ret = relocate_entry_gtt(obj, reloc);

	if (ret)
		return ret;

	/* and update the user's relocation entry */
	reloc->presumed_offset = target_offset;

	return 0;
}

static int
i915_gem_execbuffer_relocate_vma(struct i915_vma *vma,
				 struct eb_vmas *eb)
{
#define N_RELOC(x) ((x) / sizeof(struct drm_i915_gem_relocation_entry))
	struct drm_i915_gem_relocation_entry stack_reloc[N_RELOC(512)];
	struct drm_i915_gem_relocation_entry __user *user_relocs;
	struct drm_i915_gem_exec_object2 *entry = vma->exec_entry;
	int remain, ret;

	user_relocs = to_user_ptr(entry->relocs_ptr);

	remain = entry->relocation_count;
	while (remain) {
		struct drm_i915_gem_relocation_entry *r = stack_reloc;
		int count = remain;
		if (count > ARRAY_SIZE(stack_reloc))
			count = ARRAY_SIZE(stack_reloc);
		remain -= count;

		if (__copy_from_user_inatomic(r, user_relocs, count*sizeof(r[0])))
			return -EFAULT;

		do {
			u64 offset = r->presumed_offset;

			ret = i915_gem_execbuffer_relocate_entry(vma->obj,
							eb, r, vma->vm);
			if (ret)
				return ret;

			if (r->presumed_offset != offset &&
					__copy_to_user_inatomic(&user_relocs->presumed_offset,
						&r->presumed_offset,
						sizeof(r->presumed_offset))) {
				return -EFAULT;
			}

			user_relocs++;
			r++;
		} while (--count);
	}

	return 0;
#undef N_RELOC
}

static int
i915_gem_execbuffer_relocate_vma_slow(struct i915_vma *vma,
		struct eb_vmas *eb,
		struct drm_i915_gem_relocation_entry *relocs)
{
	const struct drm_i915_gem_exec_object2 *entry = vma->exec_entry;
	int i, ret;

	for (i = 0; i < entry->relocation_count; i++) {
		ret = i915_gem_execbuffer_relocate_entry(vma->obj,
					eb, &relocs[i], vma->vm);
		if (ret)
			return ret;
	}

	return 0;
}

static int
i915_gem_execbuffer_relocate(struct eb_vmas *eb,
			     struct i915_address_space *vm)
{
	struct i915_vma *vma;

	int ret = 0;

	/* This is the fast path and we cannot handle a pagefault whilst
	 * holding the struct mutex lest the user pass in the relocations
	 * contained within a mmaped bo. For in such a case we, the page
	 * fault handler would call i915_gem_fault() and we would try to
	 * acquire the struct mutex again. Obviously this is bad and so
	 * lockdep complains vehemently.
	 */
	pagefault_disable();
	list_for_each_entry(vma, &eb->vmas, exec_list) {
		ret = i915_gem_execbuffer_relocate_vma(vma, eb);
		if (ret)
			break;
	}
	pagefault_enable();

	return ret;
}

#define  __EXEC_OBJECT_HAS_PIN (1<<31)
#define  __EXEC_OBJECT_HAS_FENCE (1<<30)

static int
need_reloc_mappable(struct i915_vma *vma)
{
	struct drm_i915_gem_exec_object2 *entry = vma->exec_entry;

	return entry->relocation_count && !use_cpu_reloc(vma->obj) &&
		i915_is_ggtt(vma->vm);
}

static int
i915_gem_execbuffer_reserve_vma(struct i915_vma *vma,
				struct intel_ring_buffer *ring,
				bool *need_reloc)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_exec_object2 *entry = vma->exec_entry;
	bool has_fenced_gpu_access = INTEL_INFO(ring->dev)->gen < 4;
	bool need_fence, need_mappable;
	struct drm_i915_gem_object *obj = vma->obj;
	int ret;

	need_fence =
		has_fenced_gpu_access &&
		entry->flags & EXEC_OBJECT_NEEDS_FENCE &&
		obj->tiling_mode != I915_TILING_NONE;
	need_mappable = need_fence || need_reloc_mappable(vma);

	ret = i915_gem_object_pin(obj, vma->vm, entry->alignment, need_mappable,
				  false);
	if (ret)
		return ret;

	entry->flags |= __EXEC_OBJECT_HAS_PIN;

	if (has_fenced_gpu_access) {
		if (entry->flags & EXEC_OBJECT_NEEDS_FENCE) {
			ret = i915_gem_object_get_fence(obj);
			if (ret)
				return ret;

			if (i915_gem_object_pin_fence(obj))
				entry->flags |= __EXEC_OBJECT_HAS_FENCE;

			obj->pending_fenced_gpu_access = true;
		}
	}

	/* Ensure ppgtt mapping exists if needed */
	if (dev_priv->mm.aliasing_ppgtt && !obj->has_aliasing_ppgtt_mapping) {
		i915_ppgtt_bind_object(dev_priv->mm.aliasing_ppgtt,
				obj, obj->cache_level);

		obj->has_aliasing_ppgtt_mapping = 1;
	}

	if (entry->offset != vma->node.start) {
		entry->offset = vma->node.start;
		*need_reloc = true;
	}
	if (entry->flags & EXEC_OBJECT_WRITE) {
		obj->base.pending_read_domains = I915_GEM_DOMAIN_RENDER;
		obj->base.pending_write_domain = I915_GEM_DOMAIN_RENDER;
	}

	if(entry->flags & EXEC_OBJECT_NEEDS_GTT &&
	!obj->has_global_gtt_mapping)
		i915_gem_gtt_bind_object(obj, obj->cache_level);

	return 0;
}

static void
i915_gem_execbuffer_unreserve_vma(struct i915_vma *vma)
{
	struct drm_i915_gem_exec_object2 *entry;
	struct drm_i915_gem_object *obj = vma->obj;

	if (!drm_mm_node_allocated(&vma->node))
		return;

	entry = vma->exec_entry;

	if (entry->flags & __EXEC_OBJECT_HAS_FENCE)
		i915_gem_object_unpin_fence(obj);

	if (entry->flags & __EXEC_OBJECT_HAS_PIN)
		i915_gem_object_unpin(obj);

	entry->flags &= ~(__EXEC_OBJECT_HAS_FENCE | __EXEC_OBJECT_HAS_PIN);
}

static int
i915_gem_execbuffer_reserve(struct intel_ring_buffer *ring,
			    struct list_head *vmas,
			    bool *need_relocs)
{
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;
	struct i915_address_space *vm;
	struct list_head ordered_vmas;
	bool has_fenced_gpu_access = INTEL_INFO(ring->dev)->gen < 4;
	int retry;

	if (list_empty(vmas))
		return 0;

	vm = list_first_entry(vmas, struct i915_vma, exec_list)->vm;

	INIT_LIST_HEAD(&ordered_vmas);
	while (!list_empty(vmas)) {
		struct drm_i915_gem_exec_object2 *entry;
		bool need_fence, need_mappable;

		vma = list_first_entry(vmas, struct i915_vma, exec_list);
		obj = vma->obj;
		entry = vma->exec_entry;

		need_fence =
			has_fenced_gpu_access &&
			entry->flags & EXEC_OBJECT_NEEDS_FENCE &&
			obj->tiling_mode != I915_TILING_NONE;
		need_mappable = need_fence || need_reloc_mappable(vma);

		if (need_mappable)
			list_move(&vma->exec_list, &ordered_vmas);
		else
			list_move_tail(&vma->exec_list, &ordered_vmas);

		obj->base.pending_read_domains = I915_GEM_GPU_DOMAINS & ~I915_GEM_DOMAIN_COMMAND;
		obj->base.pending_write_domain = 0;
		obj->pending_fenced_gpu_access = false;
	}
	list_splice(&ordered_vmas, vmas);

	/* Attempt to pin all of the buffers into the GTT.
	 * This is done in 3 phases:
	 *
	 * 1a. Unbind all objects that do not match the GTT constraints for
	 *     the execbuffer (fenceable, mappable, alignment etc).
	 * 1b. Increment pin count for already bound objects.
	 * 2.  Bind new objects.
	 * 3.  Decrement pin count.
	 *
	 * This avoid unnecessary unbinding of later objects in order to make
	 * room for the earlier objects *unless* we need to defragment.
	 */
	retry = 0;
	do {
		int ret = 0;

		/* Unbind any ill-fitting objects or pin. */
		list_for_each_entry(vma, vmas, exec_list) {
			struct drm_i915_gem_exec_object2 *entry =
							vma->exec_entry;
			bool need_fence, need_mappable;

			obj = vma->obj;

			if (!drm_mm_node_allocated(&vma->node))
				continue;

			need_fence =
				has_fenced_gpu_access &&
				entry->flags & EXEC_OBJECT_NEEDS_FENCE &&
				obj->tiling_mode != I915_TILING_NONE;
			need_mappable = need_fence || need_reloc_mappable(vma);

			WARN_ON((need_mappable || need_fence) &&
			       !i915_is_ggtt(vma->vm));

			if ((entry->alignment &&
			     vma->node.start & (entry->alignment - 1)) ||
			    (need_mappable && !obj->map_and_fenceable))
				ret = i915_vma_unbind(vma);
			else
				ret = i915_gem_execbuffer_reserve_vma(vma,
							ring, need_relocs);
			if (ret)
				goto err;
		}

		/* Bind fresh objects */
		list_for_each_entry(vma, vmas, exec_list) {
			if (drm_mm_node_allocated(&vma->node))
				continue;

			ret = i915_gem_execbuffer_reserve_vma(vma,
						ring, need_relocs);
			if (ret)
				goto err;
		}

err:	/* Decrement pin count for bound objects */
		list_for_each_entry(vma, vmas, exec_list)
			i915_gem_execbuffer_unreserve_vma(vma);

		if (ret != -ENOSPC || retry++)
			return ret;

		ret = i915_gem_evict_vm(vm, true);
		if (ret)
			return ret;
	} while (1);
}

static int
i915_gem_execbuffer_relocate_slow(struct drm_device *dev,
				  struct drm_i915_gem_execbuffer2 *args,
				  struct drm_file *file,
				  struct intel_ring_buffer *ring,
				  struct eb_vmas *eb,
				  struct drm_i915_gem_exec_object2 *exec)
{
	struct drm_i915_gem_relocation_entry *reloc;
	struct i915_address_space *vm;
	struct i915_vma *vma;
	int *reloc_offset;
	int i, total, ret;
	unsigned count = args->buffer_count;
	bool need_relocs;

	if (WARN_ON(list_empty(&eb->vmas)))
		return 0;

	vm = list_first_entry(&eb->vmas, struct i915_vma, exec_list)->vm;

	/* We may process another execbuffer during the unlock... */
	while (!list_empty(&eb->vmas)) {
		vma = list_first_entry(&eb->vmas, struct i915_vma, exec_list);
		list_del_init(&vma->exec_list);
		drm_gem_object_unreference(&vma->obj->base);
	}

	mutex_unlock(&dev->struct_mutex);

	total = 0;
	for (i = 0; i < count; i++)
		total += exec[i].relocation_count;

	reloc_offset = drm_malloc_ab(count, sizeof(*reloc_offset));
	reloc = drm_malloc_ab(total, sizeof(*reloc));
	if (reloc == NULL || reloc_offset == NULL) {
		drm_free_large(reloc);
		drm_free_large(reloc_offset);
		mutex_lock(&dev->struct_mutex);
		return -ENOMEM;
	}

	total = 0;
	for (i = 0; i < count; i++) {
		struct drm_i915_gem_relocation_entry __user *user_relocs;
		u64 invalid_offset = (u64)-1;
		int j;

		user_relocs = to_user_ptr(exec[i].relocs_ptr);

		if (copy_from_user(reloc+total, user_relocs,
				   exec[i].relocation_count * sizeof(*reloc))) {
			ret = -EFAULT;
			mutex_lock(&dev->struct_mutex);
			goto err;
		}

		/* As we do not update the known relocation offsets after
		 * relocating (due to the complexities in lock handling),
		 * we need to mark them as invalid now so that we force the
		 * relocation processing next time. Just in case the target
		 * object is evicted and then rebound into its old
		 * presumed_offset before the next execbuffer - if that
		 * happened we would make the mistake of assuming that the
		 * relocations were valid.
		 */
		for (j = 0; j < exec[i].relocation_count; j++) {
			if (copy_to_user(&user_relocs[j].presumed_offset,
						&invalid_offset,
						sizeof(invalid_offset))) {
				ret = -EFAULT;
				mutex_lock(&dev->struct_mutex);
				goto err;
			}
		}

		reloc_offset[i] = total;
		total += exec[i].relocation_count;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret) {
		mutex_lock(&dev->struct_mutex);
		goto err;
	}

	/* reacquire the objects */
	eb_reset(eb);
	ret = eb_lookup_vmas(eb, exec, args, vm, file);

	if (ret)
		goto err;
	need_relocs = (args->flags & I915_EXEC_NO_RELOC) == 0;
	ret = i915_gem_execbuffer_reserve(ring, &eb->vmas, &need_relocs);

	if (ret)
		goto err;

	list_for_each_entry(vma, &eb->vmas, exec_list) {
		int offset = vma->exec_entry - exec;

		ret = i915_gem_execbuffer_relocate_vma_slow(vma, eb,
					reloc + reloc_offset[offset]);
		if (ret)
			goto err;
	}

	/* Leave the user relocations as are, this is the painfully slow path,
	 * and we want to avoid the complication of dropping the lock whilst
	 * having buffers reserved in the aperture and so causing spurious
	 * ENOSPC for random operations.
	 */

err:
	drm_free_large(reloc);
	drm_free_large(reloc_offset);
	return ret;
}

static int
i915_gem_execbuffer_wait_for_flips(struct intel_ring_buffer *ring, u32 flips)
{
	u32 plane, flip_mask;
	int ret;

	/* Check for any pending flips. As we only maintain a flip queue depth
	 * of 1, we can simply insert a WAIT for the next display flip prior
	 * to executing the batch and avoid stalling the CPU.
	 */

	for (plane = 0; flips >> plane; plane++) {
		if (((flips >> plane) & 1) == 0)
			continue;

		if (plane)
			flip_mask = MI_WAIT_FOR_PLANE_B_FLIP;
		else
			flip_mask = MI_WAIT_FOR_PLANE_A_FLIP;

		ret = intel_ring_begin(ring, 2);
		if (ret)
			return ret;

		intel_ring_emit(ring, MI_WAIT_FOR_EVENT | flip_mask);
		intel_ring_emit(ring, MI_NOOP);
		intel_ring_advance(ring);
	}

	return 0;
}

static int
i915_gem_execbuffer_move_to_gpu(struct intel_ring_buffer *ring,
				struct list_head *vmas)
{
	struct i915_vma *vma;
	uint32_t flush_domains = 0;
	uint32_t flips = 0;
	bool flush_chipset = false;
	int ret;

	list_for_each_entry(vma, vmas, exec_list) {
		struct drm_i915_gem_object *obj = vma->obj;
		ret = i915_gem_object_sync(obj, ring);
		if (ret)
			return ret;

		if (obj->base.write_domain & I915_GEM_DOMAIN_CPU)
			flush_chipset |= i915_gem_clflush_object(obj, false);

		if (obj->base.pending_write_domain)
			flips |= atomic_read(&obj->pending_flip);

		flush_domains |= obj->base.write_domain;
	}

	if (flips) {
		ret = i915_gem_execbuffer_wait_for_flips(ring, flips);
		if (ret)
			return ret;
	}

	if (flush_chipset)
		i915_gem_chipset_flush(ring->dev);

	if (flush_domains & I915_GEM_DOMAIN_GTT)
		wmb();

	/* Unconditionally invalidate gpu caches and ensure that we do flush
	 * any residual writes from the previous batch.
	 */
	return intel_ring_invalidate_all_caches(ring);
}

static bool
i915_gem_check_execbuffer(struct drm_i915_gem_execbuffer2 *exec)
{
	if (exec->flags & __I915_EXEC_UNKNOWN_FLAGS)
		return false;
	return ((exec->batch_start_offset | exec->batch_len) & 0x7) == 0;
}

static int
validate_exec_list(struct drm_i915_gem_exec_object2 *exec,
		   int count)
{
	int i;
	unsigned relocs_total = 0;
	unsigned relocs_max = UINT_MAX / sizeof(struct drm_i915_gem_relocation_entry);

	for (i = 0; i < count; i++) {
		char __user *ptr = to_user_ptr(exec[i].relocs_ptr);
		int length; /* limited by fault_in_pages_readable() */

		if (exec[i].flags & __EXEC_OBJECT_UNKNOWN_FLAGS)
			return -EINVAL;
		/* First check for malicious input causing overflow in
		 * the worst case where we need to allocate the entire
		 * relocation tree as a single array.
		 * */
		if (exec[i].relocation_count > relocs_max -
				relocs_total)

			return -EINVAL;
		relocs_total += exec[i].relocation_count;

		length = exec[i].relocation_count *
			sizeof(struct drm_i915_gem_relocation_entry);
		/*
		 * We must check that the entire relocation array is safe
		 * to read, but since we may need to update the presumed
		 * offsets during execution, check for full write access.
		 */

		if (!access_ok(VERIFY_WRITE, ptr, length))
			return -EFAULT;

		/* Here is the old function call.  Based on git, this was added in
		 * kernel 3.6, however the driver seems to compile fine pointing to a 3.5
		 * kernel.
		 * if (fault_in_multipages_readable(ptr, length))
		 */
		if (fault_in_pages_readable(ptr, length))
			return -EFAULT;
	}

	return 0;
}

static void
i915_gem_execbuffer_move_to_active(struct list_head *vmas,
				   struct intel_ring_buffer *ring)
{
	struct i915_vma *vma;

	list_for_each_entry(vma, vmas, exec_list) {
		struct drm_i915_gem_object *obj = vma->obj;
		u32 old_read = obj->base.read_domains;
		u32 old_write = obj->base.write_domain;

		obj->base.write_domain = obj->base.pending_write_domain;
		if (obj->base.write_domain == 0)
			obj->base.pending_read_domains |= obj->base.read_domains;
		obj->base.read_domains = obj->base.pending_read_domains;
		obj->fenced_gpu_access = obj->pending_fenced_gpu_access;

		i915_vma_move_to_active(vma, ring);
		if (obj->base.write_domain) {
			obj->dirty = 1;
			obj->last_write_seqno = intel_ring_get_seqno(ring);
			if (obj->pin_count) /* check for potential scanout */
				intel_mark_fb_busy(obj, ring);
		}

		trace_i915_gem_object_change_domain(obj, old_read, old_write);
	}
}

static void
i915_gem_execbuffer_retire_commands(struct drm_device *dev,
					struct drm_file *file,
					struct intel_ring_buffer *ring,
					struct drm_i915_gem_object *obj,
					struct drm_i915_gem_exec_timestamp *timestamp)
{
	/* Unconditionally force add_request to emit a full flush. */
	ring->gpu_caches_dirty = true;

	/* Add a breadcrumb for the completion of the batch buffer */
	(void)__i915_add_request(ring, file, obj, NULL, timestamp);
}


enum {
	GEM_CMD_RESCHED,
	GEM_CMD_OUT,
	GEM_CMD_DONE,
	GEM_CMD_ERROR
};
int sampled_debugs = 0;

static int
i915_schedule(struct drm_device *dev, struct intel_ring_buffer *ring, struct drm_file *file)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	int higher_priority_requestors=0;
	unsigned long diff_jiffs, new_jiffs = 0;
	int i = 0;
	struct drm_i915_file_private *temp;
	int again;

	/* Bail if GPU scheduler is not enabled */
	if (!gem_scheduler) {
		return GEM_CMD_DONE;
	}

	/* here onwards we need coherrency with this process's scheduler info */
	spin_lock(&file_priv->mm.lock);
	spin_lock(&dev_priv->scheduler.sched_lock);
	++sampled_debugs;
	if((sampled_debugs%1000)==0)
		EMGD_DEBUG("Current client pid = %06d", PID_T_FROM_DRM_FILE(file));
	/* Check if our fd was released while spinning*/
	if (file_priv->mm.outstanding_requests == -1) {
		spin_unlock(&dev_priv->scheduler.sched_lock);
		spin_unlock(&file_priv->mm.lock);
		return GEM_CMD_OUT;
	}

	/* Step 1 - Here we allow the compositor(priority 0) to "return" earlier.*/
	if (file_priv->mm.sched_priority == 0){
		spin_unlock(&dev_priv->scheduler.sched_lock);
		spin_unlock(&file_priv->mm.lock);
		return GEM_CMD_DONE;
	}

	if (file->is_master) /*not sure why DRM_MASTER is not master during emgd_driver_open?*/
		file_priv->mm.sched_priority = 0;

	/*A runtime switch controlled feature to auto increase the priority/capacity of video app*/
	if ( dev_priv->scheduler.video_switch ) {
		if ( ring->id == VCS && file_priv->mm.sched_priority == SHARED_NORMAL_PRIORITY){
			do {
				again = 0;
				list_for_each_entry(temp, &dev_priv->i915_client_list, client_link) {
					if ( file_priv != temp ) {
						if ( temp->mm.sched_priority == dev_priv->scheduler.video_priority ) {
							dev_priv->scheduler.video_priority++;
							again = 1;
							break;
						}
					}
				}
			} while (again);

			if (dev_priv->scheduler.video_priority < SHARED_NORMAL_PRIORITY) {
				EMGD_DEBUG("Bumping video app to priority %d", dev_priv->scheduler.video_priority);
				dev_priv->scheduler.req_prty_cnts[dev_priv->scheduler.video_priority] +=
					dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority];
				dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority] = 0;
				file_priv->mm.sched_priority = dev_priv->scheduler.video_priority;
				file_priv->mm.sched_capacity = dev_priv->scheduler.video_init_capacity;
				file_priv->mm.sched_period   = dev_priv->scheduler.video_init_period;
			}
			else
				EMGD_DEBUG("No priority slot available for video app to bump.");
		}
	}

	/*A runtime switch controlled feature to auto penaliza misbehaving app by decreasing its priority and capacity/period.*/
	if (dev_priv->scheduler.rogue_switch) {
		if (file_priv->mm.sched_priority != SHARED_ROGUE_PRIORITY && file_priv->mm.sched_priority != 0){	

			if(file_priv->mm.sched_avgexectime > dev_priv->scheduler.rogue_trigger){
				EMGD_DEBUG("RogueBucket: %d", PID_T_FROM_DRM_FILE(file));			

				dev_priv->scheduler.req_prty_cnts[SHARED_ROGUE_PRIORITY] += 
					dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority];
				dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority] = 0; 
				file_priv->mm.sched_priority = SHARED_ROGUE_PRIORITY;
			}
		}
	}

	/* Step 2 - lets ensure our priority is good enough to continue */
	/* check for other clients with pending higher priority requests */
	if (file_priv->mm.sched_priority) {
		/* make sure this client isnt priority-0 - i.e. highest */
		for ( i=(file_priv->mm.sched_priority-1); i>=0 ; i-- ) {
            if (dev_priv->scheduler.req_prty_cnts[i]) {
				if(i == SHARED_NORMAL_PRIORITY &&
						dev_priv->scheduler.shared_balance) {
					higher_priority_requestors += dev_priv->scheduler.req_prty_cnts[i];
					break;
				} else if(dev_priv->scheduler.req_prty_fp[i] &&
						dev_priv->scheduler.req_prty_fp[i]->mm.sched_balance)  {
					higher_priority_requestors += dev_priv->scheduler.req_prty_cnts[i];
					break;
				}
            }

			if ((sampled_debugs%1000)==0){
				EMGD_DEBUG("Check higher priorities, i = %d, count = %d",
					 i, dev_priv->scheduler.req_prty_cnts[i]);
			}

		}
		if (higher_priority_requestors) {
			spin_unlock(&dev_priv->scheduler.sched_lock);
			spin_unlock(&file_priv->mm.lock);
			if((sampled_debugs%1000)==0)
				EMGD_DEBUG("Stall");
			/* this is gonna spin mad - we need to wait on some 
			 * FIXME - replace temp hack usleep with interrupt on next seqno */
			mutex_unlock(&dev->struct_mutex);
			udelay(20);
			i=0;
			while(i915_mutex_lock_interruptible(dev) && (++i<1000000));
			if (i==1000000) {
				EMGD_DEBUG("LockFail1");
			}
			return GEM_CMD_RESCHED;
		}
	} else {
		/* priority zero! this is highest - like a drm-master, let it thru */
		spin_unlock(&dev_priv->scheduler.sched_lock);
		spin_unlock(&file_priv->mm.lock);
		return GEM_CMD_DONE;
	}

	new_jiffs = jiffies;

	/* Step 3 - lets ensure we refresh the capacity if its time */
	if (file_priv->mm.sched_priority == SHARED_NORMAL_PRIORITY) {

		if (time_is_after_jiffies(dev_priv->scheduler.shared_last_refresh_jiffs))
			diff_jiffs = dev_priv->scheduler.shared_last_refresh_jiffs - new_jiffs;
				 /* wrap around of jiffies bits? */
		else
			diff_jiffs = new_jiffs - dev_priv->scheduler.shared_last_refresh_jiffs;
		/* this means we're in the common-grouped priority - shared bucket */
		if ( time_after_eq( new_jiffs, 
			(dev_priv->scheduler.shared_last_refresh_jiffs + 
			 usecs_to_jiffies(dev_priv->scheduler.shared_period)))) {
			if (dev_priv->scheduler.shared_balance < usecs_to_jiffies(dev_priv->scheduler.shared_capacity)) {
				EMGD_DEBUG("FreshSh");
				dev_priv->scheduler.shared_balance = usecs_to_jiffies(dev_priv->scheduler.shared_capacity);
			}
			dev_priv->scheduler.shared_last_refresh_jiffs = new_jiffs;
		}
	}
	else if (file_priv->mm.sched_priority == SHARED_ROGUE_PRIORITY) {
		if (time_is_after_jiffies(dev_priv->scheduler.rogue_last_refresh_jiffs))
			diff_jiffs = dev_priv->scheduler.rogue_last_refresh_jiffs - new_jiffs;
		else
			diff_jiffs = new_jiffs - dev_priv->scheduler.rogue_last_refresh_jiffs;
		/* this means we're in the common-grouped priority - rogue bucket */
		if ( time_after_eq( new_jiffs, 
			(dev_priv->scheduler.rogue_last_refresh_jiffs + 
			 usecs_to_jiffies(dev_priv->scheduler.rogue_period)))) {
			if (dev_priv->scheduler.rogue_balance < usecs_to_jiffies(dev_priv->scheduler.rogue_capacity)) {
				EMGD_DEBUG("FreshRg");
				dev_priv->scheduler.rogue_balance = usecs_to_jiffies(dev_priv->scheduler.rogue_capacity);
			}
			dev_priv->scheduler.rogue_last_refresh_jiffs = new_jiffs;
		}
	}
	else {
		/* this process has its own special priority */
		if (time_is_after_jiffies(file_priv->mm.sched_last_refresh_jiffs))
			diff_jiffs = file_priv->mm.sched_last_refresh_jiffs - new_jiffs;
				 /* wrap around of jiffies bits? */
		else
			diff_jiffs = new_jiffs - file_priv->mm.sched_last_refresh_jiffs;

		if (time_after_eq(new_jiffs,
			(file_priv->mm.sched_last_refresh_jiffs + 
			 usecs_to_jiffies(file_priv->mm.sched_period)))) {
			if (file_priv->mm.sched_balance < usecs_to_jiffies(file_priv->mm.sched_capacity)) {
				EMGD_DEBUG("FreshPv");
				file_priv->mm.sched_balance = usecs_to_jiffies(file_priv->mm.sched_capacity);
			}
			file_priv->mm.sched_last_refresh_jiffs = new_jiffs;
		}
	}

	/* Step 4 - Ensure we have enough balance budget to continue */
	if (file_priv->mm.sched_priority == SHARED_NORMAL_PRIORITY) {
		/* this means we're in the common-grouped priority - shared bucket */
		if (dev_priv->scheduler.shared_balance > 0) {

			/* Lets see if we're using AE Policy for reservation:
			 * AE means we deduct the balance before returning from scheduler.
			 * During retirement, DONT double deduct from balance for AE.
 			 * NOTE: For rogue, we always only do this.
			 */
			if(dev_priv->scheduler.shared_aereserve) {
				if (dev_priv->scheduler.shared_balance < usecs_to_jiffies(file_priv->mm.sched_avgexectime))
					dev_priv->scheduler.shared_balance = 0;
				else
					dev_priv->scheduler.shared_balance -= usecs_to_jiffies(file_priv->mm.sched_avgexectime);
			}

			spin_unlock(&dev_priv->scheduler.sched_lock);
			spin_unlock(&file_priv->mm.lock);
			return GEM_CMD_DONE;
		} else {
			/* oh well, we dont have budget - lets just sleep the minimal time */
			if (jiffies_to_usecs(diff_jiffs) < dev_priv->scheduler.shared_period) {
				spin_unlock(&dev_priv->scheduler.sched_lock);
				spin_unlock(&file_priv->mm.lock);
				EMGD_DEBUG("NoBdgtSh");
				mutex_unlock(&dev->struct_mutex);
				udelay(100);
				i=0;
				while(i915_mutex_lock_interruptible(dev) && (++i<1000000));
				if (i==1000000) {
					EMGD_DEBUG("LockFail2");
				}
				spin_lock(&file_priv->mm.lock);
				spin_lock(&dev_priv->scheduler.sched_lock);
			} else {
				DRM_ERROR("Invalid state, no balance, but we overshot shared period!?\n");
			}
		}
	} 
	else if (file_priv->mm.sched_priority == SHARED_ROGUE_PRIORITY) {
		/* this means we're in the common-grouped priority - rogue bucket */
		if (dev_priv->scheduler.rogue_balance > 0) {
			/* Rogue AE Policy: Deduct the balance before returning from scheduler*/
			if (dev_priv->scheduler.rogue_balance < usecs_to_jiffies(file_priv->mm.sched_avgexectime))
				dev_priv->scheduler.rogue_balance = 0;
			else
				dev_priv->scheduler.rogue_balance -= usecs_to_jiffies(file_priv->mm.sched_avgexectime);

			spin_unlock(&dev_priv->scheduler.sched_lock);
			spin_unlock(&file_priv->mm.lock);
			return GEM_CMD_DONE;
		} else {
			/* oh well, we dont have budget - lets just sleep the minimal time */
			if (jiffies_to_usecs(diff_jiffs) < dev_priv->scheduler.rogue_period) {
				spin_unlock(&dev_priv->scheduler.sched_lock);
				spin_unlock(&file_priv->mm.lock);
				//udelay(dev_priv->scheduler.rogue_period - jiffies_to_usecs(diff_jiffs));
				EMGD_DEBUG("NoBdgtRg");
				mutex_unlock(&dev->struct_mutex);
				udelay(100);
				i=0;
				while(i915_mutex_lock_interruptible(dev) && (++i<1000000));
				if (i==1000000) {
					EMGD_DEBUG("LockFail2");
				}
				spin_lock(&file_priv->mm.lock);
				spin_lock(&dev_priv->scheduler.sched_lock);
			} else {
				DRM_ERROR("Invalid state, no balance, but we overshot shared period!?\n");
			}
		}
	}
	else {
		/* this process has its own special priority */
		if  (file_priv->mm.sched_balance > 0) {
			/* Lets see if we're using AE Policy for reservation:
			 * AE means we deduct the balance before returning from scheduler.
			 * During retirement, DONT double deduct from balance for AE.
			 * NOTE: For rogue, we always only do this.
			 */
			if(dev_priv->scheduler.shared_aereserve) {
				if (file_priv->mm.sched_balance < usecs_to_jiffies(file_priv->mm.sched_avgexectime))
					file_priv->mm.sched_balance = 0;
				else
					file_priv->mm.sched_balance -= usecs_to_jiffies(file_priv->mm.sched_avgexectime);
			}
			spin_unlock(&dev_priv->scheduler.sched_lock);
			spin_unlock(&file_priv->mm.lock);
			return GEM_CMD_DONE;
		} else {
			/* oh well, we dont have budget - lets just sleep the minimal time */
			if (jiffies_to_usecs(diff_jiffs) < file_priv->mm.sched_period) {
				spin_unlock(&dev_priv->scheduler.sched_lock);
				spin_unlock(&file_priv->mm.lock);

				EMGD_DEBUG("NoBdgtPv");
				mutex_unlock(&dev->struct_mutex);
				udelay(100);
				i=0;
				while(i915_mutex_lock_interruptible(dev) && (++i<1000000));
				if (i==1000000) {
					EMGD_DEBUG("LockFail3");
				}
				spin_lock(&file_priv->mm.lock);
				spin_lock(&dev_priv->scheduler.sched_lock);
			} else {
				DRM_ERROR("Invalid state, no balance, but we just overshot private period!?\n");
			}
		}
	}


	/* If we're here, we need to reschedule - we may have gone thru a sleep
	 * So we should check if our fd was released while spinning again */
	if (file_priv->mm.outstanding_requests == -1) {
		spin_unlock(&dev_priv->scheduler.sched_lock);
		spin_unlock(&file_priv->mm.lock);
		return GEM_CMD_OUT;
	}

	spin_unlock(&dev_priv->scheduler.sched_lock);
	spin_unlock(&file_priv->mm.lock);
	return GEM_CMD_RESCHED;

}


static int
i915_reset_gen7_sol_offsets(struct drm_device *dev,
			    struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret, i;

	if (!IS_GEN7(dev) || ring != &dev_priv->ring[RCS])
		return 0;

	ret = intel_ring_begin(ring, 4 * 3);
	if (ret)
		return ret;

	for (i = 0; i < 4; i++) {
		intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(ring, GEN7_SO_WRITE_OFFSET(i));
		intel_ring_emit(ring, 0);
	}

	intel_ring_advance(ring);

	return 0;
}

static int
i915_gem_do_execbuffer(struct drm_device *dev, void *data,
		       struct drm_file *file,
		       struct drm_i915_gem_execbuffer2 *args,
		       struct drm_i915_gem_exec_object2 *exec,
		       struct i915_address_space *vm)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	struct eb_vmas *eb = NULL;
	struct drm_i915_gem_object *batch_obj;
	struct drm_clip_rect *cliprects = NULL;
	struct intel_ring_buffer *ring;
	struct drm_i915_gem_exec_timestamp * timestamp = NULL;
	u32 ctx_id = i915_execbuffer2_get_context_id(*args);
	u32 exec_start, exec_len;
	int ret, mode, i, gem_sched_queued =0;
	bool need_relocs;
	u32 mask, flags;

	if (!i915_gem_check_execbuffer(args))
		return -EINVAL;

	ret = validate_exec_list(exec, args->buffer_count);
	if (ret)
		return ret;

	flags = 0;

	if (args->flags & I915_EXEC_SECURE) {
		if (!file->is_master || !capable(CAP_SYS_ADMIN))
			return -EPERM;

		flags |= I915_DISPATCH_SECURE;
	}

	if (args->flags & I915_EXEC_IS_PINNED)
		flags |= I915_DISPATCH_PINNED;

	switch (args->flags & I915_EXEC_RING_MASK) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		vlv_turbo_up(dev);
		ring = &dev_priv->ring[RCS];
		break;
	case I915_EXEC_BSD:
		vlv_turbo_up(dev);
		ring = &dev_priv->ring[VCS];
		if (ctx_id != DEFAULT_CONTEXT_ID) {
			DRM_DEBUG("Ring %s doesn't support contexts\n",
					ring->name);
			return -EPERM;
		}
		break;
	case I915_EXEC_BLT:
		ring = &dev_priv->ring[BCS];
		if (ctx_id != DEFAULT_CONTEXT_ID) {
			DRM_DEBUG("Ring %s doesn't support contexts\n",
					ring->name);
			return -EPERM;
		}
		break;
	default:
		DRM_DEBUG("execbuf with unknown ring: %d\n",
			  (int)(args->flags & I915_EXEC_RING_MASK));
		return -EINVAL;
	}
	if (!intel_ring_initialized(ring)) {
		DRM_DEBUG("execbuf with invalid ring: %d\n",
				(int)(args->flags & I915_EXEC_RING_MASK));
		return -EINVAL;
	}

	mode = args->flags & I915_EXEC_CONSTANTS_MASK;
	mask = I915_EXEC_CONSTANTS_MASK;
	switch (mode) {
	case I915_EXEC_CONSTANTS_REL_GENERAL:
	case I915_EXEC_CONSTANTS_ABSOLUTE:
	case I915_EXEC_CONSTANTS_REL_SURFACE:
		if (ring == &dev_priv->ring[RCS] &&
		    mode != dev_priv->relative_constants_mode) {
			if (INTEL_INFO(dev)->gen < 4)
				return -EINVAL;

			if (INTEL_INFO(dev)->gen > 5 &&
			    mode == I915_EXEC_CONSTANTS_REL_SURFACE)
				return -EINVAL;

			/* The HW changed the meaning on this bit on gen6 */
			if (INTEL_INFO(dev)->gen >= 6)
				mask &= ~I915_EXEC_CONSTANTS_REL_SURFACE;
		}
		break;
	default:
		DRM_DEBUG("execbuf with unknown constants: %d\n", mode);
		return -EINVAL;
	}

	if (args->buffer_count < 1) {
		DRM_DEBUG("execbuf with %d buffers\n", args->buffer_count);
		return -EINVAL;
	}

	if (args->num_cliprects != 0) {
		if (ring != &dev_priv->ring[RCS]) {
			DRM_DEBUG("clip rectangles are only valid with the render ring\n");
			return -EINVAL;
		}

		if (INTEL_INFO(dev)->gen >= 5) {
			DRM_DEBUG("clip rectangles are only valid on pre-gen5\n");
			return -EINVAL;
		}

		if (args->num_cliprects > UINT_MAX / sizeof(*cliprects)) {
			DRM_DEBUG("execbuf with %u cliprects\n",
					args->num_cliprects);
			return -EINVAL;
		}

		cliprects = kcalloc(args->num_cliprects,
				    sizeof(*cliprects),
				    GFP_KERNEL);
		if (cliprects == NULL) {
			ret = -ENOMEM;
			goto pre_mutex_err;
		}

		if (copy_from_user(cliprects,
				      to_user_ptr(args->cliprects_ptr),
				      sizeof(*cliprects)*args->num_cliprects)) {
			ret = -EFAULT;
			goto pre_mutex_err;
		}
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		goto pre_mutex_err;
again:
	if (dev_priv->ums.mm_suspended) {
		mutex_unlock(&dev->struct_mutex);
		ret = -EBUSY;
		goto pre_mutex_err;
	}

	if (!eb) {
		eb = eb_create(args, vm);
		if (eb == NULL) {
			mutex_unlock(&dev->struct_mutex);
			ret = -ENOMEM;
			goto pre_mutex_err;
		}
	}

	if (gem_scheduler && !gem_sched_queued) {
		spin_lock(&dev_priv->scheduler.sched_lock);
		dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority]++;
		dev_priv->scheduler.req_prty_fp[file_priv->mm.sched_priority] = file_priv;

		spin_unlock(&dev_priv->scheduler.sched_lock);
		gem_sched_queued = 1;
	}

	switch (i915_schedule(dev, ring, file)) {
		case GEM_CMD_RESCHED:
			goto again;
			break;
		case GEM_CMD_OUT:
			eb_destroy(eb);
			mutex_unlock(&dev->struct_mutex);
			ret = -EINVAL;
			goto pre_mutex_err;
			break;
		case GEM_CMD_DONE:
			break;
		case GEM_CMD_ERROR:
		default:
			BUG();
	}

	/* Look up object handles */
	ret = eb_lookup_vmas(eb, exec, args, vm, file);
	if (ret)
		goto err;
	/* take note of the batch buffer before we might reorder the lists */
	batch_obj = list_entry(eb->vmas.prev, struct i915_vma, exec_list)->obj;

	/* Mark exec buffers as read-only from GPU side by default */
	batch_obj->gt_ro = 1;

	/* Move the objects en-masse into the GTT, evicting if necessary. */
	need_relocs = (args->flags & I915_EXEC_NO_RELOC) == 0;
	ret = i915_gem_execbuffer_reserve(ring, &eb->vmas, &need_relocs);
	if (ret)
		goto err;

	/* The objects are in their final locations, apply the relocations. */

	if (need_relocs)
		ret = i915_gem_execbuffer_relocate(eb, vm);
	if (ret) {
		if (ret == -EFAULT) {
			ret = i915_gem_execbuffer_relocate_slow(dev, args, file, ring,
								eb, exec);
			BUG_ON(!mutex_is_locked(&dev->struct_mutex));
		}
		if (ret)
			goto err;
	}

	/* Set the pending read domains for the batch buffer to COMMAND */
	if (batch_obj->base.pending_write_domain) {
		DRM_DEBUG("Attempting to use self-modifying batch buffer\n");
		ret = -EINVAL;
		goto err;
	}
	batch_obj->base.pending_read_domains |= I915_GEM_DOMAIN_COMMAND;

	/* snb/ivb/vlv conflate the "batch in ppgtt" bit with the "non-secure
	 * batch" bit. Hence we need to pin secure batches into the global gtt.
	 * hsw should have this fixed, but let's be paranoid and do it
	 * unconditionally for now. */
	if (flags & I915_DISPATCH_SECURE && !batch_obj->has_global_gtt_mapping)
		i915_gem_gtt_bind_object(batch_obj, batch_obj->cache_level);

	ret = i915_gem_execbuffer_move_to_gpu(ring, &eb->vmas);
	if (ret)
		goto err;

	ret = i915_switch_context(ring, file, ctx_id);
	if (ret)
		goto err;

#ifdef VLV_ALWAYS_GTT_TLB
	if (ring == &dev_priv->ring[RCS] &&
	    ( (mode != dev_priv->relative_constants_mode) ||
	      (IS_VALLEYVIEW(dev)) )     ) {
		ret = intel_ring_begin(ring, 4);
		if (ret)
				goto err;

		intel_ring_emit(ring, MI_NOOP);
		intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(ring, INSTPM);
		if (IS_VALLEYVIEW(dev)) {
			mask |= 0x02200000;
			mode |= 0x00000220;
		}
		intel_ring_emit(ring, mask << 16 | mode);
		intel_ring_advance(ring);

		dev_priv->relative_constants_mode = mode;
	}
#else
	if (ring == &dev_priv->ring[RCS] &&
		mode != dev_priv->relative_constants_mode) {
		ret = intel_ring_begin(ring, 4);
		if (ret)
				goto err;

		intel_ring_emit(ring, MI_NOOP);
		intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(ring, INSTPM);
		intel_ring_emit(ring, mask << 16 | mode);
		intel_ring_advance(ring);

		dev_priv->relative_constants_mode = mode;
	}
#endif
	if (args->flags & I915_EXEC_GEN7_SOL_RESET) {
		ret = i915_reset_gen7_sol_offsets(dev, ring);
		if (ret)
			goto err;
	}

	exec_start = i915_gem_obj_offset(batch_obj, vm) +
		args->batch_start_offset;
	exec_len = args->batch_len;
	if (cliprects) {
		for (i = 0; i < args->num_cliprects; i++) {
			ret = i915_emit_box(dev, &cliprects[i],
					    args->DR1, args->DR4);
			if (ret)
				goto err;

			if (gem_scheduler && ring->timestamp_execbuffer)
				ret = ring->timestamp_execbuffer(ring,
					exec_start, exec_len, flags, &timestamp);
			else 
				ret = ring->dispatch_execbuffer(ring,
					exec_start, exec_len, flags);
			if (ret)
				goto err;
		}
	} else {
		if (gem_scheduler && ring->timestamp_execbuffer)
			ret = ring->timestamp_execbuffer(ring,
				exec_start, exec_len, flags, &timestamp);
		else 
			ret = ring->dispatch_execbuffer(ring,
				exec_start, exec_len, flags);
		if (ret)
			goto err;
	}

	trace_i915_gem_ring_dispatch(ring, intel_ring_get_seqno(ring), flags);

	i915_gem_execbuffer_move_to_active(&eb->vmas, ring);
	i915_gem_execbuffer_retire_commands(dev, file, ring, batch_obj, timestamp);

err:
	eb_destroy(eb);

	mutex_unlock(&dev->struct_mutex);

pre_mutex_err:
	if (gem_sched_queued) {
		spin_lock(&dev_priv->scheduler.sched_lock);
		dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority]--;
		if (dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority] < 0)
		{
			dev_priv->scheduler.req_prty_cnts[file_priv->mm.sched_priority] = 0;
			dev_priv->scheduler.req_prty_fp[file_priv->mm.sched_priority] = 0;
		}
		spin_unlock(&dev_priv->scheduler.sched_lock);
	}
	kfree(cliprects);
	return ret;
}

/*
 * Legacy execbuffer just creates an exec2 list from the original exec object
 * list array and passes it to the real function.
 */
int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_execbuffer2 exec2;
	struct drm_i915_gem_exec_object *exec_list = NULL;
	struct drm_i915_gem_exec_object2 *exec2_list = NULL;
	int ret, i;

	if (args->buffer_count < 1) {
		DRM_DEBUG("execbuf with %d buffers\n", args->buffer_count);
		return -EINVAL;
	}

	/* Copy in the exec list from userland */
	exec_list = drm_malloc_ab(sizeof(*exec_list), args->buffer_count);
	exec2_list = drm_malloc_ab(sizeof(*exec2_list), args->buffer_count);
	if (exec_list == NULL || exec2_list == NULL) {
		DRM_DEBUG("Failed to allocate exec list for %d buffers\n",
			  args->buffer_count);
		drm_free_large(exec_list);
		drm_free_large(exec2_list);
		return -ENOMEM;
	}
	ret = copy_from_user(exec_list,
			     to_user_ptr(args->buffers_ptr),
			     sizeof(*exec_list) * args->buffer_count);
	if (ret != 0) {
		DRM_DEBUG("copy %d exec entries failed %d\n",
			  args->buffer_count, ret);
		drm_free_large(exec_list);
		drm_free_large(exec2_list);
		return -EFAULT;
	}

	for (i = 0; i < args->buffer_count; i++) {
		exec2_list[i].handle = exec_list[i].handle;
		exec2_list[i].relocation_count = exec_list[i].relocation_count;
		exec2_list[i].relocs_ptr = exec_list[i].relocs_ptr;
		exec2_list[i].alignment = exec_list[i].alignment;
		exec2_list[i].offset = exec_list[i].offset;
		if (INTEL_INFO(dev)->gen < 4)
			exec2_list[i].flags = EXEC_OBJECT_NEEDS_FENCE;
		else
			exec2_list[i].flags = 0;
	}

	exec2.buffers_ptr = args->buffers_ptr;
	exec2.buffer_count = args->buffer_count;
	exec2.batch_start_offset = args->batch_start_offset;
	exec2.batch_len = args->batch_len;
	exec2.DR1 = args->DR1;
	exec2.DR4 = args->DR4;
	exec2.num_cliprects = args->num_cliprects;
	exec2.cliprects_ptr = args->cliprects_ptr;
	exec2.flags = I915_EXEC_RENDER;
	i915_execbuffer2_set_context_id(exec2, 0);

	ret = i915_gem_do_execbuffer(dev, data, file, &exec2, exec2_list,
				     &dev_priv->gtt.base);
	if (!ret) {
		/* Copy the new buffer offsets back to the user's exec list. */
		for (i = 0; i < args->buffer_count; i++)
			exec_list[i].offset = exec2_list[i].offset;
		/* ... and back out to userspace */
		ret = copy_to_user(to_user_ptr(args->buffers_ptr),
				   exec_list,
				   sizeof(*exec_list) * args->buffer_count);
		if (ret) {
			ret = -EFAULT;
			DRM_DEBUG("failed to copy %d exec entries "
				  "back to user (%d)\n",
				  args->buffer_count, ret);
		}
	}

	drm_free_large(exec_list);
	drm_free_large(exec2_list);
	return ret;
}

int
i915_gem_execbuffer2(struct drm_device *dev, void *data,
		     struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer2 *args = data;
	struct drm_i915_gem_exec_object2 *exec2_list = NULL;
	int ret;

	if (args->buffer_count < 1 ||
			args->buffer_count > UINT_MAX / sizeof(*exec2_list)) {
		DRM_DEBUG("execbuf2 with %d buffers\n", args->buffer_count);
		return -EINVAL;
	}

	exec2_list = kmalloc(sizeof(*exec2_list)*args->buffer_count,
			     GFP_TEMPORARY | __GFP_NOWARN | __GFP_NORETRY);
	if (exec2_list == NULL)
		exec2_list = drm_malloc_ab(sizeof(*exec2_list),
					   args->buffer_count);
	if (exec2_list == NULL) {
		DRM_DEBUG("Failed to allocate exec list for %d buffers\n",
			  args->buffer_count);
		return -ENOMEM;
	}
	ret = copy_from_user(exec2_list,
			     to_user_ptr(args->buffers_ptr),
			     sizeof(*exec2_list) * args->buffer_count);
	if (ret != 0) {
		DRM_DEBUG("copy %d exec entries failed %d\n",
			  args->buffer_count, ret);
		drm_free_large(exec2_list);
		return -EFAULT;
	}

	ret = i915_gem_do_execbuffer(dev, data, file, args, exec2_list,
				     &dev_priv->gtt.base);
	if (!ret) {
		/* Copy the new buffer offsets back to the user's exec list. */
		ret = copy_to_user(to_user_ptr(args->buffers_ptr),
				   exec2_list,
				   sizeof(*exec2_list) * args->buffer_count);
		if (ret) {
			ret = -EFAULT;
			DRM_DEBUG("failed to copy %d exec entries "
				  "back to user (%d)\n",
				  args->buffer_count, ret);
		}
	}

	drm_free_large(exec2_list);
	return ret;
}
