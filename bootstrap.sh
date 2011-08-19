#!/bin/sh

CC=gcc
YAM_B=yam.bootstrap

case `uname` in
FreeBSD)
CFLAGS=-DFREEBSD
CFLAGS=`pkg-config --cflags lua-5.1`
LDFLAGS=`pkg-config --libs lua-5.1`
;;
Linux)
CFLAGS=-DLINUX
CFLAGS=`pkg-config --cflags lua5.1`
LDFLAGS=`pkg-config --libs lua5.1`
;;
*)
echo "unknown os"
exit 1
;;
esac

echo "Building temporary ${YAM_B} binary..."
${CC} -g -std=gnu99 ${CFLAGS} ${LDFLAGS} *.c -o ${YAM_B} || exit 1
echo "Boostraping..."
./${YAM_B}
rm ${YAM_B}
