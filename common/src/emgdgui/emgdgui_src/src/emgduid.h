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

#ifndef _EMGDUID_H_
#define _EMGDUID_H_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include <microhttpd.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include <dirent.h>
#include <arpa/inet.h>

#define PORT 8888

#define RCHANNEL(A) 	((0xFF0000 & A) >> 16)
#define GCHANNEL(A) 	((0xFF00 & A) >> 8)
#define BCHANNEL(A)	(0xFF & A)

#define ALLOC_AND_BUILD_STR(DST,SRC,PARMS...) 	DST = (char*) malloc (snprintf (NULL, 0, SRC, PARMS)+1); \
						if (DST == NULL)  return MHD_NO; \
						sprintf (DST,SRC, PARMS)

#define SCALE_FB_GAMMA 32.0
#define SCALE_OVL 256.0

#define CRTC_TYPE_READONLY 1
#define CRTC_TYPE_CHECKBOX 2
#define CRTC_TYPE_TEXT 3
#define CRTC_TYPE_DIV 4

using namespace std;

int f_main(struct MHD_Connection *connection, void *cls);
string getLineFromFile(const char *filePath);
long strtolong(const char* str, long default_val);
double strtointeger(const char* str, double default_val);
long strtoindex(char* str, double optionalMultiplier);
int invalidKeyHTML(struct MHD_Connection *connection);
int helper_send_HTTP_response_and_free_malloc(char* mstr, struct MHD_Connection *connection);
int helper_send_HTTP_response_cpp_string(string cppstr, struct MHD_Connection *connection);

/* Set pages */
int f_plane(struct MHD_Connection *connection, void *cls);
int f_crtc(struct MHD_Connection *connection, void *cls);
int f_xrandr(struct MHD_Connection *connection, void *cls);
int f_color(struct MHD_Connection *connection, void *cls);


typedef struct {
	char functionname[256];
	int (*urlFunction)(struct MHD_Connection*, void* cls);
} URL_TABLE_STRUCT;


typedef struct {
	const char* sysName;  /* /sys/modules/emgd filename */
	const char* htmlName; /* Name that is used in the browser */
	const char* urlName;  /* Named passed in via HTML GET on url*/
} item_map_t;

typedef struct {
	const char* desc;     /* Description name */
	const char* sysName;  /* /sys/modules/emgd filename */
	const char* urlName;  /* Named passed in via HTML GET on url*/
	const char* htmlName; /* Short suffix used in HTML */
	int type;
} crtc_map_t;

typedef struct {
	const char* urlName;  /* Named passed in via HTML GET on url*/
} color_channel_t;

typedef struct {
	const char* desc;     /* Description */
	const char* sysName;  /* /sys/modules/emgd filename */
	double scale;         /* Amount to scale color type, i.e. scale gamma 32x */
	color_channel_t red;
	color_channel_t green;
	color_channel_t blue;
} color_t;

typedef struct {
	item_map_t *ovl;
	int ovlCount;
	item_map_t *fb;
	int fbCount;
	crtc_map_t *crtc;
	int crtcCount;
	color_t *color;
	int colorCount;
} map_t;


#endif
