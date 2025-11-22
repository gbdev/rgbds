#!/bin/bash
set -euo pipefail

pngver=1.6.51
arch="$1"

## Grab sources and check them

wget http://downloads.sourceforge.net/project/libpng/libpng16/$pngver/libpng-$pngver.tar.xz
echo a050a892d3b4a7bb010c3a95c7301e49656d72a64f1fc709a90b8aded192bed2 libpng-$pngver.tar.xz | sha256sum -c -

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
