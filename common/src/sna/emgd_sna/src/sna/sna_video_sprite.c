/***************************************************************************

 Copyright 2000-2011, 2013, 2014 Intel Corporation.  All Rights Reserved.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_video.h"

#include "intel_options.h"

#include <xf86drm.h>
#include <xf86xv.h>
#include <X11/extensions/Xv.h>
#include <fourcc.h>
#include <i915_drm.h>
#include <errno.h>

#ifdef  DRM_IOCTL_MODE_GETPLANERESOURCES
#include <drm_fourcc.h>

#define IMAGE_MAX_WIDTH		2048
#define IMAGE_MAX_HEIGHT	2048
#define MAX_CRTC_NUM		2

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, true)

static Atom xvColorKey, xvAlwaysOnTop, xvBrightness, xvContrast, xvSaturation,
			xvHue, xvGammaRed, xvGammaGreen, xvGammaBlue, xvGammaEnable,
			xvConstantAlpha;

#define MID_CONTRAST_YUV	67
#define MID_SATURATION_YUV	145
#define MID_BRIGHTNESS_YUV	-5
#define MID_HUE_YUV			0

#define SNA_HUE_MIN			0
#define SNA_HUE_MAX			1023

#define SNA_SATURATION_MIN		0
#define SNA_SATURATION_MAX		1023

#define SNA_BRIGHTNESS_MIN		-128
#define SNA_BRIGHTNESS_MAX		127

#define SNA_CONTRAST_MIN		0
#define SNA_CONTRAST_MAX		255

#define XV_COLORKEY_MIN		0
#define XV_COLORKEY_MAX		0xffffff

#define XV_GAMMA_DISABLE	0x0
#define XV_GAMMA_ENABLE		0x1

#define SNA_CONSTANT_ALPHA_MIN	0
#define SNA_CONSTANT_ALPHA_MAX	255

#define ROUND(x)	(int)((x) + 0.5)
#define FIXEDPT24_8(x)	(ROUND((x) * (1 << 8)))

#define SNA_GAMMA_MIN	FIXEDPT24_8(0.6)
#define SNA_GAMMA_MID	FIXEDPT24_8(1.0)
#define SNA_GAMMA_MAX	FIXEDPT24_8(6.0)

#define SNA_VIDEO_ATTR_VALUE_VALID(value, min, max) \
	((value) >= (min) && (value) <= (max))


static XvFormatRec formats[] = { {15}, {16}, {24} };
static const XvImageRec images[] = { XVIMAGE_YUY2, XVIMAGE_YV12, XVIMAGE_I420,
		XVIMAGE_UYVY, XVMC_RGB888, XVMC_RGB565,
#ifdef ENABLE_VAXV
	{
		FOURCC_VAXV,
		XvYUV,
		LSBFirst,
		{'V', 'A', 'X', 'V',
			0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00,
			0x38, 0x9B, 0x71},
		12,
		XvPlanar,
		3,
		0, 0, 0, 0,
		8, 8, 8,
		1, 2, 2,
		1, 2, 2,
		{'Y', 'V', 'U',
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		XvTopToBottom
	},
#endif
};
static const XvAttributeRec attribs[] = {
	{ XvSettable | XvGettable, XV_COLORKEY_MIN, XV_COLORKEY_MAX, (char *)"XV_COLORKEY" },
	{ XvSettable | XvGettable, SNA_BRIGHTNESS_MIN, SNA_BRIGHTNESS_MAX, (char *)"XV_BRIGHTNESS" },
	{ XvSettable | XvGettable, SNA_CONTRAST_MIN, SNA_CONTRAST_MAX, (char *)"XV_CONTRAST" },
	{ XvSettable | XvGettable, SNA_SATURATION_MIN, SNA_SATURATION_MAX, (char *)"XV_SATURATION" },
	{ XvSettable | XvGettable, SNA_HUE_MIN, SNA_HUE_MAX, (char *)"XV_HUE" },
	/*
	 * Gamma is expressed in 24i.8f format, accepted values are in range
	 * [0.6; 6.0]
	 */
	{ XvSettable | XvGettable, SNA_GAMMA_MIN, SNA_GAMMA_MAX, (char *)"XV_GAMMA_RED" },
	{ XvSettable | XvGettable, SNA_GAMMA_MIN, SNA_GAMMA_MAX, (char *)"XV_GAMMA_GREEN" },
	{ XvSettable | XvGettable, SNA_GAMMA_MIN, SNA_GAMMA_MAX, (char *)"XV_GAMMA_BLUE" },
	{ XvSettable | XvGettable, XV_GAMMA_DISABLE, XV_GAMMA_ENABLE, (char *)"XV_GAMMA_ENABLE" },
	{ XvSettable | XvGettable, SNA_CONSTANT_ALPHA_MIN, SNA_CONSTANT_ALPHA_MAX, (char *)"XV_CONSTANT_ALPHA" },
	{ XvSettable | XvGettable, -1, 1, (char *)"XV_PIPE" },
#ifdef ENABLE_VAXV
	{ XvSettable | XvGettable, 0, 2, (char *)VAXV_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, (char *)VAXV_VERSION_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, (char *)VAXV_CAPS_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, (char *)VAXV_FORMAT_ATTR_STR },
#endif
};

#ifdef ENABLE_VAXV
static Atom xvvaShmbo;
static Atom xvvaVersion;
static Atom xvvaCaps;
static Atom xvvaFormat;
#endif

static bool sna_video_sprite_xv_show(
		struct sna *sna,
		GCPtr gc,
		DrawablePtr draw,
		struct sna_video_frame *frame,
		struct sna_video *video,
		BoxPtr dst_box,
		RegionPtr clip,
		XvPortPtr port);

/**
 * Return settings related to current display configuration.
 *
 * @param drawable Current drawable target
 * @param crtc_id ID of crtc with most sprite coverage
 * @param mode On return, contains the mode (SINGLE, CLONE or EXTENDED)
 * @param num_planes On return, contains number of sprite planes required
 * @param crtc_id_list On return, contains a list of CRTC IDs for the sprites.
 *        Can be NULL if this is not required.
 * @return true if crtc is enabled for sprite
 */
static bool sna_video_sprite_check_mode(
		DrawablePtr drawable,
		uint32_t crtc_id,
		int * mode,
		int * num_planes,
		uint32_t* crtc_id_list)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	int rotations[MAX_CRTC_NUM];
	bool enabled[MAX_CRTC_NUM];
	bool all_enabled = true;
	bool rotated = false;
	bool crtc_enabled = false;
	int i;

	if (sna->mode.num_real_crtc == 0)
		return false;

	if (crtc_id_list) {
		memset((void*) crtc_id_list, 0, sizeof(uint32_t) * MAX_CRTC_NUM);
		crtc_id_list[0] = crtc_id;
	}

	for (i = 0; i < sna->mode.num_real_crtc; i++) {
		if ((rotations[i] = xf86_config->crtc[i]->rotation & 0xf)
					!= RR_Rotate_0)
			rotated = true;

		/* Keep track of which crtc's are enabled */
		enabled[i] =
			(xf86_config->crtc[i]->enabled &&
			 sna_crtc_is_powered(xf86_config->crtc[i]));

		all_enabled &= enabled[i];

		if (crtc_id == sna_crtc_id(xf86_config->crtc[i]) &&
			rotations[i] == RR_Rotate_0)
				crtc_enabled = enabled[i];
	}

	/* check for extended / clone /single mode */
	if (sna->mode.num_real_crtc == MAX_CRTC_NUM && all_enabled) {
		if((xf86_config->crtc[0]->x == xf86_config->crtc[1]->x) &&
			(xf86_config->crtc[0]->y == xf86_config->crtc[1]->y)) {
			/* Clone mode */
			*num_planes = 2;
			*mode = SPRITE_MODE_CLONE;
			if (crtc_id_list) {
				crtc_id_list[0] = sna_crtc_id(xf86_config->crtc[0]);
				crtc_id_list[1] = sna_crtc_id(xf86_config->crtc[1]);
			}
			if (rotated)
				crtc_enabled = false;
		} else {
			/* Extended mode */
			*num_planes = 1;
			*mode = SPRITE_MODE_EXTENDED;
		}
	} else {
		/* Single mode */
		*num_planes = 1;
		*mode = SPRITE_MODE_SINGLE;
	}

	return crtc_enabled;
}

bool sna_video_sprite_plane_grab(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes,
		uint32_t plane_reservation)
{
	int mode = 0;
	struct sna_plane *p = NULL;
	int i = 0;
	int planes_required = 0;
	uint32_t crtc_id_list[MAX_CRTC_NUM];

	planes->num_planes = 0;
	if (!sna_video_sprite_check_mode(draw, crtc_id, &mode, &planes_required,
			crtc_id_list)) {
		return false;
	}

	for ( i = 0 ; i < planes_required ; i++ ) {
		xorg_list_for_each_entry(p, &sna->sna_planes, link) {
			if( ((1 << crtc_id_list[i]) & p->crtc_ids_mask) && !p->locked
					&& (p->reserved_api & plane_reservation)) {
				planes->plane_list[planes->num_planes++] = p;
				p->locked = 1;
				p->drawable = draw;
				p->plane_ptr->crtc_id = crtc_id_list[i];
				RegionNull(&p->colorkey_clip);
				p->active = 0;
				planes->updated = true;
				break;
			}
		}
	}
	if (planes->num_planes != planes_required) {
		for ( i = 0 ; i < planes->num_planes ; i++ ) {
			p = planes->plane_list[i];
			p->locked = 0;
			p->drawable = NULL;
			p->plane_ptr->crtc_id = 0;
		}
		planes->updated = false;
		planes->num_planes = 0;
	}

	assert(planes->num_planes <= MAX_SPRITE_PLANES);

	return planes->num_planes > 0;
}

/**
 * Hide plane but keep it associated with the drawable
 *
 * @param sna Main state
 * @param p Sprite plane to hide
 */
static void sna_video_sprite_hide(
	struct sna *sna,
	struct sna_plane* p)
{
	assert( p );
	if (p->active) {
		if (drmModeSetPlane(sna->kgem.fd, p->plane_ptr->plane_id,
				p->plane_ptr->crtc_id,
				0, 0, 0, 0,	0, 0, 0, 0, 0, 0)){
			xf86DrvMsg(sna->scrn->scrnIndex, X_ERROR,
					"failed to disable plane\n");
		}
		p->active = 0;
	}
	p->locked = 0;
	RegionNull(&p->colorkey_clip);
}

void sna_video_sprite_ungrab(
	struct sna *sna,
	DrawablePtr draw)
{
	struct sna_plane *p = NULL;

	xorg_list_for_each_entry(p, &sna->sna_planes, link) {
		if( p->drawable == draw) {
			sna_video_sprite_hide(sna, p);
			p->drawable = NULL;
		}
	}
}

/**
 * Initializes and parses the sprite color parameters from Xorg
 * Configuration.
 *
 * @param video pointer to sna_video structure
 */

static void sna_video_sprite_parse_options(struct sna_video *video)
{
	struct sna *sna = video->sna;

	if (!xf86GetOptValInteger(sna->Options, OPTION_BRIGHTNESS,
				(int *) &video->brightness)) {
		video->brightness = MID_BRIGHTNESS_YUV;	/* default middle brightness */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->brightness, SNA_BRIGHTNESS_MIN,
				SNA_BRIGHTNESS_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid BRIGHTNESS %d , set brightness to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->brightness,
			MID_BRIGHTNESS_YUV);
		video->brightness = MID_BRIGHTNESS_YUV;
	}


	if (!xf86GetOptValInteger(sna->Options, OPTION_CONTRAST,
				(int *) &video->contrast)) {
		video->contrast = MID_CONTRAST_YUV;	/* default middle contrast */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->contrast, SNA_CONTRAST_MIN,
				SNA_CONTRAST_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid CONTRAST %d , set contrast to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->contrast,
			MID_CONTRAST_YUV);
		video->contrast = MID_CONTRAST_YUV;
	}


	if (!xf86GetOptValInteger(sna->Options, OPTION_SATURATION,
				(int *) &video->saturation)) {
		video->saturation = MID_SATURATION_YUV;	/* default middle saturation */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->saturation, SNA_SATURATION_MIN,
				SNA_SATURATION_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid SATURATION %d , set saturation to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->saturation,
			MID_SATURATION_YUV);
		video->saturation = MID_SATURATION_YUV;
	}


	if (!xf86GetOptValInteger(sna->Options, OPTION_HUE,
				(int *) &video->hue)) {
		video->hue = MID_HUE_YUV;	/* default middle hue */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->hue, SNA_HUE_MIN, SNA_HUE_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid HUE %d , set hue to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->hue,
			MID_HUE_YUV);
		video->hue = MID_HUE_YUV;
	}


	if (!xf86GetOptValInteger(sna->Options, OPTION_GAMMA_RED,
				(int *) &video->gamma_red)) {
		video->gamma_red = SNA_GAMMA_MID;	/* default middle gamma */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->gamma_red, SNA_GAMMA_MIN, SNA_GAMMA_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid GAMMA RED %d , set gamma_red to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->gamma_red,
			SNA_GAMMA_MID);
		video->gamma_red = SNA_GAMMA_MID;
	}

	if (!xf86GetOptValInteger(sna->Options, OPTION_GAMMA_GREEN,
				(int *) &video->gamma_green)) {
		video->gamma_green = SNA_GAMMA_MID;	/* default middle gamma */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->gamma_green, SNA_GAMMA_MIN, SNA_GAMMA_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid GAMMA GREEN %d , set gamma_green to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->gamma_green,
			SNA_GAMMA_MID);
		video->gamma_green = SNA_GAMMA_MID;
	}

	if (!xf86GetOptValInteger(sna->Options, OPTION_GAMMA_BLUE,
				(int *) &video->gamma_blue)) {
		video->gamma_blue = SNA_GAMMA_MID;	/* default middle gamma */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->gamma_blue, SNA_GAMMA_MIN, SNA_GAMMA_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid GAMMA BLUE %d , set gamma_blue to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->gamma_blue,
			SNA_GAMMA_MID);
		video->gamma_blue = SNA_GAMMA_MID;
	}

	if (!xf86GetOptValBool(sna->Options, OPTION_GAMMA_ENABLE,
				(Bool *) &video->gamma_enable)) {
		video->gamma_enable = XV_GAMMA_DISABLE;	/* default disabled */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->gamma_enable, XV_GAMMA_DISABLE,
				XV_GAMMA_ENABLE)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid GAMMA ENABLE %d , set gamma_enable to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->gamma_enable,
			0);
		video->gamma_enable = XV_GAMMA_DISABLE;
	}

	if (!xf86GetOptValInteger(sna->Options, OPTION_CONSTANT_ALPHA,
				(int *) &video->constant_alpha)) {
		video->constant_alpha = SNA_CONSTANT_ALPHA_MAX;	/* default value is 255 */
	} else if (!SNA_VIDEO_ATTR_VALUE_VALID(video->constant_alpha, SNA_CONSTANT_ALPHA_MIN,
				SNA_CONSTANT_ALPHA_MAX)) {
		xf86DrvMsg(video->sna->scrn->scrnIndex,
			X_WARNING,
			"%s: sna_video pointer %p , Invalid Constant Alpha %d , set constant alpha to default value %d\n",
			__FUNCTION__,
			(void*) video,
			video->constant_alpha,
			SNA_CONSTANT_ALPHA_MAX);
		video->constant_alpha = SNA_CONSTANT_ALPHA_MAX;
	}

	video->attribs_changed |= (BRIGHTNESS_CHANGED |
			CONTRAST_CHANGED | SATURATION_CHANGED | HUE_CHANGED |
			GAMMA_R_CHANGED | GAMMA_G_CHANGED | GAMMA_B_CHANGED |
			GAMMA_ENABLE_CHANGED | CONSTANT_ALPHA_CHANGED);
}


/**
 * Returns a list of sprite planes associated with a drawable.
 *
 * @param sna Main state
 * @param crtc_id ID of crtc with most sprite coverage
 * @param draw Current target drawable
 * @param planes Array of MAX_SPRITE_PLANES which on return will be
 *        populated with the available sprites.
 * @return true if sprites are supported in current configuration
 */
bool sna_video_sprite_planes_from_drawable(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes)
{
	int i;
	int mode = 0;
	int planes_required = 0;
	struct sna_plane *p = NULL;
	memset(planes, 0, sizeof(struct sna_video_sprite_planes));

	if (!sna_video_sprite_check_mode(draw, crtc_id, &mode, &planes_required,
			NULL)) {
		sna_video_sprite_ungrab(sna, draw);
		return false;
	}

	xorg_list_for_each_entry(p, &sna->sna_planes, link) {
		if( p->drawable == draw && planes->num_planes < MAX_SPRITE_PLANES) {
			planes->plane_list[planes->num_planes++] = p;
			p->locked = 1;
		}
	}

	if (planes->num_planes > 1) {
		return true;
	}

	if (mode != SPRITE_MODE_CLONE && planes->num_planes == 1 &&
			planes->plane_list[0]->plane_ptr->crtc_id == crtc_id) {
			return true;
	}

	for ( i = 0 ; i < planes->num_planes ; i++ ) {
		sna_video_sprite_hide(sna, planes->plane_list[i]);
	}
	planes->num_planes = 0;
	return true;
}

static bool sna_video_sprite_plane_get(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes)
{
	if (!sna_video_sprite_planes_from_drawable( sna, crtc_id, draw, planes))
		return false;

	if (planes->num_planes > 0)
		return true;

	return sna_video_sprite_plane_grab( sna, crtc_id, draw, planes,
			SPRITE_RESERVED_XV);
}


static int sna_video_sprite_stop(ClientPtr client,
				 XvPortPtr port,
				 DrawablePtr draw)
{
	struct sna_video *video = port->devPriv.ptr;
	PixmapPtr pixmap = NULL;
	struct sna_pixmap* sna_pix = NULL;
	ScreenPtr p_screen;

	if (video == NULL)
		return Success;

	/* Mark the sprite attribs for reapply in case the window is
	 * minimized or hidden.
	 * Apply default if the window is being closed (Client is Null)
	 */
	if (video->planes.num_planes) {
		if (client == NULL)
			sna_video_sprite_parse_options(video);
		else
			video->attribs_changed |= (BRIGHTNESS_CHANGED |
					CONTRAST_CHANGED | SATURATION_CHANGED | HUE_CHANGED |
					GAMMA_R_CHANGED | GAMMA_G_CHANGED | GAMMA_B_CHANGED |
					GAMMA_ENABLE_CHANGED | CONSTANT_ALPHA_CHANGED);
	}

	sna_video_sprite_ungrab( video->sna, draw );

	// Free Scaling Pixmaps
	for(int i = 0; i < MAX_VIDEO_BUFFERS; i++){
		pixmap = video->pixmaps[i];
		if(pixmap){
			/* Remove the framebuffer object if it exist */
			sna_pix = sna_pixmap(pixmap);
			if (sna_pix && sna_pix->gpu_bo->delta) {
				drmModeRmFB(video->sna->kgem.fd, sna_pix->gpu_bo->delta);
				sna_pix->gpu_bo->delta = 0;
				sna_pix->gpu_bo->scanout = false;
			}
			if(sna_pix && sna_pix->gpu_bo == video->bo)
			{
				video->bo = NULL; //The bo will be destroyed in DestroyPixmap
			}
			p_screen = pixmap->drawable.pScreen;
			if(p_screen){
				p_screen->DestroyPixmap(pixmap);
				video->pixmaps[i] = NULL;
			}
		}
	}
	video->pixmap_current = 3;
	if (video->bo)
		kgem_bo_destroy(&video->sna->kgem, video->bo);
	video->bo = NULL;
	video->vaxv = 0;
	RegionNull(&video->clip);

	sna_video_free_buffers(video);
	sna_window_set_port((WindowPtr)draw, NULL);

	return Success;
}


static void sna_video_check_and_set_drm_properties(
		struct sna_video *video,
		struct sna_plane *l_plane,
		const uint16_t bit,
		drmModePropertyPtr prop,
		const char* prop_name,
		INT32 value)

{
	if ((video->attribs_changed & bit) && prop) {
		if (drmModeObjectSetProperty(video->sna->kgem.fd,
					l_plane->plane_ptr->plane_id,
					DRM_MODE_OBJECT_PLANE,
					prop->prop_id, value)) {
			xf86DrvMsg(video->sna->scrn->scrnIndex,
					X_ERROR,
					"Failed to set %s property for \
					plane id=%d\n",
					prop_name,
					l_plane->plane_ptr->plane_id);
			return;
		}
	}
	return;
}

static void sna_video_sprite_update_drm_properties(struct sna_video *video)
{
	int i;

	/*For each video attrribute that has changed, update the kernel drm
	 * driver with the new value*/
	if (video->planes.num_planes && video->attribs_changed) {
		for (i = 0; i < video->planes.num_planes; i++) {
			struct sna_plane *l_plane = video->planes.plane_list[i];

			sna_video_check_and_set_drm_properties(video,
					l_plane, BRIGHTNESS_CHANGED,
					l_plane->properties.brightness,
					"Brightness",
					video->brightness);

			sna_video_check_and_set_drm_properties(video,
					l_plane, CONTRAST_CHANGED,
					l_plane->properties.contrast,
					"Contrast",
					video->contrast);

			sna_video_check_and_set_drm_properties(video,
					l_plane, SATURATION_CHANGED,
					l_plane->properties.saturation,
					"Saturation",
					video->saturation);

			sna_video_check_and_set_drm_properties(video,
					l_plane, HUE_CHANGED,
					l_plane->properties.hue,
					"Hue",
					video->hue);

			sna_video_check_and_set_drm_properties(video,
					l_plane, GAMMA_R_CHANGED,
					l_plane->properties.gamma_red,
					"Gamma Red",
					video->gamma_red);

			sna_video_check_and_set_drm_properties(video,
					l_plane, GAMMA_G_CHANGED,
					l_plane->properties.gamma_green,
					"Gamma Green",
					video->gamma_green);

			sna_video_check_and_set_drm_properties(video,
					l_plane, GAMMA_B_CHANGED,
					l_plane->properties.gamma_blue,
					"Gamma Blue",
					video->gamma_blue);

			sna_video_check_and_set_drm_properties(video,
					l_plane, GAMMA_ENABLE_CHANGED,
					l_plane->properties.gamma_enable,
					"Gamma Enable",
					video->gamma_enable);

			sna_video_check_and_set_drm_properties(video,
					l_plane, CONSTANT_ALPHA_CHANGED,
					l_plane->properties.constant_alpha,
					"Constant Alpha",
					video->constant_alpha);
		}
		video->attribs_changed = 0; /*reset the attribute flags*/
	}
	return;
}


static int sna_video_sprite_set_attr(ClientPtr client,
				     XvPortPtr port,
				     Atom attribute,
				     INT32 value)
{
	struct sna_video *video = port->devPriv.ptr;

	if (attribute == xvColorKey) {
		video->color_key_changed = true;
		video->color_key = value;
		DBG(("COLORKEY = %ld\n", (long)value));
	} else if (attribute == xvAlwaysOnTop) {
		DBG(("%s: ALWAYS_ON_TOP: %d -> %d\n", __FUNCTION__,
					video->AlwaysOnTop, !!value));
		video->color_key_changed = true;
		video->AlwaysOnTop = !!value;
	} else if (attribute == xvBrightness) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_BRIGHTNESS_MIN, SNA_BRIGHTNESS_MAX))
			return BadValue;
		DBG(("%s: BRIGHTNESS %d -> %d\n", __FUNCTION__,
					video->brightness, (int)value));
		video->attribs_changed |= BRIGHTNESS_CHANGED;
		video->brightness = value;
	} else if (attribute == xvContrast) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_CONTRAST_MIN, SNA_CONTRAST_MAX))
			return BadValue;
		DBG(("%s: CONTRAST %d -> %d\n", __FUNCTION__,
					video->contrast, (int)value));
		video->contrast = value;
		video->attribs_changed |= CONTRAST_CHANGED;
	} else if (attribute == xvSaturation) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_SATURATION_MIN, SNA_SATURATION_MAX))
			return BadValue;
		DBG(("%s: SATURATION %d -> %d\n", __FUNCTION__,
					video->saturation, (int)value));
		video->attribs_changed |= SATURATION_CHANGED;
		video->saturation = value;
	} else if (attribute == xvHue) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_HUE_MIN, SNA_HUE_MAX))
			return BadValue;
		DBG(("%s: HUE %d -> %d\n", __FUNCTION__,
					video->hue, (int)value));
		video->hue = value;
		video->attribs_changed |= HUE_CHANGED;
	} else if (attribute == xvGammaRed) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_GAMMA_MIN, SNA_GAMMA_MAX))
			return BadValue;
		DBG(("%s: GAMMA RED %d -> %d\n", __FUNCTION__,
					video->gamma_red, (int)value));
		video->gamma_red = value;
		video->attribs_changed |= GAMMA_R_CHANGED;
	} else if (attribute == xvGammaGreen) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_GAMMA_MIN, SNA_GAMMA_MAX))
			return BadValue;
		DBG(("%s: GAMMA GREEN %d -> %d\n", __FUNCTION__,
					video->gamma_green, (int)value));
		video->gamma_green = value;
		video->attribs_changed |= GAMMA_G_CHANGED;
	} else if (attribute == xvGammaBlue) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_GAMMA_MIN, SNA_GAMMA_MAX))
			return BadValue;
		DBG(("%s: GAMMA BLUE %d -> %d\n", __FUNCTION__,
					video->gamma_blue, (int)value));
		video->gamma_blue = value;
		video->attribs_changed |= GAMMA_B_CHANGED;
	} else if (attribute == xvGammaEnable) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, XV_GAMMA_DISABLE, XV_GAMMA_ENABLE))
			return BadValue;
		DBG(("%s: GAMMA ENABLE %d -> %d\n", __FUNCTION__,
					video->gamma_enable, (bool)value));
		video->gamma_enable = value;
		video->attribs_changed |= GAMMA_ENABLE_CHANGED;
#ifdef ENABLE_VAXV
	} else if(attribute == xvvaShmbo) {
		video->vaxv = value;
#endif
	} else if (attribute == xvConstantAlpha) {
		if (!SNA_VIDEO_ATTR_VALUE_VALID(value, SNA_CONSTANT_ALPHA_MIN, SNA_CONSTANT_ALPHA_MAX))
			return BadValue;
		DBG(("%s: CONSTANT ALPHA %d -> %d\n", __FUNCTION__,
					video->constant_alpha, (int)value));
		video->constant_alpha = value;
		video->attribs_changed |= CONSTANT_ALPHA_CHANGED;
	} else {
		return BadMatch;
	}
	return Success;
}

static int sna_video_sprite_get_attr(ClientPtr client,
				     XvPortPtr port,
				     Atom attribute,
				     INT32 *value)
{
	struct sna_video *video = port->devPriv.ptr;

	if (attribute == xvColorKey)
		*value = video->color_key;
	else if (attribute == xvAlwaysOnTop)
		*value = video->AlwaysOnTop;
	else if (attribute == xvBrightness)
		*value = video->brightness;
	else if (attribute == xvContrast)
		*value = video->contrast;
	else if (attribute == xvSaturation)
		*value = video->saturation;
	else if (attribute == xvHue)
		*value = video->hue;
	else if (attribute == xvGammaRed)
		*value = video->gamma_red;
	else if (attribute == xvGammaGreen)
		*value = video->gamma_green;
	else if (attribute == xvGammaBlue)
		*value = video->gamma_blue;
	else if (attribute == xvGammaEnable)
		*value = video->gamma_enable;
	else if (attribute == xvConstantAlpha)
		*value = video->constant_alpha;
#ifdef ENABLE_VAXV
	else if (attribute == xvvaShmbo)
		*value = video->vaxv;
	else if (attribute == xvvaVersion)
		*value = VAXV_VERSION;
	else if (attribute == xvvaCaps)
		*value = VAXV_CAP_ROTATION_180 | VAXV_CAP_DIRECT_BUFFER;
	else if (attribute == xvvaFormat)
		*value = FOURCC_YUY2;
#endif
	else
		return BadMatch;

	return Success;
}

static int sna_video_sprite_best_size(ClientPtr client,
				      XvPortPtr port,
				      CARD8 motion,
				      CARD16 vid_w, CARD16 vid_h,
				      CARD16 drw_w, CARD16 drw_h,
				      unsigned int *p_w,
				      unsigned int *p_h)
{
	struct sna_video *video = port->devPriv.ptr;
	struct sna *sna = video->sna;

	if (sna->kgem.gen >= 075) {
		*p_w = vid_w;
		*p_h = vid_h;
	} else {
		*p_w = drw_w;
		*p_h = drw_h;
	}

	return Success;
}

static void
update_dst_box_to_crtc_coords(struct sna *sna, xf86CrtcPtr crtc, BoxPtr dstBox)
{
	ScrnInfoPtr scrn = sna->scrn;
	int tmp;

	switch (crtc->rotation & 0xf) {
	case RR_Rotate_0:
		dstBox->x1 -= crtc->x;
		dstBox->x2 -= crtc->x;
		dstBox->y1 -= crtc->y;
		dstBox->y2 -= crtc->y;
		break;

	case RR_Rotate_90:
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->y1 - crtc->x;
		dstBox->y1 = scrn->virtualX - tmp - crtc->y;
		tmp = dstBox->x2;
		dstBox->x2 = dstBox->y2 - crtc->x;
		dstBox->y2 = scrn->virtualX - tmp - crtc->y;
		tmp = dstBox->y1;
		dstBox->y1 = dstBox->y2;
		dstBox->y2 = tmp;
		break;

	case RR_Rotate_180:
		tmp = dstBox->x1;
		dstBox->x1 = scrn->virtualX - dstBox->x2 - crtc->x;
		dstBox->x2 = scrn->virtualX - tmp - crtc->x;
		tmp = dstBox->y1;
		dstBox->y1 = scrn->virtualY - dstBox->y2 - crtc->y;
		dstBox->y2 = scrn->virtualY - tmp - crtc->y;
		break;

	case RR_Rotate_270:
		tmp = dstBox->x1;
		dstBox->x1 = scrn->virtualY - dstBox->y1 - crtc->x;
		dstBox->y1 = tmp - crtc->y;
		tmp = dstBox->x2;
		dstBox->x2 = scrn->virtualY - dstBox->y2 - crtc->x;
		dstBox->y2 = tmp - crtc->y;
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->x2;
		dstBox->x2 = tmp;
		break;
	}
}

static bool
sna_video_sprite_show(struct sna *sna,
			  struct sna_plane *plane,
		      struct sna_video *video,
		      struct sna_video_frame *frame,
		      xf86CrtcPtr crtc,
		      DrawablePtr draw,
		      BoxPtr dstBox)
{
	struct drm_mode_set_plane s;

	VG_CLEAR(s);
	s.plane_id = plane->plane_ptr->plane_id;

	update_dst_box_to_crtc_coords(sna, crtc, dstBox);
	if (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
		int tmp = frame->width;
		frame->width = frame->height;
		frame->height = tmp;
	}

#if defined(DRM_I915_SET_SPRITE_COLORKEY)
	if (video->color_key_changed) {
		struct drm_intel_sprite_colorkey set;

		DBG(("%s: updating color key: %x\n",
			 __FUNCTION__, video->color_key));

		set.plane_id = s.plane_id;
		set.min_value = video->color_key;
		set.max_value = video->color_key; /* not used for destkey */
		set.channel_mask = 0x7 << 24 | 0xff << 16 | 0xff << 8 | 0xff << 0;
		set.flags = 0;

		if (!video->AlwaysOnTop)
			set.flags = I915_SET_COLORKEY_DESTINATION;
		if (drmIoctl(sna->kgem.fd,
			     DRM_IOCTL_I915_SET_SPRITE_COLORKEY,
			     &set))
			xf86DrvMsg(sna->scrn->scrnIndex, X_ERROR,
				   "failed to update color key\n");

		video->color_key_changed = false;
	}
#elif defined(DRM_I915_SET_SPRITE_DESTKEY)
	if (video->color_key_changed) {
		struct drm_intel_set_sprite_destkey set;

		DBG(("%s: updating color key: %x\n",
		     __FUNCTION__, video->color_key));

		set.plane_id = s.plane_id;
		set.min_value = video->color_key;
		set.max_value = video->color_key; /* not used for destkey */
		set.channel_mask = 0x7 << 24 | 0xff << 16 | 0xff << 8 | 0xff << 0;
		set.flags = 0;
		if (!video->AlwaysOnTop)
			set.flags = I915_SET_COLORKEY_DESTINATION;

		if (drmIoctl(sna->kgem.fd,
			     DRM_IOCTL_I915_SET_SPRITE_DESTKEY,
			     &set))
			xf86DrvMsg(sna->scrn->scrnIndex, X_ERROR,
				   "failed to update color key\n");

		video->color_key_changed = false;
	}
#endif

	if (frame->bo->delta == 0) {
		uint32_t offsets[4] = {0}, pitches[4] = {0}, handles[4] = {0};
		uint32_t pixel_format;

		handles[0] = frame->bo->handle;
		pitches[0] = frame->pitch[0];
		offsets[0] = 0;

		switch (frame->id) {
		case FOURCC_RGB565:
			pixel_format = DRM_FORMAT_RGB565;
			break;
		case FOURCC_RGB888:
			pixel_format = DRM_FORMAT_XRGB8888;
			break;
		case FOURCC_UYVY:
			pixel_format = DRM_FORMAT_UYVY;
			break;
		case FOURCC_YUY2:
		default:
			pixel_format = DRM_FORMAT_YUYV;
			break;
		}

		DBG(("%s: creating new fb for handle=%d, width=%d, height=%d, stride=%d\n",
		     __FUNCTION__, frame->bo->handle,
		     frame->width, frame->height, pitches[0]));

		if (drmModeAddFB2(sna->kgem.fd,
				  frame->width, frame->height, pixel_format,
				  handles, pitches, offsets,
				  &frame->bo->delta, 0)) {
			xf86DrvMsg(sna->scrn->scrnIndex,
				   X_ERROR, "failed to add fb\n");
			return false;
		}

		frame->bo->scanout = true;
	}

	assert(frame->bo->scanout);
	assert(frame->bo->delta);

	s.crtc_id = sna_crtc_id(crtc);
	s.fb_id = frame->bo->delta;
	s.flags = 0;
	s.crtc_x = dstBox->x1;
	s.crtc_y = dstBox->y1;
	s.crtc_w = dstBox->x2 - dstBox->x1;
	s.crtc_h = dstBox->y2 - dstBox->y1;
	s.src_x = frame->image.x1 << 16;
	s.src_y = frame->image.y1 << 16;
	s.src_w = (frame->image.x2 - frame->image.x1) << 16;
	s.src_h = (frame->image.y2 - frame->image.y1) << 16;

	DBG(("%s: updating crtc=%d, plane=%d, handle=%d [fb %d], dst=(%d,%d)x(%d,%d),"
			"src=(%d,%d)x(%d,%d)\n",
	     __FUNCTION__, s.crtc_id, s.plane_id, frame->bo->handle, s.fb_id,
	     s.crtc_x, s.crtc_y, s.crtc_w, s.crtc_h,
	     s.src_x >> 16, s.src_y >> 16, s.src_w >> 16, s.src_h >> 16));

	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_SETPLANE, &s)) {
		DBG(("SET_PLANE failed: ret=%d\n", errno));
		return false;
	}

	/*
	 * the update properties function should be called after the overlay
	 * plane is setup since it requires the crtc to be associated with
	 * the plane.
	 */
	sna_video_sprite_update_drm_properties(video);

	plane->active = 1;

	frame->bo->domain = DOMAIN_NONE;
	if (video->bo != frame->bo) {
		if (video->bo)
			kgem_bo_destroy(&sna->kgem, video->bo);
		video->bo = kgem_bo_reference(frame->bo);
	}
	return true;
}


static int sna_video_sprite_put_image(ClientPtr client,
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
				      CARD16 width, CARD16 height)
{
	struct sna_video *video = port->devPriv.ptr;
	struct sna *sna = video->sna;
	struct sna_video_frame frame;
	struct sna_video_frame* flip_frame = &frame;
	xf86CrtcPtr crtc = NULL;
	BoxRec dst_box;
	RegionRec clip;
	int ret;
	int s_format_id = format->id;
#ifdef ENABLE_VAXV
	va_xv_put_image_t *xv_put = (va_xv_put_image_t *) buf;
	uint32_t name;
	int i;
#endif
	
	Bool use_blit = false;
	struct sna_video_frame s_frame; // scaled frame
	PixmapPtr pixmap = NULL;
	int pix_index = video->pixmap_current;
	RegionRec pix_clip;
	ScreenPtr screen = draw->pScreen;
	struct sna_pixmap *sna_pix = NULL;
	memset(&frame, 0, sizeof(frame));
	memset(&s_frame, 0, sizeof(s_frame));

	if(src_w != drw_w || src_h != drw_h)
	{
		use_blit = true;
	}

	if (is_planar_fourcc(format->id)
#ifdef FOURCC_VAXV
		&& format->id != FOURCC_VAXV
#endif
		)
	{
		use_blit = true;
		s_format_id = FOURCC_YUY2;
	}

	if (sna->render.video == NULL)
	{
		use_blit = false;
	}

	dst_box.x1 = draw->x + drw_x;
	dst_box.y1 = draw->y + drw_y;
	dst_box.x2 = dst_box.x1 + drw_w;
	dst_box.y2 = dst_box.y1 + drw_h;

	memcpy(&clip.extents, &dst_box, sizeof(BoxRec));
	clip.data = NULL;

	DBG(("%s: always_on_top=%d\n", __FUNCTION__, video->AlwaysOnTop));
	if (!video->AlwaysOnTop)
		RegionIntersect(&clip, &clip, gc->pCompositeClip);
	if (box_empty(&clip.extents))
		goto invisible;

	DBG(("%s: src=(%d, %d),(%d, %d), dst=(%d, %d),(%d, %d), id=%d, sizep=%dx%d, sync?=%d\n",
	     __FUNCTION__,
	     src_x, src_y, src_w, src_h,
	     drw_x, drw_y, drw_w, drw_h,
	     format->id, width, height, sync));

	DBG(("%s: region %d:(%d, %d), (%d, %d)\n", __FUNCTION__,
	     RegionNumRects(&clip),
	     clip.extents.x1, clip.extents.y1,
	     clip.extents.x2, clip.extents.y2));

	crtc = sna_covering_crtc(sna, &clip.extents, NULL);
	if (crtc == NULL)

		goto invisible;

	if (video->planes.num_planes == 0)
		video->color_key_changed = true;
	
	if (!sna_video_sprite_plane_get( sna, sna_crtc_id(crtc), draw,
			&video->planes)) {
#ifdef ENABLE_VAXV
		if (format->id == FOURCC_VAXV){
			xf86DrvMsg(sna->scrn->scrnIndex,
				   X_ERROR, "failed to get a sprite plane for VAXV\n");
			goto invisible;
		}
#endif
		xf86DrvMsg(sna->scrn->scrnIndex,
			   X_ERROR, "failed to get a sprite plane, Using Textured \n");

		video->rotation = RR_Rotate_0;
		video->textured = true;
		// Textured fallback
		return sna_video_textured_put_image(client, draw, port, gc, src_x, src_y,
						src_w, src_h, drw_x, drw_y, drw_w, drw_h, format, buf, sync,
						width, height);
	}

	/* sprites can't handle rotation natively, store it for the copy func */
	video->rotation = crtc->rotation;
	video->textured = false;

#ifdef ENABLE_VAXV
	if (xv_put == 0) {
		goto invisible;
	}
    /* If the size of the buffer same with xvva structure size and it is YUY2
     * format.
     */
    if ((format->id == FOURCC_VAXV) &&
        (xv_put->size == sizeof(va_xv_put_image_t)) &&
        (video->vaxv == 1)) {

        if (xv_put->video_image.pixel_format != FOURCC_YUY2) {
			DBG(("%s: Pixel format in VAXV 0x%lx not supported\n",
				__FUNCTION__, xv_put->video_image.pixel_format));
            return BadValue;
        }

        src_x = 0;
        src_y = 0;
        src_w = ALIGN(xv_put->video_image.buf_width, 2);
        src_h = xv_put->video_image.buf_height;
        sna_video_frame_init(video, FOURCC_YUY2, src_w,
    			src_h, &frame,
    			(use_blit ? 4 : video->alignment));
		frame.image.x1 = 0;
		frame.image.y1 = 0;
		frame.image.x2 = src_w;
		frame.image.y2 = src_h;
		dst_box.x1 = draw->x + drw_x;
		dst_box.y1 = draw->y + drw_y;
		dst_box.x2 = dst_box.x1 + src_w;
		dst_box.y2 = dst_box.y1 + src_h;

        for (i = 0; i < MAX_VIDEO_BUFFERS ; ++i) {
            // Check to see if this buffer are previously imported
            if (video->buf_list[i]) {
                name = kgem_bo_flink(&sna->kgem, video->buf_list[i]);

                if (xv_put->video_image.buf_handle == name) {
                    frame.bo = video->buf_list[i];
                    break;
                }
            }
        }

        // Import this buffer if it not exist previously
        if (i == MAX_VIDEO_BUFFERS) {
            if (video->buf_list[0]) {
            	if (video->buf_list[0]->delta) {
            		drmModeRmFB(video->sna->kgem.fd, video->buf_list[0]->delta);
            	}
            	kgem_bo_destroy(&sna->kgem, video->buf_list[0]);
            	video->buf_list[0] = NULL;
            }

            frame.bo = kgem_create_for_name(&sna->kgem,
            	xv_put->video_image.buf_handle);
			if (frame.bo == NULL){
				ret=BadAlloc;
				goto safe_return;
			}
            video->buf_list[0] = frame.bo;
        }
        frame.pitch[0] = xv_put->video_image.pitches[0];
    } else {
#endif
	sna_video_frame_init(video, format->id, width, height, &frame,
			(use_blit ? 4 : video->alignment));

	if (xvmc_passthrough(format->id)) {
		DBG(("%s: using passthough, name=%d\n",
		     __FUNCTION__, *(uint32_t *)buf));

		if (*(uint32_t*)buf == 0)
			goto invisible;

		frame.bo = kgem_create_for_name(&sna->kgem, *(uint32_t*)buf);
		if (frame.bo == NULL){			
			ret=BadAlloc;
			goto safe_return;
		}

		if (kgem_bo_size(frame.bo) < frame.size) {
			DBG(("%s: bo size=%d, expected=%d\n",
			     __FUNCTION__, kgem_bo_size(frame.bo), frame.size));
			ret=BadAlloc;
			goto safe_return;
		}

		frame.image.x2 = (frame.image.x2 - frame.image.x1);
		frame.image.y2 = (frame.image.y2 - frame.image.y1);
		frame.image.x1 = 0;
		frame.image.y1 = 0;
		frame.width = frame.image.x2 - frame.image.x1;
		frame.height = frame.image.y2 - frame.image.y1;

	} else {
		frame.image.x1 = src_x;
		frame.image.y1 = src_y;
		frame.image.x2 = src_w;
		frame.image.y2 = src_h;
		frame.bo = sna_video_buffer(video, &frame);//reuse a buffer from video->buffer[]
		if (frame.bo == NULL) {
			DBG(("%s: failed to allocate video bo\n", __FUNCTION__));
			ret=BadAlloc;
			goto safe_return;
		}

		if (!sna_video_copy_data(video, &frame, buf)) {
			DBG(("%s: failed to copy video data\n", __FUNCTION__));
			ret=BadAlloc;
			goto safe_return;
		}
	}
#ifdef ENABLE_VAXV
    }
#endif

	ret = Success;

	if(use_blit) {

        int pix_h = drw_h;
        int pix_w = drw_w;
        pix_w = pix_w  & ~1;  /* Width for YUV buffer have to be even in DRM. */

        dst_box.x2 = dst_box.x1 + pix_w;

        pix_index++;
		if(pix_index > (MAX_VIDEO_BUFFERS-1))
		{
			pix_index= 0;
		}

		pixmap = video->pixmaps[pix_index];
		if(pixmap && (pixmap->drawable.width != pix_w ||
               pixmap->drawable.height != pix_h)){
			/* Remove the framebuffer object if it exist */
			sna_pix = sna_pixmap(pixmap);
			if (sna_pix && sna_pix->gpu_bo->delta) {
				drmModeRmFB(sna->kgem.fd, sna_pix->gpu_bo->delta);
				sna_pix->gpu_bo->delta = 0;
				sna_pix->gpu_bo->scanout = false;
			}
			screen->DestroyPixmap(pixmap);
			video->pixmaps[pix_index]=NULL;
			pixmap = NULL;
		}

		if(pixmap == NullPixmap){
			pixmap = screen->CreatePixmap(screen, pix_w, pix_h,
					16, SNA_CREATE_FB);
			if (pixmap == NullPixmap){
				ret=BadAlloc;
				goto safe_return;
			}
			video->pixmaps[pix_index] = pixmap;
		}

		sna_pix = sna_pixmap_move_to_gpu(pixmap, MOVE_READ | MOVE_WRITE);
		if (!sna_pix) {
			DBG(("%s: attempting to render to a non-GPU pixmap\n",
			     __FUNCTION__));
			ret=BadAlloc;
			goto safe_return;
		}

		// Output and Scale the video buffer into a pixmap
		pix_clip.extents.x1 = 0;
		pix_clip.extents.y1 = 0;
		pix_clip.extents.x2 = pix_w;
		pix_clip.extents.y2 = pix_h;
		pix_clip.data = NULL;

		frame.src.x1 = 0;
		frame.src.y1 = 0;
		frame.src.x2 = src_w;
		frame.src.y2 = src_h;

		if (!sna->render.video(sna, video, &frame, &pix_clip, pixmap)) {
			DBG(("%s: failed to render video\n", __FUNCTION__));
			ret=BadAlloc;
			goto safe_return;

		}

		/* Setup frame to contain info for call to sna_video_sprite_show
		 */
		sna_video_frame_init(video, s_format_id,
				pixmap->drawable.width,
				pixmap->drawable.height,
				&s_frame, video->alignment);
		
		s_frame.bo = sna_pix->gpu_bo; //I don't know if we need to release this
		s_frame.image.x1 = 0;
		s_frame.image.y1 = 0;
		s_frame.image.x2 = pix_w;
		s_frame.image.y2 = pix_h;

		flip_frame = &s_frame;

		video->pixmap_current = pix_index;
	}


	if (drw_x > 0 || drw_y > 0 || drw_w < draw->width ||
		drw_h < draw->height) {
		if (!RegionEqual(&video->clip, gc->pCompositeClip)) {
			RegionRec subtract_reg;
			subtract_reg.data = NULL;
			if(RegionSubtract(&subtract_reg, gc->pCompositeClip, &clip)){
				int num_rects = RegionNumRects(&subtract_reg);
				if(num_rects > 0){
					xf86XVFillKeyHelperDrawable(draw, 0,
							&subtract_reg);
				}
			}
			RegionCopy(&video->clip, gc->pCompositeClip);
		}
	}
	sna_video_sprite_xv_show(sna, gc, draw, flip_frame, video, &dst_box, &clip,
			port);

	flip_frame->bo->domain = DOMAIN_NONE;
safe_return:
	if (xvmc_passthrough(format->id) && frame.bo) 
			kgem_bo_destroy(&sna->kgem, frame.bo);
	else sna_video_buffer_fini(video);//append at the end of the pool
	
	return ret;


invisible:
	/* If the video isn't visible on any CRTC, turn it off */
	return sna_video_sprite_stop(client, port, draw);
}

static int sna_video_sprite_query(ClientPtr client,
				  XvPortPtr port,
				  XvImagePtr format,
				  unsigned short *w,
				  unsigned short *h,
				  int *pitches,
				  int *offsets)
{
	struct sna_video *video = port->devPriv.ptr;
	struct sna_video_frame frame;
	int size;

	if (*w > IMAGE_MAX_WIDTH)
		*w = IMAGE_MAX_WIDTH;
	if (*h > IMAGE_MAX_HEIGHT)
		*h = IMAGE_MAX_HEIGHT;

	if (offsets)
		offsets[0] = 0;

	switch (format->id) {
	case FOURCC_RGB888:
	case FOURCC_RGB565:
		sna_video_frame_init(video, format->id, *w, *h, &frame,
				video->alignment);
		if (pitches)
			pitches[0] = frame.pitch[0];
		size = 4;
		break;

	case FOURCC_YV12:
	case FOURCC_I420:
		*h = ALIGN(*h, 2);
		*w = ALIGN(*w, 2);
		sna_video_frame_init(video, format->id, *w, *h, &frame, 4);
		if (pitches) {
			pitches[0] = frame.pitch[1];
			pitches[1] = frame.pitch[0];
			pitches[2] = frame.pitch[0];
		}
		if (offsets) {
			offsets[1] = frame.UBufOffset;
			offsets[2] = frame.VBufOffset;
		}
		size = frame.size;
		break;

	default:
		*w = (*w + 1) & ~1;
		*h = (*h + 1) & ~1;

		size = *w << 1;
		if (pitches)
			pitches[0] = size;
		size *= *h;
		break;
	}

	return size;
}

static int sna_video_sprite_color_key(struct sna *sna)
{
	ScrnInfoPtr scrn = sna->scrn;
	int color_key;

	if (xf86GetOptValInteger(sna->Options, OPTION_VIDEO_KEY,
				 &color_key)) {
	} else if (xf86GetOptValInteger(sna->Options, OPTION_COLOR_KEY,
					&color_key)) {
	} else {
		color_key =
		    (1 << scrn->offset.red) |
		    (1 << scrn->offset.green) |
		    (((scrn->mask.blue >> scrn->offset.blue) - 1) << scrn->offset.blue);
	}

	return color_key & ((1 << scrn->depth) - 1);
}

void sna_video_sprite_setup(struct sna *sna, ScreenPtr screen)
{
	XvAdaptorPtr adaptor;
	struct sna_video *video;
	struct sna_plane *plane=NULL;
	int i, nports = 0;

	if (sna->flags & SNA_IS_HOSTED)
		return;

	xorg_list_for_each_entry(plane, &sna->sna_planes, link) {
		++nports;
	}

	if( nports == 0 )
		return;

	adaptor = sna_xv_adaptor_alloc(sna);
	if (!adaptor)
		return;

	video = calloc(nports, sizeof(*video));
	adaptor->pPorts = calloc(nports, sizeof(XvPortRec));
	if (video == NULL || adaptor->pPorts == NULL) {
		free(video);
		free(adaptor->pPorts);
		sna->xv.num_adaptors--;
		return;
	}

	adaptor->type = XvInputMask | XvImageMask;
	adaptor->pScreen = screen;
	adaptor->name = (char *)"Intel(R) Video Sprite";
	adaptor->nEncodings = 1;
	adaptor->pEncodings = xnfalloc(sizeof(XvEncodingRec));
	adaptor->pEncodings[0].id = 0;
	adaptor->pEncodings[0].pScreen = screen;
	adaptor->pEncodings[0].name = (char *)"XV_IMAGE";
	adaptor->pEncodings[0].width = IMAGE_MAX_WIDTH;
	adaptor->pEncodings[0].height = IMAGE_MAX_HEIGHT;
	adaptor->pEncodings[0].rate.numerator = 1;
	adaptor->pEncodings[0].rate.denominator = 1;
	adaptor->pFormats = formats;
	adaptor->nFormats = sna_xv_fixup_formats(screen, formats,
						 ARRAY_SIZE(formats));
	adaptor->nAttributes = ARRAY_SIZE(attribs);
	adaptor->pAttributes = (XvAttributeRec *)attribs;
	adaptor->pImages = (XvImageRec *)images;
	adaptor->nImages = 3;
	if (sna->kgem.gen == 071)
		adaptor->nImages = (sizeof(images)/sizeof(XF86ImageRec));

	adaptor->ddAllocatePort = sna_xv_alloc_port;
	adaptor->ddFreePort = sna_xv_free_port;
	adaptor->ddPutVideo = NULL;
	adaptor->ddPutStill = NULL;
	adaptor->ddGetVideo = NULL;
	adaptor->ddGetStill = NULL;
	adaptor->ddStopVideo = sna_video_sprite_stop;
	adaptor->ddSetPortAttribute = sna_video_sprite_set_attr;
	adaptor->ddGetPortAttribute = sna_video_sprite_get_attr;
	adaptor->ddQueryBestSize = sna_video_sprite_best_size;
	adaptor->ddPutImage = sna_video_sprite_put_image;
	adaptor->ddQueryImageAttributes = sna_video_sprite_query;

	for( i = 0; i < nports; ++i )
	{
		struct sna_video *v = &video[i];
		XvPortPtr port = &adaptor->pPorts[i];

		v->sna = sna;
		/* Actual alignment is 64 bytes. But the stride must
		 * be at least 512 bytes.  Take the easy fix and align on 512
		 * bytes unconditionally.
		 */
		v->alignment = 512;
		v->color_key = sna_video_sprite_color_key(sna);
		v->color_key_changed = true;
		v->desired_crtc = NULL;
		sna_video_sprite_parse_options(v);
		v->rotation = RR_Rotate_0;
		v->pixmap_current = MAX_VIDEO_BUFFERS;
		v->vaxv = 0;
		RegionNull(&v->clip);

		port->id = FakeClientID(0);
		AddResource(port->id, XvGetRTPort(), port);
		port->pAdaptor = adaptor;
		port->pNotify =  NULL;
		port->pDraw =  NULL;
		port->client =  NULL;
		port->grab.client =  NULL;
		port->time = currentTime;
		port->devPriv.ptr = v;
	}

	adaptor->base_id = adaptor->pPorts[0].id;
	adaptor->nPorts = nports;

	xvColorKey = MAKE_ATOM("XV_COLORKEY");
	xvAlwaysOnTop = MAKE_ATOM("XV_ALWAYS_ON_TOP");
	xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
	xvContrast = MAKE_ATOM("XV_CONTRAST");
	xvSaturation = MAKE_ATOM("XV_SATURATION");
	xvHue = MAKE_ATOM("XV_HUE");
	xvGammaRed = MAKE_ATOM("XV_GAMMA_RED");
	xvGammaGreen = MAKE_ATOM("XV_GAMMA_GREEN");
	xvGammaBlue = MAKE_ATOM("XV_GAMMA_BLUE");
	xvGammaEnable = MAKE_ATOM("XV_GAMMA_ENABLE");
	xvConstantAlpha = MAKE_ATOM("XV_CONSTANT_ALPHA");
#ifdef ENABLE_VAXV
	xvvaShmbo = MAKE_ATOM(VAXV_ATTR_STR);
	xvvaVersion = MAKE_ATOM(VAXV_VERSION_ATTR_STR);
	xvvaCaps = MAKE_ATOM(VAXV_CAPS_ATTR_STR);
	xvvaFormat = MAKE_ATOM(VAXV_FORMAT_ATTR_STR);
#endif
}

/**************************/

/**
 * Updates target and source sizes so that alignment is suitable for
 * sprite hardware.
 *
 * @param frame Video target frame state
 * @param dst_box Target position to display video
 */
static void sna_video_sprite_dst_size_adjust(
		struct sna_video_frame* frame,
		BoxPtr dst_box)
{
	int dst_w = dst_box->x2 - dst_box->x1;
	int src_w = frame->image.x2 - frame->image.x1;

	if (src_w < dst_w) {
		dst_box->x2 = dst_box->x1 + src_w;
	} else if (src_w > dst_w) {
		src_w = dst_w;
		if (frame->id != FOURCC_RGB888 && frame->id != FOURCC_RGB565) {
			src_w = src_w & ~1;
			dst_box->x2 = dst_box->x1 + src_w;
		}
		frame->image.x2 = frame->image.x1 + src_w;
	}
}

/**
 * DRI2 function to present a sprite on multiple displays.  This is a cut down
 * version of the sna_video_sprite_put_image function.
 *
 * @param sna Main state
 * @param draw Drawable target
 * @param planes List of sprite planes to display
 * @param src_bo Source bo to display
 * @return true if one or more sprites were displayed
 */
bool sna_video_sprite_dri_show(
		struct sna *sna,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes,
		struct kgem_bo *src_bo)
{
	struct sna_video video = {0};
	struct sna_video_frame frame = {0};
	xf86CrtcPtr crtc = NULL;
	BoxRec dst_box;
	RegionRec clip;
	uint32_t width, height;
	uint32_t src_x, src_y, src_w, src_h;
	uint32_t drw_x, drw_y, drw_w, drw_h;
	int i, j;
	int sprites_visible = 0;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);

	/* Return if we no longer own the sprite.  Sometimes this function
	 * can be called after the Window has been closed. */
	if (planes->num_planes == 0 || planes->plane_list[0]->drawable != draw)
		return false;

	/* Fill the initial stuff */
	width = draw->width;
	height = draw->height;
	src_x = src_y = drw_x = drw_y = 0;
	src_w = drw_w = width;
	src_h = drw_h = height;

	video.sna = sna;
	video.alignment = 64;
	video.color_key = sna_video_sprite_color_key(sna);
	video.color_key_changed = true;
	sna_video_sprite_parse_options(&video);
	video.rotation = RR_Rotate_0;
	RegionNull(&video.clip);

	clip.extents.x1 = draw->x + drw_x;
	clip.extents.y1 = draw->y + drw_y;
	clip.extents.x2 = clip.extents.x1 + drw_w;
	clip.extents.y2 = clip.extents.y1 + drw_h;
	clip.data = NULL;

	if (draw->type == DRAWABLE_WINDOW) {
		WindowPtr win = (WindowPtr)draw;

		if (is_clipped(&win->clipList, draw)) {
			DBG(("%s: draw=(%d, %d), delta=(%d, %d), clip.extents=(%d, %d), (%d, %d)\n",
						__FUNCTION__, draw->x, draw->y,
						get_drawable_dx(draw), get_drawable_dy(draw),
						win->clipList.extents.x1, win->clipList.extents.y1,
						win->clipList.extents.x2, win->clipList.extents.y2));

			pixman_region_intersect(&clip, &win->clipList, &clip);
		}
	}

	sna_video_frame_init(&video, FOURCC_RGB888, width, height, &frame,
			video.alignment);

	for ( i = 0 ; i < planes->num_planes ; i++ )
	{
		struct sna_plane* p = planes->plane_list[i];
		crtc = NULL;
		video.desired_crtc = NULL;
		if (xf86_config && planes->num_planes > 1)
		{
			for ( j = 0 ; j < sna->mode.num_real_crtc ; j++ ) {
				if (sna_crtc_id(xf86_config->crtc[j]) ==
						p->plane_ptr->crtc_id &&
					xf86_config->crtc[j]->enabled)
				{
					video.desired_crtc = xf86_config->crtc[j];
					break;
				}
			}
		}
	if (!sna_video_clip_helper(&video, &frame,
					   &crtc, &dst_box,
					   src_x, src_y, draw->x + drw_x, draw->y + drw_y,
					   src_w, src_h, drw_w, drw_h,
					   &clip))
		{
			sna_video_sprite_hide(sna, p);
			continue;
		}

		if (!crtc || (planes->num_planes > 1 && crtc != video.desired_crtc))
		{
			sna_video_sprite_hide(sna, p);
			continue;
		}
		if (box_empty(&clip.extents))
		{
			sna_video_sprite_hide(sna, p);
			continue;
		}

		sna_video_sprite_dst_size_adjust(&frame, &dst_box);

		frame.bo = src_bo;
		frame.pitch[0] = frame.bo->pitch; // sna_video_frame_init calculates this
										  // wrongly
		video.tiled = frame.bo->tiling;
		video.planes.num_planes = 0;      /* Not used */
		video.bo = frame.bo;

		video.color_key_changed = true;
		if (!sna_video_sprite_show(sna, p, &video, &frame,
				crtc, draw, &dst_box)) {
			DBG(("%s: failed to show video frame\n", __FUNCTION__));
			return false;
		}
		if (!RegionEqual(&p->colorkey_clip, &clip) || planes->updated) {
			xf86XVFillKeyHelperDrawable(draw, video.color_key, &clip);
			RegionCopy(&p->colorkey_clip, &clip);
		}
		sprites_visible++;
	}

	return (sprites_visible > 0);
}

/**
 * XVideo function to present a sprite on multiple displays.
 *
 * @param sna Main state
 * @param gc A pointer to the graphics context
 * @param draw Drawable target
 * @param frame Video target frame state
 * @param video General video state
 * @param dst_box Target position to display video
 * @param clip Clip region to display color key
 * @param port Current XVideo port
 *
 * @return true if one or more sprites were displayed
 */
bool sna_video_sprite_xv_show(
		struct sna *sna,
		GCPtr gc,
		DrawablePtr draw,
		struct sna_video_frame *frame,
		struct sna_video *video,
		BoxPtr dst_box,
		RegionPtr clip,
		XvPortPtr port)
{
	xf86CrtcPtr crtc = NULL;
	int i=0, j;
	struct sna_video_sprite_planes* planes = &video->planes;
	struct sna_plane* p = NULL;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	int sprites_visible = 0;
	int image_dst_x = dst_box->x1;
	int image_dst_y = dst_box->y1;
	int image_dst_w = dst_box->x2 - dst_box->x1;
	int image_dst_h = dst_box->y2 - dst_box->y1;
	int image_src_w = frame->image.x2 - frame->image.x1;
	int image_src_h = frame->image.y2 - frame->image.y1;

	for ( i = 0 ; i < planes->num_planes ; i++ ){
		p = planes->plane_list[i];
		video->desired_crtc = NULL;
		if (xf86_config && planes->num_planes > 1) {
			for ( j = 0 ; j < sna->mode.num_real_crtc ; j++ ) {
				if (sna_crtc_id(xf86_config->crtc[j]) ==
						p->plane_ptr->crtc_id &&
					xf86_config->crtc[j]->enabled){
					video->desired_crtc = xf86_config->crtc[j];
					break;
				}
			}
		}
		if (!sna_video_clip_helper(video, frame, &crtc,
						dst_box, 0,  0,
						image_dst_x, image_dst_y,
						image_src_w, image_src_h,
						image_dst_w, image_dst_h,
					   clip)){
			sna_video_sprite_hide(sna, p);
			continue;
		}

		if (!crtc || (planes->num_planes > 1 && crtc != video->desired_crtc)){
			sna_video_sprite_hide(sna, p);
			continue;
		}
		if (box_empty(&clip->extents)){
			sna_video_sprite_hide(sna, p);
			continue;
		}


		sna_video_sprite_dst_size_adjust(frame, dst_box);

		video->color_key_changed = true;
		if (!sna_video_sprite_show(sna, p, video, frame,
				crtc, draw, dst_box)) {
			DBG(("%s: failed to show video frame for plane %d\n",
					__FUNCTION__, p->plane_ptr->plane_id));
			sna_video_sprite_hide(sna, p);
			continue;
		}
		if (!video->AlwaysOnTop && !RegionEqual(&p->colorkey_clip, clip)) {
			xf86XVFillKeyHelperDrawable(draw, video->color_key, clip);
			RegionCopy(&p->colorkey_clip, clip);
		}
		sna_window_set_port((WindowPtr)draw, port);
		sprites_visible++;
	}

	return (sprites_visible > 0);
}

#else

void sna_video_sprite_setup(struct sna *sna, ScreenPtr screen)
{
}

bool sna_video_sprite_plane_grab(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes)
{
	return false;
}

void sna_video_sprite_ungrab(
		struct sna *sna,
		DrawablePtr draw)
{
}

struct sna_plane* sna_video_sprite_planes_from_drawable(
		struct sna *sna,
		uint32_t crtc_id,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes)
{
	return NULL;
}

bool sna_video_sprite_dri_show(
		struct sna *sna,
		DrawablePtr draw,
		struct sna_video_sprite_planes* planes,
		struct kgem_bo *src_bo)
{
	return false;
}

#endif
