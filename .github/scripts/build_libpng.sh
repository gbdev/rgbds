#!/bin/bash
set -euo pipefail

pngver=1.6.53

## Grab sources and check them

curl -LOJ "http://prdownloads.sourceforge.net/libpng/libpng-$pngver.tar.xz?download"
# Brew doesn't provide any sha256sum, so we're making do with `sha2` instead.
if [ "$(sha2 -q -256 libpng-$pngver.tar.xz)" != 1d3fb8ccc2932d04aa3663e22ef5ef490244370f4e568d7850165068778d98d4 ]; then
	sha2 -256 libpng-$pngver.tar.xz
	echo Checksum mismatch! Aborting. >&2
	exit 1
fi

## Extract sources and patch them

tar -xvf libpng-$pngver.tar.xz

## Start building!

mkdir -p build
cd build
../libpng-$pngver/configure --disable-shared --enable-static \
	CFLAGS="-O3 -flto -DNDEBUG -mmacosx-version-min=10.9 -arch x86_64 -arch arm64 -fno-exceptions"
make -kj
make install prefix="$PWD/../libpng-staging"
