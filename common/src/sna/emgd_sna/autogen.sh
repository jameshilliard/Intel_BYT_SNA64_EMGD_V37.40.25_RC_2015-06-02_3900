#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

if [ $(uname -m) == "x86_64" ]; then
    export CFLAGS="$CFLAGS -m64"
else
    export CFLAGS="$CFLAGS -m32"
fi

if test -z "$NOCONFIGURE"; then
    if [ -n "${XFREE}"  ]; then
        echo XFREE dir is set
        export XORG_CFLAGS="$XORG_CFLAGS -I$XFREE/usr/include/xorg -I$XFREE/usr/include -I$XFREE/usr/include/pixman-1 -I$XFREE/usr/include/libdrm -I$XFREE/usr/include/X11/dri/"
        export XORG_LIBS="$XORG_LIBS -L$XFREE/usr/lib"
        $srcdir/configure --with-sysroot=$XFREE "$@"
    else
        $srcdir/configure "$@"
    fi
fi
