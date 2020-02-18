#!/bin/sh

# This script was written by ISSOtm while looking at Arch Linux's PKGBUILD for
# the corresponding package. (And its dependencies)
# https://aur.archlinux.org/packages/mingw-w64-libpng/

set -e

pngver=1.6.37
_apngver=$pngver
_arch="$1"


## Install mingw-configure and mingw-env (both build dependencies)

install -m 755 .github/actions/mingw-env.sh /usr/bin/mingw-env

sed "s|@TRIPLE@|${_arch}|g" .github/actions/mingw-configure.sh > ${_arch}-configure
install -m 755 ${_arch}-configure /usr/bin/


## Grab sources and check them

wget http://downloads.sourceforge.net/sourceforge/libpng/libpng-$pngver.tar.xz
wget http://downloads.sourceforge.net/project/apng/libpng/libpng16/libpng-$_apngver-apng.patch.gz
sha256sum -c .github/actions/mingw-w64-libpng-dev.sha256sums

## Extract sources

tar -xf libpng-$pngver.tar.xz
gunzip libpng-$_apngver-apng.patch.gz


## Start building!

cd libpng-$pngver
# Patch in apng support
patch -p0 ../libpng-$_apngver-apng.patch

mkdir -p build-${_arch}
cd build-${_arch}
${_arch}-configure LDFLAGS=-static-libgcc
make
make install
