/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_dispatch_drv.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2014, Intel Corporation.
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
 *  This file contains the top level dispatch table definition and includes
 *  the common header files necessary to interface with the EMGD-KMS
 *  graphics driver.
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPATCH_DRV_H
#define _IGD_DISPATCH_DRV_H

#include <igd_pd.h>
#include <igd_ovl.h>

/*!
 * @ingroup render_group
 * @brief Dispatch table for accessing all runtime driver functions.
 *
 * This is the dispatch table for the driver. All rendering and driver
 * manipulation functionality is done by calling functions within this
 * dispatch table.
 * This dispatch table will be populated during the igd_module_init()
 * function call. Upon returning from that call all usable members
 * will be populated. Any members left as NULL are not supported in the
 * current HAL configuration and cannot be used.
 */
typedef struct _igd_dispatch {

	/*!
	 * @brief Return the list of available DCs.
	 *
	 * query_dc() returns the live zero terminated Display Configuration list.
	 *  All usable display configurations are returned from the HAL. The IAL
	 *  must select one from the list when calling alter_displays().
	 *
	 * @param driver_handle The driver handle returned from igd_driver_init()
	 *
	 * @param dc_list The returned display configuration(s) list. The IAL
	 *  should use a dc from the list when calling alter_displays(). The
	 *  dc_list is zero terminated and live. It should not be altered by the
	 *  IAL. See @ref dc_defines
	 *
	 * @param flags modifies the behavior of the function.
	 *   See: @ref query_dc_flags
	 *
	 * @return 0: Success.
	 * @return -IGD_INVAL:  Otherwise
	 */
	int (*query_dc)(igd_driver_h driver_handle, unsigned long request,
		unsigned long **dc_list, unsigned long flags);
	/*!
	 *  This function returns the list of available pixel formats for the
	 *  framebuffer and the list of available pixel formats for the cursor.
	 *
	 *  Both lists end with NULL.  They are read only and should
	 *  not be modified.
	 *
	 *  @bug To be converted to take a driver handle for IEGD 5.1
	 *
	 *  @param display_handle A igd_display_h type returned from a previous
	 *    display->alter_displays() call.
	 *
	 *  fb_list_pfs  - Returns the list of pixel formats for the framebuffer.
	 *
	 *  cu_list_pfs - Returns the list of pixel formats for the cursor.
	 *
	 *
	 * Returns:
	 *  0: Success
	 *  -IGD_INVAL: Error;
	 */
	int (*get_pixelformats)(igd_display_h display_handle,
		unsigned long **fb_list_pfs, unsigned long **cu_list_pfs,
		unsigned long **overlay_pfs, unsigned long **render_pfs,
		unsigned long **texture_pfs);


	/*!
	 * Gets attributes for a display. SS will allocate the memory required to
	 * return the *attr_list. This is a live copy of attributes used by both
	 * IAL and HAL. Don't deallocate this memory. This will be freed by the
	 * HAL.
	 *
	 * @param driver_handle Driver handle returned from a call to
	 *   igd_driver_init()
	 *
	 * @param num_attrs pointer to return the number of attributes
	 *    returned in attr_list.
	 *
	 * @param attr_list pointer to return the attributes.
	 *
	 * @returns 0 onSuccess
	 * @returns -IGD_ERROR_NOATTR No attributes defined for this display
	 * @returns -IGD_INVAL otherwise
	 */
	int (*get_attrs)(igd_driver_h driver_handle,
			unsigned short port_number,
			unsigned long *num_attrs,
			igd_attr_t **attr_list);

	/*!
	 * set attributes for a display.
	 *
	 * @param driver_handle Driver handle returned from a call to
	 *   igd_driver_init()
	 *
	 * @param num_attrs pointer to return the number of attributes
	 *   returned in attr_list. This is equal to the num_attrs returned by
	 *   igd_get_attrs().
	 *
	 * @param attr_list pointer returned from igd_get_attrs(). Change
	 *   the attributes to desired values.
	 *
	 * @returns 0 on Success
	 * @returns -IGD_ERROR_NOATTR No attributes defined for this display
	 * @returns -IGD_INVAL Otherwise
	 */
	int (*set_attrs)(igd_driver_h driver_handle,
			emgd_crtc_t *emgd_crtc,
			unsigned short port_number,
			unsigned long num_attrs,
			igd_attr_t *attr_list);

	/*!
	 * Save all driver registers and then restore all
	 *   registers from a previously saved set.
	 * This function is optional in the HAL. The caller must check that it
	 * is non-null before calling.
	 *
	 * @param driver_handle The driver handle returned from igd_driver_init().
	 *
	 * @return 0 on Success
	 * @return <0 on Error
	 */
	int (*driver_save_restore)(igd_driver_h driver_handle);

	int (*pwr_alter)(igd_driver_h driver_handle, unsigned int power_state);

	/*!
	 * Gets the value of a runtime driver parameter. These parameters are
	 * each defined with a unique ID and may be altered at runtime.
	 *
	 * @note: There is a wrapper for this function in the dispatch table that
	 * takes a display instead of a driver handle. This version is for use
	 * when displays are not yet available.
	 *
	 * @return 0 Success
	 * @return -IGD_INVAL Error
	 */
	int (*get_param)(igd_driver_h driver_handle, unsigned long id,
		unsigned long *value);

	/*!
	 * Sets the value of a runtime driver parameter. These parameters are
	 * each defined with a unique ID and may be altered at runtime.
	 *
	 * Note: There is a wrapper for this function in the dispatch table that
	 * takes a display instead of a driver handle. This version is for use
	 * when displays are not yet available.
	 *
	 * @return 0 Success
	 * @return  -IGD_INVAL Error
	 */
	int (*set_param)(igd_driver_h driver_handle, unsigned long id,
		unsigned long value);



	/* igd_ovl.h */
	/*!
	 * Alter the overlay associated with the given display_h.
	 *  Only 1 video surface can be associated with a given display_h
	 *  (i.e. you can not have 2 video surfaces using the same display).
	 *  This function should be called once for each unique framebuffer.
	 *  Therefore, single, twin, clone, and vertical extended  modes
	 *  should call this function once, and the HAL will properly display
	 *  video using the primary overlay and second overlay if necessary.
	 *  Whereas extended should call this function twice (once for each
	 *  display_h) if the video spans multiple displays.
	 *  In DIH, this function can be called once for each display, since a
	 *  video surface can not span displays in DIH.
	 *
	 * The primary display in clone, the primary display in vertical extended,
	 *  and the primary display in extended will always use the primary
	 *  overlay.  The secondary display in clone, the secondary display in
	 *  vertical extended, and the secondary display in extended will always
	 *  use the secondary overlay.  The hardware overlay resources (excluding
	 *  any video memory) will be allocated internal to the HAL during the
	 *  _igd_dispatch::alter_displays() call.  Video memory surfaces required
	 *  internal to the HAL, for stretching or pixel format conversions, will
	 *  be dynamically allocated and freed as necessary.
	 *
	 * @param display_h Input display used with this overlay
	 * @param appcontext_h Input appcontext which may be used for blend.
	 *     This can be the same appcontext which is used for blend and
	 *     2d (DD and XAA/EXA).
	 * @param src_surf Input src surface information
	 * @param src_rect Input src surface rectangle.
	 *     This is useful for clone, extended, and vertical extended modes
	 *     where the entire src_surf may not be displayed.
	 * @param dest_rect Input dest surface rectangle.  This is relative to the
	 *     framebuffer not the display (so clone mode works properly).
	 * @param ovl_info Input overlay information to display.
	 *     The color key, video quality, and gamma must be valid
	 *     and correct when the overlay is on.  That means NO passing in NULL
	 *     values to use previous settings.
	 * @param flags Input to turn on/off the overlay and set other flags.
	 *   See: @ref alter_ovl_flags
	 *
	 * @returns 0 (IGD_SUCCESS) on Success
	 * @returns <0 (-igd_errno) on Error.  The overlay will be off, no need
	 *     for the IAL to call the HAL to turn the overlay off.
	 *     If -IGD_ERROR_INVAL is returned, something is either to big or
	 *      to small for the overlay to handle, or it is panned off of the
	 *      displayed.  This is likely not critical, since it may be stretched
	 *      or panned back, so the overlay can support it.  The IAL
	 *      should not return an error to the application, or else the
	 *      application will likely exit.
	 *     If -IGD_ERROR_HWERROR is returned, something is outside of what
	 *      the overlay can support (pitch to large, dot clock to large,
	 *      invalid pixel format, ring or flip not happening).  In this case,
	 *      the IAL should return an error to the application, since
	 *      additional calls will always fail as well.
	 */
	int (*alter_ovl)(igd_display_h display_h,
		unsigned long        ovl_plane_id,
		igd_appcontext_h     appcontext_h,
		emgd_framebuffer_t  *src_surf,
		igd_rect_t          *src_rect,
		igd_rect_t          *dest_rect,
		igd_ovl_info_t      *ovl_info,
		unsigned int         flags);
    int (*alter_ovl_osd)(igd_display_h display_h,
   		unsigned long        ovl_plane_id,
        igd_appcontext_h     appcontext_h,
        emgd_framebuffer_t  *sub_surface,
        igd_rect_t          *sub_src_rect,
        igd_rect_t          *sub_dest_rect,
        igd_ovl_info_t      *ovl_info,
        unsigned int         flags);

	/*!
	 * Set a new z-order for the overlay planes on this display pipeline
	 *
	 * @param display_h Input Display used with this overlay
	 * @param z_order Input Structure with valid bits identifying the new
	 *        z-order for this display pipeline. Please read the comments
	 *        surrounding this structure in igd_ovl.h for details
	 *
	 * @returns 0 (IGD_SUCCESS) on Success - will return success
	 *    even if overlay is currently in use or not in use.
	 * @returns <0 (-igd_errno) on Error
	 */
	int (*alter_ovl_attr)(igd_display_h display_h,
			unsigned long ovl_plane_id,
			igd_ovl_pipeblend_t * pipeblend,
			int update_pipeblend, int query_pipeblend,
			igd_ovl_color_correct_info_t * colorcorrect,
			int update_colorcorrect,
			igd_ovl_gamma_info_t * gamma,
			int update_gamma);

	/* igd_ovl.h */
	/*!
	 *  Retrieve the kernel mode initialization parameters for overlay.
	 *  This function should be called by user-mode drivers as they are
	 *  initialized, to retrieve overlay initialization parameters
	 *  discovered and held by the kernel driver.
	 *
	 * @returns 0 (IGD_SUCCESS) on Success
	 * @returns <0 (-igd_errno) on Error.
	 */
	int (*get_ovl_init_params)(igd_driver_h driver_handle,
			igd_ovl_caps_t * ovl_caps);

	/*!
	 * Query the overlay to determine if an event is complete or if a given
	 * capability is present.
	 *
	 * @param display_h Display used with this overlay
	 * @param flags Used to check for which event is complete or which
	 *     capability is present.
	 *     See @ref query_ovl_flags
	 *
	 * @returns TRUE if event has occured or capability is available
	 * @returns FALSE if event is pending or capability is not available
	 */
	int (*query_ovl)(igd_display_h display_h,
		unsigned long ovl_plane_id,
		unsigned int flags);

	/*!
	 * Query the overlay for the maximum width and height given the input
	 * src video pixel format.
	 *
	 * @param display_h Display used with this overlay
	 * @param pf Input src video pixel format
	 * @param max_width Output maximum overlay width supported (in pixels)
	 * @param max_height Output maximum overlay height supported (in pixels)
	 *
	 * @returns 0 (IGD_SUCCESS) on Success - will return success
	 *    even if overlay is currently in use.
	 * @returns <0 (-igd_errno) on Error
	 */
	int (*query_max_size_ovl)(igd_display_h display_h,
		unsigned long ovl_plane_id,
		unsigned long pf,
		unsigned int *max_width,
		unsigned int *max_height);

	/* igd_pwr.h */
	int (*pwr_query)(igd_driver_h driver_handle, unsigned int power_state);

	/*!
	 * This function is to get the EDID block of the specified size. The size
	 * is returned from previous call to igd_get_EDID_info().
	 *
	 * @note The return value indicates success/failure, blocksize should be
	 *  set by the API to the actual number of bytes transferred upon return.
	 * @param driver_handle The driver handle returned from igd_driver_init().
	 * @param port_number Specific display to query for EDID block data.
	 * @param edid_ptr Buffer to hold the EDID block data, must be 128bytes.
	 * @param block_number Tells the API which EDID block to read.
	 *
	 *  @returns 0 on Success
	 *  @returns -IGD_ERROR_EDID Error while reading EDID or no EDID
	 *  @returns -IGD_INVAL Other error
	 */
	int (*get_EDID_block)(igd_driver_h driver_handle,
		unsigned short port_number,
		unsigned char *edid_ptr,
		unsigned char block_number);

	/*
	 * This function calculates the register values for the appropriate sprite
	 * plane (engine) and stores them in regs.
	 *
	 * @param display_h Input display used with this overlay
	 * @param engine The id of the sprite plane
	 * @param src_surf Input src surface information
	 * @param src_rect Input src surface rectangle.
	 *     This is useful for clone, extended, and vertical extended modes
	 *     where the entire src_surf may not be displayed.
	 * @param dest_rect Input dest surface rectangle.  This is relative to the
	 *     framebuffer not the display (so clone mode works properly).
	 * @param ovl_info Input overlay information to display.
	 *     The color key, video quality, and gamma must be valid
	 *     and correct when the overlay is on.  That means NO passing in NULL
	 *     values to use previous settings.
	 * @param flags Input to turn on/off the overlay and set other flags.
	 *   See: @ref alter_ovl_flags
	 * @param regs The calculated register values would be store in this
	 */
	int (*calc_spr_regs)(igd_display_h display_h, unsigned long engine, emgd_framebuffer_t *src_surf, 
		igd_rect_t *src_rect, igd_rect_t *dest_rect, igd_ovl_info_t *ovl_info, 
		unsigned int flags, void *regs);

	/*
	 * This function writes to the hardware (sprite registers) the values that
	 * are plassed in regs.
	 *
	 * @param display_h Input display used with this overlay
	 * @param engine The id of the sprite plane
	 * @param ovl_info Input overlay information to display.
	 *     The color key, video quality, and gamma must be valid
	 *     and correct when the overlay is on.  That means NO passing in NULL
	 *     values to use previous settings.
	 * @param regs The register values required to write to the hardware are 
	 * passed in this
	 */
	int (*commit_spr_regs)(igd_display_h display_h, unsigned long engine, igd_ovl_info_t *ovl_info, 
		void *regs);
} igd_dispatch_t;

/*!
 * @brief Function to initialize the HAL and detect supported chipset.
 *
 * Initialize the driver and determine if the hardware is supported without
 * altering the hardware state in any way. Init info is populated based on
 * collected data.
 *
 * A driver handle is returned. This handle is an opaque data structure
 * used with future HAL calls. The contents or meaning of the value are
 * not known outside the HAL; however, a NULL value should be considered
 * a failure.
 *
 * @param init_info Device details returned from the init process. This
 *   structure will be populated by the HAL during the call.
 *
 * @return igd_driver_h (Non-NULL) on Success
 * @return NULL on failure
 */
igd_driver_h igd_driver_init(igd_init_info_t *init_info);


/*!
 *  Configure the driver. This sets the driver for future operation, it
 * may alter the hardware state. Must be called after driver init and
 * before ANY other driver commands.
 *
 * @param driver_handle: A driver handle as returned by igd_driver_init()
 *
 * @return <0 on Error
 * @return 0 On Success
 */
int igd_driver_config(igd_driver_h driver_handle);

/*!
 *
 *  Initializes individual modules to a runable state. Init time parameters
 * may be provided to alter the default behavior of the driver.
 * See @ref init_param
 *
 * The dispatch table for all graphics operations is returned. The dispatch
 * table may return NULL pointers for unsupported functions due to
 * optional modules. This dispatch table is used to access HAL functionality
 * throughout the life of the driver.
 * See @ref _igd_dispatch
 *
 * @param driver_handle as returned from igd_driver_init().
 * @param dsp dispatch table to be populated during the call.
 * @param params Input parameters to alter default behavior.
 *   See @ref init_param
 *
 * @return 0 Success
 * @return <0 on Error
 */
int igd_module_init(igd_driver_h driver_handle,
	igd_dispatch_t **dsp,
	igd_param_t *params);


#endif /*_IGD_DISPATCH_DRV_H*/
