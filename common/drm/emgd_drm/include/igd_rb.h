/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_rb.h
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
 *  Example Usage:
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_RB_H
#define _IGD_RB_H

#include <igd_mode.h>

/*
 * Flags for igd_sync()
 */
#define IGD_SYNC_BLOCK         0x00000000
#define IGD_SYNC_NONBLOCK      0x00000001
/*
 * No Flush pipe will not issue an MI Flush before the store dword
 * There is no guarentee that the store dword will not complete before
 * an earlier command unless the caller is sure that a store dword alone
 * is sufficient.
 */
#define IGD_SYNC_NOFLUSH_PIPE  0x00000010
/*
 * Do not flush the render cache as part of this flush. Map Cache will
 * be invalidated always.
 */
#define IGD_SYNC_NOFLUSH_CACHE 0x00000020
/* Generate a user Interrupt after the sync completes */
#define IGD_SYNC_INTERRUPT   0x00000040
/*
 * Flush the pipe now but delay the cache flush until the
 * sync is checked.
 */
#define IGD_SYNC_DELAY_FLUSH 0x00000080
/*
 * Synchronize queue with video decode engine.
 */
#define IGD_SYNC_VIDEO       0x00000100

#endif /* _IGD_RB_H */
