#!/bin/bash
set -euo pipefail

pngver=1.6.50

## Grab sources and check them

curl -LOJ "http://prdownloads.sourceforge.net/libpng/libpng-$pngver.tar.xz?download"
# Brew doesn't provide any sha256sum, so we're making do with `sha2` instead.
if [ "$(sha2 -q -256 libpng-$pngver.tar.xz)" != 4df396518620a7aa3651443e87d1b2862e4e88cad135a8b93423e01706232307 ]; then
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
