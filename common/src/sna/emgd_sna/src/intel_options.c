#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "intel_options.h"

const OptionInfoRec intel_options[] = {
	{OPTION_ACCEL_DISABLE,	"NoAccel",	OPTV_BOOLEAN,	{0},	0},
	{OPTION_ACCEL_METHOD,	"AccelMethod",	OPTV_STRING,	{0},	0},
	{OPTION_BACKLIGHT,	"Backlight",	OPTV_STRING,	{0},	0},
	{OPTION_DRI,		"DRI",		OPTV_STRING,	{0},	0},
	{OPTION_COLOR_KEY,	"ColorKey",	OPTV_INTEGER,	{0},	0},
	{OPTION_VIDEO_KEY,	"VideoKey",	OPTV_INTEGER,	{0},	0},
	{OPTION_TILING_2D,	"Tiling",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_TILING_FB,	"LinearFramebuffer",	OPTV_BOOLEAN,	{0},	0},
	{OPTION_SWAPBUFFERS_WAIT, "SwapbuffersWait", OPTV_BOOLEAN,	{0},	1},
	{OPTION_TRIPLE_BUFFER,	"TripleBuffer", OPTV_BOOLEAN,	{0},	1},
	{OPTION_PREFER_OVERLAY, "XvPreferOverlay", OPTV_BOOLEAN, {0}, 0},
	{OPTION_XV_ADAPTERS, "XvAdapters", OPTV_STRING, {0}, 0},
	{OPTION_BRIGHTNESS, "XvBrightness", OPTV_INTEGER, {0}, FALSE},
	{OPTION_CONTRAST, "XvContrast", OPTV_INTEGER, {0}, FALSE},
	{OPTION_SATURATION, "XvSaturation", OPTV_INTEGER, {0}, FALSE},
	{OPTION_HUE, "XvHue", OPTV_INTEGER, {0}, FALSE},
	{OPTION_GAMMA_RED, "XvGammaRed", OPTV_INTEGER, {0}, FALSE},
	{OPTION_GAMMA_GREEN, "XvGammaGreen", OPTV_INTEGER, {0}, FALSE},
	{OPTION_GAMMA_BLUE, "XvGammaBlue", OPTV_INTEGER, {0}, FALSE},
	{OPTION_GAMMA_ENABLE, "XvGammaEnable", OPTV_BOOLEAN, {0}, FALSE},
	{OPTION_CONSTANT_ALPHA, "XvConstantAlpha", OPTV_INTEGER, {0}, FALSE},
	{OPTION_HOTPLUG,	"HotPlug",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_REPROBE,	"ReprobeOutputs", OPTV_BOOLEAN,	{0},	0},
#ifdef INTEL_XVMC
	{OPTION_XVMC,	"XvMC",		OPTV_BOOLEAN,	{0},	1},
#endif
#ifdef USE_SNA
	{OPTION_QB_SPLASH,     "SplashScreen",     OPTV_BOOLEAN, {0}, FALSE},
	{OPTION_ZAPHOD,		"ZaphodHeads",	OPTV_STRING,	{0},	0},
	{OPTION_VIRTUAL,	"VirtualHeads",	OPTV_INTEGER,	{0},	0},
	{OPTION_TEAR_FREE,	"TearFree",	OPTV_BOOLEAN,	{0},	0},
	{OPTION_CRTC_PIXMAPS,	"PerCrtcPixmaps", OPTV_BOOLEAN,	{0},	0},
#endif
#ifdef USE_UXA
	{OPTION_FALLBACKDEBUG,	"FallbackDebug",OPTV_BOOLEAN,	{0},	0},
	{OPTION_DEBUG_FLUSH_BATCHES, "DebugFlushBatches", OPTV_BOOLEAN, {0}, 0},
	{OPTION_DEBUG_FLUSH_CACHES, "DebugFlushCaches", OPTV_BOOLEAN, {0}, 0},
	{OPTION_DEBUG_WAIT, "DebugWait", OPTV_BOOLEAN, {0}, 0},
	{OPTION_BUFFER_CACHE,	"BufferCache",	OPTV_BOOLEAN,   {0},    1},
#endif
	{OPTION_PLANE_A_RESERVATION, "PlaneAReservation", OPTV_ANYSTR, {0}, FALSE},
	{OPTION_PLANE_B_RESERVATION, "PlaneBReservation", OPTV_ANYSTR, {0}, FALSE},
	{OPTION_PLANE_C_RESERVATION, "PlaneCReservation", OPTV_ANYSTR, {0}, FALSE},
	{OPTION_PLANE_D_RESERVATION, "PlaneDReservation", OPTV_ANYSTR, {0}, FALSE},
	{OPTION_PLANE_ZORDER_CRTC0,  "PlaneZOrderCRTC0",  OPTV_ANYSTR, {0}, FALSE},
	{OPTION_PLANE_ZORDER_CRTC1,  "PlaneZOrderCRTC1",  OPTV_ANYSTR, {0}, FALSE},
	{-1,			NULL,		OPTV_NONE,	{0},	0}
};

OptionInfoPtr intel_options_get(ScrnInfoPtr scrn)
{
	OptionInfoPtr options;

	xf86CollectOptions(scrn, NULL);
	if (!(options = malloc(sizeof(intel_options))))
		return NULL;

	memcpy(options, intel_options, sizeof(intel_options));
	xf86ProcessOptions(scrn->scrnIndex, scrn->options, options);

	return options;
}
