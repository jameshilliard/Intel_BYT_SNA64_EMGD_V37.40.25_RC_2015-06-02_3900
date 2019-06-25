/*
 * Copyright © 2013 Intel Corporation
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
 */

#ifndef VAXV_H
#define VAXV_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_VAXV

typedef struct _va_xv_buf {
    unsigned int  buf_handle;
    unsigned long pixel_format;
    int buf_width, buf_height;
    unsigned int pitches[3];
    unsigned int offsets[3];
} va_xv_buf_t;

#define FOURCC_VAXV              (('V' << 24) | ('X' << 16) | ('A' << 8) | 'V')
typedef struct _va_xv_put_image {
    int size;
    va_xv_buf_t video_image;
    unsigned int flags;
} va_xv_put_image_t;

#define VAXV_ATTR_STR            "VAXV_SHMBO"
#define VAXV_VERSION_ATTR_STR    "VAXV_VERSION"
#define VAXV_CAPS_ATTR_STR       "VAXV_CAPS"
#define VAXV_FORMAT_ATTR_STR     "VAXV_FORMAT"

#define VAXV_MAJOR               0
#define VAXV_MINOR               1

#define VAXV_VERSION             ((VAXV_MAJOR << 16) | VAXV_MINOR)

/* The shared buffer object in put image are directly used on sprite */
#define VAXV_CAP_DIRECT_BUFFER   1

/* Scalling support */
#define VAXV_CAP_SCALLING_UP     (1 << 4)
#define VAXV_CAP_SCALLING_DOWN   (1 << 5)
#define VAXV_CAP_SCALLING_FLAG   VAXV_CAP_SCALLING_UP | VAXV_CAP_SCALLING_DOWN

/* Rotation support */
#define VAXV_CAP_ROTATION_90     (1 << 8)
#define VAXV_CAP_ROTATION_180    (1 << 9)
#define VAXV_CAP_ROTATION_270    (1 << 10)
#define VAXV_CAP_ROTATION_FLAG   VAXV_CAP_ROTATION_90 | \
	VAXV_CAP_ROTATION_180 | VAXV_CAP_ROTATION_270


#endif // #ifdef ENABLE_VAXV

#endif // #ifdef VAXV_H
