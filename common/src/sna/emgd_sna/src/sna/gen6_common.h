/*
 * Copyright © 2011-2013 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifndef GEN6_COMMON_H
#define GEN6_COMMON_H

#include "sna.h"

#define NO_RING_SWITCH 0
#define PREFER_RENDER 0

static inline bool is_uncached(struct sna *sna,
			       struct kgem_bo *bo)
{
	return bo->scanout && !sna->kgem.has_wt;
}

inline static bool can_switch_to_blt(struct sna *sna,
				     struct kgem_bo *bo,
				     unsigned flags)
{
	if (sna->kgem.ring != KGEM_RENDER)
		return true;

	if (NO_RING_SWITCH)
		return false;

	if (!sna->kgem.has_semaphores)
		return false;

	if (flags & COPY_LAST)
		return true;

	if (bo && RQ_IS_BLT(bo->rq))
		return true;

	return kgem_ring_is_idle(&sna->kgem, KGEM_BLT);
}

inline static bool can_switch_to_render(struct sna *sna,
					struct kgem_bo *bo)
{
	if (sna->kgem.ring == KGEM_RENDER)
		return true;

	if (NO_RING_SWITCH)
		return false;

	if (!sna->kgem.has_semaphores)
		return false;

	if (bo && !RQ_IS_BLT(bo->rq) && !is_uncached(sna, bo))
		return true;

	return !kgem_ring_is_idle(&sna->kgem, KGEM_RENDER);
}

static inline bool untiled_tlb_miss(struct kgem_bo *bo)
{
	if (kgem_bo_is_render(bo))
		return false;

	return bo->tiling == I915_TILING_NONE && bo->pitch >= 4096;
}

static int prefer_blt_bo(struct sna *sna, struct kgem_bo *bo)
{
	if (bo->rq)
		return RQ_IS_BLT(bo->rq);

	if (sna->flags & SNA_POWERSAVE)
		return true;

	return bo->tiling == I915_TILING_NONE || is_uncached(sna, bo);
}

inline static bool force_blt_ring(struct sna *sna)
{
	if (sna->flags & SNA_POWERSAVE)
		return true;

	if (sna->kgem.mode == KGEM_RENDER)
		return false;

	if (sna->render_state.gt < 2)
		return true;

	return false;
}

inline static bool prefer_blt_ring(struct sna *sna,
				   struct kgem_bo *bo,
				   unsigned flags)
{
	assert(!force_blt_ring(sna));
	assert(!kgem_bo_is_render(bo));

	return can_switch_to_blt(sna, bo, flags);
}

inline static bool prefer_render_ring(struct sna *sna,
				      struct kgem_bo *bo)
{
	if (sna->flags & SNA_POWERSAVE)
		return false;

	if (sna->render_state.gt < 2)
		return false;

	return can_switch_to_render(sna, bo);
}

inline static bool
prefer_blt_composite(struct sna *sna, struct sna_composite_op *tmp)
{
	if (untiled_tlb_miss(tmp->dst.bo) ||
	    untiled_tlb_miss(tmp->src.bo))
		return true;

	if (force_blt_ring(sna))
		return true;

	if (kgem_bo_is_render(tmp->dst.bo) ||
	    kgem_bo_is_render(tmp->src.bo))
		return false;

	if (prefer_render_ring(sna, tmp->dst.bo))
		return false;

	if (!prefer_blt_ring(sna, tmp->dst.bo, 0))
		return false;

	return prefer_blt_bo(sna, tmp->dst.bo) || prefer_blt_bo(sna, tmp->src.bo);
}

static inline bool prefer_blt_fill(struct sna *sna,
				   struct kgem_bo *bo,
				   unsigned flags)
{
	if (PREFER_RENDER)
		return PREFER_RENDER < 0;

	if (flags & (FILL_POINTS | FILL_SPANS) &&
	    can_switch_to_blt(sna, bo, 0))
		return true;

	if (untiled_tlb_miss(bo))
		return true;

	if (force_blt_ring(sna))
		return true;

	if (kgem_bo_is_render(bo))
		return false;

	if (prefer_render_ring(sna, bo))
		return false;

	if (!prefer_blt_ring(sna, bo, 0))
		return false;

	return prefer_blt_bo(sna, bo);
}

void gen6_render_flush(struct sna *sna);
void gen6_render_retire(struct kgem *kgem);
void gen6_render_expire(struct kgem *kgem);
void gen6_render_context_switch(struct kgem *kgem, int new_mode);

#endif /* GEN6_COMMON_H */