#!/bin/sh

export CFLAGS
export LDFLAGS

echo "Configuring DriveFUSION for compilation"
libtoolize --copy --force
aclocal
autoheader
automake -a -c
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    if [ -d /usr/share/man ]; then
	mandir=/usr/share/man
	infodir=/usr/share/info
    else
	mandir=/usr/man
	infodir=/usr/info
    fi
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --infodir=${infodir} --mandir=${mandir} --enable-maintainer-mode "$@"
else
    ./configure "$@"
fi
