#----------------------------------------------------------------------------
# Copyright (c) 2002-2012, Intel Corporation.
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
#
#----------------------------------------------------------------------------

INSTALL_PKG ?= emgdgui
MHD_PATH= libs/libmicrohttpd
PROG	= emgduid
GEN_SUFFIX = _generated
AT := @
CC	= g++

ifeq ($(shell uname -m), x86_64)
 ARCH_FLAGS = -m64
else
 ARCH_FLAGS = -m32
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

VERSION = -DBUILD_MAJOR_NUM=$(MAJORNUM) -DBUILD_MINOR_NUM=$(MINORNUM) -DBUILD_BUILD_NUM=$(BUILDNUM)

INCLUDE	= -I$(MHD_PATH)/src/include \
	-I$(MHD_PATH)/src/include/plibc -Isrc
CFLAGS	= $(ARCH_FLAGS) $(VERSION) -std=gnu++0x
LDFLAGS	= -L$(MHD_PATH)/src/daemon/.libs
LLIBS	= -lpthread \
	-l:libmicrohttpd.a
OBJ	= table.o main_page.o set_pages.o $(PROG)$(GEN_SUFFIX).o

all:: prep  $(PROG)

prep:
	$(AT)cd src; \
	bash prebuild.sh $(PROG)$(GEN_SUFFIX).cpp
	$(AT)if [ ! -f $(MHD_PATH)/src/daemon/.libs/libmicrohttpd.a ]; then \
		echo "Compiling libmicrohttpd"; tar -C libs -xf libs/libmicrohttpd.tgz; \
		cd $(MHD_PATH);unset LIBS;unset CC; \
		./configure "CFLAGS=$(ARCH_FLAGS)" "LDFLAGS=$(ARCH_FLAGS)" --disable-https; make; \
	fi

$(PROG): $(OBJ)
	$(AT)$(CC) $(INCLUDE) $(LDFLAGS) $(CFLAGS) $(GUI_DEBUG) -o $(PROG) $(OBJ) $(LDFLAGS) $(LLIBS)

$(PROG)$(GEN_SUFFIX).o:  src/$(PROG)$(GEN_SUFFIX).cpp
	$(AT)echo -e "Compiling $@"
	$(AT)$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

table.o: src/table.cpp
	$(AT)echo -e "Compiling $@"
	$(AT)$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

main_page.o: src/main_page.cpp
	$(AT)echo -e "Compiling $@"
	$(AT)$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

set_pages.o: src/set_pages.cpp
	$(AT)echo -e "Compiling $@"
	$(AT)$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean::
	$(AT)rm -rf $(INSTALL_PKG)
	$(AT)rm -f src/$(PROG)$(GEN_SUFFIX).cpp $(PROG)
	$(AT)rm -f $(OBJ)

distclean:: clean
	$(AT)rm -rf $(MHD_PATH)

package:
	$(AT)rm -rf $(INSTALL_PKG)
	$(AT)mkdir $(INSTALL_PKG)
	$(AT)cp $(PROG) $(INSTALL_PKG)
	$(AT)cp -r browser_inc  $(INSTALL_PKG)
	$(AT)cp README.txt $(INSTALL_PKG)

