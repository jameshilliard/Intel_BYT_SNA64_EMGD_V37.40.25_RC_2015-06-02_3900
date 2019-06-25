/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: config.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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
 *  This file contains the compile options for the IGD compile. It is included
 *  by all IGD OAL and RAL modules. Do not remove valid options from this
 *  file, simply comment them out.
 *  Eventually a config tool will auto generate this file based on selected
 *  options.
 *-----------------------------------------------------------------------------
 */

#ifndef _HAL_CONFIG_H
#define _HAL_CONFIG_H

/*
 *  * Select ONE of these to be defined. It controls which OAL port
 *   * is used during the build and where the output goes.
 *    */
/* #define CONFIG_OAL linux */
#define CONFIG_OAL linux-user
/* #define CONFIG_OAL xfree86 */
/* #define CONFIG_OAL null */

/* #define CONFIG_LIMIT_MODES */
#define CONFIG_MODE_640x480x60
#define CONFIG_MODE_640x480x70
#define CONFIG_MODE_640x480x72
#define CONFIG_MODE_640x480x75
#define CONFIG_MODE_640x480x85
#define CONFIG_MODE_640x480x100
#define CONFIG_MODE_640x480x120
#define CONFIG_MODE_720x480x60
#define CONFIG_MODE_720x576x50
#define CONFIG_MODE_800x480x60
#define CONFIG_MODE_800x600x60
#define CONFIG_MODE_800x600x70
#define CONFIG_MODE_800x600x72
#define CONFIG_MODE_800x600x75
#define CONFIG_MODE_800x600x85
#define CONFIG_MODE_800x600x100
#define CONFIG_MODE_800x600x120
#define CONFIG_MODE_960x540x60
#define CONFIG_MODE_1024x768x60
#define CONFIG_MODE_1024x768x70
#define CONFIG_MODE_1024x768x75
#define CONFIG_MODE_1024x768x85
#define CONFIG_MODE_1024x768x100
#define CONFIG_MODE_1024x768x120
#define CONFIG_MODE_1152x864x60
#define CONFIG_MODE_1152x864x70
#define CONFIG_MODE_1152x864x72
#define CONFIG_MODE_1152x864x75
#define CONFIG_MODE_1152x864x85
#define CONFIG_MODE_1152x864x100
#define CONFIG_MODE_1280x720x60
#define CONFIG_MODE_1280x720x75
#define CONFIG_MODE_1280x720x85
#define CONFIG_MODE_1280x720x100
#define CONFIG_MODE_1280x768x60
#define CONFIG_MODE_1280x768x75
#define CONFIG_MODE_1280x768x85
#define CONFIG_MODE_1280x960x60
#define CONFIG_MODE_1280x960x75
#define CONFIG_MODE_1280x960x85
#define CONFIG_MODE_1280x1024x60

/*
 * Which Cores are supported
 *
 * Use Defaults
 */

/*
 * This macro configures the DRM/kernel's EMGD_DEBUG() and EMGD_DEBUG_S() macros to
 * use the KERN_INFO message priority, instead of the normal KERN_DEBUG message
 * priority.  This is useful for bugs (e.g. crashes) where dmesg can't be used
 * to obtain debug messages.
 */
/* #define CONFIG_USE_INFO_PRIORITY */

/*
 * Which of the optional modules are included in the build
 * for the most part this is for modules that need an init
 * or power entry point.
 *
 * Use Defaults.
 */

/*
 * Default FB/Display Resolution
 */
#define CONFIG_DEFAULT_WIDTH  640
#define CONFIG_DEFAULT_HEIGHT 480
#define CONFIG_DEFAULT_PF     IGD_PF_ARGB32


/*
  power modes supported
  0 -don't support
  1 - support

  Use Defaults.
*/

/*
 * Turn off fences for performance analysis. 3d makes use of "Use Fences"
 * So this will make fences regions become linear but everything should
 * still work.
 *
 * #define CONFIG_NOFENCES
 */

/* Don't enable Dynamic port driver loading for simple driver. For simple, 
 * one can limit the port drivers by enabling CONFIG_LIMIT_PDS to 
 * required port drivers *
 * 
 * Enable Dynamic port driver loading
 *
 * #define IGD_DPD_ENABLED 1 */

/* Enable required port drivers. */
#define CONFIG_LIMIT_PDS 1
#define CONFIG_PD_ANALOG 1
#define CONFIG_PD_HDMI   1
#define CONFIG_PD_DP   1

#define CONFIG_LINK_PD_ANALOG
#define CONFIG_LINK_PD_HDMI
#define CONFIG_LINK_PD_DP

#define CONFIG_DECODE

#define CONFIG_ST

#include <config_default.h>

#include <linux/version.h>

/* This macro is temporary and will be deprecated once power management is fully
 * working in EMGD kernel modules. This is just for developers who is currently working
 * on enabling it. This will turn on as default. If any issue can turn off this Macro.
 */
#define CONFIG_POWER_MANAGEMENT_SUPPORT

#endif

