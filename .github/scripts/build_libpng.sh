#!/bin/bash
set -euo pipefail

pngver=1.6.56

## Grab sources and check them

curl -LOJ "http://prdownloads.sourceforge.net/libpng/libpng-$pngver.tar.xz?download"
echo f7d8bf1601b7804f583a254ab343a6549ca6cf27d255c302c47af2d9d36a6f18 libpng-$pngver.tar.xz | \
	shasum -a 256 -c -

## Extract sources and patch them

tar -xvf libpng-$pngver.tar.xz

## Start building!

mkdir -p build
cd build
../libpng-$pngver/configure --disable-shared --enable-static \
	CFLAGS="-O3 -flto -DNDEBUG -mmacosx-version-min=10.9 -arch x86_64 -arch arm64 -fno-exceptions"
make -kj
make install prefix="$PWD/../libpng-staging"
