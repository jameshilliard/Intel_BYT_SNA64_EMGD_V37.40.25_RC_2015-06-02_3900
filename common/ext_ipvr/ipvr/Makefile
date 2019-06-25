#----------------------------------------------------------------------------
# Filename: Makefile
#----------------------------------------------------------------------------
# Copyright (c) 2015, Intel Corporation.
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
export IPVR_TOPLEVEL = IPVR Driver

KERNELRELEASE ?= ipvr_standalone

ifeq "$(KERNELRELEASE)" "ipvr_standalone"
	KERNELVER ?= $(shell uname -r)
	KERNELDIR ?= /lib/modules/$(KERNELVER)/build
	INSTALLDIR ?= /lib/modules/$(KERNELVER)/kernel/drivers/gpu/drm/ipvr
else
	KERNELDIR ?= ./
endif

BLUE = \033[34m
OFF = \033[0m
BUILD ?= release
CONFIG_DRM_IPVR ?= m

# Get the include paths pointed to the right place.
ifeq "$(KERNELRELEASE)" "ipvr_standalone"
	export  IPVR_MOD_DIR ?= $(CURDIR)
else
	export  IPVR_MOD_DIR ?= drivers/gpu/drm/ipvr
endif

ccflags-y := -Iinclude/drm

ipvr-y := \
        ipvr_drv.o \
        ipvr_bo.o \
        ipvr_exec.o \
        ipvr_fence.o \
        ipvr_gem.o \
        ipvr_mmu.o \
        ipvr_debug.o \
        ipvr_trace.o \
        ved_pm.o \
        ved_cmd.o \
        ved_fw.o

obj-$(CONFIG_DRM_IPVR) += ipvr.o

CFLAGS_ipvr_trace.o := -I$(src)

all:: clean modules

modules::
	@echo $(CURDIR)
	@echo "$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules"
	@$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules

clean::
	@rm -f $(ipvr-y)
	@rm -f ipvr.o ipvr.mod.* ipvr.ko Module.* modules.order
	@find . -name "*.cmd" -exec rm '{}' \;

install::
	install -o root -g root -m 755 -d $(INSTALLDIR)
	install -o root -g root -m 744 ipvr.ko $(INSTALLDIR)
	/sbin/depmod -a

uninstall::
	rm -rf $(INSTALLDIR)/ipvr.ko
	/sbin/depmod -a

package:: clean
	@echo -e "$(BLUE)Packaging $(IPVR_TOPLEVEL)$(OFF)";
	mkdir -p $(EGD_PKG)
	tar -C $(IPVR_MOD_DIR) --exclude "CVS" -czf $(EGD_PKG)/ipvr.tgz *
