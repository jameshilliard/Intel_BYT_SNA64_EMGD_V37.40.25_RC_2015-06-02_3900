/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: sched.h
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
 *  This file contains Linux kernel implementations for the OAL scheduling
 *  abstractions.
 *-----------------------------------------------------------------------------
 */

#ifndef _OAL_LINUX_OSSCHED_H
#define _OAL_LINUX_OSSCHED_H

#include <config.h>

#include <linux/sched.h>

typedef unsigned long os_alarm_t;

static __inline os_alarm_t _linux_kernel_set_alarm(unsigned long t)
{
  return (msecs_to_jiffies(t) + jiffies);
}

static __inline int _linux_kernel_test_alarm(os_alarm_t t)
{
  return (jiffies >= t) ? 1 : 0;
}

#define OS_SET_ALARM(t) _linux_kernel_set_alarm(t)
#define OS_TEST_ALARM(t) _linux_kernel_test_alarm(t)

#endif
