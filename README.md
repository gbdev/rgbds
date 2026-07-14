# RGBDS

[RGBDS](https://github.com/gbdev/rgbds) (Rednex Game Boy Development System) is
a free assembler/linker package for the Game Boy and Game Boy Color.

It consists of four tools:

- **RGBASM:** assembler
- **RGBLINK:** linker
- **RGBFIX:** ROM checksum/header fixer
- **RGBGFX:** PNG-to-Game Boy graphics converter

## Documentation

The [full documentation](https://rgbds.gbdev.io/docs/) is available online,
including the [development history](https://rgbds.gbdev.io/docs/rgbds.7) and
[version history](https://rgbds.gbdev.io/versions), generated from the man pages
in this repository.

The documentation website's own source code is available in the
[rgbds-www](https://github.com/gbdev/rgbds-www) repository.

## Community

If you have questions about the RGBDS code or its organization, you can contact
the maintainers on the [GBDev Discord server](https://discord.gg/RjJKA8wrD4) or
[other community channels](https://gbdev.io/chat), or via email at
`rgbds at gbdev dot io`.

If you want to help maintain RGBDS, please read [the contribution guide](/docs/CONTRIBUTING.md).

## Installing

The [platform-specific installation instructions](https://rgbds.gbdev.io/install/)
are available online. To [build from source](https://rgbds.gbdev.io/install/source)
using `make` or `cmake`, briefly:

```sh
make
sudo make install
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Both build systems support a prefix (e.g. to install into a specific directory)
and a suffix (e.g. to append the version number or commit ID):

```sh
make
sudo make install PREFIX=install_dir/ SUFFIX=-$(git rev-parse --short HEAD)
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSUFFIX=-$(git rev-parse --short HEAD)
cmake --build build
cmake --install build --prefix install_dir
```

On Windows, any `SUFFIX` should include the `.exe` extension.
