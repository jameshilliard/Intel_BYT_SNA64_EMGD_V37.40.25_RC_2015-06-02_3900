/******************************************************************************
 * Copyright 2014 Intel Corporation All Rights Reserved.
 *
 * The source code, information and material ("Material") contained herein is
 * owned by Intel Corporation or its suppliers or licensors, and title to such
 * Material remains with Intel Corporation or its suppliers or licensors. The 
 * Material contains proprietary information of Intel or its suppliers and 
 * licensors. The Material is protected by worldwide copyright laws and treaty 
 * provisions. No part of the Material may be used, copied, reproduced, modified,
 * published, uploaded, posted, transmitted, distributed or disclosed in any way 
 * without Intel's prior express written permission. No license under any patent,
 * copyright or other intellectual property rights in the Material is granted to 
 * or conferred upon you, either expressly, by implication, inducement, estoppel 
 * or otherwise. Any license under such intellectual property rights must be 
 * express and approved by Intel in writing.
 *
 * Unless otherwise agreed by Intel in writing, you may not remove or alter this
 * notice or any other notice embedded in Materials by Intel or Intel's suppliers 
 * or licensors in any way.
 *
 ********************************************************************************/
var NUMBER_OF_SPRITE_PLANES=4;
var planeIdList = ["Sprite-A", "Sprite-B", "Sprite-C", "Sprite-D"];
var crtcList = ["crtc0", "crtc1"]; /*This must match sysfs values */
var pipesEnum = new Object();
pipesEnum[crtcList[0]+"_display_plane"] = "01";
pipesEnum[crtcList[0]+"_sprite1"] = "02";
pipesEnum[crtcList[0]+"_sprite2"] = "04";
pipesEnum[crtcList[1]+"_display_plane"] = "01";
pipesEnum[crtcList[1]+"_sprite1"] = "02";
pipesEnum[crtcList[1]+"_sprite2"] = "04";

//slider code
$(function() {
	createJquerySliders();
});

function createJquerySliders()
{
	$( ".gamma_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0.6,
		max: 6.0,
		step: 0.1,
		value: 1.0,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".color_gamma_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0.6,
		max: 6.0,
		step: 0.1,
		value: 1.0,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".brightness_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: -127,
		max: 128,
		step: 1,
		value: -5,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".color_brightness_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0,
		max: 255,
		step: 1,
		value: 128,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".contrast_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0,
		max: 255,
		step: 1,
		value: 67,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".color_contrast_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0,
		max: 255,
		step: 1,
		value: 128,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".saturation_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0,
		max: 1023,
		step: 1,
		value: 145,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
				$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
	$( ".hue_slider" ).slider({
		orientation: "horizontal",
		range: "min",
		min: 0,
		max: 1023,
		step: 1,
		value: 0,
		slide: function( event, ui ) {
			if (!event)
				event = window.event;
			var control = event.target;
			$( "#"+control.id.substr(0, control.id.lastIndexOf("_"))).val( ui.value );
		}
	});
}

function setPlanesCorrectionSlider()
{
	// Get the color from the text box and set the slider to match
	// Note the ids of the text box and slider must match except the slider should add a _slider suffix
	$(".gamma_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
				$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 1.0);
		}
	});
	$(".brightness_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
				$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", -5);
		}
	});
	$(".contrast_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
					$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 67);
		}
	});
	$(".saturation_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
					$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 145);
		}
	});
	$(".hue_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
					$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 0);
		}
	});
}

function setColorCorrectionSlider()
{
	// Get the color from the text box and set the slider to match
	// Note the ids of the text box and slider must match except the slider should add a _slider suffix
	$(".color_gamma_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
					$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 1.0);
		}
	});
	$(".color_brightness_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
				$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 128);
		}
	});
	$(".color_contrast_slider").each(function()
	{
		var sliderElement = document.getElementById(($(this).attr("id")+"").substring(0, ($(this).attr("id")+"").lastIndexOf("_")));
		if(sliderElement != null )
		{
			var value = parseFloat(sliderElement.value);
			if(isNaN(value) === false)
				$(this).slider("option", "value", value);
			else
				$(this).slider("option", "value", 128);
		}
	});
}
//end slider code

//Accordion code
$(function() {
	$( ".closedPanel" ).accordion({
		collapsible: true,
		active: false,			
		autoHeight: false				
	});
});

$(function() {
	$( ".openPanel" ).accordion({
		collapsible: true,				
		autoHeight: false
	});
});
//end Accordion code

/* Set all input buttons in the jquery theme */
$(function() {
	$( "input:button" ).button();			
}); 

$(function() {
	$( "input:submit" ).button();
});

$(function() {
	$( ".help_icon" ).button({
		icons: {
		primary: "ui-icon-help"
		},
		text: false
	});			
}); 
	
//dialog that pops-up when a control fails validation.
$(function() {
	$( "#dialog-validate-form" ).dialog({
		autoOpen: false,
		modal: true,	
		minHeight: 250,
		buttons: {
			Ok: function() {
				$( this ).dialog( "close" );
			}
		}
	});
});

/* set settingstabs class to jquery tabs */
$(function(){  $('.settingstabs').tabs();  });

$(function() {
	$( ".jradio" ).buttonset();
});



//function that can be used for simulating a javascript event. Used on situations like onLoad when initialization code is in the event handlers.
function fireEvent(inputElement,eventName)
{
    if (document.createEventObject)
	{
		var eventHandler = "on"+eventName; //currently used for onchange. can be used to replace other calls.
		inputElement.fireEvent(eventHandler);
    }
    else
	{
		var event = document.createEvent("HTMLEvents");
		event.initEvent(eventName, true, true); 
		inputElement.dispatchEvent(event);
    }
}

function addPlaneConfig(configClass, configNumber)
{	
	var str="";
	str += '<fieldset>'+
	' <legend>Plane Config</legend>'+
	'  <table border=0 align="center">'+
	'   <tr>'+
	'   <td>'+
	'	<div id="planeSortable'+configNumber+'" class="plane_sortable '+configClass+'">'+
	'	</div>'+

	'	<fieldset>'+
	'	 <legend>Gamma</legend>'+
	'	 <table border=0 align="center" width="600px">'+
	'	   <tr>'+
	'		<td>'+
	'		 <input size="5" type="text" name="global_sliderValueOverlayGammaR'+configNumber+'" readOnly="true" id="global_sliderValueOverlayGammaR'+configNumber+'" class="OverlayGammaR gamma" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;"/>'+
	'		 <div id="global_sliderValueOverlayGammaR'+configNumber+'_slider" class="gamma_slider red_slider"></div>'+
	'		</td><td>'+
	'		<input size="5" type="text" name="global_sliderValueOverlayGammaG'+configNumber+'"  readOnly="true" id="global_sliderValueOverlayGammaG'+configNumber+'"  class="OverlayGammaG gamma" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;"/>'+
	'		<div id="global_sliderValueOverlayGammaG'+configNumber+'_slider" class="gamma_slider green_slider"></div>'+
	'		</td><td>'+
	'		<input size="5" type="text" name="global_sliderValueOverlayGammaB'+configNumber+'" readOnly="true" id="global_sliderValueOverlayGammaB'+configNumber+'" class="OverlayGammaB gamma" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;"/>'+
	'		<div id="global_sliderValueOverlayGammaB'+configNumber+'_slider" class="gamma_slider blue_slider"></div>'+
	'		</td>'+
	'		</tr>'+
	'		</table>'+
	'		</fieldset>'+
	'		<br>'+
	'		<fieldset>'+
	'		 <legend>Other correction</legend>'+
	'		  <table border=0 align="center" width="600px">'+
	'			<tr>'+
	'			<td>'+
	'			Brightness'+
	'			<input size="5" type="text" name="global_sliderValueOverlay2'+configNumber+'"  readOnly="true" id="global_sliderValueOverlay2'+configNumber+'"  class="OverlayBrightness brightness" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;" />'+
	'			<div id="global_sliderValueOverlay2'+configNumber+'_slider" class="brightness_slider"></div>'+
	'			</td><td>'+
	'			Contrast'+
	'			<input size="5" type="text" name="global_sliderValueOverlay3'+configNumber+'" readOnly="true" id="global_sliderValueOverlay3'+configNumber+'" class="OverlayContrast contrast" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;"  />'+
	'			<div id="global_sliderValueOverlay3'+configNumber+'_slider" class="contrast_slider"></div>'+
	'			</td></tr><tr><td>'+
	'			Saturation'+
	'			<input size="5" type="text" name="global_sliderValueOverlay4'+configNumber+'" readOnly="true" id="global_sliderValueOverlay4'+configNumber+'" class="OverlaySaturation saturation" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;" />'+
	'			<div id="global_sliderValueOverlay4'+configNumber+'_slider" class="saturation_slider"></div>'+
	'			</td>'+
	'			<td>'+
	'			Hue'+
	'			<input size="5" type="text" name="global_sliderValueOverlay5'+configNumber+'" readOnly="true" id="global_sliderValueOverlay5'+configNumber+'" class="OverlayHue hue" style="border:0; color:#f6931f; font-weight:bold; margin-left: 15px;" />'+
	'			<div id="global_sliderValueOverlay5'+configNumber+'_slider" class="hue_slider"></div>'+
	'			</td>'+
	'			</tr>'+
	'			</table>'+
	'			</fieldset>'+
	'			</td><td valign="top">'+
//	'			<a href="help/lite_overlay_color_corr.htm" target="_blank"><img src="images/questionmark_16w.gif"></a>'+
	'		  </td>'+
	'		  </tr>'+
	'		</table>'+
	'	   </fieldset>';
	
	return str;
}

function createPlaneConfigs()
{
	var str="";
	var i;
	document.getElementById('sprite_plane_settings').innerHTML = "";
	str += addPlaneConfig("primaryPlaneConfig", "1");
	
	for(i=2; i<=NUMBER_OF_SPRITE_PLANES;i++)
	{
		str+=addPlaneConfig("hiddenSortable", i);
	}
	
	document.getElementById('sprite_plane_settings').innerHTML += str;
	
		$(function() {
		$( "div.plane_sortable" ).sortable({
			revert: true,
			connectWith: ".plane_sortable",
			receive: function(event, ui){
				togglePlaneConfigs();
			}
		});
		$( ".plane_sortable" ).disableSelection();
		
		});
	
	$(".primaryPlaneConfig").each(function(){
			var i;
			var str="";
			for(i=0;i<NUMBER_OF_SPRITE_PLANES;i++)
			{
				str += '	 <div id="'+planeIdList[i]+'" class="plane_draggable ui-state-default">'+planeIdList[i]+'</div>';
			}
			$(this).html(str);
		});
	
	togglePlaneConfigs();
	createJquerySliders();
}

function togglePlaneConfigs()
{
	var hidden = false;
	$("div.plane_sortable").each(function(){
		if($(this).sortable("toArray").length == 0)
		{
			if(!hidden)
			{
				$(this).removeClass("hiddenSortable");
				$(this).parents("fieldset").show();
				hidden = true;
			}
			else
				$(this).addClass("hiddenSortable");
		}		
		});
	$(".hiddenSortable").each(function(){
		$(this).parents("fieldset").hide();
	});
}

function setPlaneText() {
			var i;
			var dropableID;
			var str;
			var configNumber;

			/* Move plane draggables to right location  */
			for (configNumber=1; configNumber<=NUMBER_OF_SPRITE_PLANES;configNumber++) {
				dropableID = document.getElementById('planeSortable'+configNumber);
				str = '   <div id="'+planeIdList[configNumber-1]+'" class="plane_draggable ui-state-default">'+planeIdList[configNumber-1]+'</div>';
				dropableID.innerHTML = str;
				togglePlaneConfigs(); /* update show/hide plane panels after move dropables around */
			} 


			for (i=0; i<planeArray.length; i++) {
				for (var id in planeArray[i]) {
					document.getElementById(id+(i+1)).value = planeArray[i][id];
				} 
			}
}


function addPipeConfig(pipe_prefix, sprite1, sprite2, orderStr)
{
	var str = '<ul id="'+pipe_prefix+'_config" class="pipesortable zorder">';
	if (orderStr == null || orderStr.length == 0)
		orderStr = "0x010204";
	
	orderStr = orderStr.substr(orderStr.length - 6); /* last 6 digits */
	for (var i=0; i<= 4; i+=2) {
		if (orderStr.substr(i,2) == "01")
			str += '<li class="ui-state-default" id="'+pipe_prefix+'_display_plane" value="01">Display Plane</li>';
		else if (orderStr.substr(i,2) == "02")
			str += '<li class="ui-state-default" id="'+pipe_prefix+'_sprite1" value="02">'+sprite1+'</li>';
		else if (orderStr.substr(i,2) == "04")
			str += '<li class="ui-state-default" id="'+pipe_prefix+'_sprite2" value="04">'+sprite2+'</li>';
	}
	str += '</ul>';
	return str;
}

function createPipeConfigs()
{
	/* Note: "z" comes from table.cpp, htmlName for zorder */
	var orderStr; 
	var c1 = document.getElementById(crtcList[0]+"_z");
	if (c1 != null) {
		orderStr = document.getElementById(crtcList[0]+'_config_hidden').value;
		c1.innerHTML = addPipeConfig(crtcList[0], "Sprite A", "Sprite B", orderStr);
	}
	var c2 = document.getElementById(crtcList[1]+"_z", "");
	if (c2 != null) {
		orderStr = document.getElementById(crtcList[1]+'_config_hidden').value;
		c2.innerHTML = addPipeConfig(crtcList[1], "Sprite C", "Sprite D", orderStr);
	}
	$(".pipesortable").sortable();
	$(".pipesortable").disableSelection();
}

function crtcClick(formName)
{
	$('.zorder').each(function()  {
		var hidden_form = document.getElementById($(this).attr('id')+'_hidden');
		if (hidden_form != null) {
			hidden_form.value = getZorder($(this).sortable("toArray"));
		} 
	});

	document.forms[formName].submit();
}

function getZorder(zorderArray)
{
	var str="0x00";

	str+=pipesEnum[zorderArray[0]];
	str+=pipesEnum[zorderArray[1]];
	str+=pipesEnum[zorderArray[2]];

	return str;
}

function getDisplayType(displayChoice)
{
	var selection = document.getElementById(displayChoice).options[document.getElementById(displayChoice).selectedIndex].value;
	return selection.substring(0,selection.length-1);
}

function setSecondaryDisplayOptions()
{
	var primaryDisplay = getDisplayType('global_cbo_primary_display');

	if(primaryDisplay=="hdmi"||primaryDisplay=="dp"||primaryDisplay=="edp")
	{
		document.getElementById('primary_port_connector_div').style.display = "block";
		
		if($("#global_cbo_secondary_display option[value='vga1']").length <= 0)
			$("#global_cbo_secondary_display").append('<option value="vga1">VGA</option>');
	}
	else if(primaryDisplay == "vga")
	{
		document.getElementById('primary_port_connector_div').style.display = "none";
		$("#global_cbo_secondary_display option[value='vga1']").remove();
	}
}

function setPrimaryDisplayOptions()
{
	var secondaryDisplay = getDisplayType('global_cbo_secondary_display');

	if(secondaryDisplay=="hdmi"||secondaryDisplay=="dp"||secondaryDisplay=="edp")
	{
		document.getElementById('secondary_port_connector_div').style.display = "";
		
		if($("#global_cbo_primary_display option[value='vga1']").length <= 0)
			$("#global_cbo_primary_display").append('<option value="vga1">VGA</option>');
	}
	else if(secondaryDisplay == "vga")
	{
		document.getElementById('secondary_port_connector_div').style.display = "none";
		$("#global_cbo_primary_display option[value='vga1']").remove();
	}
}

function primaryPortConnectorSelected(event)
{
	var control = getControlFromEvent(event);
	
	if(control.value == "B"){
		document.getElementById("secondary_port_connector_radio2").checked = "yes";
	}
	else{
		document.getElementById("secondary_port_connector_radio1").checked = "yes";
	}
}

function secondaryPortConnectorSelected(event)
{
	var control = getControlFromEvent(event);
	
	if(control.value == "B"){
		document.getElementById("primary_port_connector_radio2").checked = "yes";
	}
	else{
		document.getElementById("primary_port_connector_radio1").checked = "yes";
	}
}


function fillColorCorrectionPanels()
{
	$( ".panel" ).each(function(){
		var prefix = $(this).attr("id").substring(0, $(this).attr("id").indexOf("_"));
		document.getElementById(prefix+'_colorcorrection_panel').innerHTML=createColorCorrectionTab(prefix);
	});
	
	createJquerySliders();
}

function createColorCorrectionTab(prefix)
{
	str =           "<table border=0 align='center'>"+
					"	<tr><td>"+
					"		<fieldset>"+
					"			<legend>Gamma Correction</legend>"+
					"			<table border=0 align='center' width='600px'>"+
					"			<tr>"+
					"			<td>"+
					"			<input size='5' type='text' type='text' name='"+prefix+"_sliderValueGammaR' id='"+prefix+"_sliderValueGammaR' readOnly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;' />"+
					"			  <div id='"+prefix+"_sliderValueGammaR_slider' class='gamma_slider red_slider'></div>"+
					"			</td><td>"+
					"			 <input size='5' type='text' name='"+prefix+"_sliderValueGammaG' readOnly='true' id='"+prefix+"_sliderValueGammaG' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueGammaG_slider' class='gamma_slider green_slider'></div>"+
					"			  </td><td>"+
					"			  <input size='5' type='text' name='"+prefix+"_sliderValueGammaB' id='"+prefix+"_sliderValueGammaB' readOnly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueGammaB_slider' class='gamma_slider blue_slider'></div>"+
					"			</td>"+
					"			</tr>"+
					"			</table>"+
					"		</fieldset>"+
					"		<br>"+
					"		<fieldset>"+
					"			<legend>Brightness Correction</legend>"+
					"			<table border=0 align='center' width='600px'>"+
					"			<tr>"+
					"			<td>"+
					"			  <input size='5' type='text' name='"+prefix+"_sliderValueBrightR'  id='"+prefix+"_sliderValueBrightR' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueBrightR_slider' class='brightness_slider red_slider'></div>"+
					"			</td><td>"+
					"			 <input size='5' type='text' name='"+prefix+"_sliderValueBrightG' id='"+prefix+"_sliderValueBrightG' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueBrightG_slider' class='brightness_slider green_slider'></div>"+
					"			</td><td>"+
					"			<input size='5' type='text' name='"+prefix+"_sliderValueBrightB' id='"+prefix+"_sliderValueBrightB' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueBrightB_slider' class='brightness_slider blue_slider'></div>"+
					"			</td>"+
					"			</tr>"+
					"			</table>"+
					"		</fieldset>"+
					"		<br>"+
					"		<fieldset>"+
					"			<legend>Contrast Correction</legend>"+
					"			<table border=0 align='center' width='600px'>"+
					"			<tr>"+
					"			<td>"+
					"			  <input size='5' type='text' name='"+prefix+"_sliderValueContrastR' id='"+prefix+"_sliderValueContrastR' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueContrastR_slider' class='brightness_slider red_slider'></div>"+
					"			</td><td>"+
					"			<input size='5' type='text' name='"+prefix+"_sliderValueContrastG' id='"+prefix+"_sliderValueContrastG' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueContrastG_slider' class='brightness_slider green_slider'></div>"+
					"			</td><td>"+
					"			</td><td>"+
					"			 <input size='5' type='text' name='"+prefix+"_sliderValueContrastB' id='"+prefix+"_sliderValueContrastB' readonly='true' style='border:0; color:#f6931f; font-weight:bold; margin-left: 15px;'/>"+
					"			  <div id='"+prefix+"_sliderValueContrastB_slider' class='brightness_slider blue_slider'></div>"+
					"			</td><td>"+
					"			</td>"+
					"			</tr>"+
					"			</table>"+
					"		</fieldset>"+
					"		<br>"+
					"  </td><td valign='top'>"+
					"	<a href='help/lite_color_correct.htm' target='_blank'><img src='images/questionmark_16w.gif'></a>"+
					"  </td></tr>"+
					"</table>";
					
					return str;
}

function fillCRTCSettingsPanels()
{
	  prefix = "general_primary"
      str = "<fieldset>"+
					 "	<legend>Primary CRTC Settings</legend>"+
					 "  <label for='"+prefix+"_display_width' id='"+prefix+"_display_width_label'>Display Width</label>"+
					 "  <input type='text' id='"+prefix+"_display_width'  name='"+prefix+"_display_width' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_width_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='global_display_height' id='global_display_height_label'>Display Height</label>"+
					 "  <input type='text' id='"+prefix+"_display_height'  name='"+prefix+"_display_height' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_height_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='"+prefix+"_display_refresh' id='"+prefix+"_display_refresh_label'>Display Refresh</label>"+
					 "  <input type='text' id='"+prefix+"_display_refresh'  name='"+prefix+"_display_refresh' minValue='50' maxValue='120' label='"+prefix+"_framebuffer_refresh_label' onBlur='validateUserInput(event);'/><br>"	
	  str += "<input type='checkbox' id='general_primary_framebuffer' name='general_primary_framebuffer' panel='"+prefix+"_fbsettings' onClick='toggleFramebufferSettings(event)'/><label>Show Advanced Framebuffer Settings</label>";			
	  str += createFrameBufferSettingsTab(prefix);
	  str += "</fieldset>";	  
	  document.getElementById(prefix+'_crtcsettings').innerHTML=str;	  
	  
	  prefix = "general_secondary";
      str = "<fieldset>"+
					 "	<legend>Secondary CRTC Settings</legend>"+
					 "  <label for='"+prefix+"_display_width' id='"+prefix+"_display_width_label'>Display Width</label>"+
					 "  <input type='text' id='"+prefix+"_display_width'  name='"+prefix+"_display_width' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_width_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='global_display_height' id='global_display_height_label'>Display Height</label>"+
					 "  <input type='text' id='"+prefix+"_display_height'  name='"+prefix+"_display_height' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_height_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='"+prefix+"_display_refresh' id='"+prefix+"_display_refresh_label'>Display Refresh</label>"+
					 "  <input type='text' id='"+prefix+"_display_refresh'  name='"+prefix+"_display_refresh' minValue='50' maxValue='120' label='"+prefix+"_framebuffer_refresh_label' onBlur='validateUserInput(event);'/><br>"
						
	  str += "<input type='checkbox' id='general_secondary_framebuffer' name='general_secondary_framebuffer' panel='"+prefix+"_fbsettings' onClick='toggleFramebufferSettings(event)'/><label>Show Advanced Framebuffer Settings</label>";
	  str += createFrameBufferSettingsTab(prefix);
	  str += "</fieldset>";
	  document.getElementById(prefix+'_crtcsettings').innerHTML=str;
}

function toggleFramebufferSettings(event)
{
	var control = getControlFromEvent(event);
	if(control.checked)
	{
		$("#"+control.id).each(function(){
			var panel = $(this).attr("panel");
			document.getElementById(panel).style.display = "block";
			});	
	}
	else{
		$("#"+control.id).each(function(){
			var panel = $(this).attr("panel");
			document.getElementById(panel).style.display = "none";
			});
	}
}

function createFrameBufferSettingsTab(prefix)
{
	if(prefix.indexOf("secondary") != -1)
		str = 	"<div id='"+prefix+"_fbsettings' name='"+prefix+"_fbsettings' style='display:none'>"
	else
		str = 	"<div id='"+prefix+"_fbsettings' name='"+prefix+"_fbsettings' style='display:none'>"
	str +=			 "<fieldset>"+
					 "	<legend>Framebuffer Settings</legend>"+
					 "  <label for='"+prefix+"_framebuffer_width' id='"+prefix+"_framebuffer_width_label'>Width</label>"+
					 "  <input type='text' id='"+prefix+"_framebuffer_width'  name='"+prefix+"_framebuffer_width' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_width_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='global_framebuffer_height' id='global_framebuffer_height_label'>Height</label>"+
					 "  <input type='text' id='"+prefix+"_framebuffer_height'  name='"+prefix+"_framebuffer_height' minValue='0' maxValue='2048' label='"+prefix+"_framebuffer_height_label' onBlur='validateUserInput(event);'/><br>"+
					 "  <label for='"+prefix+"_framebuffer_depth' id='"+prefix+"_framebuffer_depth_label'>Depth</label>"+
					 "  <input type='text' id='"+prefix+"_framebuffer_depth'  name='"+prefix+"_framebuffer_depth' value='24'label='"+prefix+"_framebuffer_depth_label' onBlur='validateUserInput(event);'/><br>"+
					 "  Color Quality <select name='"+prefix+"_framebuffer_color_quality' id='"+prefix+"_framebuffer_color_quality'>"+
					 "	<option value='8'>8</option>"+
					 "	<option value='16'>16</option>"+
					 "	<option value='24'>24</option>"+
					 "	<option value='32' selected='selected'>32</option>"+
					 "  </select>"+
					 "</fieldset>"+
			"</div>"
					 
	return str;
}


function displayAbout()
{
	var about = "";
	$( '#dialog-options-form' ).dialog( 'close' );
	 $.ajax({url: "xml/about.xml",
		dataType: "xml",async: false,
		success: function(data)
		{
			about = $(data).find("content").text().replace(/^\s+|\s+$/g, '');
			about = about+$(data).find("version").text();
		}
	 });
	alert(about);
}


function getControlFromEvent(event)
{
	if (!event) 
		event = window.event;
	
	var control = event.srcElement?event.srcElement:event.target;
	
	return control;
}


function enableCentering(event)
{
	var control = getControlFromEvent(event);
	
	var controlArray = document.getElementsByName(control.value);
	
	for(i=0;i<controlArray.length;i++)
	{
		controlArray[i].disabled = control.checked;
	}
	
}

