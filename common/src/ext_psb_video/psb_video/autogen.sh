#! /bin/sh
autoreconf -v --install

if [ $(uname -m) == "x86_64" ]; then
    export CFLAGS="$CFLAGS -m64"
    export PKG_CONFIG_PATH=/usr/lib64/pkgconfig
else
    export CFLAGS="$CFLAGS -m32"
    export PKG_CONFIG_PATH=/usr/lib/pkgconfig
fi

./configure "$@"
