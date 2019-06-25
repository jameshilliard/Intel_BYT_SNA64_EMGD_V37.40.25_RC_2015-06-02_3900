For support VP8 on Baytrail use VXD392
========================================================
VP8 only enable using on kernel LTSI 3.10.61


kernel space:
--------------------------------------------------------------------
1. emgd_drm:
	Has add ipvr support code into emgd_drm, make and install emgd_drm, then support will take effect.
For turn off VP8, export MSVXD_SUPPORT=no, and turn on VP8, export MSVDX_SUPPORT=yes, when kernel >= 3.10.61, VP8 turn on as default.
please load emgd.ko first, then load ipvr.ko, and unload ipvr.ko first, then upload emgd.ko.

command:
a.  git clone ssh://<your name>@git-amr-1.devtools.intel.com:29418/lin-emgd_drm emgd_drm
b.  cd emgd_drm
c.  git checkout master
d.  make and make install

2. ipvr:
	add CONFIG_DRM_IPVR in kernel config
	In linux-ltsi-3.10.61/drivers/gpu/drm/, Makefile and Kconfig need
to be changed accordingly, has integrated with emgd_drm changes into same
pathes: drm_Kconfig.patch and drm_Makefile.patch

command:
a.  git clone ssh://<your name>@git-amr-1.devtools.intel.com:29418/lin-ext_ipvr ipvr
b.  cd ipvr
c.  git checkout master
d.  make and make install

or make them within kernel source
a.  add CONFIG_DRM_IPVR=m and CONFIG_DRM_EMGD=m in kernel config, disable I915
b.  in kernel root dir,
    patch -p1 < <your path>/lin-emgd_vlv/patches/drm/Linux_Kernel_3_10_61_LTSI/drm_Kconfig.patch
    patch -p1 < <your path>/lin-emgd_vlv/patches/drm/Linux_Kernel_3_10_61_LTSI/drm_Makefile.patch
c.  cp emgd_drm to <kernel root dir>/drivers/gpu/drm/emgd
    cp ipvr to <kernel root dir>/drivers/gpu/drm/ipvr
d.  in kernel root dir,
    make modules_prepare
    make modules SUBDIRS=drivers/gpu/drm
f.  cp emgd.ko /lib/modules/<your kernel>/kernel/drivers/gpu/drm/emgd/emgd.ko
    cp ipvr.ko /lib/modules/<your kernle>/kernel/drivers/gpu/drm/ipvr/ipvr.ko
    depmod -a
e.  echo 0 > /sys/class/vtconsole/vtcon1/bind
    rmmod emgd.ko
    modprobe emgd firmware="emgd.bin"
    modprobe ipvr
    ls /dev/dri, there are card0 and card1, then vp8 kernel module load OK

user space:
--------------------------------------------------------------------
due to package dependency, should install libDRM and libva before compile
psb_video and libva_wrapper
for F18 64 bits, some packages need --libdir=/usr/lib64 when configure,
ensure that correct libdir used, for example gstreamer vaapi

1. Require libDRM:
a. git clone git://anongit.freedesktop.org/mesa/drm
   cd drm
   git checkout libdrm-2.4.52
b. patch the patch file under lin-emgd_vlv/patches/common/VA_Driver_i965/VP8
   patch -p1 < 0001-libdrm-add-ipvr-into-libdrm-2.4.52.patch
c. /autogen.sh --prefix=/usr (./autogen.sh --prefix=/usr --libdir=/usr/lib64 for 64 bits)
   make
   make install

2. Require Libva 1.5.0:
a. git clone ssh://<yourname>@git-amr-1.devtools.intel.com:29418/lin-ext_libva
  git checkout e6f7e1cd296d3694fbc56e7b569ac6f85d9e2d0e
b. ./autogen.sh --prefix=/usr
   make
   make install

3. psb video:
	beside emgd dependencies, need to yum install libXrandr-devel.i686
	psb video should configure and make after libva

command:
a. yum install libXrandr-devel.i686
b. git clone ssh://<your name>@git-amr-1.devtools.intel.com:29418/lin-
ext_psb_video psb_video
c. git checkout master
d. ./autogen.sh --prefix=/usr (./autogen.sh --prefix=/usr --libdir=/usr/
lib64 for 64 bits)
e. make
f. make install

4. libva wrapper:
	should configure and make after libva

command:
a. git clone ssh://<your name>@git-amr-1.devtools.intel.com:29418/lin-libva_wrapper libva_wrapper
b. git checkout master
c ./autogen.sh --prefix=/usr (./autogen.sh --prefix=/usr --libdir=/usr/lib64 for 64 bits)
d. make
e. make install

5. gstreamer vaapi:
	need to apply vp8 patch, in /lin-emgd_vlv/patches/common/VA_Driver_i965/VP8/0001-gstreaemr-vaapi-For-support-VP8-on-Baytrail-use-VXD3.patch

command:
a. git clone https://github.com/01org/gstreamer-vaapi
   cd gstreamer-vaapi
   git submodule init
   git submodule update
   patch -p1 <your path>/lin-emgd_vlv/patches/common/VA_Driver_i965/VP8/0001-gstreaemr-vaapi-For-support-VP8-on-Baytrail-use-VXD3.patch
b. ./autogen.sh --prefix=/usr (./autogen.sh --prefix=/usr --libdir=/usr/lib64 for 64 bits)
   make
   make install

6. Need to export environment variable before run gstreamer:
	export LIBVA_DRIVER_NAME=wrapper

7. gstreamer pipeline:
	gst-launch-1.0 filesrc location=<vp8 clip> ! matroskademux ! queue ! vaapidecode ! vaapisink

