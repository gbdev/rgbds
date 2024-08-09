#!/bin/bash
set -euo pipefail

pngver=1.6.43

## Grab sources and check them

curl -LOJ "http://prdownloads.sourceforge.net/libpng/libpng-$pngver.tar.xz?download"
# Brew doesn't provide any sha256sum, so we're making do with `sha2` instead.
if [ "$(sha2 -q -256 libpng-$pngver.tar.xz)" != 6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c ]; then
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
