/***************************************************************************

Copyright 2000, 2013, 2014 Intel Corporation.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifndef SNA_VIDEO_H
#define SNA_VIDEO_H

#include "vaxv.h"
#include <xf86_OSproc.h>
#include <xf86xv.h>
#include <fourcc.h>

#if defined(XvMCExtension) && defined(ENABLE_XVMC)
#define SNA_XVMC 1
#endif

#define FOURCC_XVMC (('C' << 24) + ('M' << 16) + ('V' << 8) + 'X')
#define FOURCC_RGB565 ((16 << 24) + ('B' << 16) + ('G' << 8) + 'R')
#define FOURCC_RGB888 ((24 << 24) + ('B' << 16) + ('G' << 8) + 'R')

/*
 * Below, a dummy picture type that is used in XvPutImage
 * only to do an overlay update.
 * Introduced for the XvMC client lib.
 * Defined to have a zero data size.
 */
#define XVMC_YUV { \
	FOURCC_XVMC, XvYUV, LSBFirst, \
	{'X', 'V', 'M', 'C', 0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}, \
	12, XvPlanar, 3, 0, 0, 0, 0, 8, 8, 8, 1, 2, 2, 1, 2, 2, \
	{'Y', 'V', 'U', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
	XvTopToBottom \
}

#define XVMC_RGB565 { \
	FOURCC_RGB565, XvRGB, LSBFirst, \
	{'P', 'A', 'S', 'S', 'T', 'H', 'R', 'O', 'U', 'G', 'H', 'R', 'G', 'B', '1', '6'}, \
	16, XvPacked, 1, 16, 0x1f<<11, 0x3f<<5, 0x1f<<0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	{'B', 'G', 'R', 'X', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
	XvTopToBottom \
}

#define XVMC_RGB888 { \
	FOURCC_RGB888, XvRGB, LSBFirst, \
	{'P', 'A', 'S', 'S', 'T', 'H', 'R', 'O', 'U', 'G', 'H', 'R', 'G', 'B', '2', '4'}, \
	32, XvPacked, 1, 24, 0xff<<16, 0xff<<8, 0xff<<0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
	{'B', 'G', 'R', 'X', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
	XvTopToBottom \
}

#define MAX_SPRITE_PLANES   	2

#define SPRITE_MODE_SINGLE		0x0
#define SPRITE_MODE_EXTENDED	0x1
#define SPRITE_MODE_CLONE		0x2

#define MAX_VIDEO_BUFFERS		3

#define SPRITE_RESERVED_NONE    0x0
#define SPRITE_RESERVED_DRI     0x1
#define SPRITE_RESERVED_XV      0x2
#define SPRITE_RESERVED_ANY     (~0)

struct sna_video_sprite_planes {
	int num_planes;
	struct sna_plane* plane_list[MAX_SPRITE_PLANES];
	bool updated;
};

struct sna_video {
	struct sna *sna;

	int brightness;
	int contrast;
	int saturation;
	int hue;
	int gamma_red;
	int gamma_blue;
	int gamma_green;
	bool gamma_enable;
	int constant_alpha;
	xf86CrtcPtr desired_crtc;

	RegionRec clip;

/* sna overlay implementation of gamma */
	uint32_t gamma0;
	uint32_t gamma1;
	uint32_t gamma2;
	uint32_t gamma3;
	uint32_t gamma4;
	uint32_t gamma5;

	int color_key;
	int color_key_changed;
	int attribs_changed;
#define BRIGHTNESS_CHANGED		(1 << 0x0)
#define CONTRAST_CHANGED		(1 << 0x1)
#define SATURATION_CHANGED		(1 << 0x2)
#define HUE_CHANGED				(1 << 0x3)
#define GAMMA_R_CHANGED			(1 << 0x4)
#define GAMMA_B_CHANGED			(1 << 0x5)
#define GAMMA_G_CHANGED			(1 << 0x6)
#define GAMMA_ENABLE_CHANGED	(1 << 0x7)
#define CONSTANT_ALPHA_CHANGED	(1 << 0x8)


	/** YUV data buffers */
	struct kgem_bo *buf_list[MAX_VIDEO_BUFFERS];

	int alignment;
	bool tiled;
	bool textured;
	Rotation rotation;
	struct sna_video_sprite_planes planes;
	struct kgem_bo *bo;

	int SyncToVblank;	/* -1: auto, 0: off, 1: on */
	int AlwaysOnTop;

	int pixmap_current;
	PixmapPtr pixmaps[MAX_VIDEO_BUFFERS];   /* Triple buffer */

	int vaxv; /* Vaxv mode */
};

struct sna_video_frame {
	struct kgem_bo *bo;
	uint32_t id;
	uint32_t size;
	uint32_t UBufOffset;
	uint32_t VBufOffset;

	uint16_t width, height;
	uint16_t pitch[2];

	/* extents */
	BoxRec image;
	BoxRec src;
};

static inline XvScreenPtr to_xv(ScreenPtr screen)
{
	return dixLookupPrivate(&screen->devPrivates, XvGetScreenKey());
}

void sna_video_init(struct sna *sna, ScreenPtr screen);
void sna_video_overlay_setup(struct sna *sna, ScreenPtr screen);
void sna_video_sprite_setup(struct sna *sna, ScreenPtr screen);
void sna_video_textured_setup(struct sna *sna, ScreenPtr screen);
void sna_video_destroy_window(WindowPtr win);

XvAdaptorPtr sna_xv_adaptor_alloc(struct sna *sna);
int sna_xv_fixup_formats(ScreenPtr screen,
			 XvFormatPtr formats,
			 int num_formats);
int sna_xv_alloc_port(unsigned long port, XvPortPtr in, XvPortPtr *out);
int sna_xv_free_port(XvPortPtr port);

static inline int xvmc_passthrough(int id)
{
	switch (id) {
	case FOURCC_XVMC:
	case FOURCC_RGB565:
	case FOURCC_RGB888:
		return true;
	default:
		return false;
	}
}

static inline int is_planar_fourcc(int id)
{
	switch (id) {
	case FOURCC_YV12:
	case FOURCC_I420:
	case FOURCC_XVMC:
#ifdef FOURCC_VAXV
	case FOURCC_VAXV:
#endif
		return 1;
		default:
		return 0;
	}
}

bool
sna_video_clip_helper(struct sna_video *video,
		      struct sna_video_frame *frame,
		      xf86CrtcPtr *crtc_ret,
		      BoxPtr dst,
		      short src_x, short src_y,
		      short drw_x, short drw_y,
		      short src_w, short src_h,
		      short drw_w, short drw_h,
		      RegionPtr reg);

void
sna_video_frame_init(struct sna_video *video,
		     int id, short width, short height,
		     struct sna_video_frame *frame,
		     int align);

struct kgem_bo *
sna_video_buffer(struct sna_video *video,
		 struct sna_video_frame *frame);

bool
sna_video_copy_data(struct sna_video *video,
		    struct sna_video_frame *frame,
		    const uint8_t *buf);

void sna_video_buffer_fini(struct sna_video *video);

void sna_video_free_buffers(struct sna_video *video);

static inline XvPortPtr
sna_window_get_port(WindowPtr window)
{
	return ((void **)__get_private(window, sna_window_key))[2];
}

static inline void
sna_window_set_port(WindowPtr window, XvPortPtr port)
{
	((void **)__get_private(window, sna_window_key))[2] = port;
}

bool sna_video_sprite_plane_grab(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes,
		uint32_t plane_reservation);

void sna_video_sprite_ungrab(
		struct sna *sna,
		DrawablePtr draw);

bool sna_video_sprite_planes_from_drawable(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes);

bool sna_video_sprite_dri_show(
		struct sna *sna,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes,
		struct kgem_bo *src_bo);


int
sna_video_textured_put_image(ClientPtr client,
			     DrawablePtr draw,
			     XvPortPtr port,
			     GCPtr gc,
			     INT16 src_x, INT16 src_y,
			     CARD16 src_w, CARD16 src_h,
			     INT16 drw_x, INT16 drw_y,
			     CARD16 drw_w, CARD16 drw_h,
			     XvImagePtr format,
			     unsigned char *buf,
			     Bool sync,
			     CARD16 width, CARD16 height);


#endif /* SNA_VIDEO_H */
