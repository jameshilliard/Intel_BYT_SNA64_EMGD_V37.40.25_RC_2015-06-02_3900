#----------------------------------------------------------------------------
# Filename: Makefile.gnu
#----------------------------------------------------------------------------
# Copyright (c) 2002-2014, Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#----------------------------------------------------------------------------
export EGD_TOPLEVEL = DRM Driver

KERNELRELEASE ?= emgd_standalone

ifeq "$(KERNELRELEASE)" "emgd_standalone"
	KERNELVER ?= $(shell uname -r)
	KERNELDIR ?= /lib/modules/$(KERNELVER)/build
	INSTALLDIR ?= /lib/modules/$(KERNELVER)/kernel/drivers/gpu/drm/emgd
else
	KERNELDIR ?= ./
endif

BLUE = \033[34m
OFF = \033[0m
BUILD ?= release
CONFIG_DRM_EMGD ?= m

# Get the include paths pointed to the right place.
ifeq "$(KERNELRELEASE)" "emgd_standalone"
	export  EMGD_MOD_DIR ?= $(CURDIR)
else
	export  EMGD_MOD_DIR ?= drivers/gpu/drm/emgd
endif

# To support out of sync DRM tree for LTSI kernel.
LTSI = $(shell awk '/^EXTRAVERSION/ {print toupper($$3)}' $(KERNELDIR)/Makefile)
ifneq (,$(findstring -LTSI, $(LTSI)))
    ccflags-y += -DLTSI_KERNEL
endif

# Determine the version and build numbers
# FIXME: The BUILDNUM not able include year, month, date and hours in EMGD DDX.
# 	 EMGD_PATCHLEVEL will get assign the value from BUILDNUM. Due to
# 	 struct XF86ModuleVersionInfo contains patchlevel is data type CARD16
# 	 which is an unsigned integer value that is maximum of 16 bits long.
#	 We only allow to limit the BUILDNUM at here contains year, work week,
#	 and day. The BUILDNUM at here is use for the local build differential.
#	 For the official release, build team will update the BUILDNUM to
#	 official build number.
#	 Example:
#	 	BUILDNUM ?= `date +%y%V%w` will be replace to
#	 		BUILDNUM ?= 14263
#
BUILDNUM ?= 3900
MAJORNUM ?= 2
MINORNUM ?= 0

VERSION = -DIGD_MAJOR_NUM=$(MAJORNUM) -DIGD_MINOR_NUM=$(MINORNUM) -DIGD_BUILD_NUM=$(BUILDNUM)

EXTRA_CFLAGS += $(VERSION)


# Properly set the driver build date that the DRM driver will report.
BUILDDATE ?= $(shell date +%Y%m%d)

PROJECT_INCLUDES = \
	   -I$(EMGD_MOD_DIR)/include \
	   -I$(EMGD_MOD_DIR)/src/display/mode/cmn \
	   -I$(EMGD_MOD_DIR)/src/video/overlay/cmn \
	   -I$(EMGD_MOD_DIR)/src/include \
	   -I$(EMGD_MOD_DIR)/src/cfg \
	   -I$(EMGD_MOD_DIR)/src/pal/lpd \
	   -I$(EMGD_MOD_DIR)/src/core

ifeq "$(KERNELRELEASE)" "emgd_standalone"
	PROJECT_INCLUDES += -I$(KERNELDIR)/include/drm \
						-I$(KERNELDIR)/include/uapi/drm
else
	PROJECT_INCLUDES += -Iinclude/drm \
						-Iinclude/uapi/drm
endif

ifeq "$(strip $(BUILD))" "release"
	ccflags-y += -DRELEASE
	# Uncomment following 2 for full debug verboseness in release build
	#ccflags-y += -DDEBUG_BUILD_TYPE
	#ccflags-y += -DFULL_DEBUG_VERBOSITY
else
	# FIXME: Looks like this causes conflicts in the emgd code.
	ccflags-y += -DDEBUG
	ccflags-y += -DDEBUG_BUILD_TYPE
endif

KVER = $(shell uname -r)
KMAJ = $(shell echo $(KVER) | \
        sed -e 's/^\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*.*/\1/')
KMIN = $(shell echo $(KVER) | \
        sed -e 's/^[0-9][0-9]*\.\([0-9][0-9]*\)\.[0-9][0-9]*.*/\1/')
KREV = $(shell echo $(KVER) | \
        sed -e 's/^[0-9][0-9]*\.[0-9][0-9]*\.\([0-9][0-9]*\).*/\1/')

kver_ge =  $(shell \
        echo test | awk '{if($(KMAJ) < $(1)) {print 0} else { \
        if($(KMIN) < $(2)) {print 0} else { \
        if($(KREV) < $(3)) {print 0} else { print 1 } \
        }}}' \
        )

ifeq ($(call kver_ge,3,10,61),1)
	MSVDX_SUPPORT ?= yes
else
	MSVDX_SUPPORT ?= no
endif

export KERNELDIR
ifneq "$(origin KERNELDIR)" "environment"
EMGD_PATH = $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
FLIP_FILE_PATH = $(EMGD_PATH)/../../../../include/drm/drm_flip.h
ifneq ($(wildcard $(realpath $(FLIP_FILE_PATH))),)
ccflags-y += -DKERNEL_HAS_ATOMIC_PAGE_FLIP
endif
else
ifneq ($(wildcard $(KERNELDIR)/include/drm/drm_flip.h),)
ccflags-y += -DKERNEL_HAS_ATOMIC_PAGE_FLIP
endif
endif

ifeq "$(KCOVERAGE_BUILD)" "1"
	# Enable kernel profiling for entire files egd_drm directory
	# using for kernel code coverage.
	GCOV_PROFILE := y

        # To exclude single file for kernel profiling.
        # e.g a single file init_snb.o :
        #       GCOV_PROFILE_init_snb.o :=n
	GCOV_PROFILE_init_snb.o :=n
	GCOV_PROFILE_micro_init_snb.o :=n
	GCOV_PROFILE_interrupt_snb.o :=n
	GCOV_PROFILE_clocks_snb.o :=n
	GCOV_PROFILE_kms_mode_snb.o :=n
	GCOV_PROFILE_i2c_snb.o :=n
	GCOV_PROFILE_dsp_snb.o :=n
	GCOV_PROFILE_lvds.o :=n
	GCOV_PROFILE_reg_snb.o :=n
	GCOV_PROFILE_pwr_snb.o :=n
	GCOV_PROFILE_spr_snb.o :=n
	GCOV_PROFILE_spr_cache_snb.o :=n
	GCOV_PROFILE_micro_spr_snb.o :=n
   	GCOV_PROFILE_ovl_coeff.o :=n
endif

ifdef WAYLAND_BROKEN_ON_OVERLAY
	ccflags-y += -DWAYLAND_BROKEN_ON_OVERLAY
endif

ifeq "$(shell echo $(MSVDX_SUPPORT) | tr '[A-Z]' '[a-z]')" "yes"
	ccflags-y += -DMSVDX_SUPPORT
endif

EXTRA_CFLAGS += $(PROJECT_INCLUDES) \
		-DDRIVER_DATE="\"$(BUILDDATE)\""

EMGD_OBJS := \
	src/core/emgd_fb.o \
	src/core/emgd_fbcon.o \
	src/core/emgd_crtc.o \
	src/core/emgd_encoder.o \
	src/core/emgd_connector.o \
	src/core/emgd_sysfs.o \
	src/core/emgd_drv.o \
	src/core/emgd_ovl.o \
	src/core/emgd_panel.o \
	src/core/i915/i915_debugfs.o \
	src/core/i915/i915_gpu_error.o \
	src/core/i915/i915_gem.o \
	src/core/i915/i915_gem_context.o \
	src/core/i915/i915_gem_debug.o \
	src/core/i915/i915_gem_dmabuf.o \
	src/core/i915/i915_gem_evict.o \
	src/core/i915/i915_gem_execbuffer.o \
	src/core/i915/i915_gem_gtt.o \
	src/core/i915/i915_gem_stolen.o \
	src/core/i915/i915_gem_tiling.o \
	src/core/i915/intel_ringbuffer.o \
	src/core/i915/intel_uncore.o \
	src/core/i915/intel_pm.o \
	src/core/i915/i915_trace_points.o \
	src/core/i915/i915_emgd_helper.o \
	src/core/i915/intel_atomic.o \
	src/core/default_config.o \
	src/core/splash_screen.o \
	src/core/splash_video.o \
	src/core/init/cmn/igd_init.o \
	src/core/init/gn6/init_snb.o \
	src/core/init/gn6/micro_init_snb.o \
	src/core/init/gn7/init_vlv.o \
	src/core/init/gn7/micro_init_vlv.o \
	src/core/interrupt/cmn/igd_interrupt.o \
	src/core/interrupt/gn6/interrupt_snb.o \
	src/core/interrupt/gn7/interrupt_vlv.o \
	src/display/mode/cmn/kms_mode.o \
	src/display/mode/cmn/match.o \
	src/display/mode/cmn/vga_mode.o \
	src/display/mode/gn6/clocks_snb.o \
	src/display/mode/gn6/kms_mode_snb.o \
	src/display/mode/gn7/clocks_vlv.o \
	src/display/mode/gn7/kms_mode_vlv.o \
	src/display/pd/cmn/pd.o \
	src/display/pi/cmn/igd_pi.o \
	src/display/pi/cmn/displayid.o \
	src/display/pi/cmn/pd_init_all.o \
	src/display/pi/cmn/edid.o \
	src/display/pi/cmn/pi.o \
	src/display/pi/cmn/mode_table.o \
	src/display/pi/gn6/i2c_snb.o \
	src/display/pi/gn7/i2c_vlv.o \
	src/display/dsp/cmn/dsp.o \
	src/display/dsp/gn6/dsp_snb.o \
	src/display/dsp/gn7/dsp_vlv.o \
	src/pal/analog/analog.o \
	src/pal/analog/analog_sense.o \
	src/pal/hdmi/hdmi_port.o \
	src/pal/dp/dp_port.o \
	src/utils/pci.o \
	src/utils/memmap.o \
	src/utils/math_fix.o \
	src/video/overlay/cmn/ovl_coeff.o \
	src/video/overlay/cmn/igd_ovl.o \
	src/video/overlay/cmn/ovl_cache_util.o \
	src/video/overlay/gn6/spr_snb.o \
	src/video/overlay/gn6/spr_cache_snb.o \
	src/video/overlay/gn7/spr_vlv.o \
	src/video/overlay/gn7/spr_cache_vlv.o \
	src/video/overlay/gn6/micro_spr_snb.o \
	src/video/overlay/gn7/micro_spr_vlv.o \
	src/state/reg/cmn/reg.o \
	src/state/reg/gn6/reg_snb.o \
	src/state/reg/gn7/reg_vlv.o \
	src/state/power/cmn/igd_pwr.o \
	src/state/power/gn6/pwr_snb.o \
	src/state/power/gn7/pwr_vlv.o

ifeq "$(shell echo $(MSVDX_SUPPORT) | tr '[A-Z]' '[a-z]')" "yes"
	EMGD_OBJS += src/core/emgd_ved.o
endif

emgd-y := \
	$(EMGD_OBJS) \

emgd-$(CONFIG_COMPAT) += src/core/i915/i915_ioc32.o

obj-$(CONFIG_DRM_EMGD) += emgd.o

all:: clean modules

modules::
	@echo $(CURDIR) -- $(BUILD)
	@echo "$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules"
	@$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules
ifeq "$(EMGD_CHANGELOG)" "1"
	git log --pretty=format:"%h - %an (%ae): %ci %n%s" --shortstat --since=2.weeks | awk 'sub("$$", "\r")' >  ../patches/drm/EMGD_CHANGES.txt
endif

clean::
	@rm -f $(emgd-y)
	@rm -f emgd.o emgd.mod.* emgd.ko Module.* modules.order
	@find . -name "*.cmd" -exec rm '{}' \;
ifeq "$(KCOVERAGE_BUILD)" "1"
	@find . -name "*.gcno" -exec rm -f {} +
endif

install::
	install -o root -g root -m 755 -d $(INSTALLDIR)
	install -o root -g root -m 744 emgd.ko $(INSTALLDIR)
	/sbin/depmod -a

uninstall::
	rmmod $(INSTALLDIR)/emgd.ko
	rm -rf $(INSTALLDIR)/emgd.ko
	/sbin/depmod -a

debug::
	export BUILD=debug; $(MAKE) modules

package:: clean
	@echo -e "$(BLUE)Packaging $(EGD_TOPLEVEL)$(OFF)";
	mkdir -p $(EGD_PKG)
	tar -C $(EMGD_MOD_DIR) --exclude "CVS" -czf $(EGD_PKG)/emgd_drm.tgz *

