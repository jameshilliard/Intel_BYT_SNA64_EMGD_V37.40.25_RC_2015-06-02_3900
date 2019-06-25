/*
 * Copyright © 2008-2010,2012-2014 Intel Corporation
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
 *    Zou Nan hai <nanhai.zou@intel.com>
 *    Xiang Hai hao<haihao.xiang@intel.com>
 *
 */

#include <linux/version.h>
#include "drmP.h"
#include "i915_reg.h"
#include "intel_bios.h"
#include "intel_ringbuffer.h"
#include "drm_emgd_private.h"
#include "i915_drm.h"
#include "i915_trace.h"
#include "intel_drv.h"

/*#define RINGBUFFER_PROPER_PIPE_CONTROL_FIXUP*/
int pipe_control_count = 0;

static inline int ring_space(struct intel_ring_buffer *ring)
{
	int space = (ring->head & HEAD_ADDR) - (ring->tail + I915_RING_FREE_SPACE);
	if (space < 0)
		space += ring->size;
	return space;
}

static int
gen2_render_ring_flush(struct intel_ring_buffer *ring,
                      u32      invalidate_domains,
                      u32      flush_domains)
{
	u32 cmd;
	int ret;

	cmd = MI_FLUSH;
	if (((invalidate_domains|flush_domains) & I915_GEM_DOMAIN_RENDER) == 0)
		cmd |= MI_NO_WRITE_FLUSH;

	if (invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
		cmd |= MI_READ_FLUSH;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static int
gen4_render_ring_flush(struct intel_ring_buffer *ring,
                      u32      invalidate_domains,
                      u32      flush_domains)
{
	struct drm_device *dev = ring->dev;
	u32 cmd;
	int ret;

	/*
	 * read/write caches:
	 *
	 * I915_GEM_DOMAIN_RENDER is always invalidated, but is
	 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
	 * also flushed at 2d versus 3d pipeline switches.
	 *
	 * read-only caches:
	 *
	 * I915_GEM_DOMAIN_SAMPLER is flushed on pre-965 if
	 * MI_READ_FLUSH is set, and is always flushed on 965.
	 *
	 * I915_GEM_DOMAIN_COMMAND may not exist?
	 *
	 * I915_GEM_DOMAIN_INSTRUCTION, which exists on 965, is
	 * invalidated when MI_EXE_FLUSH is set.
	 *
	 * I915_GEM_DOMAIN_VERTEX, which exists on 965, is
	 * invalidated with every MI_FLUSH.
	 *
	 * TLBs:
	 *
	 * On 965, TLBs associated with I915_GEM_DOMAIN_COMMAND
	 * and I915_GEM_DOMAIN_CPU in are invalidated at PTE write and
	 * I915_GEM_DOMAIN_RENDER and I915_GEM_DOMAIN_SAMPLER
	 * are flushed at any MI_FLUSH.
	 */

	cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
	if ((invalidate_domains|flush_domains) & I915_GEM_DOMAIN_RENDER)
		cmd &= ~MI_NO_WRITE_FLUSH;
	if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
		cmd |= MI_EXE_FLUSH;

	if (invalidate_domains & I915_GEM_DOMAIN_COMMAND &&
	    (IS_G4X(dev) || IS_GEN5(dev)))
		cmd |= MI_INVALIDATE_ISP;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

/**
 * Emits a PIPE_CONTROL with a non-zero post-sync operation, for
 * implementing two workarounds on gen6.  From section 1.4.7.1
 * "PIPE_CONTROL" of the Sandy Bridge PRM volume 2 part 1:
 *
 * [DevSNB-C+{W/A}] Before any depth stall flush (including those
 * produced by non-pipelined state commands), software needs to first
 * send a PIPE_CONTROL with no bits set except Post-Sync Operation !=
 * 0.
 *
 * [Dev-SNB{W/A}]: Before a PIPE_CONTROL with Write Cache Flush Enable
 * =1, a PIPE_CONTROL with any non-zero post-sync-op is required.
 *
 * And the workaround for these two requires this workaround first:
 *
 * [Dev-SNB{W/A}]: Pipe-control with CS-stall bit set must be sent
 * BEFORE the pipe-control with a post-sync op and no write-cache
 * flushes.
 *
 * And this last workaround is tricky because of the requirements on
 * that bit.  From section 1.4.7.2.3 "Stall" of the Sandy Bridge PRM
 * volume 2 part 1:
 *
 *     "1 of the following must also be set:
 *      - Render Target Cache Flush Enable ([12] of DW1)
 *      - Depth Cache Flush Enable ([0] of DW1)
 *      - Stall at Pixel Scoreboard ([1] of DW1)
 *      - Depth Stall ([13] of DW1)
 *      - Post-Sync Operation ([13] of DW1)
 *      - Notify Enable ([8] of DW1)"
 *
 * The cache flushes require the workaround flush that triggered this
 * one, so we can't use it.  Depth stall would trigger the same.
 * Post-sync nonzero is what triggered this second workaround, so we
 * can't use that one either.  Notify enable is IRQs, which aren't
 * really our business.  That leaves only stall at scoreboard.
 */
static int
intel_emit_post_sync_nonzero_flush(struct intel_ring_buffer *ring)
{
	u32 scratch_addr = ring->scratch.gtt_offset + 128;
	int ret;


	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(5));
	intel_ring_emit(ring, PIPE_CONTROL_CS_STALL |
			PIPE_CONTROL_STALL_AT_SCOREBOARD);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT); /* address */
	intel_ring_emit(ring, 0); /* low dword */
	intel_ring_emit(ring, 0); /* high dword */
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(5));
	intel_ring_emit(ring, PIPE_CONTROL_QW_WRITE);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT); /* address */
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static int
gen6_render_ring_flush(struct intel_ring_buffer *ring,
                         u32 invalidate_domains, u32 flush_domains)
{
	u32 flags = 0;
	u32 scratch_addr = ring->scratch.gtt_offset + 128;
	int ret;
	/* Force SNB workarounds for PIPE_CONTROL flushes */
	ret = intel_emit_post_sync_nonzero_flush(ring);
	if (ret)
		return ret;

	/* Just flush everything.  Experiments have shown that reducing the
	 * number of bits based on the write domains has little performance
	 * impact.
	 */
	if (flush_domains) {
		flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
		/*
		 * Ensure that any following seqno writes only happen
		 * when the render cache is indeed flushed.
		 */
		flags |= PIPE_CONTROL_CS_STALL;
	}
	if (invalidate_domains) {
		flags |= PIPE_CONTROL_TLB_INVALIDATE;
		flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;
		/*
		 * TLB invalidate requires a post-sync write.
		 */
		flags |= PIPE_CONTROL_QW_WRITE | PIPE_CONTROL_CS_STALL;
	}

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4));
	intel_ring_emit(ring, flags);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, 0);
	intel_ring_advance(ring);

	return 0;
}

static int
gen7_render_ring_cs_stall_wa(struct intel_ring_buffer *ring)
{
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4));
	intel_ring_emit(ring, PIPE_CONTROL_CS_STALL |
			PIPE_CONTROL_STALL_AT_SCOREBOARD);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, 0);
	intel_ring_advance(ring);

	return 0;
}


static int
gen7_render_ring_flush(struct intel_ring_buffer *ring,
		u32 invalidate_domains, u32 flush_domains)
{
	u32 flags = 0;
	u32 scratch_addr = ring->scratch.gtt_offset + 128;
	int ret;
   /*
	* Ensure that any following seqno writes only happen when the render
	* cache is indeed flushed.
	*
	* Workaround: 4th PIPE_CONTROL command (except the ones with only
	* read-cache invalidate bits set) must have the CS_STALL bit set. We
	* don't try to be clever and just set it unconditionally.
	*/
	    flags |= PIPE_CONTROL_CS_STALL;


	/* Just flush everything.  Experiments have shown that reducing the
	 * number of bits based on the write domains has little performance
	 * impact.
	 */
	if (flush_domains) {
		flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
	}
	if (invalidate_domains) {
		flags |= PIPE_CONTROL_TLB_INVALIDATE;
		flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;
		/*
		 *  TLB invalidate requires a post-sync write.
		 */
		flags |= PIPE_CONTROL_QW_WRITE;
		flags |= PIPE_CONTROL_GLOBAL_GTT_IVB;

        /* Workaround: we must issue a pipe_control with CS-stall bit
		 * set before a pipe_control command that has the state cache
		 * invalidate bit set. */
		gen7_render_ring_cs_stall_wa(ring);

	}

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4));
	intel_ring_emit(ring, flags);
	intel_ring_emit(ring, scratch_addr);
	intel_ring_emit(ring, 0);
	intel_ring_advance(ring);

	return 0;
}

int parsed_batchbuffer = 0;
int last_tail_render  = 0;
int last_tail_blitter = 0;
int last_tail_video   = 0;

typedef struct decode_opcode_lookup {
	u32 opcode;
	u32 opcode_mask;
	struct decode_opcode_lookup * table;
	int is_header;
	int is_reserved;
	int is_startbatch;
	int is_endbatch;
	int is_pipectrl;
	int is_userirq;
	u32 len_mask;
	int len_shift;
	int len_minus;
	int len_plus;
} decode_opcode_lookup_t;
/*
 **********************************************************
 *
 * VERY IMPORTANT NOTE ON THE FOLLOWING TABLES
 * EACH ENTRY HAS A OPCODE_MASK MEMBER. (2ND VARIABLE OF 
 * EACH ENTRY). ANY INSERTION OF A NEW ENTRY INTO ANY TABLE
 * MUST ENSURE THAT THE POSITION OF THE NEW ENTRY MUST
 * FOLLOW THE FOLLOWING SORTING RULE:
 *
 * --> ENTRY WITH OPCODE_MASKING WITH LARGEST NUMBER OF
 *     CONTIGUOUS MSB'S BE AT THE VERY TOP!
 *
 * *********************************************************
 */
decode_opcode_lookup_t decode_2d[] = {
	{/*All 2D instructions? */ 0x00000000, 0x00000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*End Marker           */ 0xFFFFFFFF, 0x00000000, NULL,       0,0,0,0,0,0,  0,0,0,0}
};
decode_opcode_lookup_t decode_mi[] = {
	{/*MI-NOOP              */ 0x00000000, 0xFF800000, NULL,       0,0,0,0,0,0,  0x00000000,0,0,1},
	{/*Ring/Batch Start cmd */ 0x18800000, 0xFF800000, NULL,       0,0,1,0,0,0,  0x00000000,0,0,2},
	{/*Ring/Batch End cmd   */ 0x05000000, 0xFF800000, NULL,       0,0,0,1,0,0,  0x000000FF,0,0,2},
	{/*Cond Batch End cmd   */ 0x1b000000, 0xFF800000, NULL,       0,0,0,1,0,0,  0x000000FF,0,0,2},
	{/*MI User Interrupt    */ 0x01000000, 0xFF800000, NULL,       0,0,0,0,0,0/*1*/,  0x00000000,0,0,1},
	{/*Single DWORD         */ 0x00000000, 0xF8000000, NULL,       0,0,0,0,0,0,  0x00000000,0,0,1},
	{/*Two DWORD            */ 0x08000000, 0xF8000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*Store Data cmds      */ 0x10000000, 0xF8000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*End Marker           */ 0xFFFFFFFF, 0x00000000, NULL,       0,0,0,0,0,0,  0,0,0,0}
};
decode_opcode_lookup_t decode_3d[] = {
	{/*All 3D instructions? */ 0x00000000, 0x00000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*End Marker           */ 0xFFFFFFFF, 0x00000000, NULL,       0,0,0,0,0,0,  0,0,0,0}
};

decode_opcode_lookup_t decode_hdr2[] = {
	{/*Common Pipelined     */ 0x60000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*Common Non-Pipelined */ 0x61000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*Reserved instructions*/ 0x62000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x63000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x64000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x65000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x66000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x67000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Single DWORD cmds    */ 0x68000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0,0,0,1},
	{/*Single DWORD cmds    */ 0x69000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0,0,0,1},
	{/*Reserved instructions*/ 0x6A000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x6B000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x6C000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x6D000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x6E000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x6F000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Media State cmds     */ 0x70000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*Media Object cmds    */ 0x71000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x0000FFFF,0,0,2},
	{/*Media Object cmds    */ 0x72000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x0000FFFF,0,0,2},
	{/*Reserved instructions*/ 0x73000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x74000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x75000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x76000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x77000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*3D State Pipelined   */ 0x78000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*3D State NonPipelined*/ 0x79000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*PIPE_CONTROL         */ 0x7A000000, 0xFF000000, NULL,       0,0,0,0,1,0,  0x000000FF,0,0,2},
	{/*3D Primitive         */ 0x7B000000, 0xFF000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*Reserved instructions*/ 0x7C000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x7D000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x7E000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x7F000000, 0xFF000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*End Marker           */ 0xFFFFFFFF, 0x00000000, NULL,       0,0,0,0,0,0,  0,0,0,0}
};

decode_opcode_lookup_t decode_hdr0[] = {
	{/*MI instructions*/       0x00000000, 0xE0000000, decode_mi,  1,0,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x20000000, 0xE0000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*2D  instructions*/      0x40000000, 0xE0000000, NULL,       0,0,0,0,0,0,  0x000000FF,0,0,2},
	{/*MoreCmd1 instructions*/ 0x60000000, 0xE0000000, decode_hdr2,1,0,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0x80000000, 0xE0000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0xA0000000, 0xE0000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0xC0000000, 0xE0000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*Reserved instructions*/ 0xE0000000, 0xE0000000, NULL,       0,1,0,0,0,0,  0,0,0,0},
	{/*End Marker           */ 0xFFFFFFFF, 0x00000000, NULL,       0,0,0,0,0,0,  0,0,0,0}
};

/* for decode_hdr, mask the instruction with 0xE0000000 first */

#define MAX_BATCH_PAGES 16

int intel_ringbuffer_decode_pipecontrol_fixup(
		struct intel_ring_buffer *ring, u32 * data_map,
		decode_opcode_lookup_t * decoder_table, int in_batch,
		int old_tail, int new_tail, int pipe_ctrl_count)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;

	u32 * map;
	u32 cmd, fixup_cmd;
	u32 last_nonzero_cmd_inbatch = 0;
	int zeroes_inbatch = 0;
	int num_dwords;
	int curr_dword = 0;
	int payload_size;
	
	decode_opcode_lookup_t * decode;
	int entry_count;
	
	decode_opcode_lookup_t * decoder_table2;
	decode_opcode_lookup_t * decode2;
	int entry_count2;

	u32 batch_offset;
	u32 * batch_map;
 	int remap_count = 0;
	
	EMGD_DEBUG("----------------> Gonna write tail! -->");
	if(new_tail < old_tail) {
              old_tail = 0;
        }
	if(data_map){
		map = data_map;
	} else {
		map = ring->virtual_start + old_tail;
	}
	num_dwords = in_batch? 1: ((new_tail - old_tail) / 4);

/* #define VERBOSE_CMD_INSTRUCTION_PARSING */
#ifdef VERBOSE_CMD_INSTRUCTION_PARSING
	EMGD_ERROR("       (( NUM DWORDS = %d ))!!", num_dwords);
#else
	EMGD_DEBUG("       (( NUM DWORDS = %d ))!!", num_dwords);
#endif
	while(num_dwords>0) {
 
		if(in_batch) {

			if( (curr_dword>=MAX_BATCH_PAGES*1024) || (remap_count == 0) ){ 
				
				if(remap_count > 0 ){
				   iounmap(map); /*Unmap the old batch buffer*/
				}

				/*Calculate the batch_offset used in remapping batch buffer*/
				batch_offset  = *data_map + PAGE_SIZE*MAX_BATCH_PAGES*remap_count;
				/*Map the new batch buffer*/
				batch_map = ioremap(dev_priv->gtt.mappable_base +
							batch_offset,
							PAGE_SIZE*MAX_BATCH_PAGES);
				if(batch_map){
					   map = batch_map;
					   /*Setting curr_dword to be at correct location in order to prevent parsing incorrect instruction*/
					   curr_dword = curr_dword % (MAX_BATCH_PAGES*1024);
				   	   remap_count++;
				}
				else{
					   EMGD_DEBUG("Can't map batch-start ptr!");
				  	   return pipe_ctrl_count;
				}
			}
		}

		cmd = *(map+curr_dword);
#ifdef VERBOSE_CMD_INSTRUCTION_PARSING
		EMGD_ERROR("Step1 - NEXT CMD = 0x%08x", cmd);
#else
		EMGD_DEBUG("Step1 - NEXT CMD = 0x%08x", cmd);
#endif
		entry_count = 0;
		payload_size = 0;
		while (!payload_size && decoder_table[entry_count].opcode != 0xFFFFFFFF) {
			/*EMGD_DEBUG("Step2");*/
			if((cmd & decoder_table[entry_count].opcode_mask) ==
				decoder_table[entry_count].opcode){
				decode = &(decoder_table[entry_count]);
				if(decode->is_reserved){
					EMGD_ERROR("BAIL! Stumbled Reserved instruction! = 0x%08x", cmd);
					return pipe_ctrl_count;
				} else if(decode->is_header){
					decoder_table2 = decode->table;
					entry_count2 = 0;
					if(!decoder_table2){
						EMGD_ERROR("BAIL! 2nd Level Header Decoder Table NULL! = 0x%08x", cmd);
						return pipe_ctrl_count;
					}
					while (!payload_size && decoder_table2[entry_count2].opcode != 0xFFFFFFFF) {
						/*EMGD_DEBUG("Step3");*/
						if ((cmd & decoder_table2[entry_count2].opcode_mask) ==
							decoder_table2[entry_count2].opcode) {
							decode2 = &(decoder_table2[entry_count2]);
							if (decode2->is_reserved) {
								EMGD_ERROR("BAIL! Stumbled Reserved instruction! = 0x%08x", cmd);
								return pipe_ctrl_count;
							} else if (decode2->is_header) {
								EMGD_ERROR("BAIL! Stumbled 2nd HEADER instruction! = 0x%08x", cmd);
								return pipe_ctrl_count;
							} else if (decode2->is_pipectrl) {
								++pipe_ctrl_count;
								if (pipe_ctrl_count == 4) {
									fixup_cmd = *(map+curr_dword+1);
									if(!(fixup_cmd & BIT20)){
										fixup_cmd |= (BIT20 /*CS_STALL*/ /* BIT12 | PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH*/ | BIT1 /*STALL_PIXEL_SCOREBOARD*/);
										/*
										flags |= STALL_PIXEL_SCOREBOARD BIT1;
										flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
										flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
										flags |= STALL_DEPTH BIT13;
										flags |= POST_SYNC_OP;
										*/
										*(map+curr_dword+1) = fixup_cmd;
									} else if (!(fixup_cmd & (BIT0 | BIT1 | BIT12 | BIT13 | BIT14 | BIT15) )){
										fixup_cmd |= (0 /* BIT12 | PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH*/ | BIT1 /*STALL_PIXEL_SCOREBOARD*/);
										*(map+curr_dword+1) = fixup_cmd;
									}
									pipe_ctrl_count = 0;
								} else {
									EMGD_DEBUG("Found pipe_ctrl2");
								}
								payload_size = (((cmd & decode2->len_mask) >> decode2->len_shift) - decode2->len_minus + decode2->len_plus);
							} else if (decode2->is_startbatch) {
								batch_offset = *(map+curr_dword+1);
								pipe_ctrl_count = intel_ringbuffer_decode_pipecontrol_fixup(
										ring, &batch_offset, decode_hdr0, 1,
										0, 0, pipe_ctrl_count);
								/*batch_map = io_mapping_map_wc(dev_priv->mm.gtt_mapping, batch_offset);*/

								/* FIXME: For Gen7, use io_mappping for VLV but need to get gem_bo
								 * from gtt_offset and use kmap for IVB since that has LLC support
								 */

								/*io_mapping_unmap(batch_map);*/
								payload_size = (((cmd & decode2->len_mask) >> decode2->len_shift) - decode2->len_minus + decode2->len_plus);
							} else if (decode2->is_endbatch) {
								if (!in_batch) {
									EMGD_ERROR("BAIL! END BATCH not in BATCH!");
								}
#ifdef VERBOSE_CMD_INSTRUCTION_PARSING
								EMGD_ERROR("EXIT BATCH <-<-<-<- !");
#endif
								iounmap(map);
	
								return pipe_ctrl_count;
							} else if (decode2->is_userirq){
								/* Not sure yet, but based on current debugging, anticipate
								 * yet another runtime RB tweak for MI_USER_INTERRUPT.
								 * For now, this is merely a placeholder
								 */
								/*EMGD_ERROR("BLANKOUT! MI_USER_INTERRUPT-2!");
								dump_stack();
								cmd = 0;
								*(map+curr_dword) = cmd;*/
								payload_size = 1;
							} else {
								payload_size = (((cmd & decode2->len_mask) >> decode2->len_shift) - decode2->len_minus + decode2->len_plus);
								if(in_batch) {
									if(cmd){
										last_nonzero_cmd_inbatch = cmd;
										zeroes_inbatch = 0;
									} else {
										++zeroes_inbatch;
									}
								}
								if(zeroes_inbatch>200) {
									EMGD_ERROR("Trending lost with zeroes! Last nonzero cmd = 0x%08x", last_nonzero_cmd_inbatch);
								}
								/*EMGD_DEBUG("Stepy");*/
							}
						} else {
							++entry_count2;
						}
					}
					if(!payload_size){
						EMGD_ERROR("BAIL! Unknown instruction2! = 0x%08x", cmd);
						return pipe_ctrl_count;
					}
					
				} else if(decode->is_pipectrl){
					++pipe_ctrl_count;
					if(pipe_ctrl_count == 4){
						fixup_cmd = *(map+curr_dword+1);
						if(!(fixup_cmd & BIT20)){
							fixup_cmd |= (BIT20 /*CS_STALL*/ /* BIT12 | PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH*/ | BIT1 /*STALL_PIXEL_SCOREBOARD*/);
							/*
							flags |= STALL_PIXEL_SCOREBOARD BIT1;
							flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
							flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
							flags |= STALL_DEPTH BIT13;
							flags |= POST_SYNC_OP;
							*/
							*(map+curr_dword+1) = fixup_cmd;
						} else if (!(fixup_cmd & (BIT0 | BIT1 | BIT12 | BIT13 | BIT14 | BIT15) )){
							fixup_cmd |= (0 /* BIT12 | PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH*/ | BIT1 /*STALL_PIXEL_SCOREBOARD*/);
							*(map+curr_dword+1) = fixup_cmd;
						}
						pipe_ctrl_count = 0;
					} else {
						EMGD_DEBUG("Found pipe_ctrl1");
					}
					payload_size = (((cmd & decode->len_mask) >> decode->len_shift) - decode->len_minus + decode->len_plus);
				} else if(decode->is_startbatch){
					batch_offset = *(map+curr_dword+1);

					pipe_ctrl_count = intel_ringbuffer_decode_pipecontrol_fixup(
							ring, &batch_offset, decode_hdr0, 1,
							0, 0, pipe_ctrl_count);
					/*batch_map = io_mapping_map_wc(dev_priv->mm.gtt_mapping, batch_offset);*/

					/* FIXME: For Gen7, use io_mappping for VLV but need to get gem_bo
					 * from gtt_offset and use kmap for IVB since that has LLC support
					 */
					
					/*io_mapping_unmap(batch_map);*/
						
					payload_size = (((cmd & decode->len_mask) >> decode->len_shift) - decode->len_minus + decode->len_plus);
				} else if(decode->is_endbatch){
					if (!in_batch) {
						EMGD_ERROR("BAIL! END BATCH not in BATCH!");
					}
#ifdef VERBOSE_CMD_INSTRUCTION_PARSING
					EMGD_ERROR("EXIT BATCH <-<-<-<- !");
#endif
					iounmap(map);

					return pipe_ctrl_count;
				} else if (decode->is_userirq){
					/* Not sure yet, but based on current debugging, anticipate
					 * yet another runtime RB tweak for MI_USER_INTERRUPT.
					 * For now, this is merely a placeholder
					 */
					/*EMGD_ERROR("BLANKOUT! MI_USER_INTERRUPT-2!");
					dump_stack();
					cmd = 0;
					*(map+curr_dword) = cmd;*/
					payload_size = 1;
				} else {
					payload_size = (((cmd & decode->len_mask) >> decode->len_shift) - decode->len_minus + decode->len_plus);
					if(in_batch) {
						if(cmd){
							last_nonzero_cmd_inbatch = cmd;
							zeroes_inbatch = 0;
						} else {
							++zeroes_inbatch;
						}
					}
					if(zeroes_inbatch>200) {
						EMGD_ERROR("Trending lost with zeroes! Last nonzero cmd = 0x%08x", last_nonzero_cmd_inbatch);
					}
					/*EMGD_DEBUG("Stepx");*/
				}
			} else {
				++entry_count;
			}
		}
		if(!payload_size){
			EMGD_ERROR("BAIL! Unknown instruction1! = 0x%08x", cmd);
			return pipe_ctrl_count;
		}
		if(in_batch){
			curr_dword += payload_size;
			continue;
		}
		/*EMGD_ERROR("Step4");*/
		if(num_dwords < payload_size) {
			EMGD_ERROR("!!!!!!!!!!!!! Payload size mismatch !!!!!!!!!!!!!!! - last cmd = 0x%08x", cmd);
			EMGD_ERROR("!!!!!!!!!!!!! Payload size mismatch !!!!!!!!!!!!!!! - payload  =     %d", payload_size);
		}
		num_dwords -= payload_size;
		curr_dword += payload_size;
		EMGD_DEBUG("Step5 newcmd payload = %d, num_dwords = %d", payload_size, num_dwords);
	}

	if (num_dwords < 0) {
		EMGD_ERROR("ERROR num_dwords ERROR = %d", num_dwords);
	}

	return pipe_ctrl_count;

}
static void ring_write_tail(struct intel_ring_buffer *ring,
			    u32 value)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
#ifdef  RINGBUFFER_PROPER_PIPE_CONTROL_FIXUP
	int last_tail;
#endif

#ifdef  RINGBUFFER_PROPER_PIPE_CONTROL_FIXUP
	if (IS_VALLEYVIEW(ring->dev)) {
		EMGD_DEBUG("----------------> Gonna write tail! -->");
		switch (ring->id){
			case RCS:
				EMGD_DEBUG("                    ---> Render Ring ---> 0x%08x", value);
				last_tail = last_tail_render;
				last_tail_render = value;
				break;
			case VCS:
				EMGD_DEBUG("                    ---> Video  Ring ---> 0x%08x", value);
				last_tail = last_tail_video;
				last_tail_video = value;
				break;
			case BCS:
				EMGD_DEBUG("                    ---> Bltter Ring ---> 0x%08x", value);
				last_tail = last_tail_blitter;
				last_tail_blitter = value;
				break;
			default:
				EMGD_DEBUG("                    ---> unknown ring!?");
				last_tail = last_tail_render;
				last_tail_render = value;
				break;
		}
		if (1) /*{ ring->id == RING_RENDER)*/ {
			/*dump_stack();*/
			EMGD_DEBUG("pipe_ctrl_count before fixup = %d", pipe_control_count);
			pipe_control_count = intel_ringbuffer_decode_pipecontrol_fixup(
				ring, NULL, decode_hdr0, 0,
				last_tail, value, pipe_control_count);
			EMGD_DEBUG("pipe_ctrl_count after fixup  = %d", pipe_control_count);
		}
	}
#endif

	I915_WRITE_TAIL(ring, value);
}

u32 intel_ring_get_active_head(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 acthd_reg = INTEL_INFO(ring->dev)->gen >= 4 ?
			RING_ACTHD(ring->mmio_base) : ACTHD;

	return I915_READ(acthd_reg);
}

static void ring_setup_phys_status_page(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	u32 addr;

	addr = dev_priv->status_page_dmah->busaddr;
	if (INTEL_INFO(ring->dev)->gen >= 4)
		addr |= (dev_priv->status_page_dmah->busaddr >> 28) & 0xf0;
	I915_WRITE(HWS_PGA, addr);
}

static int init_ring_common(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj = ring->obj;
	int ret = 0;
	u32 head;

	if (HAS_FORCE_WAKE(dev))
		gen6_gt_force_wake_get(dev_priv);

	if (I915_NEED_GFX_HWS(dev))
		intel_ring_setup_status_page(ring);
	else
		ring_setup_phys_status_page(ring);

	/* Stop the ring if it's running. */
	I915_WRITE_CTL(ring, 0);
	I915_WRITE_HEAD(ring, 0);
	ring->write_tail(ring, 0);

	head = I915_READ_HEAD(ring) & HEAD_ADDR;

	/* G45 ring initialization fails to reset head to zero */
	if (head != 0) {
		DRM_DEBUG_KMS("%s head not reset to zero "
			      "ctl %08x head %08x tail %08x start %08x\n",
			      ring->name,
			      I915_READ_CTL(ring),
			      I915_READ_HEAD(ring),
			      I915_READ_TAIL(ring),
			      I915_READ_START(ring));

		I915_WRITE_HEAD(ring, 0);

		if (I915_READ_HEAD(ring) & HEAD_ADDR) {
			DRM_ERROR("failed to set %s head to zero "
				  "ctl %08x head %08x tail %08x start %08x\n",
				  ring->name,
				  I915_READ_CTL(ring),
				  I915_READ_HEAD(ring),
				  I915_READ_TAIL(ring),
				  I915_READ_START(ring));
		}
	}

	/* Initialize the ring. This must happen _after_ we've cleared the ring
	 * registers with the above sequence (the readback of the HEAD registers
	 * also enforces ordering), otherwise the hw might lose the new ring
	 * register values. */
	I915_WRITE_START(ring, i915_gem_obj_ggtt_offset(obj));
	I915_WRITE_CTL(ring,
			((ring->size - PAGE_SIZE) & RING_NR_PAGES)
			| RING_VALID);

	/* If the head is still not zero, the ring is dead */
	if (wait_for((I915_READ_CTL(ring) & RING_VALID) != 0 &&
				I915_READ_START(ring) == i915_gem_obj_ggtt_offset(obj) &&
				(I915_READ_HEAD(ring) & HEAD_ADDR) == 0, 50)) {
		DRM_ERROR("%s initialization failed "
				"ctl %08x head %08x tail %08x start %08x\n",
				ring->name,
				I915_READ_CTL(ring),
				I915_READ_HEAD(ring),
				I915_READ_TAIL(ring),
				I915_READ_START(ring));
		ret = -EIO;
		goto out;
	}

	if (!drm_core_check_feature(ring->dev, DRIVER_MODESET))
		i915_kernel_lost_context(ring->dev);
	else {
		ring->head = I915_READ_HEAD(ring);
		ring->tail = I915_READ_TAIL(ring) & TAIL_ADDR;
		ring->space = ring_space(ring);
		ring->last_retired_head = -1;
	}

	if (gem_scheduler) {
		/* For GEM scheduler, we need timestamp gem buffers */
		ring->timestamp_obj = i915_gem_alloc_object(ring->dev, PAGE_SIZE);
		if (ring->timestamp_obj) {
			if (HAS_LLC(dev))
				i915_gem_object_set_cache_level(ring->timestamp_obj, I915_CACHE_LLC);

			/* This Timestamp GEM buffer has to be pinned thru the life of the ring */
			if (WARN_ON(i915_gem_obj_ggtt_pin(ring->timestamp_obj, PAGE_SIZE, true, false))) {
				drm_gem_object_unreference_unlocked(&ring->timestamp_obj->base);
				ring->timestamp_obj = NULL;
				gem_scheduler = 0;
				DRM_ERROR("Ring disabling GEM_Scheduler - gem pin failed\n");
			} else {
				/* We also need a kernel virt ptr to this Timestamp GEM buffer*/
				if (!HAS_LLC(dev)) {
					ring->timestamp_virt = ioremap(
							dev_priv->gtt.mappable_base +
							i915_gem_obj_ggtt_offset(ring->timestamp_obj), PAGE_SIZE);
				} else {
					ring->timestamp_virt =  kmap(sg_page(ring->timestamp_obj->pages->sgl));
				}
				if (!ring->timestamp_virt) {
					gem_scheduler = 0;
					DRM_ERROR("Ring disabling GEM_Scheduler - gem alloc failed\n");
				}
			}
		} else {
			gem_scheduler = 0;
			DRM_ERROR("Ring disabling GEM_Scheduler - gem alloc failed\n");
		}
	}

out:
	if (HAS_FORCE_WAKE(dev))
		gen6_gt_force_wake_put(dev_priv);

	return ret;
}

static int
init_pipe_control(struct intel_ring_buffer *ring)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int ret;

	if (ring->scratch.obj)
		return 0;

	ring->scratch.obj = i915_gem_alloc_object(ring->dev, 4096);
	if (ring->scratch.obj == NULL) {
		DRM_ERROR("Failed to allocate seqno page\n");
		ret = -ENOMEM;
		goto err;
	}

	dev = ring->scratch.obj->base.dev;
	dev_priv = dev->dev_private;

	if ((INTEL_INFO(dev)->gen >= 6) && !IS_VALLEYVIEW(dev))
		i915_gem_object_set_cache_level(ring->scratch.obj, I915_CACHE_LLC);

	ret = i915_gem_obj_ggtt_pin(ring->scratch.obj, 4096, true, false);
	if (ret)
		goto err_unref;

	ring->scratch.gtt_offset = i915_gem_obj_ggtt_offset(ring->scratch.obj);
	if (IS_VALLEYVIEW(dev)) {
		ring->scratch.cpu_page = ioremap(
					   /* dev_priv->context->device_context.fb_adr + */
					   dev_priv->gtt.mappable_base +
				       i915_gem_obj_ggtt_offset(ring->scratch.obj),
				       PAGE_SIZE);
	} else {
		ring->scratch.cpu_page = kmap(sg_page(ring->scratch.obj->pages->sgl));
	}

	if (ring->scratch.cpu_page == NULL) {
		ret = -ENOMEM;
		goto err_unpin;
	}

	DRM_DEBUG_DRIVER("%s pipe control offset: 0x%08x\n",
			ring->name, ring->scratch.gtt_offset);
	return 0;

err_unpin:
	i915_gem_object_unpin(ring->scratch.obj);
err_unref:
	drm_gem_object_unreference(&ring->scratch.obj->base);
err:
	return ret;
}

static int init_render_ring(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = init_ring_common(ring);

	if (INTEL_INFO(dev)->gen > 3)
		I915_WRITE(MI_MODE, _MASKED_BIT_ENABLE(VS_TIMER_DISPATCH));

	/* We need to disable the AsyncFlip performance optimisations in order
	 * to use MI_WAIT_FOR_EVENT within the CS. It should already be
	 * programmed to '1' on all products.
	 *
	 * WaDisableAsyncFlipPerfMode:snb,ivb,hsw,vlv
	 */
	if (INTEL_INFO(dev)->gen >= 6)
		I915_WRITE(MI_MODE, _MASKED_BIT_ENABLE(ASYNC_FLIP_PERF_DISABLE));

	/* Required for the hardware to program scanline values for waiting */
	if (INTEL_INFO(dev)->gen == 6)
		I915_WRITE(GFX_MODE,
				_MASKED_BIT_ENABLE(GFX_TLB_INVALIDATE_ALWAYS));

	if (IS_GEN7(dev)) {
		I915_WRITE(GFX_MODE_GEN7,
				_MASKED_BIT_DISABLE(GFX_TLB_INVALIDATE_ALWAYS) |
				_MASKED_BIT_ENABLE(GFX_REPLAY_MODE));
		if (IS_VALLEYVIEW(dev))
			I915_WRITE(MI_MODE, I915_READ(MI_MODE) |
					_MASKED_BIT_ENABLE(MI_FLUSH_ENABLE));
	}


	if (INTEL_INFO(dev)->gen >= 5) {
		ret = init_pipe_control(ring);
		if (ret)
			return ret;
	}

	if (IS_GEN6(dev)) {
		/* From the Sandybridge PRM, volume 1 part 3, page 24:
		 * "If this bit is set, STCunit will have LRA as replacement
		 *  policy. [...] This bit must be reset.  LRA replacement
		 *  policy is not supported."
		 */
		I915_WRITE(CACHE_MODE_0,
				_MASKED_BIT_DISABLE(CM0_STC_EVICT_DISABLE_LRA_SNB));

		/* This is not explicitly set for GEN6, so read the register.
		 * see intel_ring_mi_set_context() for why we care.
		 * TODO: consider explicitly setting the bit for GEN5
		 */
		ring->itlb_before_ctx_switch =
			!!(I915_READ(GFX_MODE) & GFX_TLB_INVALIDATE_ALWAYS);
	}

	if (INTEL_INFO(dev)->gen >= 6)
		I915_WRITE(INSTPM, _MASKED_BIT_ENABLE(INSTPM_FORCE_ORDERING));

	if (HAS_L3_DPF(dev))
		I915_WRITE_IMR(ring, ~GT_PARITY_ERROR(dev));

	return ret;
}

static void render_ring_cleanup(struct intel_ring_buffer *ring)
{
	struct drm_device *dev;

	if (ring->scratch.obj == NULL)
		return;

	dev = ring->scratch.obj->base.dev;
	if (IS_VALLEYVIEW(dev))
		iounmap(ring->scratch.cpu_page);
	else
		kunmap(sg_page(ring->scratch.obj->pages->sgl));

	i915_gem_object_unpin(ring->scratch.obj);

	drm_gem_object_unreference(&ring->scratch.obj->base);
	ring->scratch.obj = NULL;
}

static void
update_mboxes(struct intel_ring_buffer *ring,
	    u32 mmio_offset)
{
/* NB: In order to be able to do semaphore MBOX updates for varying number
 * of rings, it's easiest if we round up each individual update to a
 * multiple of 2 (since ring updates must always be a multiple of 2)
 * even though the actual update only requires 3 dwords.
 */
#define MBOX_UPDATE_DWORDS 4
	intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
	intel_ring_emit(ring, mmio_offset);
	intel_ring_emit(ring, ring->outstanding_lazy_seqno);
	intel_ring_emit(ring, MI_NOOP);
}

/**
 * gen6_add_request - Update the semaphore mailbox registers
 * 
 * @ring - ring that is adding a request
 * @seqno - return seqno stuck into the ring
 *
 * Update the mailbox registers in the *other* rings with the current seqno.
 * This acts like a signal in the canonical semaphore.
 */
static int
gen6_add_request(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_ring_buffer *useless;
	int i, ret;
	u32 mbox_reg = 0;

	ret = intel_ring_begin(ring, ((I915_NUM_RINGS-1) *
				      MBOX_UPDATE_DWORDS) +
				      4);
	if (ret)
		return ret;
#undef MBOX_UPDATE_DWORDS

	for_each_ring(useless, dev_priv, i) {
		mbox_reg = ring->signal_mbox[i];
		if (mbox_reg != GEN6_NOSYNC)
			update_mboxes(ring, mbox_reg);
	}

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, ring->outstanding_lazy_seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	return 0;
}

static inline bool i915_gem_has_seqno_wrapped(struct drm_device *dev,
					      u32 seqno)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	return dev_priv->last_seqno < seqno;
}

/**
 * intel_ring_sync - sync the waiter to the signaller on seqno
 *
 * @waiter - ring that is waiting
 * @signaller - ring which has, or will signal
 * @seqno - seqno which the waiter will block on
 */
static int
gen6_ring_sync(struct intel_ring_buffer *waiter,
              struct intel_ring_buffer *signaller,
              u32 seqno)
{
	int ret;
	u32 dw1 = MI_SEMAPHORE_MBOX |
		  MI_SEMAPHORE_COMPARE |
		  MI_SEMAPHORE_REGISTER;

	/* Throughout all of the GEM code, seqno passed implies our current
	 * seqno is >= the last seqno executed. However for hardware the
	 * comparison is strictly greater than.
	 */
	seqno -= 1;

	WARN_ON(signaller->semaphore_register[waiter->id] ==
		MI_SEMAPHORE_SYNC_INVALID);

	ret = intel_ring_begin(waiter, 4);
	if (ret)
		return ret;

	/* If seqno wrap happened, omit the wait with no-ops */
	if (likely(!i915_gem_has_seqno_wrapped(waiter->dev, seqno))) {
		intel_ring_emit(waiter,
				dw1 |
				signaller->semaphore_register[waiter->id]);
		intel_ring_emit(waiter, seqno);
		intel_ring_emit(waiter, 0);
		intel_ring_emit(waiter, MI_NOOP);
	} else {
		intel_ring_emit(waiter, MI_NOOP);
		intel_ring_emit(waiter, MI_NOOP);
		intel_ring_emit(waiter, MI_NOOP);
		intel_ring_emit(waiter, MI_NOOP);
	}
	intel_ring_advance(waiter);

	return 0;
}

#define PIPE_CONTROL_FLUSH(ring__, addr__)					\
do {									\
	intel_ring_emit(ring__, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |		\
		 PIPE_CONTROL_DEPTH_STALL);				\
	intel_ring_emit(ring__, (addr__) | PIPE_CONTROL_GLOBAL_GTT);			\
	intel_ring_emit(ring__, 0);							\
	intel_ring_emit(ring__, 0);							\
} while (0)

static int
pc_render_add_request(struct intel_ring_buffer *ring)
{
	u32 scratch_addr = ring->scratch.gtt_offset + 128;
	int ret;

	/* For Ironlake, MI_USER_INTERRUPT was deprecated and apparently
	 * incoherent with writes to memory, i.e. completely fubar,
	 * so we need to use PIPE_NOTIFY instead.
	 *
	 * However, we also need to workaround the qword write
	 * incoherence by flushing the 6 PIPE_NOTIFY buffers out to
	 * memory before requesting an interrupt.
	 */
	ret = intel_ring_begin(ring, 32);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |
			PIPE_CONTROL_WRITE_FLUSH |
			PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE);
	intel_ring_emit(ring, ring->scratch.gtt_offset | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, ring->outstanding_lazy_seqno);
	intel_ring_emit(ring, 0);
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128; /* write to separate cachelines */
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |
			PIPE_CONTROL_WRITE_FLUSH |
			PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE |
			PIPE_CONTROL_NOTIFY);
	intel_ring_emit(ring, ring->scratch.gtt_offset | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, ring->outstanding_lazy_seqno);
	intel_ring_emit(ring, 0);
	intel_ring_advance(ring);

	return 0;
}

static u32
gen6_ring_get_seqno(struct intel_ring_buffer *ring, bool lazy_coherency)
{
	/* Workaround to force correct ordering between irq and seqno writes on
	 * ivb (and maybe also on snb) by reading from a CS register (like
	 * ACTHD) before reading the status page. */
	if (!lazy_coherency)
		intel_ring_get_active_head(ring);
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static u32
ring_get_seqno(struct intel_ring_buffer *ring, bool lazy_coherency)
{
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static void
ring_set_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	intel_write_status_page(ring, I915_GEM_HWS_INDEX, seqno);
}

	static u32
pc_render_get_seqno(struct intel_ring_buffer *ring, bool lazy_coherency)
{
	return ring->scratch.cpu_page[0];
}

	static void
pc_render_set_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	ring->scratch.cpu_page[0] = seqno;
}

	static bool
gen5_ring_get_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	if (!dev->irq_enabled)
		return false;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (ring->irq_refcount.gt++ == 0) {
		dev_priv->gt_irq_mask &= ~ring->irq_enable_mask;
		I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
		POSTING_READ(GTIMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	return true;
}

	static void
gen5_ring_put_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (--ring->irq_refcount.gt == 0) {
		dev_priv->gt_irq_mask |= ring->irq_enable_mask;
		I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
		POSTING_READ(GTIMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);
}

	static bool
i9xx_ring_get_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	if (!dev->irq_enabled)
		return false;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (ring->irq_refcount.gt++ == 0) {
		dev_priv->irq_mask &= ~ring->irq_enable_mask;
		I915_WRITE(IMR, dev_priv->irq_mask);
		POSTING_READ(IMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	return true;
}

static void
i9xx_ring_put_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (--ring->irq_refcount.gt == 0) {
		dev_priv->irq_mask |= ring->irq_enable_mask;
		I915_WRITE(IMR, dev_priv->irq_mask);
		POSTING_READ(IMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);
}

void intel_ring_setup_status_page(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 mmio = 0;

	/* The ring status page addresses are no longer next to the rest of
	 * the ring registers as of gen7.
	 */
	if (IS_GEN7(dev)) {
		switch (ring->id) {
		case RCS:
			mmio = RENDER_HWS_PGA_GEN7;
			break;
		case BCS:
			mmio = BLT_HWS_PGA_GEN7;
			break;
		case VCS:
			mmio = BSD_HWS_PGA_GEN7;
			break;
		}
	} else if (IS_GEN6(ring->dev)) {
		mmio = RING_HWS_PGA_GEN6(ring->mmio_base);
	} else {
		mmio = RING_HWS_PGA(ring->mmio_base);
	}

	I915_WRITE(mmio, (u32)ring->status_page.gfx_addr);
	POSTING_READ(mmio);
}

static int
bsd_ring_flush(struct intel_ring_buffer *ring,
	       u32     invalidate_domains,
	       u32     flush_domains)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_FLUSH);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

static int
i9xx_add_request(struct intel_ring_buffer *ring)
{
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, ring->outstanding_lazy_seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	return 0;
}

static bool
gen6_ring_get_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	if (!dev->irq_enabled)
	       return false;

	/* It looks like we need to prevent the gt from suspending while waiting
	 * for an notifiy irq, otherwise irqs seem to get lost on at least the
	 * blt/bsd rings on ivb. */
	gen6_gt_force_wake_get(dev_priv);

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (ring->irq_refcount.gt++ == 0) {
		if (HAS_L3_DPF(dev) && ring->id == RCS)
			I915_WRITE_IMR(ring,
				       ~(ring->irq_enable_mask |
					 GT_PARITY_ERROR(dev)));
		else
			I915_WRITE_IMR(ring, ~ring->irq_enable_mask);
		dev_priv->gt_irq_mask &= ~ring->irq_enable_mask;
		I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
		POSTING_READ(GTIMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	return true;
}

static void
gen6_ring_put_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long flags;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (--ring->irq_refcount.gt == 0) {
		if (HAS_L3_DPF(dev) && ring->id == RCS)
			I915_WRITE_IMR(ring, ~GT_PARITY_ERROR(dev));
		else
			I915_WRITE_IMR(ring, ~0);
		dev_priv->gt_irq_mask |= ring->irq_enable_mask;
		I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
		POSTING_READ(GTIMR);
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	gen6_gt_force_wake_put(dev_priv);
}

static int
i965_dispatch_execbuffer(struct intel_ring_buffer *ring,
		u32 offset, u32 length,
		unsigned flags)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring,
			MI_BATCH_BUFFER_START |
			MI_BATCH_GTT |
			(flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE_I965));
	intel_ring_emit(ring, offset);
	intel_ring_advance(ring);

	return 0;
}

/* Just userspace ABI convention to limit the wa batch bo to a resonable size */
#define I830_BATCH_LIMIT (256*1024)
static int
i830_dispatch_execbuffer(struct intel_ring_buffer *ring,
				u32 offset, u32 len,
				unsigned flags)
{
	int ret;

	if (flags & I915_DISPATCH_PINNED) {
		ret = intel_ring_begin(ring, 4);
		if (ret)
			return ret;

		intel_ring_emit(ring, MI_BATCH_BUFFER);
		intel_ring_emit(ring, offset | (flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE));
		intel_ring_emit(ring, offset + len - 8);
		intel_ring_emit(ring, MI_NOOP);
		intel_ring_advance(ring);
	} else {
		u32 cs_offset = ring->scratch.gtt_offset;

		if (len > I830_BATCH_LIMIT)
			return -ENOSPC;

		ret = intel_ring_begin(ring, 9+3);
		if (ret)
			return ret;
		/* Blit the batch (which has now all relocs applied) to the stable batch
		 * scratch bo area (so that the CS never stumbles over its tlb
		 * invalidation bug) ... */
		intel_ring_emit(ring, XY_SRC_COPY_BLT_CMD |
				XY_SRC_COPY_BLT_WRITE_ALPHA |
				XY_SRC_COPY_BLT_WRITE_RGB);
		intel_ring_emit(ring, BLT_DEPTH_32 | BLT_ROP_GXCOPY | 4096);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, (DIV_ROUND_UP(len, 4096) << 16) | 1024);
		intel_ring_emit(ring, cs_offset);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, 4096);
		intel_ring_emit(ring, offset);
		intel_ring_emit(ring, MI_FLUSH);

		/* ... and execute it. */
		intel_ring_emit(ring, MI_BATCH_BUFFER);
		intel_ring_emit(ring, cs_offset | (flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE));
		intel_ring_emit(ring, cs_offset + len - 8);
		intel_ring_advance(ring);
	}

	return 0;
}

static int
i915_dispatch_execbuffer(struct intel_ring_buffer *ring,
                               u32 offset, u32 len,
							   unsigned flags)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_BATCH_BUFFER_START | MI_BATCH_GTT);
	intel_ring_emit(ring, offset | (flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE));
	intel_ring_advance(ring);

	return 0;
}

static void cleanup_status_page(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_object *obj;

	obj = ring->status_page.obj;
	if (obj == NULL)
		return;

	if (IS_VALLEYVIEW(dev_priv->dev))
		iounmap(ring->status_page.page_addr);
	else
		kunmap(sg_page(obj->pages->sgl));
	i915_gem_object_unpin(obj);
	drm_gem_object_unreference(&obj->base);
	ring->status_page.obj = NULL;
}

static int init_status_page(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int ret;

	obj = i915_gem_alloc_object(dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate status page\n");
		ret = -ENOMEM;
		goto err;
	}

	if ((INTEL_INFO(dev)->gen >= 6) && !IS_VALLEYVIEW(dev))
		i915_gem_object_set_cache_level(obj, I915_CACHE_LLC);

	ret = i915_gem_obj_ggtt_pin(obj, 4096, true, false);
	if (ret != 0) {
		goto err_unref;
	}

	ring->status_page.gfx_addr = i915_gem_obj_ggtt_offset(obj);
	if (IS_VALLEYVIEW(dev)) {
		ring->status_page.page_addr = ioremap(
							  dev_priv->gtt.mappable_base +
							  /* dev_priv->context->device_context.fb_adr + */
						      i915_gem_obj_ggtt_offset(obj),
						      PAGE_SIZE);
	} else {
		ring->status_page.page_addr = kmap(sg_page(obj->pages->sgl));
	}

	if (ring->status_page.page_addr == NULL) {
		ret = -ENOMEM;
		goto err_unpin;
	}
	ring->status_page.obj = obj;
	memset(ring->status_page.page_addr, 0, PAGE_SIZE);

	DRM_DEBUG_DRIVER("%s hws offset: 0x%08x\n",
			ring->name, ring->status_page.gfx_addr);

	return 0;

err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(&obj->base);
err:
	return ret;
}


static int init_phys_status_page(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;

	if (!dev_priv->status_page_dmah) {
		dev_priv->status_page_dmah =
			drm_pci_alloc(ring->dev, PAGE_SIZE, PAGE_SIZE);
		if (!dev_priv->status_page_dmah)
			return -ENOMEM;
	}

	ring->status_page.page_addr = dev_priv->status_page_dmah->vaddr;
	memset(ring->status_page.page_addr, 0, PAGE_SIZE);

	return 0;
}


static int intel_init_ring_buffer(struct drm_device *dev,
								 struct intel_ring_buffer *ring)
{
	struct drm_i915_gem_object *obj;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	ring->size = 32 * PAGE_SIZE;
	memset(ring->sync_seqno, 0, sizeof(ring->sync_seqno));

	init_waitqueue_head(&ring->irq_queue);

	if (I915_NEED_GFX_HWS(dev)) {
		ret = init_status_page(ring);
		if (ret)
			return ret;
    } else {
		BUG_ON(ring->id != RCS);
		ret = init_phys_status_page(ring);
		if (ret)
			return ret;
	}

	obj = i915_gem_alloc_object(dev, ring->size);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate ringbuffer\n");
		ret = -ENOMEM;
		goto err_hws;
	}

	/* mark ring buffers as read-only from GPU side by default */
	obj->gt_ro = 1;

	ring->obj = obj;

	ret = i915_gem_obj_ggtt_pin(obj, PAGE_SIZE, true, false);
	if (ret)
		goto err_unref;

	ret = i915_gem_object_set_to_gtt_domain(obj, true);
	if (ret)
		goto err_unpin;

	ring->virtual_start = ioremap_wc(dev_priv->gtt.mappable_base  + i915_gem_obj_ggtt_offset(obj),
									ring->size);
	if (ring->virtual_start == NULL) {
		DRM_ERROR("Failed to map ringbuffer.\n");
		ret = -EINVAL;
		goto err_unpin;
	}

	ret = ring->init(ring);
	if (ret)
		goto err_unmap;

	/* Workaround an erratum on the i830 which causes a hang if
	 * the TAIL pointer points to within the last 2 cachelines
	 * of the buffer.
	 */
	ring->effective_size = ring->size;
	if (IS_I830(ring->dev) || IS_845G(ring->dev))
		ring->effective_size -= 128;

	return 0;

err_unmap:
	iounmap(ring->virtual_start);
err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(&obj->base);
	ring->obj = NULL;
err_hws:
	cleanup_status_page(ring);
	return ret;
}

void intel_cleanup_ring_buffer(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv;
	struct drm_device *dev = ring->dev;
	int ret;

	if (ring->obj == NULL)
		return;

	/* Disable the ring buffer. The ring must be idle at this point */
	dev_priv = ring->dev->dev_private;
	ret = intel_ring_idle(ring);
	if (ret)
		DRM_ERROR("failed to quiesce %s whilst cleaning up: %d\n",
			  ring->name, ret);

	I915_WRITE_CTL(ring, 0);

	iounmap(ring->virtual_start);

	i915_gem_object_unpin(ring->obj);
	drm_gem_object_unreference(&ring->obj->base);
	ring->obj = NULL;

	/* If GEM Scheduler is enabled, then clean up the timestamp buffers */
	if (gem_scheduler && ring->timestamp_obj) {
		/* 1st -  lets unmap the Timestamp GEM buffer if it exists */
		if (ring->timestamp_virt) {
			if (!HAS_LLC(dev)) {
				iounmap(ring->timestamp_virt);
			} else {
				kunmap(sg_page(ring->timestamp_obj->pages->sgl));
			}
			ring->timestamp_virt = NULL;
		}
		/* 2nd - lets unpin and free the GEM buffer itself */
		/* For GEM scheduler, we need timestamp gem buffers */
		if (ring->timestamp_obj) {
			i915_gem_object_unpin(ring->timestamp_obj);
			drm_gem_object_unreference(&ring->timestamp_obj->base);
			ring->timestamp_obj = NULL;
		}
	}

	if (ring->cleanup)
		ring->cleanup(ring);

	cleanup_status_page(ring);
}

static int intel_ring_wait_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	int ret;

	ret = i915_wait_seqno(ring, seqno);
	if (!ret)
		i915_gem_retire_requests_ring(ring);

	return ret;
}

static int intel_ring_wait_request(struct intel_ring_buffer *ring, int n)
{
	struct drm_i915_gem_request *request;
	u32 seqno = 0;
	int ret;

	i915_gem_retire_requests_ring(ring);

	if (ring->last_retired_head != -1) {
		ring->head = ring->last_retired_head;
		ring->last_retired_head = -1;
		ring->space = ring_space(ring);
		if (ring->space >= n)
			return 0;
	}

	list_for_each_entry(request, &ring->request_list, list) {
		int space;

		if (request->tail == -1)
			continue;

		space = request->tail - (ring->tail + I915_RING_FREE_SPACE);
		if (space < 0)
			space += ring->size;
		if (space >= n) {
			seqno = request->seqno;
			break;
		}

		/* Consume this request in case we need more space than
		 * is available and so need to prevent a race between
		 * updating last_retired_head and direct reads of
		 * I915_RING_HEAD. It also provides a nice sanity check.
		 */
		request->tail = -1;
	}

	if (seqno == 0)
		return -ENOSPC;

	ret = intel_ring_wait_seqno(ring, seqno);
	if (ret)
		return ret;

	if (WARN_ON(ring->last_retired_head == -1))
		return -ENOSPC;

	ring->head = ring->last_retired_head;
	ring->last_retired_head = -1;
	ring->space = ring_space(ring);
	if (WARN_ON(ring->space < n))
		return -ENOSPC;

	return 0;
}

static int ring_wait_for_space(struct intel_ring_buffer *ring, int n)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long end;
	int ret;

	ret = intel_ring_wait_request(ring, n);
	if (ret != -ENOSPC)
        return ret;

	trace_i915_ring_wait_begin(ring);
	/* With GEM the hangcheck timer should kick us out of the loop,
	 * leaving it early runs the risk of corrupting GEM state (due
	 * to running on almost untested codepaths). But on resume
	 * timers don't work yet, so prevent a complete hang in that
	 * case by choosing an insanely large timeout. */
	end = jiffies + 60 * HZ;

	do {
		ring->head = I915_READ_HEAD(ring);
		ring->space = ring_space(ring);
		if (ring->space >= n) {
			trace_i915_ring_wait_end(ring);
			return 0;
		}

		if (dev->primary->master) {
			struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
			if (master_priv->sarea_priv)
				master_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;
		}

		msleep(1);

		ret = i915_gem_check_wedge(&dev_priv->gpu_error,
								   dev_priv->mm.interruptible);
		if (ret)
			return ret;
	} while (!time_after(jiffies, end));
	trace_i915_ring_wait_end(ring);
	return -EBUSY;
}

static int intel_wrap_ring_buffer(struct intel_ring_buffer *ring)
{
	uint32_t __iomem *virt;
	int rem = ring->size - ring->tail;

	if (ring->space < rem) {
		int ret = ring_wait_for_space(ring, rem);
		if (ret)
			return ret;
	}

	virt = ring->virtual_start + ring->tail;
	rem /= 4;
	while (rem--)
		iowrite32(MI_NOOP, virt++);

	ring->tail = 0;
	ring->space = ring_space(ring);

	return 0;
}

int intel_ring_idle(struct intel_ring_buffer *ring)
{
	u32 seqno;
	int ret;

	/* We need to add any requests required to flush the objects and ring */
	if (ring->outstanding_lazy_seqno) {
		ret = i915_add_request(ring, NULL, NULL);
		if (ret)
			return ret;
	}

	/* Wait upon the last request to be completed */
	if (list_empty(&ring->request_list))
		return 0;

	seqno = list_entry(ring->request_list.prev,
			struct drm_i915_gem_request,
			list)->seqno;

	return i915_wait_seqno(ring, seqno);
}

static int
intel_ring_alloc_seqno(struct intel_ring_buffer *ring)
{
	if (ring->outstanding_lazy_seqno)
		return 0;

	if (ring->preallocated_lazy_request == NULL) {
		struct drm_i915_gem_request *request;

		request = kmalloc(sizeof(*request), GFP_KERNEL);
		if (request == NULL)
			return -ENOMEM;

		ring->preallocated_lazy_request = request;
	}

	return i915_gem_get_seqno(ring->dev, &ring->outstanding_lazy_seqno);
}

static int __intel_ring_begin(struct intel_ring_buffer *ring,
                             int bytes)
{
	int ret;

	if (unlikely(ring->tail + bytes > ring->effective_size)) {
		ret = intel_wrap_ring_buffer(ring);
		if (unlikely(ret))
			return ret;
	}

	if (unlikely(ring->space < bytes)) {
		ret = ring_wait_for_space(ring, bytes);
		if (unlikely(ret))
			return ret;
	}

	ring->space -= bytes;
	return 0;
}

int intel_ring_begin(struct intel_ring_buffer *ring,
		     int num_dwords)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	int ret;

	ret = i915_gem_check_wedge(&dev_priv->gpu_error,
							   dev_priv->mm.interruptible);
	if (ret)
		return ret;
	/* Preallocate the olr before touching the ring */
	ret = intel_ring_alloc_seqno(ring);
	if (ret)
		return ret;

	return __intel_ring_begin(ring, num_dwords * sizeof(uint32_t));
}

void intel_ring_init_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;

	BUG_ON(ring->outstanding_lazy_seqno);

	if (INTEL_INFO(ring->dev)->gen >= 6) {
		I915_WRITE(RING_SYNC_0(ring->mmio_base), 0);
		I915_WRITE(RING_SYNC_1(ring->mmio_base), 0);
	}

	ring->set_seqno(ring, seqno);
	ring->hangcheck.seqno = seqno;
}


void intel_ring_advance(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;

	ring->tail &= ring->size - 1;
	if (dev_priv->gpu_error.stop_rings & intel_ring_flag(ring))
		return;
	ring->write_tail(ring, ring->tail);
}

static void gen6_bsd_ring_write_tail(struct intel_ring_buffer *ring,
				     u32 value)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;

	/* Every tail move must follow the sequence below */

	/* Disable notification that the ring is IDLE. The GT
	 * will then assume that it is busy and bring it out of rc6.
	 */
	I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
			_MASKED_BIT_ENABLE(GEN6_BSD_SLEEP_MSG_DISABLE));

	/* Clear the context id. Here be magic! */
	I915_WRITE64(GEN6_BSD_RNCID, 0x0);

	/* Wait for the ring not to be idle, i.e. for it to wake up. */
	if (wait_for((I915_READ(GEN6_BSD_SLEEP_PSMI_CONTROL) &
					GEN6_BSD_SLEEP_INDICATOR) == 0,
				50))
		DRM_ERROR("timed out waiting for the BSD ring to wake up\n");

	/* Now that the ring is fully powered up, update the tail */
	I915_WRITE_TAIL(ring, value);
	POSTING_READ(RING_TAIL(ring->mmio_base));

	/* Let the ring send IDLE messages to the GT again,
	 * and so let it sleep to conserve power when idle.
	 */
	I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
			_MASKED_BIT_DISABLE(GEN6_BSD_SLEEP_MSG_DISABLE));
}

static int gen6_bsd_ring_flush(struct intel_ring_buffer *ring,
			       u32 invalidate, u32 flush)
{
	uint32_t cmd;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	cmd = MI_FLUSH_DW;
	/*
	 * Bspec vol 1c.5 - video engine command streamer:
	 * "If ENABLED, all TLBs will be invalidated once the flush
	 * operation is complete. This bit is only valid when the
	 * Post-Sync Operation field is a value of 1h or 3h."
	 */
	if (invalidate & I915_GEM_GPU_DOMAINS)
		cmd |= MI_INVALIDATE_TLB | MI_INVALIDATE_BSD |
				MI_FLUSH_DW_STORE_INDEX | MI_FLUSH_DW_OP_STOREDW;
	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, I915_GEM_HWS_SCRATCH_ADDR | MI_FLUSH_DW_USE_GTT);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

static int
hsw_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
		u32 offset, u32 len,
		unsigned flags)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring,
			MI_BATCH_BUFFER_START | MI_BATCH_PPGTT_HSW |
			(flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE_HSW));
	/* bit0-7 is the length on GEN6+ */
	intel_ring_emit(ring, offset);
	intel_ring_advance(ring);

	return 0;
}

int tmpcount1 = 200;

static inline uint32_t
get_timestamp_before(struct intel_ring_buffer *ring)
{
	return (i915_gem_obj_ggtt_offset(ring->timestamp_obj)) +
	       ((uint32_t)((off_t)(&ring->timestamp_head->before)));
}

static inline uint32_t
get_timestamp_after(struct intel_ring_buffer *ring)
{
	return (i915_gem_obj_ggtt_offset(ring->timestamp_obj)) +
	       ((uint32_t)((off_t)(&ring->timestamp_head->after)));
}

static inline uint32_t
get_timestamp_pid(struct intel_ring_buffer *ring)
{
	return (i915_gem_obj_ggtt_offset(ring->timestamp_obj)) +
	       ((uint32_t)((off_t)(&ring->timestamp_head->pid)));
}

static inline uint32_t
get_timestamp_block(struct intel_ring_buffer *ring)
{
	return ((uint32_t)((off_t)(&ring->timestamp_head->pid)));
}

/*
The actual timestamp register size is 64 bits (63:36 reserved, 35:0 timestamp)
and should be retrieved using the PIPE_CONTROL method according to the spec. 
Unfortunately the PIPE_CONTROL method, originally employed by the render ring
only captures 32 bits (out of 64) from the timestamp register.
				
Blitter ring and video ring however, must employ a different method, 
MI_STORE_REGISTER_MEM to capture from the timestamp register. By executing the command
twice (capturing 32 bits each) we were successful in getting the full 64 bits
value of the register (note that only 36 bits represent timestamp value).
				
This essentially creates inconsistencies where the timestamp value retrieved
by render ring is bounded by (2^32)-1 while on the other hand, the timestamp
value retrieved by blitter and video ring is bounded by (2^36)-1.

Also, there are many known HW WA's related specifically to the PIPE_CONTROL when 
used with a batch that contains flushing type ops. Therefore we now "mask" the first 
32 bits of the timestamp register for all three rings by exeuting MI_STORE_REGISTER_MEM
once instead of twice like before in blitter ring and video ring. As a result, the value
now in 32 bits wraparound every ~5.7 minutes. I.e. all three rings now employ the
same method and function to retrieve the value from the timestamp register.
*/

static int
gen6_blt_timestamp_execbuffer(struct intel_ring_buffer *ring,
				u32 offset, u32 len, unsigned flags,
				struct drm_i915_gem_exec_timestamp ** timestamp)
{
	int ret,x;
	struct drm_i915_gem_exec_timestamp * ts0 = ring->timestamp_virt;

	if (timestamp) {
		*timestamp = (struct drm_i915_gem_exec_timestamp*)
			(((unsigned char *)ring->timestamp_virt) + (int)get_timestamp_block(ring));
	}
	
	if (!intel_ring_begin(ring, 8)) {
		intel_ring_emit(ring, MI_STORE_DWORD_IMM(3) | 
			MI_MEM_VIRTUAL/* is BIT22 = Global Gtt */);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, get_timestamp_pid(ring));
		intel_ring_emit(ring, current->pid);
		intel_ring_emit(ring, ring->name[0]);
		intel_ring_emit(ring, MI_STORE_REGISTER_MEM);
		intel_ring_emit(ring, TIMESTAMP);
		intel_ring_emit(ring, get_timestamp_before(ring));
		intel_ring_advance(ring);
	}
	ret = ring->dispatch_execbuffer(ring, offset, len, flags);
	if (ret)
		return ret;

	if (!intel_ring_begin(ring, 4)) {
		intel_ring_emit(ring, MI_STORE_REGISTER_MEM);
		intel_ring_emit(ring, TIMESTAMP);
		intel_ring_emit(ring, get_timestamp_after(ring));
		intel_ring_emit(ring, MI_NOOP);
		intel_ring_advance(ring);
	}

	if(tmpcount1<200) {
		for(x = 0; x < 32; ++x) {
			EMGD_ERROR("RING%02d-TS%03d : ", ring->id, x);
			EMGD_ERROR("      -> PID = 0x%08x", (unsigned int)ts0->pid);
			EMGD_ERROR("      -> B4  = 0x%08x", (unsigned int)(ts0->before>>6));
			EMGD_ERROR("      -> AFT = 0x%08x", (unsigned int)(ts0->after>>6));
			++ts0;
		}
	}

	++tmpcount1;
	return ret;
}


static int
gen6_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
			      u32 offset, u32 len,
				  unsigned flags)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring,
		   MI_BATCH_BUFFER_START |
		   (flags & I915_DISPATCH_SECURE ? 0 : MI_BATCH_NON_SECURE_I965));
	/* bit0-7 is the length on GEN6+ */
	intel_ring_emit(ring, offset);
	intel_ring_advance(ring);

	return 0;
}

/* Blitter support (SandyBridge+) */

static int gen6_ring_flush(struct intel_ring_buffer *ring,
			   u32 invalidate, u32 flush)
{
	uint32_t cmd;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	cmd = MI_FLUSH_DW;
	/*
	 * Bspec vol 1c.3 - blitter engine command streamer:
	 * "If ENABLED, all TLBs will be invalidated once the flush
	 * operation is complete. This bit is only valid when the
	 * Post-Sync Operation field is a value of 1h or 3h."
	 */
	if (invalidate & I915_GEM_DOMAIN_RENDER)
		cmd |= MI_INVALIDATE_TLB | MI_FLUSH_DW_STORE_INDEX |
				MI_FLUSH_DW_OP_STOREDW;
	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, I915_GEM_HWS_SCRATCH_ADDR | MI_FLUSH_DW_USE_GTT);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

int intel_init_render_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[RCS];

	ring->name = "render ring";
	ring->id = RCS;
	ring->mmio_base = RENDER_RING_BASE;

	if (INTEL_INFO(dev)->gen >= 6) {
		ring->add_request = gen6_add_request;
		ring->flush = gen7_render_ring_flush;
		if (INTEL_INFO(dev)->gen == 6)
			ring->flush = gen6_render_ring_flush;
		ring->irq_get = gen6_ring_get_irq;
		ring->irq_put = gen6_ring_put_irq;
		ring->irq_enable_mask = GT_RENDER_USER_INTERRUPT;
		ring->get_seqno = gen6_ring_get_seqno;
		ring->set_seqno = ring_set_seqno;
		ring->sync_to = gen6_ring_sync;
		ring->semaphore_register[RCS] = MI_SEMAPHORE_SYNC_INVALID;
		ring->semaphore_register[VCS] = MI_SEMAPHORE_SYNC_RV;
		ring->semaphore_register[BCS] = MI_SEMAPHORE_SYNC_RB;
		ring->signal_mbox[RCS] = GEN6_NOSYNC;
		ring->signal_mbox[VCS] = GEN6_VRSYNC;
		ring->signal_mbox[BCS] = GEN6_BRSYNC;
	} else if (IS_GEN5(dev)) {
		ring->add_request = pc_render_add_request;
		ring->flush = gen4_render_ring_flush;
		ring->get_seqno = pc_render_get_seqno;
		ring->set_seqno = pc_render_set_seqno;
		ring->irq_get = gen5_ring_get_irq;
		ring->irq_put = gen5_ring_put_irq;
		ring->irq_enable_mask = GT_RENDER_USER_INTERRUPT |
					GT_RENDER_PIPECTL_NOTIFY_INTERRUPT;
	} else {
		ring->add_request = i9xx_add_request;
		if (INTEL_INFO(dev)->gen < 4)
			ring->flush = gen2_render_ring_flush;
		else
			ring->flush = gen4_render_ring_flush;
		ring->get_seqno = ring_get_seqno;
		ring->set_seqno = ring_set_seqno;
		ring->irq_get = i9xx_ring_get_irq;
		ring->irq_put = i9xx_ring_put_irq;
		ring->irq_enable_mask = I915_USER_INTERRUPT;
	}
	ring->write_tail = ring_write_tail;
	if (IS_HASWELL(dev))
		ring->dispatch_execbuffer = hsw_ring_dispatch_execbuffer;
	else if (INTEL_INFO(dev)->gen >= 6)
		ring->dispatch_execbuffer = gen6_ring_dispatch_execbuffer;
	else if (INTEL_INFO(dev)->gen >= 4)
		ring->dispatch_execbuffer = i965_dispatch_execbuffer;
	else if (IS_I830(dev) || IS_845G(dev))
		ring->dispatch_execbuffer = i830_dispatch_execbuffer;
	else
		ring->dispatch_execbuffer = i915_dispatch_execbuffer;
	
	if (INTEL_INFO(dev)->gen >= 6) 
		ring->timestamp_execbuffer = gen6_blt_timestamp_execbuffer;
	else 
		ring->timestamp_execbuffer = NULL;

	ring->init = init_render_ring;
	ring->cleanup = render_ring_cleanup;

	/* Workaround batchbuffer to combat CS tlb bug. */
	if (HAS_BROKEN_CS_TLB(dev)) {
		struct drm_i915_gem_object *obj;
		int ret;

		obj = i915_gem_alloc_object(dev, I830_BATCH_LIMIT);
		if (obj == NULL) {
			DRM_ERROR("Failed to allocate batch bo\n");
			return -ENOMEM;
		}

		ret = i915_gem_obj_ggtt_pin(obj, 0, true, false);
		if (ret != 0) {
			drm_gem_object_unreference(&obj->base);
			DRM_ERROR("Failed to ping batch bo\n");
			return ret;
		}

		ring->scratch.obj = obj;
		ring->scratch.gtt_offset = i915_gem_obj_ggtt_offset(obj);
	}

	return intel_init_ring_buffer(dev, ring);
}

int intel_render_ring_init_dri(struct drm_device *dev, u64 start, u32 size)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[RCS];
	int ret;

	ring->name = "render ring";
	ring->id = RCS;
	ring->mmio_base = RENDER_RING_BASE;

	if (INTEL_INFO(dev)->gen >= 6) {
		/* non-kms not supported on gen6+ */
		return -ENODEV;
	}

	/* Note: gem is not supported on gen5/ilk without kms (the corresponding
	 * gem_init ioctl returns with -ENODEV). Hence we do not need to set up
	 * the special gen5 functions. */
	ring->add_request = i9xx_add_request;
	if (INTEL_INFO(dev)->gen < 4)
		ring->flush = gen2_render_ring_flush;
	else
		ring->flush = gen4_render_ring_flush;
	ring->get_seqno = ring_get_seqno;
	ring->set_seqno = ring_set_seqno;
	ring->irq_get = i9xx_ring_get_irq;
	ring->irq_put = i9xx_ring_put_irq;
	ring->irq_enable_mask = I915_USER_INTERRUPT;
	ring->write_tail = ring_write_tail;
	if (INTEL_INFO(dev)->gen >= 4)
		ring->dispatch_execbuffer = i965_dispatch_execbuffer;
	else if (IS_I830(dev) || IS_845G(dev))
		ring->dispatch_execbuffer = i830_dispatch_execbuffer;
	else
		ring->dispatch_execbuffer = i915_dispatch_execbuffer;
	ring->init = init_render_ring;
	ring->cleanup = render_ring_cleanup;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);

	ring->size = size;
	ring->effective_size = ring->size;
	if (IS_I830(ring->dev) || IS_845G(ring->dev))
		ring->effective_size -= 128;

	ring->virtual_start = ioremap_wc(start, size);
	if (ring->virtual_start == NULL) {
		DRM_ERROR("can not ioremap virtual address for"
				" ring buffer\n");
		return -ENOMEM;
	}

	if (!I915_NEED_GFX_HWS(dev)) {
		ret = init_phys_status_page(ring);
		if (ret)
			return ret;
	}

	return 0;
}

int intel_init_bsd_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[VCS];

	ring->name = "bsd ring";
	ring->id = VCS;

	ring->write_tail = ring_write_tail;
	if (IS_GEN6(dev) || IS_GEN7(dev)) {
		ring->mmio_base = GEN6_BSD_RING_BASE;
		/* gen6 bsd needs a special wa for tail updates */
		if (IS_GEN6(dev))
			ring->write_tail = gen6_bsd_ring_write_tail;
		ring->flush = gen6_bsd_ring_flush;
		ring->add_request = gen6_add_request;
		ring->get_seqno = gen6_ring_get_seqno;
		ring->set_seqno = ring_set_seqno;
		ring->irq_enable_mask = GT_BSD_USER_INTERRUPT;
		ring->irq_get = gen6_ring_get_irq;
		ring->irq_put = gen6_ring_put_irq;
		ring->dispatch_execbuffer = gen6_ring_dispatch_execbuffer;
		ring->timestamp_execbuffer = gen6_blt_timestamp_execbuffer;
		ring->sync_to = gen6_ring_sync;
		ring->semaphore_register[RCS] = MI_SEMAPHORE_SYNC_VR;
		ring->semaphore_register[VCS] = MI_SEMAPHORE_SYNC_INVALID;
		ring->semaphore_register[BCS] = MI_SEMAPHORE_SYNC_VB;
		ring->signal_mbox[RCS] = GEN6_RVSYNC;
		ring->signal_mbox[VCS] = GEN6_NOSYNC;
		ring->signal_mbox[BCS] = GEN6_BVSYNC;
	} else {
		ring->mmio_base = BSD_RING_BASE;
		ring->flush = bsd_ring_flush;
		ring->add_request = i9xx_add_request;
		ring->get_seqno = ring_get_seqno;
		ring->set_seqno = ring_set_seqno;
		if (IS_GEN5(dev)) {
			ring->irq_enable_mask = ILK_BSD_USER_INTERRUPT;
			ring->irq_get = gen5_ring_get_irq;
			ring->irq_put = gen5_ring_put_irq;
		} else {
			ring->irq_enable_mask = I915_BSD_USER_INTERRUPT;
			ring->irq_get = i9xx_ring_get_irq;
			ring->irq_put = i9xx_ring_put_irq;
		}
		ring->dispatch_execbuffer = i965_dispatch_execbuffer;
		ring->timestamp_execbuffer = NULL;
	}
	ring->init = init_ring_common;

	return intel_init_ring_buffer(dev, ring);
}

int intel_init_blt_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[BCS];

	ring->name = "blitter ring";
	ring->id = BCS;

	ring->mmio_base = BLT_RING_BASE;
	ring->write_tail = ring_write_tail;
	ring->flush = gen6_ring_flush;
	ring->add_request = gen6_add_request;
	ring->get_seqno = gen6_ring_get_seqno;
	ring->set_seqno = ring_set_seqno;
	ring->irq_enable_mask = GT_BLT_USER_INTERRUPT;
	ring->irq_get = gen6_ring_get_irq;
	ring->irq_put = gen6_ring_put_irq;
	ring->dispatch_execbuffer = gen6_ring_dispatch_execbuffer;
	if (INTEL_INFO(dev)->gen >= 6) 
		ring->timestamp_execbuffer = gen6_blt_timestamp_execbuffer;
	else 
		ring->timestamp_execbuffer = NULL;
	ring->sync_to = gen6_ring_sync;
	ring->semaphore_register[RCS] = MI_SEMAPHORE_SYNC_BR;
	ring->semaphore_register[VCS] = MI_SEMAPHORE_SYNC_BV;
	ring->semaphore_register[BCS] = MI_SEMAPHORE_SYNC_INVALID;
	ring->signal_mbox[RCS] = GEN6_RBSYNC;
	ring->signal_mbox[VCS] = GEN6_VBSYNC;
	ring->signal_mbox[BCS] = GEN6_NOSYNC;
	ring->init = init_ring_common;

	return intel_init_ring_buffer(dev, ring);
}

int
intel_ring_flush_all_caches(struct intel_ring_buffer *ring)
{
	int ret;

	if (!ring->gpu_caches_dirty)
		return 0;

	ret = ring->flush(ring, 0, I915_GEM_GPU_DOMAINS);
	if (ret)
		return ret;

	trace_i915_gem_ring_flush(ring, 0, I915_GEM_GPU_DOMAINS);

	ring->gpu_caches_dirty = false;
	return 0;
}

int
intel_ring_invalidate_all_caches(struct intel_ring_buffer *ring)
{
	uint32_t flush_domains;
	int ret;

	flush_domains = 0;
	if (ring->gpu_caches_dirty)
		flush_domains = I915_GEM_GPU_DOMAINS;

	ret = ring->flush(ring, I915_GEM_GPU_DOMAINS, flush_domains);
	if (ret)
		return ret;

	trace_i915_gem_ring_flush(ring, I915_GEM_GPU_DOMAINS, flush_domains);

	ring->gpu_caches_dirty = false;
	return 0;
}
