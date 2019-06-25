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

struct MHD_Daemon *my_daemon;
extern map_t map;

const char *page_index  = 
	/* REPLACE_INDEX_HTML  */
" <!--****************************************************************************** \n"
"  *  Copyright (c) 2002-2014, Intel Corporation. \n"
"  *  Permission is hereby granted, free of charge, to any person obtaining a copy \n"
"  *  of this software and associated documentation files (the 'Software'), to deal \n"
"  *  in the Software without restriction, including without limitation the rights \n"
"  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell \n"
"  *  copies of the Software, and to permit persons to whom the Software is \n"
"  *  furnished to do so, subject to the following conditions: \n"
"  * \n"
"  *  The above copyright notice and this permission notice shall be included in \n"
"  *  all copies or substantial portions of the Software. \n"
"  * \n"
"  *  THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR \n"
"  *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, \n"
"  *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE \n"
"  *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER \n"
"  *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, \n"
"  *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN \n"
"  *  THE SOFTWARE. \n"
"  * \n"
"  ********************************************************************************--> \n"
" <HTML> \n"
" <head><title>EMGDGUI</title> \n"
" 	<link type='text/css' href='browser_inc/jquery-ui-1.8.12.custom.css' rel='stylesheet' /> \n"
" 	<style type='text/css'> \n"
" 	body { margin:0;padding:0 } \n"
" 	.red_slider, .green_slider, .blue_slider, .brightness_slider, .color_brightness_slider,  \n"
" 	.contrast_slider, .color_contrast_slider, .saturation_slider, .hue_slider { \n"
" 		float: left; \n"
" 		clear: left; \n"
" 		width: 200px; \n"
" 		margin: 15px; \n"
" 	} \n"
" 	.red_slider .ui-slider-range { background: #ef2929; } \n"
" 	.green_slider .ui-slider-range { background: #8ae234; } \n"
" 	.blue_slider .ui-slider-range { background: #729fcf; } \n"
" 	.color_brightness_slider .ui-slider-range { background: #f6931f; } \n"
" 	.brightness_slider .ui-slider-range { background: #f6931f; } \n"
" 	.color_contrast_slider .ui-slider-range { background: #f6931f; } \n"
" 	.contrast_slider .ui-slider-range { background: #f6931f; } \n"
" 	.saturation_slider .ui-slider-range { background: #f6931f; } \n"
" 	.hue_slider .ui-slider-range { background: #f6931f; } \n"
" 	.mode_draggable { width: 160px; height: 80px; padding: 5px; } \n"
" 	.mode_sortable { width: 340px; height: 95px; padding: 5px;} \n"
" 	.primarySortable { background-color:#606060; } \n"
" 	.mode_sortable>div { float: left; } \n"
" 	.plane_draggable { width: 100px; height: 40px; padding: 0.5em; } \n"
" 	.plane_sortable { width:720px; height: 60px; padding: 0.5em; background-color:#606060;} \n"
" 	.plane_sortable>div { float: left; } \n"
" 	.pipesortable { width:120px; padding: 0.5em; background-color:#606060; list-style-type: none;} \n"
" 	.pipesortable>li { padding: 0.5em; width: 100px; height: 40px; text-align: center; } \n"
" 	.monitorTitle { display: block; width: 100%; text-align: center; font-weight: bold; color: #0860ab; } \n"
" 	.monitorImage { background-image:url('browser_inc/images/monitor1.jpg'); background-repeat:no-repeat; } \n"
" 	</style> \n"
" 	<script type='text/javascript' src='browser_inc/jquery.js'></script> \n"
" 	<script type='text/javascript' src='browser_inc/jquery-ui-1.8.11.min.js'></script> \n"
" 	<script type='text/javascript' src='browser_inc/indexfuncs.js'></script> \n"
" 	<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=ISO-8859-1'> \n"
" 	<script>		                 \n"
"  \n"
" 	$(function() { \n"
" 		$( '.leftbutton' ).button({ \n"
" 			icons: { \n"
" 				primary: 'ui-icon-circle-triangle-w' \n"
" 			}, \n"
" 			text: false, \n"
" 		}); \n"
" 	}); \n"
"  \n"
" 	$(function() { \n"
" 		$( '.rightbutton' ).button({ \n"
" 			icons: { \n"
" 				primary: 'ui-icon-circle-triangle-e' \n"
" 			}, \n"
" 		text: false \n"
" 		}); \n"
" 	}); \n"
"  \n"
" 	$(function() { \n"
" 		$( '.radio_display_div' ).buttonset(); \n"
" 	}); \n"
"  \n"
" 	$(function() { \n"
" 		$( '.mode_sortable' ).sortable({ \n"
" 			revert: true, \n"
" 			connectWith: '.mode_sortable' \n"
" 		}); \n"
" 		$( '.mode_sortable' ).disableSelection(); \n"
"  \n"
" 	}); \n"
"  \n"
" 	</script> \n"
" 	<script type='text/javascript'> \n"
" 	 \n"
" 		var colorArray = new Object(); \n"
" 		/* colorArray['VGA-1'] = new Object(); \n"
" 		colorArray['VGA-1']['sliderGammaR'] = 5; \n"
" 		colorArray['VGA-1']['sliderGammaG'] = 6; \n"
" 		colorArray['VGA-1']['sliderGammaB'] = 7; */ \n"
" 		// The next line will be replaced by current colors from the driver. Do not change. \n"
" 		// %s  \n"
"  \n"
" 		var portArray=[ \n"
" 			//'VGA','HDMI-1','HDMI-2','DP-1','DP-2','(disabled)' \n"
" 			//  %s \n"
" 		]; /* (disabled) must be last element */ \n"
" 		var displayIndex=[0, 1]; \n"
" 	 \n"
" 		var planeArray = new Array(); \n"
" 		/* \n"
" 		planeArray[0] = new Object(); \n"
" 		planeArray[0]['global_sliderValueOverlayGammaR'] = 100; \n"
" 		planeArray[0]['global_sliderValueOverlayGammaG'] = 100; \n"
" 		planeArray[0]['global_sliderValueOverlayGammaB'] = 150; */ \n"
" 		// The next line will be replaced by current plane settings from the driver. Do not change. \n"
" 		// %s \n"
" 	 \n"
" 		function setDefaultPlane() {			 \n"
" 			$('.gamma').each(function()	{ \n"
" 				document.getElementById($(this).attr('id')).value = 1.0; \n"
" 			}); \n"
" 			$('.brightness').each(function() { \n"
" 				document.getElementById($(this).attr('id')).value = -5; \n"
" 			}); \n"
" 			$('.contrast').each(function() { \n"
" 				document.getElementById($(this).attr('id')).value = 67; \n"
" 			}); \n"
" 			$('.saturation').each(function() { \n"
" 				document.getElementById($(this).attr('id')).value = 145; \n"
" 			}); \n"
" 			$('.hue').each(function() { \n"
" 				document.getElementById($(this).attr('id')).value = 0; \n"
" 			}); \n"
" 			setPlanesCorrectionSlider(); \n"
" 		} \n"
"  \n"
" 		function setDefaultColors() {			 \n"
" 			$('.color_gamma_slider').each(function()	{ \n"
" 				document.getElementById(($(this).attr('id')+'').substring(0, ($(this).attr('id')+'').lastIndexOf('_'))).value = 1.0; \n"
" 			}); \n"
" 			$('.color_brightness_slider').each(function() { \n"
" 				document.getElementById(($(this).attr('id')+'').substring(0, ($(this).attr('id')+'').lastIndexOf('_'))).value = 128; \n"
" 			}); \n"
" 			$('.color_contrast_slider').each(function() { \n"
" 				document.getElementById(($(this).attr('id')+'').substring(0, ($(this).attr('id')+'').lastIndexOf('_'))).value = 128; \n"
" 			}); \n"
" 			setColorCorrectionSlider(); \n"
" 		} \n"
" 		 \n"
" 		function setColors() { \n"
" 			if (document.form_color_correction.radio_display == null) {				 \n"
" 				return; \n"
" 			} \n"
" 			len = document.form_color_correction.radio_display.length \n"
" 			if (typeof len == 'undefined') { \n"
" 				var d = document.form_color_correction.radio_display.value; \n"
" 				for (var id in colorArray[d]) { \n"
" 					document.getElementById(id).value = colorArray[d][id];             \n"
" 				}  \n"
" 			} else { \n"
" 				for (i = 0; i <len; i++) { \n"
" 					if (document.form_color_correction.radio_display[i].checked) { \n"
" 						var d = document.form_color_correction.radio_display[i].value; \n"
" 						for (var id in colorArray[d]) { \n"
" 							document.getElementById(id).value = colorArray[d][id]; 						 \n"
" 						}					 \n"
" 						break;	 \n"
" 					} \n"
" 				} \n"
" 			} \n"
" 			setColorCorrectionSlider(); \n"
" 			setPlanesCorrectionSlider(); \n"
" 		} \n"
"  \n"
" 		function getURLParmByName(name) { \n"
" 			var reg = name + '=' + '(.+?)(&|$)'; \n"
" 			var r1 = (RegExp(reg).exec(location.search)||[,null])[1]; \n"
" 			if (r1 == null) \n"
" 				return 'null'; \n"
" 			var r2 = r1.replace(/[+]/g,' '); \n"
" 			return unescape(r2); \n"
" 		} \n"
"  \n"
" 		function submitDisplayPosition() { \n"
" 			var idsInOrder = $('#displaySortable').sortable('toArray'); \n"
" 			var f = document.forms['form_display'];			 \n"
" 			if (idsInOrder[0] == 'display0') { \n"
" 				/* original order */ \n"
" 				f.displayName1.value = portArray[displayIndex[0]]; \n"
" 				f.displayName2.value = portArray[displayIndex[1]]; \n"
" 			} else { \n"
" 				f.displayName1.value = portArray[displayIndex[1]]; \n"
" 				f.displayName2.value = portArray[displayIndex[0]]; \n"
" 			} \n"
" 			f.submit(); \n"
" 		} \n"
"  \n"
" 		/* direction is either +1 or -1 integer */ \n"
" 		function cycleDisplay(direction, index) { \n"
" 			var id = 'dragLabel' + index; \n"
" 			displayIndex[index] = (displayIndex[index] + direction + portArray.length) % (portArray.length); \n"
" 			if (displayIndex[0] == displayIndex[1]) { \n"
" 				/* increment again */ \n"
" 				displayIndex[index] = (displayIndex[index] + direction + portArray.length) % (portArray.length); \n"
" 			} \n"
" 			document.getElementById(id).innerHTML=portArray[displayIndex[index]]; \n"
" 		} \n"
"  \n"
" 		function buildPlaneGetString(name, configNumber) { \n"
" 			var result=''; \n"
" 			name += '_'; \n"
" 			//TODO use names from backend instead of specifiying 'gamma_red' or 'global_slider...' \n"
" 			result += name+'gamma_red='+document.getElementById('global_sliderValueOverlayGammaR'+configNumber).value + '&'; \n"
" 			result += name+'gamma_green='+document.getElementById('global_sliderValueOverlayGammaG'+configNumber).value + '&'; \n"
" 			result += name+'gamma_blue='+document.getElementById('global_sliderValueOverlayGammaB'+configNumber).value + '&'; \n"
" 			result += name+'brightness='+document.getElementById('global_sliderValueOverlay2'+configNumber).value + '&'; \n"
" 			result += name+'contrast='+document.getElementById('global_sliderValueOverlay3'+configNumber).value + '&'; \n"
" 			result += name+'saturation='+document.getElementById('global_sliderValueOverlay4'+configNumber).value + '&'; \n"
" 			result += name+'hue='+document.getElementById('global_sliderValueOverlay5'+configNumber).value + '&'; \n"
" 			return result; \n"
" 		} \n"
"  \n"
" 		function applyPlanes() { \n"
" 			var order; \n"
" 			var i \n"
" 			var configNumber; \n"
" 			var getStr = '?'; \n"
" 			var keyValue = encodeURIComponent(getURLParmByName('key')); \n"
" 			if (keyValue != 'null') { \n"
" 				getStr += 'key=' + keyValue + '&'; \n"
" 			} \n"
"  \n"
" 			for (configNumber=1; configNumber<=NUMBER_OF_SPRITE_PLANES;configNumber++) { \n"
" 				order = $(document.getElementById('planeSortable'+configNumber)).sortable('toArray'); \n"
" 				for (i=0; i<order.length; i++) { \n"
" 					getStr += buildPlaneGetString(order[i], configNumber); \n"
" 				} \n"
" 			} \n"
"  \n"
" 			var f = document.forms['form_plane']; \n"
" 			f.method = 'POST';  \n"
" 			/* We still want GET, but HTML only passes GET parameters in the action string if it is POST */ \n"
" 			f.action = f.action + getStr; \n"
" 			f.submit();	 \n"
" 		} \n"
"  \n"
" 		function onLoad() { \n"
" 			createPlaneConfigs(); \n"
" 			setPlaneText(); \n"
" 			setColors(); \n"
"  \n"
" 			createPipeConfigs(); \n"
"  \n"
" 			if (document.getElementById('cbo_pmode2').length == 0) { \n"
" 				/* Empty combo box of display 2 resolutions */ \n"
" 				displayIndex[1] = portArray.length - 1; //Set to disabled  \n"
" 				$('#fieldset_pmode2').hide(); \n"
" 			} \n"
" 			/* Unless something is wrong, portArray should always have at least 1 display and a (disabled) item */ \n"
" 			if (portArray.length <= 1) { \n"
" 				document.getElementById('xrandrWarningLabel').innerHTML = 'xrandr not found on the system. Disabling modeset.'; \n"
" 				document.getElementById('modesetApply').disabled = true; \n"
" 				$('#fieldset_pmode1').hide(); \n"
" 			}  \n"
" 		 \n"
" 			if (portArray.length <= 2) { \n"
" 				$('.displayButton').each(function() { \n"
" 					$(this).hide(); \n"
" 				}); \n"
" 			} \n"
"  \n"
" 			/* Initialize port name in various places */ \n"
" 			for (var i=0; i<2; i++) { \n"
" 				var d = portArray[displayIndex[i]]; \n"
" 				document.getElementById('pmode' + (i+1) + 'Name').value = d; \n"
" 				document.getElementById('dragLabel'+i).innerHTML=d; \n"
" 				document.getElementById('resLabel'+i).innerHTML=d; \n"
" 			} \n"
"  \n"
" 			/* Prepare the key the user entered for sending to a new page */ \n"
" 			var keyValue = getURLParmByName('key'); \n"
" 			if (keyValue != 'null') { \n"
" 				$('.hidden_key').each(function()      { \n"
" 					$(this).attr('value', keyValue); \n"
" 				}); \n"
" 			} \n"
" 		} \n"
" 		window.onload = onLoad;  \n"
" 	</script> \n"
"  \n"
" </head><body> \n"
" <table width='100%'><tr><td></td><td width='795px'> \n"
" 	<table width='100%' bgcolor='#005AA1' class='ui-corner-all'><tr><td><img src='browser_inc/images/logo3.jpg'></td><td>  \n"
" 	</td></tr> \n"
" 	</table> \n"
" 	<table width='795px' border='0' > <tr> <td valign='top'>		   \n"
" 	<div id='general_tabs' class='ui-tabs ui-widget ui-widget-content ui-corner-all settingstabs'> \n"
" 		<ul class='ui-tabs-nav ui-helper-reset ui-helper-clearfix ui-widget-header ui-corner-all'> \n"
" 			<li class='ui-state-default ui-corner-top ui-tabs-selected ui-state-active'><a href='#general_driver_info'>Driver Info</a></li> \n"
" 			<li class='ui-state-default ui-corner-top'><a href='#general_crtcs'>CRTCs/Pipes</a></li> \n"
" 			<li class='ui-state-default ui-corner-top'><a href='#general_planes'>Planes</a></li> \n"
" 			<li class='ui-state-default ui-corner-top'><a href='#general_mode'>Modeset</a></li> \n"
" 			<li class='ui-state-default ui-corner-top'><a href='#color_correction'>Color Correction</a></li> \n"
" 		</ul> \n"
" 		<div id='general_driver_info' class='ui-tabs-panel ui-widget-content ui-corner-bottom'> \n"
" 	 	<table width='750' class='ui-widget ui-widget-content ui-corner-all'> \n"
" 		 <tr><td width='200'><p>Product Name</td><td>%s</td> \n"
" 		 <tr><td>EMGD Version</td><td>%s</td> \n"
" 		 <tr><td>EMGDGUI Version</td><td>%d.%d.%d</td> \n"
" 		 <tr><td>Build Date</td><td>%s</td> \n"
" 		 <tr><td>Chipset</td><td>%s</td> \n"
" 		</table>		 \n"
" 		<p> \n"
" 		%s \n"
" 		</p><br><p> \n"
" 		Intel (R) Embedded Media and Graphics Driver (EMGD) Copyright (c) 2003-2014, Intel Corporation \n"
" 		</p>        \n"
" 		</div> \n"
" 		<div id='general_crtcs' class='ui-tabs-panel ui-widget-content ui-corner-bottom ui-tabs-hide'> \n"
" 			%s \n"
" 		</div> \n"
" 		<div id='general_planes' class='ui-tabs-panel ui-widget-content ui-corner-bottom ui-tabs-hide'> \n"
" 			<p><input id='general_planes_apply_plane_configuration' type='button' value='Apply plane configurations' onClick='applyPlanes()'/> \n"
" 				<input id='general_planes_defaults' type='button' value='Defaults' onclick='setDefaultPlane();'/></p> \n"
" 			<form name='form_plane' id='form_plane' action='plane'></form> \n"
" 			<div id='sprite_plane_settings'> \n"
"  			</div> \n"
" 		</div> \n"
" 		<div id='general_mode' class='ui-tabs-panel ui-widget-content ui-corner-bottom ui-tabs-hide'> \n"
" 			<label id='xrandrWarningLabel'></label> \n"
" 			<p> \n"
" 			<div id='displaySortable' class='mode_sortable primarySortable'> \n"
" 				<div id='display0' class='mode_draggable monitorImage'> \n"
" 					<label class='monitorTitle' id='dragLabel0' >Port 1</label><p> \n"
" 					<table width='100%'> \n"
" 						<tr><td><button class='ui-button displayButton leftbutton' onClick='cycleDisplay(-1, 0);'></button></td> \n"
" 						<td align='right'><button class='displayButton rightbutton' onClick='cycleDisplay(1, 0);'></button></td> \n"
" 					</table> \n"
" 				</div> \n"
" 				<div id='display1' class='mode_draggable monitorImage'> \n"
" 					<label class='monitorTitle' id='dragLabel1' >Port 2</label><p> \n"
" 					<table width='100%'> \n"
" 						<tr><td><button class='ui-button displayButton leftbutton' onClick='cycleDisplay(-1, 1);'></button></td> \n"
" 						<td align='right'><button class='displayButton rightbutton' onClick='cycleDisplay(1, 1);'></button></td> \n"
" 					</table> \n"
" 				</div> \n"
" 			</div> \n"
" 			<form name='form_display' id='form_display' method='GET' action='xr'> \n"
" 			display 1 \n"
" 			<select name='cbo_xrandr_position' id='cbo_xrandr_position'> \n"
" 				<option value='--same-as'%s>same as</option> \n"
" 				<option value='--below'%s>below</option> \n"
" 				<option value='--right-of'%s>right of</option> \n"
" 				<option value='--above'%s>above</option> \n"
" 				<option value='--left-of'%s>left of</option> \n"
" 			</select> \n"
" 			display 2<br> \n"
" 			<div class='radio_display_div'> \n"
" 			Primary display: <input type='radio' checked id='radio_primary1' value='1' name='radio_primary'><label for='radio_primary1'>Display 1</label><input type='radio' id='radio_primary2' value='2' name='radio_primary'><label for='radio_primary2'>Display 2</label> \n"
" 		</div> \n"
" 			<p> \n"
" 			<input type='hidden' name='key' class='hidden_key' id='key1' value='' /> \n"
" 			<input type='hidden' name='displayName1' id='displayName1' value='' /> \n"
" 			<input type='hidden' name='displayName2' id='displayName2' value='' /> \n"
" 			<input type='hidden' name='pmode1Name' id='pmode1Name' value='' /> \n"
" 			<input type='hidden' name='pmode2Name' id='pmode2Name' value='' /> \n"
" 			<fieldset id='fieldset_pmode1'> \n"
" 				<legend id='resLabel0'>Port 1</legend> \n"
" 					Resolution					 \n"
" 					<select name='pmode1' id='cbo_pmode1'>%s</select> \n"
" 			</fieldset> \n"
" 			<fieldset id='fieldset_pmode2'> \n"
" 				<legend id='resLabel1'>Port 2</legend> \n"
" 					Resolution \n"
" 					<select name='pmode2' id='cbo_pmode2'>%s</select> \n"
" 						<option></option> \n"
" 					</select> \n"
" 			</fieldset> \n"
" 			<p> \n"
" 			<input type='button' id='modesetApply' value='Apply' onClick='submitDisplayPosition()'/> \n"
" 			</form> \n"
" 		</div>	 \n"
" 		<div id='color_correction' class='ui-tabs-panel ui-widget-content ui-corner-bottom ui-tabs-hide'> \n"
"  <form name='form_color_correction' id='form_color_correction' method='GET' action='color'> \n"
"  \n"
" 			<table border=0 align='center'> \n"
" 				<tr><td> \n"
" 				<div class='radio_display_div'> \n"
" 				%s \n"
" 				<!--<br><input type='radio' name='radio_display' value='VGA-1' onClick='setColors()'>VGA-1-test<br> \n"
" 				<input type='radio' name='radio_display' value='VGA-2' onClick='setColors()'>VGA-2-test<br> --> \n"
" 				&nbsp;&nbsp;Select the display, set a color and apply \n"
" 				</div> \n"
" 				</td></tr> \n"
" 				<tr><td> \n"
" 				<fieldset> \n"
" 					<legend>Gamma Correction</legend> \n"
" 					<table border=0 align='center' width='600px'> \n"
" 						<tr><td> \n"
" 							<input size='5' type='text' type='text' name='sliderGammaR' id='sliderGammaR' readOnly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;' /> \n"
" 							<div id='sliderGammaR_slider' class='color_gamma_slider red_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderGammaG' readOnly='true' id='sliderGammaG' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							  <div id='sliderGammaG_slider' class='color_gamma_slider green_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderGammaB' id='sliderGammaB' readOnly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderGammaB_slider' class='color_gamma_slider blue_slider'></div> \n"
" 						</td></tr> \n"
" 					</table> \n"
" 				</fieldset> \n"
" 				<br> \n"
"  \n"
" 				<fieldset> \n"
" 					<legend>Brightness Correction</legend> \n"
" 					<table border=0 align='center' width='600px'> \n"
" 						<tr><td> \n"
" 							<input size='5' type='text' name='sliderBrightR'  id='sliderBrightR' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderBrightR_slider' class='color_brightness_slider red_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderBrightG' id='sliderBrightG' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderBrightG_slider' class='color_brightness_slider green_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderBrightB' id='sliderBrightB' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderBrightB_slider' class='color_brightness_slider blue_slider'></div> \n"
" 						</td></tr> \n"
" 					</table> \n"
" 				</fieldset> \n"
" 				<br> \n"
" 				<fieldset> \n"
" 					<legend>Contrast Correction</legend> \n"
" 					<table border=0 align='center' width='600px'> \n"
" 						<tr><td> \n"
" 							<input size='5' type='text' name='sliderContrastR' id='sliderContrastR' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderContrastR_slider' class='color_contrast_slider red_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderContrastG' id='sliderContrastG' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderContrastG_slider' class='color_contrast_slider green_slider'></div> \n"
" 						</td><td> \n"
" 							<input size='5' type='text' name='sliderContrastB' id='sliderContrastB' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/> \n"
" 							<div id='sliderContrastB_slider' class='color_contrast_slider blue_slider'></div> \n"
" 						</td><td> \n"
" 					</table> \n"
" 				</fieldset> \n"
" 				<br> \n"
" 			  </td></tr> \n"
" 			</table> \n"
" 			<input type='hidden' name='key' class='hidden_key' id='key2' value='' /> \n"
" 			<input id='color_correction_apply' type='submit' value='Apply'/> <input id='color_correction_defaults' type='button' value='Defaults' onclick='setDefaultColors();'/> \n"
" 			</form> \n"
" 		</div> \n"
" 	</div> </td></tr></table> \n"
"  \n"
"  \n"
" </td><td></td></tr></table> \n"
" </body></HTML> \n"
;

int helper_send_HTTP_response_and_free_malloc(char* mstr, struct MHD_Connection *connection)
{
	struct MHD_Response *response;
	int ret;
	response = MHD_create_response_from_buffer (strlen (mstr), mstr,
                                              MHD_RESPMEM_MUST_FREE);
	if (response == NULL) {
		free (mstr);
		return MHD_NO; 
	}
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;
}

int helper_send_HTTP_response_cpp_string(string cppstr, struct MHD_Connection *connection)
{
	struct MHD_Response *response;
	int ret;
	response = MHD_create_response_from_buffer (strlen (cppstr.c_str()), (char*) cppstr.c_str(),
                                              MHD_RESPMEM_MUST_COPY);
	if (response == NULL) {
		return MHD_NO;
	}
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;
}

/* str must be a string representation of a non-negative int 
 * Returns -1 otherwise
 */
long strtoindex(char* str, double optionalMultiplier)
{
	char *end;
	long ret = -1;
	if (str != NULL && strlen(str) > 0) {
		errno = 0;    /* To distinguish success/failure after call */
		if (optionalMultiplier != 0 && optionalMultiplier != 1) {
			ret = (optionalMultiplier*strtod(str, &end)) + 0.5;
		} else {
			ret = strtoul(str, &end, 0);
		}
		if (errno == 0 && *end == '\0') {
			//success
		} else { 
			ret = -1;
		}
	}
	return ret;	
}

long strtolong(const char* str, long default_val) {
	char *end;
	unsigned long ret;	
	if (str != NULL && strlen(str) > 0) {
		errno = 0;    /* To distinguish success/failure after call */
		ret = strtoul(str, &end, 0);
		if (errno == 0 && *end == '\0') {
			return ret;	
		} 
	}
	return default_val;
}

double strtointeger(const char* str, int default_val) {
	if(str != NULL && strlen(str) > 0) {
		return atof(str);
	}
	return default_val;
}


string getLineFromFile(const char *filePath) {
	string line;
	ifstream myfile (filePath);
	if (myfile.is_open()) {
		getline (myfile,line);
		myfile.close();
	}
	return line;
}

int invalidKeyHTML(struct MHD_Connection *connection) {
	string s;
	s = "<HTML><p>Invalid key</p>\n";
	s +="<a href='/default'>Return</a></HTML>";

	return helper_send_HTTP_response_cpp_string(s, connection);
}

static int f_default(struct MHD_Connection *connection, void *cls) {
	char *key = (char*) cls;
	string s;

	if (key == NULL)
		return f_main(connection, cls);

	s = "<HTML><p>Secure key required. Please enter key phrase<p>\n";
	s += "<form method='GET' action='main'>\n";
	s += "<input type='text' name='key'/><br>\n";
	s += "<input type='submit' value='Submit'/>\n";
	s += "</form></HTML>";

	return helper_send_HTTP_response_cpp_string(s, connection);
}

static const URL_TABLE_STRUCT urlTable[] = {
	{"/main", &f_main},
	{"/crtc", &f_crtc},
	{"/color", &f_color},
	{"/plane", &f_plane},
	{"/xr", &f_xrandr}
};

static ssize_t read_file_block(void *file_ptr, uint64_t pos, char *buffer, size_t max)
{
	fseek((FILE*)file_ptr, pos, SEEK_SET);
	return fread(buffer, 1, max, (FILE*)file_ptr);
}

static void callback_close_file(void *file_ptr)
{
	fclose ((FILE*) file_ptr);
}

static int established_connection(void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data, size_t *upload_data_size, void **ptr)
{
	static int aptr;
	struct MHD_Response *response;
	int ret, i,pageCount;
	FILE *file;
	struct stat buf;

	if (0 != strcmp (method, MHD_HTTP_METHOD_GET)) {
		//printf("Will ignore any %s data\n", method);
		//return MHD_NO;              /* unexpected method */
	}
	if (&aptr != *ptr) {
		/* do never respond on first call */
		*ptr = &aptr;
		return MHD_YES;
	}
	*ptr = NULL;                  /* reset when done */

	//First see if need to serve a file
	if (0 == stat (&url[1], &buf))
		file = fopen (&url[1], "rb");
	else
		file = NULL;
	if (file != NULL) {
		response = MHD_create_response_from_callback (buf.st_size, 32 * 1024,     /* 32k page size */
                                        &read_file_block,
                                        file,
                                        &callback_close_file);
		if (response == NULL) {
			fclose (file);
			return MHD_NO;
		}
		ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response (response);
		return ret;
	}

	// See if URL matches internal function
	pageCount = sizeof(urlTable) / sizeof(URL_TABLE_STRUCT);  
	for (i=0; i<pageCount; i++) {
		if (strcmp(url,urlTable[i].functionname) == 0)
			return urlTable[i].urlFunction(connection, cls);
	} 

	// default when no match
	return f_default(connection, cls);
}

void signal_exit(int num) {
	printf("\nCleaning up and exiting. (Caught signal %d)\n", num);
	MHD_stop_daemon (my_daemon);
	exit(num);
}

int main (int argc, char *const *argv) {
	char *key = NULL;
	string input_line;

	if (argc > 2) {
		printf("Only 1 argument expected. Syntax:\n");
		printf("  %s <optional key phrase>\n     or\n", argv[0]);
		printf("  %s --no-key\n", argv[0]);
		return 0;
	} else if (argc == 2) {
		if (strstr(argv[1], "--no-key") == NULL) {
			key = argv[1];
		}
	} else {
		printf("Please enter a security key (or just press enter for no key)\n");
		getline(cin, input_line);
		key =(char*) input_line.c_str();
		if (key !=NULL && key[0] == '\0') {
			key = NULL;
			printf("Security warning:\n\tNo key provided. Any user can connect to the daemon\n");
		}
	}
	printf("Daemon started. Open a browser to localhost:%d\n", PORT);
	
	/* Set up structure to only allow local incoming connections */
	struct sockaddr_in ip_addr;
	memset (&ip_addr, 0, sizeof (struct sockaddr_in));
	ip_addr.sin_family = AF_INET;
	ip_addr.sin_port = htons (PORT);
	inet_pton (AF_INET, "127.0.0.1", &ip_addr.sin_addr);

	my_daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
			PORT, NULL, NULL, 
			&established_connection, key, 
			MHD_OPTION_SOCK_ADDR, &ip_addr, /* comment out this line to allow non-local connections */
			MHD_OPTION_END);
	if (my_daemon == NULL)
		return 1;

	//TODO evaluate sigprocmask
	signal (SIGINT, signal_exit);
	signal (SIGTERM, signal_exit);
	//system("firefox localhost:8888");

	while (1) {
		sleep(10);
	}

	return 0;
}

