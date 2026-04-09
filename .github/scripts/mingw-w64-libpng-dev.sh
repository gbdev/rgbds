#!/bin/bash
set -euo pipefail

pngver=1.6.56
arch="$1"

## Grab sources and check them

wget http://downloads.sourceforge.net/project/libpng/libpng16/$pngver/libpng-$pngver.tar.xz
echo f7d8bf1601b7804f583a254ab343a6549ca6cf27d255c302c47af2d9d36a6f18 \*libpng-$pngver.tar.xz | \
	sha256sum -c -

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
