/*
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
 * Copyright (c) Imagination Technologies Limited, UK
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Waldo Bastian <waldo.bastian@intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thread1_bin.h"

#define MTX_SIZE (40*1024)
#define MTX_SIZE1 (56*1024)
#define MTX_SIZE2 (56*1024)
#define MTX_SIZE3 (84*1024)
#define STACKGUARDWORD          ( 0x10101010 )
#define UNINITILISE_MEM         ( 0xcd )
#define LINKED_LIST_SIZE	( 32*5 )
#define FIP_SIZE		( 296 )

struct msvdx_fw {
    unsigned int ver;
    unsigned int  text_size;
    unsigned int data_size;
    unsigned int data_location;
};

int main()
{
    unsigned long i = 0;
    FILE *ptr = NULL;
    struct msvdx_fw fw;
    FIRMWARE fw_DE3;

    fw_DE3 = sFirmware1366_FS; /* VXD3xx_DEVA_DDK_3_0_5 */
    fw_DE3 = sFirmware1366_SS; /* VXD3xx_DEVA_DDK_3_0_5 */
    fw_DE3 = sFirmware1419_FS; /* VXD3xx_DEVA_DDK_3_1_2 */
    fw_DE3 = sFirmware_FS; /* VXD3xx_DEVA_DDK_3_2_4 */
    fw_DE3 = sFirmware_SS; /* VXD3xx_DEVA_DDK_3_2_4 */
    //fw_DE3 = sFirmware0000_SS; /* 1366based, for VP only, be able to disable WDT in Csim */
    fw_DE3 = sFirmware_SS_DE3_3_20;
    //fw_DE3 = sFirmware_SS_1472_3_8;

    fw.ver = 0x0496;
    fw.text_size = fw_DE3.uiTextSize / 4;
    fw.data_size = fw_DE3.uiDataSize / 4;;
    fw.data_location = fw_DE3.DataOffset + 0x82880000;

    ptr = fopen("msvdx_fw_mfld_DE2.0.bin", "w");
    if (ptr == NULL) {
        fprintf(stderr, "Create msvdx_fw_mfld_DE2.0.bin failed\n");
        exit(-1);
    }
    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&fw_DE3.pui8Text[i*4], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&fw_DE3.pui8Data[i*4], 4, 1, ptr);
    }
    fclose(ptr);

    return 0;
}
