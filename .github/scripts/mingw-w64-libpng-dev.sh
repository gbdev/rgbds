#!/bin/bash
set -euo pipefail

pngver=1.6.53
arch="$1"

## Grab sources and check them

wget http://downloads.sourceforge.net/project/libpng/libpng16/$pngver/libpng-$pngver.tar.xz
echo 1d3fb8ccc2932d04aa3663e22ef5ef490244370f4e568d7850165068778d98d4 libpng-$pngver.tar.xz | sha256sum -c -

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
