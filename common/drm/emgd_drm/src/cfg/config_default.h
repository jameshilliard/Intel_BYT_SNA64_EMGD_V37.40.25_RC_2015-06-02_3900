/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: config_default.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
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
 *-----------------------------------------------------------------------------
 * Description:
 *  This file is used in conjunction with the platform's config.h to
 *  gererate a full set of build defines. This file should provide defaults
 *  for defines such that a platform's config.h can include only the
 *  minimal set of non-standard options.
 *  Defines should be named such that:
 *  CONFIG_<FOO>: Is defined or undefined suitable for use with ifdef and
 *  can be used with the build system's DIRS_FOO or OBJECTS_FOO. Any
 *  CONFIG_FOO added here must also have an entry in config_helper.c.
 *  CONFIG_ENABLE_FOO: Should be defined always and defined to a 1 or 0.
 *  This is suitble for use in if(CONFIG_ENABLE_FOO) and expected that
 *  a compiler will optimize away if(0)'s.
 *  CONFIG_LIMIT_FOO: Should prevent some default set of FOO defines
 *  from being included. For instance CONFIG_LIMIT_MODES prevents the
 *  long default list of default modes from being used and instead the
 *  platform's config.h must define the requested modes manually.
 *-----------------------------------------------------------------------------
 */

#ifndef _HAL_CONFIG_DEFAULT_H
#define _HAL_CONFIG_DEFAULT_H

#ifndef CONFIG_MICRO
#define CONFIG_FULL
#endif

#ifndef CONFIG_LIMIT_CORES
#define CONFIG_SNB
#define CONFIG_IVB
#define CONFIG_VLV
#endif /* CONFIG_LIMIT_CORES */

#ifndef CONFIG_LIMIT_MODULES
#define CONFIG_INIT
#define CONFIG_REG
#define CONFIG_POWER
#define CONFIG_MODE
#define CONFIG_DSP
#define CONFIG_PI
#define CONFIG_PD
#define CONFIG_APPCONTEXT
#define CONFIG_OVERLAY
#endif /* CONFIG_LIMIT_MODULES */

#ifndef CONFIG_LIMIT_PDS
#define CONFIG_PD_ANALOG
#define CONFIG_PD_LVDS
#define CONFIG_PD_HDMI
#define CONFIG_PD_SDVO
#define CONFIG_PD_SOFTPD
#endif

#ifndef CONFIG_DEBUG_FLAGS

#ifdef FULL_DEBUG_VERBOSITY
#define DFLT_DBG_FG 1
#else
#define DFLT_DBG_FG 0
#endif

#define CONFIG_DEBUG_FLAGS			      \
		DFLT_DBG_FG,	/* DSP Module */		      \
		DFLT_DBG_FG,	/* Mode Module */		      \
		DFLT_DBG_FG,	/* Init Module */		      \
		DFLT_DBG_FG,	/* Overlay Module */	      \
		DFLT_DBG_FG,	/* Power Module */		      \
		DFLT_DBG_FG,	/* State Module */		      \
		DFLT_DBG_FG,	/* Kernel Mode Setting */     \
		DFLT_DBG_FG,	/* Port Driver Module */      \
		DFLT_DBG_FG,  /* Individual port drivers */ \
		DFLT_DBG_FG,  /* Splash screen/video */     \
		DFLT_DBG_FG,  /* sysfs debug*/			  \
\
		DFLT_DBG_FG,  /* General kernel DRM */      \
\
		DFLT_DBG_FG,	/* Global Tracing */	      \
		DFLT_DBG_FG,	/* Uncategorized Debug */

#endif

#endif /* _HAL_CONFIG_DEFAULT_H */

