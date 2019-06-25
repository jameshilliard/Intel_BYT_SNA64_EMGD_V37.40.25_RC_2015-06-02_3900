/* Copyright (c) 2002-2014, Intel Corporation.
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
 */
#include "emgduid.h"

static item_map_t ovlMap[] = {
	{
		"gamma_red",                    /* sysName */
		"global_sliderValueOverlayGammaR", /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"gamma_green",                  /* sysName */
		"global_sliderValueOverlayGammaG", /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"gamma_blue",                   /* sysName */
		"global_sliderValueOverlayGammaB", /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"brightness",                   /* sysName */
		"global_sliderValueOverlay2",      /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"contrast",                     /* sysName */
		"global_sliderValueOverlay3",      /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"saturation",                   /* sysName */
		"global_sliderValueOverlay4",      /* htmlName */
		NULL,                              /* urlName */
	},
	{
		"hue",                          /* sysName */
		"global_sliderValueOverlay5",      /* htmlName */
		NULL,                              /* urlName */
	}
};

static item_map_t fbMap[] = {
	{
		"fb_width",   /* sysName */
		"width",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_height",   /* sysName */
		"height",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_depth",   /* sysName */
		"depth",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_bpp",   /* sysName */
		"bpp",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_pitch",   /* sysName */
		"pitch",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_tiling",   /* sysName */
		"tiling",      /* htmlName */
		NULL,         /* urlName */
	},
	{
		"fb_pin_count",   /* sysName */
		"pin count",      /* htmlName */
		NULL,         /* urlName */
	}
};

static crtc_map_t crtcMap[] = {
	{
		"DPMS", /* Description */
		"DPMS", /* sysName */
		NULL,    /* urlName */
		NULL,    /* htmlName */
		CRTC_TYPE_READONLY
	},
	{
		"Framebuffer blend overlay", /* Description */
		"fb_blend_ovl", /* sysName */
		"blend_ovl",    /* urlName */
		NULL,    /* htmlName */
		CRTC_TYPE_CHECKBOX
	},
	{
		"Palette Persistence", /* Description */
		"palette_persistence", /* sysName */
		"palette_persistence",    /* urlName */
		NULL,    /* htmlName */
		CRTC_TYPE_CHECKBOX
	},
	{
		"Overlay plane z-order", 
		"ovl_zorder",
		"ovl_zorder",
		"z", /* htmlName */
		CRTC_TYPE_DIV
	}
};

static color_t colorMap[] {
	{
		"brightness", /* Description */
		"brightness", /* sysName */
		1,            /* scale */
		{
			"sliderBrightR",  /* urlName */
		},
		{
			"sliderBrightG",  /* urlName */
		},
		{
			"sliderBrightB",  /* urlName */
		}
	},
	{
		"contrast", /* Description */
		"contrast", /* sysName */
		1,            /* scale */
		{
			"sliderContrastR",  /* urlName */
		},
		{
			"sliderContrastG",  /* urlName */
		},
		{
			"sliderContrastB",  /* urlName */
		}
	},
	{
		"gamma", /* Description */
		"gamma", /* sysName */
		SCALE_FB_GAMMA,  /* scale */
		{
			"sliderGammaR",  /* urlName */
		},
		{
			"sliderGammaG",  /* urlName */
		},
		{
			"sliderGammaB",  /* urlName */
		}
	},
};

map_t map = {
	ovlMap,
	sizeof(ovlMap)/sizeof(ovlMap[0]),
	fbMap,
	sizeof(fbMap)/sizeof(fbMap[0]),
	crtcMap,
	sizeof(crtcMap)/sizeof(crtcMap[0]),
	colorMap,
	sizeof(colorMap)/sizeof(colorMap[0])
};

