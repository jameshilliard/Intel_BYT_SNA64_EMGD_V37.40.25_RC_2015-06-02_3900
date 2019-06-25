/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: igd_display_pipeline.h
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
 *
 *-----------------------------------------------------------------------------
 */

#ifndef _IGD_DISPLAY_PIPELINE_H_
#define _IGD_DISPLAY_PIPELINE_H_

#ifndef FWTOOLS
/* KMS-related Headers */
#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include "i915/intel_ringbuffer.h"
#include <pd.h>
#include <edid.h>
#include <displayid.h>
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*Constant Macro*/
#define RATE_270_MHZ 270000
#define RATE_162_MHZ 162000
#define DP_LINKBW_1_62_GBPS 0x6
#define DP_LINKBW_2_7_GBPS 0xA

/*!
 * @name Cursor Info Flags
 * @anchor cursor_info_flags
 *
 * Flags for use in the Cursor Info Structure
 *
 * @{
 */
#define IGD_CURSOR_ON              0x00000001
#define IGD_CURSOR_OFF             0x00000000
#define IGD_CURSOR_LOAD_ARGB_IMAGE 0x00000010
#define IGD_CURSOR_LOAD_XOR_IMAGE  0x00000020

#define IGD_CURSOR_GAMMA           0x00000002
#define IGD_CURSOR_LOCAL           0x00000004  /* Local Memory */
#define IGD_CLONE_CURSOR           0x00000001
/*! @} */

/*!
 * @brief Cursor Plane information
 *
 * This data structure is used as input and output to the
 * _igd_dispatch::alter_cursor() dispatch function.
 */
typedef struct _igd_cursor_info {
	unsigned long width;
	unsigned long height;
	unsigned long pixel_format;
	/*
	 * Use the XOR offset for all 2 bit formats, and the ARGB offset for
	 * 32bit formats. If either offset is 0 it should be assumed that the
	 * HAL cannot support it.
	 */
	unsigned long xor_offset;
	unsigned long xor_pitch;
	unsigned long argb_offset;
	unsigned long argb_pitch;
	long x_offset;
	long y_offset;
	long hot_x;
	long hot_y;
	unsigned long palette[4];
	unsigned long flags;
	unsigned long rotation;
	unsigned long render_scale_x;
	unsigned long render_scale_y;
	unsigned short off_screen;
	void *cursor_bo;
} igd_cursor_info_t;

/*!
 * @name Get Scanline Defines
 * @anchor get_scanline_defs
 *
 * These defines are returned from the _igd_dispatch::get_scanline()
 * dispatch function in the place of scanline number when no number exists.
 *
 * @{
 */
#define IGD_IN_VBLANK -1
/*! @} */

/*!
 * @name Acess i2c Flags
 * @anchor access_i2c_flags
 *
 * These flags are used to control the flow of data in the
 * igd_dispatch::access_i2c() dispatch function.
 *
 * @{
 */
#define IGD_I2C_READ   0x00000001
#define IGD_I2C_WRITE  0x00000002
/*! @} */

/*!
 * @brief I2C data packet for use with _igd_dispatch::access_i2c()
 */
typedef struct _igd_i2c_reg {
	/*! @brief Serial bus id [0..6] for alm core, and 0 for whitney core */
	unsigned char bus_id;
	/*! @brief Data address byte of the device */
	unsigned char dab;
	/*! @brief Device register */
	unsigned char reg;
	/*! @brief Buffer for register values */
	unsigned char *buffer;
	/*! @brief number of bytes to read or write */
	unsigned char num_bytes;
	/*! @brief i2c bus speed in KHz */
	unsigned long i2c_speed;
} igd_i2c_reg_t;

/*!
 * @{
 */
#define IGD_CLEAR_FB                 0x02000000
/*! @} */

/*!
 * @name Alloc Display Flags (deprecated)
 *
 * Flags for use with the igd_alloc_display function call.
 * - IGD_NEW_ALL Default, request for new plane, pipe, port
 * - IGD_NEW_PIPE Request for an additional pipe & port, using the same plane
 *    from previous allocation
 * - IGD_NEW_PORT Request for an addition port, using the same pipe and plane
 *    from previous allocation
 *
 * @{
 */
#define IGD_NEW_MASK           0x00700000
#define IGD_NEW_ALL            0x00400000
#define IGD_NEW_PIPE           0x00200000
#define IGD_NEW_PORT           0x00100000
/*! @} */

#define IGD_INVALID_MODE 0

/*****************/
/* Plane Features */
/*****************/
#define IGD_PLANE_FEATURES_MASK      0x000000FF
#define IGD_PLANE_DISPLAY            0x00000001
#define IGD_PLANE_OVERLAY            0x00000002
#define IGD_PLANE_SPRITE             0x00000004
#define IGD_PLANE_CURSOR             0x00000008
#define IGD_PLANE_VGA                0x00000010
#define IGD_PLANE_DOUBLE             0x00000020
/*#define IGD_PLANE_CLONE            0x00000040 currently unused */
#define IGD_PLANE_DIH                0x00000080
#define IGD_PLANE_USE_PIPEA          0x00000100
#define IGD_PLANE_USE_PIPEB          0x00000200
#define IGD_PLANE_IS_PLANEA          0x00000400
#define IGD_PLANE_IS_PLANEB          0x00000800

/*****************/
/* Pipe Features */
/*****************/
/* pipe's supported features */
#define IGD_PIPE_FEATURES_MASK       0x000000FF
#define IGD_PIPE_DOUBLE              0x00000001
/* the following 2 bits are not pipe features but
 * pipe identification bits that share the same
 * pipe_features variable of the pipe structure
 * NOTE that these bit-wise OR flags in nibble-2
 * are also used in the port_features variable
 * of the port structure (same locations) but
 * called IGD_PORT_USE_PIPEX
 */
#define IGD_PIPE_IS_PIPEA            0x00000100
#define IGD_PIPE_IS_PIPEB            0x00000200

/*******************/
/* Cursor Features */
/*******************/
/*
 * Cursor's supported features
 */
/*
 * cursor's pipe usage regulations. These are copies of the PIPE_IS_ bits so
 * that a quick (pipe_features & plane_features & MASK) will tell you if
 * the pipe can be used.
 */
#define IGD_CURSOR_USE_PIPE_MASK       0x00000F00
#define IGD_CURSOR_USE_PIPEA           IGD_PIPE_IS_PIPEA
#define IGD_CURSOR_USE_PIPEB           IGD_PIPE_IS_PIPEB


/*****************/
/* Port types    */
/*****************/
/*
 * Port type definitions to be used in display_info flags.
 * Also used in port_type field of internal port data structure.
 */
#define IGD_PORT_MASK             0x000FF000
#define IGD_PORT_ANY              0x000FF000
#define IGD_PORT_ANALOG           0x00001000
#define IGD_PORT_DIGITAL          0x00002000 /* sDVO, HDMI-DVI, DP, eDP*/
#define IGD_PORT_LVDS             0x00004000
#define IGD_PORT_TV               0x00010000 /* Integrated TV (Alviso)  */
#define IGD_PORT_MIPI_PORT        0x00080000 /* MIPI Display */

/* Atom E6xx requires LPC device, define a port type to differentiate
 * with other devices such as 0:2:0, 0:3:0 */
#define IGD_PORT_LPC              0x80000000   /* LPC device 0:31:0 */

/*
 * Standard port definitions
 */
#define IGD_PORT_TYPE_TV          0x1
#define IGD_PORT_TYPE_SDVOB       0x2
#define IGD_PORT_TYPE_SDVOC       0x3
#define IGD_PORT_TYPE_LVDS        0x4
#define IGD_PORT_TYPE_ANALOG      0x5
#define IGD_PORT_TYPE_DPB         0x6
#define IGD_PORT_TYPE_DPC         0x7



/*****************/
/* Port features */
/*****************/
/*
 * Port's supported features
 *
 * port features also uses IGD_PORT_SHARE_MASK thus port feature bits
 * cannot collide with IGD_PORT_SHARE_MASK
 */
#define IGD_PORT_FEATURES_MASK       0x000000FF
#define IGD_RGBA_COLOR               0x00000001
#define IGD_RGBA_ALPHA               0x00000002
#define IGD_VGA_COMPRESS             0x00000004 /* Compress VGA to 640x480 */
#define IGD_PORT_GANG                0x00000008
#define IGD_IS_PCH_PORT              0x00000010
/*
 * port's pipe usage regulations. These are copies of the PIPE_IS_ bits so
 * that a quick (pipe_features & port_features & MASK) will tell you if
 * the pipe can be used.
 */
#define IGD_PORT_USE_PIPE_MASK       0x00000F00
#define IGD_PORT_USE_PIPE_MASK_SHIFT 8
#define IGD_PORT_USE_PIPEA           IGD_PIPE_IS_PIPEA
#define IGD_PORT_USE_PIPEB           IGD_PIPE_IS_PIPEB
/*
 * Ports Sharing information. The port in question can share a pipe with the
 * listed ports. If a shares with b, b must share with a too.
 * Must be the same as IGD_PORT_MASK = 0x3f000
 */
#define IGD_PORT_SHARE_MASK          IGD_PORT_MASK
#define IGD_PORT_SHARE_ANALOG        IGD_PORT_ANALOG
#define IGD_PORT_SHARE_DIGITAL       IGD_PORT_DIGITAL
#define IGD_PORT_SHARE_LVDS          IGD_PORT_LVDS
#define IGD_PORT_SHARE_TV            IGD_PORT_TV

/* MAX rings, planes and ports connected to a pipe */
#define IGD_MAX_PIPE_QUEUES      4
#define IGD_MAX_PIPE_PLANES      5
#define IGD_MAX_PIPE_DISPLAYS    4
#define IGD_MAX_PIPES            2
#define MAX_DISPLAYS             IGD_MAX_DISPLAYS /* From igd_mode.h */

/* Parameters to mode_update_plane_pipe_ports */
#define MODE_UPDATE_PLANE  0x1
#define MODE_UPDATE_PIPE   0x2
#define MODE_UPDATE_PORT   0x4
#define MODE_UPDATE_NONE   0x0

#define MODE_IS_SUPPORTED(t) (t->mode_info_flags & IGD_MODE_SUPPORTED)
#define MODE_IS_VGA(t) \
	((((pd_timing_t *)t)->flags & IGD_MODE_VESA) &&	\
		(t->mode_number < 0x1D))

#define IGD_KMS_PIPEA (IGD_PIPE_IS_PIPEA >> IGD_PORT_USE_PIPE_MASK_SHIFT)
#define IGD_KMS_PIPEB (IGD_PIPE_IS_PIPEB >> IGD_PORT_USE_PIPE_MASK_SHIFT)

#define KMS_PIPE_ID(pipe_features) \
		((pipe_features & IGD_PORT_USE_PIPE_MASK) >> IGD_PORT_USE_PIPE_MASK_SHIFT)

/* #define DC_PORT_NUMBER(dc, i) ((dc >> (i * 4)) & 0x0f) */
#define DC_PORT_NUMBER IGD_DC_PORT_NUMBER

#ifndef FWTOOLS
typedef struct _igd_port_info {
	char pd_name[64];
	unsigned long driver_version;	/* Formatted as pd_version_t */
	uint32_t port_num;				/* Port Number */
	unsigned long display_type;		/* Type of display */
	int connected;					/* 1 - Display connected, 0 - Otherwise */
	char port_name[8];             /* Default port name (DVOA, sDVO-A, etc.) */
	unsigned long flags;           /* Attribute flags, currently used for interlace */
} igd_port_info_t;
#endif

/*
 * emgd_framebuffer igd_flags
 * ALAN: I think this might also be redundant very soon
 */
#define IGD_SURFACE_TYPE_MASK     0x000000FF
#define IGD_SURFACE_DISPLAY       0x00000001
#define IGD_SURFACE_OVERLAY       0x00000002
#define IGD_SURFACE_CURSOR        0x00000004

#define IGD_SURFACE_GEMFLAGS_MASK 0x0000FF00
#define IGD_SURFACE_TILED         0x00000100

/* Sysfs node object.  Defined in emgd_sysfs.c */
struct emgd_obj_t;
#ifndef FWTOOLS
/**
 * emgd_framebuffer_t
 *
 * Data structure that contains details relavent to the framebuffer
 * configuration. Used with igd_alter_display() to alter the framebuffer
 * size and format.
 *
 * The framebuffer may be larger or smaller than the display timings. When
 * larger the output will be cropped and can be used in a panning
 * configuration. When smaller the output will be centered.
 *
 */
typedef struct _emgd_framebuffer {
	struct drm_framebuffer base;
	/* drm_framebuffer already has the 4 basic basic infos:
	 *      - width
	 *      - height
	 *      - pitch
	 *      - bpp/depth combo (translates into drm and igd pixel formats)
	 */
	struct drm_i915_gem_object *bo;
	/* this pointer to gem_bo is used as an indicator of
	 * whether this framebuffer has been allocated or not
	 * NULL means its not yet allocated. HAL needs this.
	 */

	 /* User can use addfb2 to add up to 4 framebuffers.
	  * This was not previously handled by emgd, other_bo[]
	  * will store the gem_bo of the remaining framebuffers
	  */
	struct drm_i915_gem_object *other_bo[3];

	unsigned long igd_pixel_format;
	unsigned long igd_flags;
	unsigned long u_offset; /* Needed for YUV Planar modes. */
	unsigned int u_pitch;   /* Needed for YUV Planar modes. */
	unsigned long v_offset; /* Needed for YUV Planar modes. */
	unsigned int v_pitch;   /* Needed for YUV Planar modes. */

	struct emgd_obj *sysfs_obj;
	struct drm_i915_gem_object *obj;
} emgd_framebuffer_t;

/*
 * NOTE: The plane typedef is a generic type. Each plane type has an
 * equivalent typedef that is more specific to the type of plane. They
 * MUST remain equivalent. If you change one you must change them all.
 */
#if 0
typedef struct _igd_plane {
	unsigned long plane_reg;       /* plane control register */
	unsigned long plane_features;  /* plane feature list */
	int           inuse;           /* plane inuse ? */
	int           ref_cnt;         /* # of displays using this plane */
	unsigned long *pixel_formats;  /* supported pixel formats */
	void *plane_info;              /* ptr to plane_info */
	struct _igd_plane *mirror;     /* pointer to mirror plane */
} igd_plane_t;
#endif

/******************************************************************************
 * WARNING!!!! igd_display_plane & igd_cursor_t structures should be identical:
 *      The size of the structures and the location onf the variables !
 ******************************************************************************
 */
typedef struct _igd_display_plane {
	unsigned long plane_reg;         /* plane contron register */
	unsigned long plane_features;    /* list of plane features */
	int           inuse;             /* plane inuse ? */
	int           ref_cnt;           /* # of displays using this plane */
	unsigned long *pixel_formats;    /* list of pixel formats supported */
	emgd_framebuffer_t *fb_info; /* attached fb to this plane */
	struct _igd_display_plane *mirror;  /* pointer to mirror plane */
	unsigned long xoffset;
	unsigned long yoffset;
} igd_display_plane_t, *pigd_display_plane_t;
typedef struct _igd_cursor {
	unsigned long cursor_reg;        /* cursor control register */
	unsigned long plane_features;    /* cursor plane features */
	int           inuse;             /* is this cursor in use? */
	int           ref_cnt;           /* # of displays using this plane */
	unsigned long *pixel_formats;    /* list of pixel_formats supported */
	igd_cursor_info_t *cursor_info;
	struct _igd_cursor *mirror;  /* pointer to mirror plane */
} igd_cursor_t;
/*****************************************************************************/
#endif
/* Flags for to HAL-device-dependant program_clock "operation" parameter */
#define PROGRAM_PRE_PIPE    0x1
#define PROGRAM_POST_PIPE   0x2
#define PROGRAM_PRE_PORT    0x3
#define PROGRAM_POST_PORT   0x4

#ifndef FWTOOLS
typedef struct _igd_clock {
	unsigned long dpll_control;       /* DPLL control register */
	unsigned long mnp;                /* FPx0 register */
	unsigned long p_shift;            /* Bit location of P within control */
	unsigned long actual_dclk;		  /* The actual dotclock after calculating dpll */
}igd_clock_t;

typedef struct _igd_display_pipe {
	unsigned long pipe_num;             /* 0 Based index */
	unsigned long pipe_reg;             /* pipe configuration register */
	unsigned long timing_reg;           /* timing register(htotal) */
	unsigned long palette_reg;          /* palette register */
	igd_clock_t   *clock_reg;           /* DPLL clock registers */
	unsigned long pipe_features;        /* pipe features */
	int           inuse;                /* pipe allocated? TRUE/FALSE */
	int           ref_cnt;              /* # of displays using this pipe */
	void          *sprite1;     /* sprite connected to this pipe */
	void          *sprite2;     /* sprite connected to this pipe */
	igd_timing_info_t     *timing;     /* current timings on the port */
	unsigned long dclk;                /* current dclk running on this pipe */
}igd_display_pipe_t, *pigd_display_pipe_t;

typedef struct _igd_display_port {
	unsigned long port_type;            /* port type */
	uint32_t      port_number;          /* port number */
	char          port_name[8];         /* port name DVO A, B, C, LVDS, ANALOG */
	unsigned long port_reg;             /* port control register */
	unsigned long i2c_reg;              /* GPIO pins for i2c on this port */
	unsigned long i2c_dab;              /* i2c Device Address Byte */
	unsigned long ddc_reg;              /* GPIO pins for DDC on this port */
	unsigned long ddc_dab;
	unsigned long port_features;        /* port features */
	unsigned long clock_bits;           /* Clock input to use */
	int           inuse;                /* port is in use */
	unsigned long  power_state;         /* D Power state for the display/port */
	unsigned long bl_power_state;       /* D Power state for the FP backlight */
	struct _igd_display_port *mult_port;/* pointer to multiplexed port,
										 * if it is used in that way */
	igd_display_info_t    *pt_info;     /* port timing info */
	pd_driver_t           *pd_driver;
	void                  *pd_context;  /* Context returned from PD */
	pd_callback_t         *callback;    /* DD Callback to passed to PD */
	unsigned long         num_timing;   /* number of timings available */
	igd_timing_info_t     *timing_table; /* static/dynamic PD timings list */
	unsigned long         i2c_speed;    /* Connected encoder's I2C bus speed */
	unsigned long         ddc_speed;    /* DDC speed in KHz */
	igd_param_fp_info_t   *fp_info;     /* Flat panel parameter info if any */
	igd_param_dtd_list_t  *dtd_list;    /* EDID-less DTD info if any */
	igd_param_attr_list_t *attr_list;   /* Saved attributes if any */
	igd_attr_t            *tmp_attr;    /* Temp attr array, for copying */
	unsigned int          tmp_attr_num; /* Number of attr in temp array */
	igd_timing_info_t     *fp_native_dtd; /* FP native DTD */
	unsigned long         pd_type;      /* Display type given by port driver */
	unsigned long         pd_flags;     /* port driver flags */
	unsigned long         saved_bl_power_state;

	/* This attribute list is designed to eventually suck in things above
	 * such as fb_info.  For now, it only has color correction attributes */
	igd_attr_t            *attributes;

	unsigned char         firmware_type;
	union {
		displayid_t       *displayid;
		edid_t            *edid;         /* EDID information */
	};

	/* Used for S3D */
	/* - s3d_supported - used to indicate if a 3D monitor is attached to the port
	 * - s3d_format    - for future use to store specific 3D format supported by
	 *                   a 3D monitor. Currently is not used and set to 0 */
	unsigned char         s3d_supported;
	unsigned short        s3d_format;
	unsigned long		  bits_per_color;

}igd_display_port_t, *pigd_display_port_t;


/* This structure is used to save mode state.
 * Rightnow, it is saving state of current port and its port driver state.
 * This information is used while restoring to a previously saved mode
 * state.
 * TODO: This can be extended to save all display modules (mode, dsp, pi) reg
 * information along with port driver's state information. This requires
 * changes to exiting reg module. */
#define MAX_PORT_DRIVERS     20
typedef struct _mode_pd_state {
		igd_display_port_t *port;       /* display port */
		void               *state;      /* and its port driver state */
} mode_pd_state_t;

typedef struct _mode_state_t {
	mode_pd_state_t pd_state[MAX_PORT_DRIVERS];
} mode_state_t;


/**
 * emgd_page_flip_work_t
 *
 * Contains all the information required complete a page flip.
 */
typedef struct _emgd_page_flip_work {
	struct work_struct          work;
	struct drm_device          *dev;
	struct drm_i915_gem_object *old_fb_obj;
	struct drm_i915_gem_object *new_fb_obj;
	struct drm_i915_gem_object *old_stereo_obj[2];
	struct drm_i915_gem_object *new_stereo_obj[2];
	void	*emgd_crtc;

	/* Userspace event to send back upon flip completion. */
	struct drm_pending_vblank_event *event;
	atomic_t pending;
#define INTEL_FLIP_INACTIVE    0
#define INTEL_FLIP_PENDING     1
#define INTEL_FLIP_COMPLETE    2
} emgd_page_flip_work_t;
#endif

enum pipe {
	PIPE_A = 0,
	PIPE_B,
	PIPE_C,
	I915_MAX_PIPES
};
enum plane {
	PLANE_A = 0,
	PLANE_B,
	PLANE_C
};

enum hpd_pin {
	HPD_CRT,
	HPD_SDVO_B,
	HPD_SDVO_C,
	HPD_DP_B,
	HPD_DP_C,
	HPD_NUM_PINS
};

#ifndef FWTOOLS
/**
 * This holds information about an individual encoder
 */
typedef struct _emgd_encoder {
	struct drm_encoder base;

	/* Same as base.crtc except when in the middle of Atomoc mode set */
	void *new_crtc;
	unsigned long       clone_mask;
	igd_display_port_t *igd_port;
	mode_pd_state_t     state;
	/* flags is a bit mask. For information
	 * on the different masks, see ENCODER_FLAG_xx
	 */
	unsigned long       flags;
	/* Track the active connectors tied to this encoder */
	bool connectors_active;
	enum hpd_pin hpd_pin;
} emgd_encoder_t;

/**
 *  * This holds information about an individual output
 *   */
typedef struct _emgd_connector {
	struct drm_connector  base;

	emgd_encoder_t       *encoder;
	/* Same as encoder except when in the middle of Atomoc mode set */
	emgd_encoder_t       *new_encoder;
	unsigned long         type;
	struct list_head      property_map;
	unsigned long         num_of_properties;

	struct drm_i915_private *priv;
	struct emgd_obj *sysfs_obj;
} emgd_connector_t;

/**
 *  * This holds information on our framebuffer device.
 *   */
typedef struct _emgd_fbdev {
	struct drm_fb_helper      helper;

	emgd_framebuffer_t       *emgd_fb;
	u32                       pseudo_palette[17];
	struct list_head          fbdev_list;
	struct drm_i915_private *priv;
} emgd_fbdev_t;

#ifndef CONFIG_MICRO
/*typedef struct _igd_display_port igd_display_port_t;*/
void convert_displayid_to_edid(igd_display_port_t *port,
		unsigned char *edid_buf);
void convert_color_char(unsigned char *edid_ptr, displayid_t *did);
#endif
#endif
/*eDP panel brightness control 1-100%*/
#define MIN_BRIGHTNESS 1
#define MAX_BRIGHTNESS 100

#endif /* _IGD_DISPLAY_PIPELINE_H_ */
