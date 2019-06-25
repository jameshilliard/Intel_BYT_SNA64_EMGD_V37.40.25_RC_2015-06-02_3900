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

extern map_t map;
const char *set_page =
	"<HTML><head><title>Set page</title>\n"
	"<link type='text/css' href='browser_inc/jquery-ui-1.8.12.custom.css' rel='stylesheet' />\n"
	"<script type='text/javascript' src='browser_inc/jquery.js'></script>\n"
	"<script type='text/javascript' src='browser_inc/jquery-ui-1.8.11.min.js'></script>\n"
	"<script>\n"
	"$(function() {\n"
	"	$( 'input:submit' ).button();\n"
	"});\n"
	"</script>\n"
	"</head><body>\n"
	"<table width='100%'><tr><td></td><td width='795px'>\n"
	"<table width='100%' bgcolor='#005AA1' class='ui-corner-all'><tr><td><img src='browser_inc/images/logo3.jpg'></td><td>\n"
	"</td></tr>\n</table>\n"
	"\n<div class='ui-tabs ui-widget ui-widget-content ui-corner-all settingstabs'>\n"
	"%s\n<p>\n"
	"<form method='GET' action='%s'>\n"
	"<input type='hidden' name='key' value='%s'/>\n"
	"<input type='submit' value='Return'/>\n"
	"</form>\n"
	"</div>\n\n"
	"</td><td></td></tr></table>\n"
	"</body></HTML>";

static int fileWriteSingleLine(char* filePath, char* output) {
	ofstream myfile (filePath);
	if (myfile.is_open()) {
		myfile << output;
		myfile.close();
	} else {
		return 2;
	}
	return 0;
}

/* Type must match the filename in /sys/module/emgd/control/VGA-1/ directory */
static int f_color_helper(int r, int g, int b, const char* display, const char* type) {
	char filePath[256];
	char hexOut[16];

	//TODO handle error better, such as if only one channel is invalid then apply the other two?
	if (r >= 0 && r < 256 && g >= 0 && g < 256 && b >= 0 && b < 256) {
		sprintf(hexOut, "0x00%02X%02X%02X", r, g, b);
		sprintf(filePath, "/sys/module/emgd/control/%s/%s", display, type);
		return fileWriteSingleLine(filePath, hexOut);
	} else {
		return 1;
	}
}

int f_color(struct MHD_Connection *connection, void *cls) {
	string page;
	char *mstr;
	int i;
	int r,g,b;
	const char* display_str = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "radio_display");
	char *key = (char*) cls;
	const char *inputKey = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "key");
	if (key != NULL) {
		if (inputKey == NULL || strcmp(key, inputKey) != 0) {
			return invalidKeyHTML(connection);
		}
	}

	if (display_str == NULL) {
		page += "Invalid display<br>";
	} else {
		for (i=0; i<map.colorCount; i++) {
			r = strtoindex((char*)MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
					map.color[i].red.urlName),map.color[i].scale);
			g = strtoindex((char*)MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
					map.color[i].green.urlName),map.color[i].scale);
			b = strtoindex((char*)MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
					map.color[i].blue.urlName),map.color[i].scale);

			if (f_color_helper(r, g, b, display_str, map.color[i].sysName) > 0) {
				page += "Unable to set ";
				page += map.color[i].desc;
				page += "<br>\n";
			}
		}
		page += "Color set complete<br>\n";
	}

	ALLOC_AND_BUILD_STR(mstr,set_page, page.c_str(), "main#color_correction", inputKey);
	return helper_send_HTTP_response_and_free_malloc(mstr, connection);
}

int f_xrandr(struct MHD_Connection *connection, void *cls) {
	char *mstr;
	char *key = (char*) cls;
	const char *d1Input = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "displayName1");
	const char *d2Input = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "displayName2");
	const char *xpos = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "cbo_xrandr_position");
	const char *pmode1 = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmode1");
	const char *pmode2 = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmode2");
	const char *pmode1Name = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmode1Name");
	const char *pmode2Name = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmode2Name");
	const char *primaryDisplay = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "radio_primary");
	char *display1 = NULL;
	char *display2 = NULL;
	string status;
	int result;
	string d1ModeText = " --auto ";
	string d2ModeText = " --auto ";
	const char *inputKey = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "key");
	if (key != NULL) {
		if (inputKey == NULL || strcmp(key, inputKey) != 0) {
			return invalidKeyHTML(connection);
		}
	}

	status = "xrandr --output ";

	/* Look for valid display name */
	if (d1Input != NULL && d1Input[0] != '(') {
		display1 = (char*) d1Input;
	}

	/* Look for valid display 2 name. */
	if (d2Input != NULL && d2Input[0] != '(') {
		if (display1 == NULL) {
			/* first display was invalid, make this display 1 */
			display1 = (char*) d2Input;
		} else {
			display2 = (char*) d2Input;
		}
	}

	if (display1 == NULL) {
		status = "Invalid displays";
	} else {

		/* See if the name associated with mode resolution matches chosen display name */
		if (pmode1Name!=NULL && strcmp(display1, pmode1Name) == 0 && pmode1 != NULL) {
			d1ModeText = " --mode ";
			d1ModeText += pmode1;
			d1ModeText += " ";
		} else	if (pmode2Name!=NULL && strcmp(display1, pmode2Name) == 0 && pmode2 != NULL) {
			d1ModeText = " --mode ";
			d1ModeText += pmode2;
			d1ModeText += " ";
		}

		if (display2 != NULL) {
			if (pmode1Name!=NULL && strcmp(display2, pmode1Name) == 0 && pmode1 != NULL) {
				d2ModeText = " --mode ";
				d2ModeText += pmode1;
				d2ModeText += " ";
			} else	if (pmode2Name!=NULL && strcmp(display2, pmode2Name) == 0 && pmode2 != NULL) {
				d2ModeText = " --mode ";
				d2ModeText += pmode2;
				d2ModeText += " ";
			}
		}

		/* Example single: xrandr --output VGA1 --auto  */
		status += display1;
		status += d1ModeText;
		if (display1 != NULL && display2 != NULL) {
			/* Example dual: xrandr --output VGA1 --auto --primary --left-of HDMIA1 --output HDMIA1 --auto */
			if (primaryDisplay != NULL && primaryDisplay[0] == '1')
				status += " --primary ";
			if(xpos!=NULL)
				status += xpos;
			status += " ";
			status += display2;
			status += " --output ";
			status += display2;
			status += d2ModeText;
			if (primaryDisplay != NULL && primaryDisplay[0] == '2')
        status += " --primary ";
		}
	}

	result = system(status.c_str());
	if (result != 0) {
		status += "\n<p>Error executing xrandr command\n";
	}

	ALLOC_AND_BUILD_STR(mstr, set_page, status.c_str(), "main#general_mode", inputKey);
	return helper_send_HTTP_response_and_free_malloc(mstr, connection);
}

int f_crtc(struct MHD_Connection *connection, void *cls) {
	char *mstr;
	char *key = (char*) cls;
	int i;
	char filePath[256];
	string page;
	char *urlValue;
	const char *crtcName = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "crtc_hidden");
	const char *inputKey = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "key");
	if (key != NULL) {
		if (inputKey == NULL || strcmp(key, inputKey) != 0) {
			return invalidKeyHTML(connection);
		}
	}

	for (i=0; i< map.crtcCount; i++) {
		if (map.crtc[i].urlName != NULL) {
			if(crtcName!= NULL)
				sprintf(filePath, "/sys/module/emgd/control/%s/%s", crtcName, map.crtc[i].sysName);
			urlValue =(char*) MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, map.crtc[i].urlName);

			if (map.crtc[i].type == CRTC_TYPE_CHECKBOX) {
				if (urlValue == NULL) { /*checkbox was unchecked */
					fileWriteSingleLine(filePath, (char*) "0");
				} else {
					fileWriteSingleLine(filePath, (char*) "1");
				}
			}

			if (map.crtc[i].type == CRTC_TYPE_TEXT || map.crtc[i].type == CRTC_TYPE_DIV) {
				fileWriteSingleLine(filePath, urlValue);
			}
		}
	}

	page += "CRTC settings complete\n";

	ALLOC_AND_BUILD_STR(mstr, set_page, page.c_str(), "main#general_crtcs", inputKey);
	return helper_send_HTTP_response_and_free_malloc(mstr, connection);
}

int f_plane(struct MHD_Connection *connection, void *cls) {
	char *mstr;
	string page;
	char *key = (char*) cls;
	const char *inputKey = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "key");
	char *tmp_input;
	char tmp[256];
	char filePath[256];
	int p, i;
	long tempLong;
	bool check_range = false;


	const char *prefixPlane[] = {"Sprite-A", "Sprite-B", "Sprite-C", "Sprite-D"};
	const char *sysPrefix[] = {"PlaneId1", "PlaneId2", "PlaneId3", "PlaneId4"};
	//TODO dynamically detect PlaneIDs? Map to Sprite name better?

	page = "<HTML><body>\n";

	if (key != NULL) {
		if (inputKey == NULL || strcmp(key, inputKey) != 0) {
			return invalidKeyHTML(connection);
		}
	}

	for (p=0;p<sizeof(prefixPlane)/sizeof(prefixPlane[0]); p++, check_range = false) {
		sprintf(filePath, "/sys/module/emgd/control/%s/%s", sysPrefix[p], "gamma_on");
		fileWriteSingleLine(filePath, (char*) "1"); //Turn on gamma correction
		for (i=0; i<map.ovlCount; i++) {
			sprintf(filePath, "/sys/module/emgd/control/%s/%s", sysPrefix[p], map.ovl[i].sysName);
			snprintf(tmp,255,"%s_%s", prefixPlane[p], map.ovl[i].sysName); /*build a string like "Sprite-A_ovlgammaG" */
			tmp_input = (char*) MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, tmp);
			if(tmp_input != NULL) {
				if(std::string(map.ovl[i].sysName).find("gamma") != std::string::npos){
					tempLong = strtoindex(tmp_input,SCALE_OVL);
						check_range = true;
				} else {
					tempLong = strtol(tmp_input, NULL, 10);

					if(std::string(map.ovl[i].sysName).find("brightness") != std::string::npos){
						if(tempLong >= -127 && tempLong <= 128)
							check_range = true;
					}

					if(std::string(map.ovl[i].sysName).find("saturation") != std::string::npos){
						if(tempLong >= 0 && tempLong <=127 )
							check_range = true;
					}
					if(std::string(map.ovl[i].sysName).find("contrast") != std::string::npos) {
						if(tempLong >= 0 && tempLong <= 255)
							check_range = true;
					}
					if(std::string(map.ovl[i].sysName).find("hue") != std::string::npos) {
						if(tempLong >= 0 && tempLong <= 1023)
							check_range = true;
					}
				}
			} else {
				check_range = false;
			} 
			
			if (check_range) {
				sprintf(tmp, "0x%lx", tempLong);
				fileWriteSingleLine(filePath, tmp);
			} else {
				page += "<p>Unable to set ";
				page += tmp;
				page += "</p>\n";
			}
		}
	}

	page += "Plane configuration complete.\n";

	ALLOC_AND_BUILD_STR(mstr,set_page, page.c_str(), "main#general_planes", inputKey);
	return helper_send_HTTP_response_and_free_malloc(mstr, connection);
}
