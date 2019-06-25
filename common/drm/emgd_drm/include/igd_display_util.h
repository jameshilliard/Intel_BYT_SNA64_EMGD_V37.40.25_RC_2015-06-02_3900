/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_display_util.h
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
 *
 *-----------------------------------------------------------------------------
 */


#define PLANE(emgd_crtc) (emgd_crtc->igd_plane)
#define PIPE(emgd_crtc) (emgd_crtc->igd_pipe)
#define CURSOR(emgd_crtc) ((igd_cursor_t*)(emgd_crtc->igd_cursor))
//#define OVERLAY(emgd_crtc) (emgd_crtc->igd_sprite)
/*#define PORT_OWNER(display)  \
	((igd_display_port_t *)(((igd_display_context_t *)display)->port[display->port_number-1]))
	we need to defetures this
*/

static __inline igd_display_port_t * PORT(emgd_crtc_t * emgd_crtc)
{
	igd_display_port_t *port         = NULL;
	struct drm_device  *dev          = NULL;
	struct drm_encoder *encoder      = NULL;
	emgd_encoder_t     *emgd_encoder = NULL;

	dev = ((struct drm_crtc *)(&emgd_crtc->base))->dev;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (((struct drm_crtc *)(&emgd_crtc->base)) == encoder->crtc) {
			emgd_encoder = container_of(encoder, emgd_encoder_t, base);
			port         = emgd_encoder->igd_port;
			break;
		}
	}

	return port;
}

static __inline emgd_crtc_t * CRTC_FROM_ENCODER(emgd_encoder_t *emgd_encoder)
{
	struct drm_encoder *encoder   = NULL;
	struct drm_device  *dev       = NULL;
	struct drm_crtc    *crtc      = NULL;
	emgd_crtc_t        *emgd_crtc = NULL;

	encoder = &emgd_encoder->base;
	dev     = encoder->dev;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc != NULL) {
			if (crtc == encoder->crtc) {
				emgd_crtc = container_of(crtc, emgd_crtc_t, base);
				break;
			}
		}
	}
	return emgd_crtc;
}







