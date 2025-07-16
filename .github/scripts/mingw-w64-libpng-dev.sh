#!/bin/bash
set -euo pipefail

pngver=1.6.50
arch="$1"

## Grab sources and check them

wget http://downloads.sourceforge.net/project/libpng/libpng16/$pngver/libpng-$pngver.tar.xz
echo 4df396518620a7aa3651443e87d1b2862e4e88cad135a8b93423e01706232307 libpng-$pngver.tar.xz | sha256sum -c -

## Extract sources and patch them

tar -xf libpng-$pngver.tar.xz

## Start building!

mkdir -p build
cd build
../libpng-$pngver/configure \
	--host="$arch" --target="$arch" \
	--prefix="/usr/$arch" \
	--enable-shared --disable-static \
	CPPFLAGS="-D_FORTIFY_SOURCE=2" \
	CFLAGS="-O2 -pipe -fno-plt -fno-exceptions --param=ssp-buffer-size=4" \
	LDFLAGS="-Wl,-O1,--sort-common,--as-needed -fstack-protector"
make -kj
sudo make install
