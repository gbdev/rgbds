# RGBDS

RGBDS (Rednex Game Boy Development System) is a free assembler/linker package
for the Game Boy and Game Boy Color. It consists of:

- RGBASM (assembler)
- RGBLINK (linker)
- RGBFIX (checksum/header fixer)
- RGBGFX (PNG-to-Game Boy graphics converter)

This is a fork of the original RGBDS which aims to make the programs more like
other UNIX tools.

This toolchain is maintained [on GitHub](https://github.com/gbdev/rgbds).

The documentation of this toolchain can be [viewed online](https://rgbds.gbdev.io/docs/),
including its [basic usage and development history](https://rgbds.gbdev.io/docs/rgbds.7).
It is generated from the man pages found in this repository.
The source code of the website itself is on GitHub as well under the repository
[rgbds-www](https://github.com/gbdev/rgbds-www).

If you want to contribute or maintain RGBDS, read [CONTRIBUTING.md](CONTRIBUTING.md).
If you have questions regarding the code, its organization, etc. you can find the maintainers
[on the GBDev community channels](https://gbdev.io/chat) or via mail at `rgbds at gbdev dot io`.

## Installing RGBDS

The [installation procedure](https://rgbds.gbdev.io/install) is available
online for various platforms. [Building from source](https://rgbds.gbdev.io/install/source)
is possible using `make` or `cmake`; follow the link for more detailed instructions.

```sh
make
sudo make install
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Two parameters available when building are a prefix (e.g. to put the executables in a directory)
and a suffix (e.g. to append the version number or commit ID).

```sh
make
sudo make install PREFIX=install_dir/ SUFFIX=-$(git rev-parse --short HEAD)
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSUFFIX=-$(git rev-parse --short HEAD)
cmake --build build
cmake --install build --prefix install_dir
```

(If you set a `SUFFIX`, it should include the `.exe` extension on Windows.)
