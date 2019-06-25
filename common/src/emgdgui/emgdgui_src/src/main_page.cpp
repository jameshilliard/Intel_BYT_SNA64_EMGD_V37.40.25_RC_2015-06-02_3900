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
extern char *page_index;   

static string fbHTML(char *path) {
	string fb;
	char tmp[256];
	char *fbNumber;
	int i;

	int num = readlink(path, tmp, sizeof(tmp));
	if (num < 0) {
		return fb;
	}
	tmp[num] = '\0';
	fbNumber = strrchr(tmp, '/')+1; /* Find last '/', ie filename */

	fb += "<fieldset><legend>Framebuffer ";
	fb += fbNumber;
	fb += "</legend>\n";

	for (i=0; i<map.fbCount; i++) {	
		snprintf(tmp, 255, "%s/%s", path, map.fb[i].sysName);
		fb += map.fb[i].htmlName;
		fb += ": " + getLineFromFile(tmp) + "<br>\n";
	}

	fb += "</fieldset>\n";

	return fb;
}

static void getPostion(string *postionHTML){
	FILE *f;
	char line[512];
	char *x, *y, *LastSpace;
	int Connected_Line = 0;

	f = popen("xrandr -q", "r");
	if(f == NULL) {
		printf("xrandr failed\n");
		return;
	}
	while(fgets(line, sizeof(line)-1, f) != NULL) {
		/* xrandr foramt as DisplayName. connected widthxheight+x+y (...)
		 * Search ' connected' to detect connected or not */
		if (strstr(line, " connected") != NULL){
			/* Get x y value by search '+' */
			x = strchr(line, '+');
			x++;
			y = strchr(x, '+');
			*y = 0;
			y++;
			LastSpace = strchr(y, ' ');
			*LastSpace = 0;

			/* Mode    display2.x display2.y display1.x display1.y 	  Postion
			 * Single 	0	 0	   Null		Null	   Null
			 * Clone	0 	 0	    0		0	  same as
			 * Extend 	0	 1	    0		0	  below
			 * Extend	1	 0	    0		0	  right of
			 * Extend	0	 0	    0		1	  above
			 * Extend 	0	 0	    1		0	  left of
			 * */
			if(atoi(y)){
				postionHTML[(2*Connected_Line) + 1] = " selected='selected'";
			}
			if(atoi(x)){
				postionHTML[(2*Connected_Line) + 2] = " selected='selected'";
			}
			Connected_Line++;
			/* assumes no more than 2 active displays */
			if (Connected_Line > 2) {
				printf("More than 2 active displays\n");
				break;
			}
		}
	}
	pclose(f);
}

 /* portHTML must be an allocated array of size 3.
 *  format: Index 0 stores port names in HTML.  It will be ordered:
 *  connected ports, disconnected ports, and a disabled option last. 
 *  example: 'VGA','HDMI-1','HDMI-2','(disabled)'. 
 * Index 1-2 store HTML combobox resolutions for specific display. example: <option>1920x1080</option>. 
 * assumes no more than 2 active displays */
static void queryXrandr(string *portHTML){
	FILE *f;
	string s;
	char line[512];
	char tmp[512];
	int numConnected = 0;
	string disconnectedPorts;
	portHTML[0] = "\n"; /* first line is commented out in index.html, so a newline is required */

	int status = system("which xrandr &> /dev/null");
	if (status != 0) {
	/* TODO check does this handle Wayland case? */ 
		portHTML[0] += "'(unknown)'";
		printf("xrandr not found\n");
		return; 
	}

	f = popen("xrandr -q", "r");
	if (f == NULL) {
		printf("xrandr failed\n");
		portHTML[0] += "'(unknown)'";
		return; 
	}

	while (fgets(line, sizeof(line)-1, f) != NULL) {
		if (strstr(line, "disconnected") != NULL) {
			sscanf(line, "%s", tmp);
      sprintf(line, "'%s', ", tmp);
			disconnectedPorts += line;
		} else if (strstr(line, "connected") != NULL) { 
			numConnected++;
			sscanf(line, "%s", tmp);
			sprintf(line, "'%s', ", tmp);
			portHTML[0] += line;
		}	else if (strstr(line, "Screen") != NULL) {
			/* ignore for now */
		} else if (numConnected > 0 && numConnected < 3) { /*this is likely a resolution mode line */
			sscanf(line, "%s", tmp); //First 'word' surrounded by whitespace is the resolution

			/* Expect a resolution like 1280x1024, so checking it contain an 'x' */
			if (strstr(tmp, "x") != NULL) {
				if (strstr(line, "*") != NULL) {
					/* An '*' in the second word on the line denotes current resolution */
					sprintf(line, "<option selected='true'>%s</option>\n", tmp);
				} else {
					sprintf(line, "<option>%s</option>\n", tmp);
				}
				portHTML[numConnected] += line; /* only connected ports will have a resolution list */
			}
		}
	}
	pclose(f);
	portHTML[0] += disconnectedPorts + "'(disabled)'";
	
	return; 
}

static string getPlaneSettings(char* name, int num) {
	string resultHTML;
	int i;
	char tmp[256];
	char path[256];
	char tmphtml[1024];
	double value;

	snprintf(tmphtml, 1023, "planeArray[%d] = new Object();\n", num);
	resultHTML += tmphtml;
	snprintf(path, 255, "/sys/module/emgd/control/%s", name);
	
	for (i=0; i<map.ovlCount; i++) {
		snprintf(tmp, 255, "%s/%s", path, map.ovl[i].sysName);
		if (std::string(map.ovl[i].sysName).find("gamma") == std::string::npos ) {
			value = strtolong(getLineFromFile(tmp).c_str(), 0);
		} else {	
			value = strtolong(getLineFromFile(tmp).c_str(), 0)/SCALE_OVL;
		}
		snprintf(tmphtml, 1023, "planeArray[%d]['%s'] = %.1f;\n", num, map.ovl[i].htmlName, value);
		resultHTML += tmphtml;
	}

	return resultHTML;
}

static string getCRTC(char* name, int crtc_count) {
	string crtcHTML, line;
	char path[256];
	char tmp[512];
	char checkedOn[] = " checked='yes' ";
	char checkedOff[] = "";
	char *checkedPtr;
	int i;

	snprintf(path, 255, "/sys/module/emgd/control/%s/current_mode", name);
	ifstream myfile (path);
	if (myfile.is_open()) {
		crtcHTML += "<div class='";
		if (crtc_count == 0)
			crtcHTML += "open";
		else
			crtcHTML += "closed";
		crtcHTML += "Panel'><h3><a href='#'>";
		crtcHTML += name;
		crtcHTML += "</a></h3><div>\n<table><tr><td width='250' valign='top'>\n"
				"<fieldset><legend>Current mode</legend>\n";
		while ( myfile.good() ) {
			getline (myfile,line);
			if (!line.empty())
				crtcHTML += line + "<br>\n";
		}
		myfile.close();
		crtcHTML += "</fieldset>\n<p>";

		snprintf(path, 255, "/sys/module/emgd/control/%s/framebuffer", name);
		crtcHTML += fbHTML(path)+ "<p>\n"
			"</td><td valign='top'>\n";

		crtcHTML += "<form method='GET' name='form_";
		crtcHTML += name;
		crtcHTML += "' action='crtc'>\n"
			"<input type='hidden' id='crtc_hidden'  name='crtc_hidden' value='";
		crtcHTML += name;
		crtcHTML += "' />\n"
			"<input type='hidden' id='crtc_key' name='key' class='hidden_key' value=''>\n";

		for (i=0; i<map.crtcCount;i++) {
			snprintf(path, 255, "/sys/module/emgd/control/%s/%s", name, map.crtc[i].sysName);					

			if (map.crtc[i].type == CRTC_TYPE_READONLY) {
				snprintf(tmp, 511, "%s: %s<br>\n", map.crtc[i].desc, getLineFromFile(path).c_str());
				crtcHTML += tmp;
			} 
			else if (map.crtc[i].type == CRTC_TYPE_CHECKBOX) {
				if (strtolong(getLineFromFile(path).c_str(), 0) >= 1)
					checkedPtr = checkedOn;
				else
					checkedPtr = checkedOff;
				snprintf(tmp, 511, "<input type='checkbox' name='%s' value='1' %s>%s<br>\n", 
					map.crtc[i].urlName, checkedPtr, map.crtc[i].desc);
				crtcHTML += tmp;
			}
			else if (map.crtc[i].type == CRTC_TYPE_TEXT) {	
				snprintf(tmp, 511, "%s: <input type='text' name='%s' value='%s'><br>\n", 
					map.crtc[i].desc, map.crtc[i].urlName, getLineFromFile(path).c_str());
				crtcHTML += tmp;
			}
			else if (map.crtc[i].type == CRTC_TYPE_DIV) {
				crtcHTML += "<p>\n";
				crtcHTML += map.crtc[i].desc;
				crtcHTML += ":<br>\n";
				snprintf(tmp, 511, "<div id='%s_%s'></div>\n<input type='hidden' name='%s' id='%s_config_hidden' value='%s'><p>\n",
					name, map.crtc[i].htmlName, 
					map.crtc[i].urlName, name, getLineFromFile(path).c_str());
				crtcHTML += tmp;
			}
		}
		
		crtcHTML += "<p><input type='button' value='Apply' onClick='crtcClick(\"form_";
		crtcHTML += name;
		crtcHTML += "\")'/>\n"
			"</form>\n"
			"</td></tr></table></div></div>\n";
	}
	return crtcHTML;
}

int f_main(struct MHD_Connection *connection, void *cls) {
	char *mstr;
	char *n;
	string crtcHTML, addl_str, radioHTML, arrayHTML("\n"), planeHTML("\n");
	long value;
	DIR *d;
	struct dirent *dir;
	char tmp[1024];
	char defaultStr[] = " checked ";
	char emptyStr[] = "";
	char *defaultPtr = defaultStr;
	int crtc_count=0;
	int radioColor = 0;
	int plane_count=0;
	int i;
	char *key = (char*) cls;
	const char *inputKey = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "key");
	if (key != NULL) {
		if (inputKey == NULL || strcmp(key, inputKey) != 0) {
			return invalidKeyHTML(connection);
		}
	}

	string info_desc = getLineFromFile("/sys/module/emgd/info/description");
	string info_version = getLineFromFile("/sys/module/emgd/info/version"); 
	string info_date = getLineFromFile("/sys/module/emgd/info/builddate");
	string info_chipset = getLineFromFile("/sys/module/emgd/info/chipset");

	if (info_version.empty()) {
		addl_str += "Unable to find EMGD<p>";
	} else {

	d = opendir("/sys/module/emgd/control");
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			n = dir->d_name;
			if (strncmp("crtc", n, 4) == 0) {
				crtcHTML += getCRTC(n, crtc_count);
				crtc_count++;
			} else if ((strncmp("VGA", n, 3) == 0) || (strncmp("HDMI", n, 4) == 0) || (strncmp("DP", n, 2) == 0)) {
				string name(n);
				snprintf(tmp, 1024, "<input type='radio' id='radio_display%d' name='radio_display' value='%s' %s onClick='setColors()'>"
					"<label for='radio_display%d'>%s</label>\n", radioColor, n, defaultPtr, radioColor, n);
				radioHTML += tmp;
				defaultPtr = emptyStr;

				snprintf(tmp, 1023, "colorArray['%s'] = new Object();\n", n);
				arrayHTML += tmp;	
				for (i=0; i<map.colorCount; i++) {
					snprintf(tmp, 1023, "/sys/module/emgd/control/%s/%s", n, map.color[i].sysName);
					value = strtolong(getLineFromFile(tmp).c_str(), 0);
					snprintf(tmp, 1023, "colorArray['%s']['%s'] = %.1f;\ncolorArray['%s']['%s'] = %.1f;\ncolorArray['%s']['%s'] = %.1f;\n",
						n, map.color[i].red.urlName,   RCHANNEL(value)/map.color[i].scale,
						n, map.color[i].green.urlName, GCHANNEL(value)/map.color[i].scale,
						n, map.color[i].blue.urlName,  BCHANNEL(value)/map.color[i].scale);
					arrayHTML += tmp;
				}
				radioColor++;
			} else if (strncmp("PlaneId", n, 7) == 0) {
				planeHTML += getPlaneSettings(n, plane_count);
				plane_count++;
			}
		}
		closedir(d);
	}
	addl_str += "<p>\n";

	}

	string postionHTML[5];
	getPostion(postionHTML);

	string portHTML[3];	
	queryXrandr(portHTML);

	ALLOC_AND_BUILD_STR(mstr,page_index,
		arrayHTML.c_str(), portHTML[0].c_str(), planeHTML.c_str(),
		info_desc.c_str(), info_version.c_str(), 
		BUILD_MAJOR_NUM, BUILD_MINOR_NUM, BUILD_BUILD_NUM,
		info_date.c_str(), info_chipset.c_str(), 
		addl_str.c_str(),
		crtcHTML.c_str(),
		postionHTML[0].c_str(),postionHTML[1].c_str(),postionHTML[2].c_str(),postionHTML[3].c_str(),postionHTML[4].c_str(),
		portHTML[1].c_str(), portHTML[2].c_str(), 
		radioHTML.c_str());
	return helper_send_HTTP_response_and_free_malloc(mstr,connection);
}

