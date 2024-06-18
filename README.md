# RGBDS

RGBDS (Rednex Game Boy Development System) is a free assembler/linker package
for the Game Boy and Game Boy Color. It consists of:

- RGBASM (assembler)
- RGBLINK (linker)
- RGBFIX (checksum/header fixer)
- RGBGFX (PNG‐to‐Game Boy graphics converter)

This is a fork of the original RGBDS which aims to make the programs more like
other UNIX tools.


This toolchain is maintained [on GitHub](https://github.com/gbdev/rgbds).

The documentation of this toolchain can be [viewed online](https://rgbds.gbdev.io/docs/).
It is generated from the man pages found in this repository.
The source code of the website itself is on GitHub as well under the repository
[rgbds-www](https://github.com/gbdev/rgbds-www).

If you want to contribute or maintain RGBDS, or you have questions regarding the code, its
organization, etc. you can find the maintainers [on the gbdev community channels](https://gbdev.io/chat)
or via mail at `rgbds at gbdev dot io`.

## 1. Installing RGBDS

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

## 2. RGBDS Folder Organization

The RGBDS source code file structure is as follows:

```
.
├── .github/
│   ├── scripts/
│   │   └── ...
│   └── workflows/
│       └── ...
├── contrib/
│   ├── zsh_compl/
│   │   └── ...
│   └── ...
├── include/
│   └── ...
├── man/
│   └── ...
├── src/
│   ├── asm/
│   │   └── ...
│   ├── extern/
│   │   └── ...
│   ├── fix/
│   │   └── ...
│   ├── gfx/
│   │   └── ...
│   ├── link/
│   │   └── ...
│   ├── CMakeLists.txt
│   └── ...
├── test/
│   ├── ...
│   └── run-tests.sh
├── .clang-format
├── CMakeLists.txt
├── Dockerfile
├── Makefile
└── README.md
```

- `.github/` - files and scripts related to the integration of the RGBDS codebase with
  GitHub.
  * `scripts/` - scripts used by workflow files.
  * `workflows/` - CI workflow description files.
- `contrib/` - scripts and other resources which may be useful to users and developers of
  RGBDS.
  * `zsh_compl` contains tab completion scripts for use with zsh. Put them somewhere in
    your `fpath`, and they should auto-load.
  * `bash_compl` contains tab completion scripts for use with bash. Run them with `source`
    somewhere in your `.bashrc`, and they should load every time you open a shell.
- `include/` - header files for the respective source files in `src`.
- `man/` - manual pages.
- `src/` - source code of RGBDS.
  * Note that the code unique to each RGBDS tool is stored in its respective subdirectory
    (RGBASM's code is in `src/asm/`, for example). `src/extern/` contains code imported from
    external sources.
- `test/` - testing framework used to verify that changes to the code don't break or
  modify the behavior of RGBDS.
- `.clang-format` - code style for automated C++ formatting with
  [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html).
- `Dockerfile` - defines how to build RGBDS with Docker.

## 3. History

- 1996-10-01: Carsten Sørensen (a.k.a. SurfSmurf) releases
  [xAsm](http://otakunozoku.com/RGBDSdocs/asm.htm),
  [xLink](http://otakunozoku.com/RGBDSdocs/link.htm), and
  [RGBFix](http://otakunozoku.com/RGBDSdocs/fix.htm),
  a Game Boy SM83 (GBZ80) assembler/linker system for DOS/Win32.
- 1997-07-03: Sørensen releases [ASMotor](http://otakunozoku.com/RGBDSdocs/geninfo.htm),
  packaging the three programs together and moving towards making them a
  general-purpose target-independent system.
- 1999-08-01: Justin Lloyd (a.k.a. Otaku no Zoku) adapts ASMotor to re-focus
  on SM83 assembly/machine code, and releases this version as
  [RGBDS](http://otakunozoku.com/rednex-gameboy-development-system/).
- 2009-06-11: Vegard Nossum adapts the code to be more UNIX-like and releases
  this version as [rgbds-linux](https://github.com/vegard/rgbds-linux).
- 2010-01-12: Anthony J. Bentley [forks](https://github.com/bentley) Nossum's
  repository. The fork becomes the reference implementation of RGBDS.
- 2015-01-18: stag019 begins implementing [RGBGFX](https://github.com/stag019/rgbgfx),
  a PNG‐to‐Game Boy graphics converter, for eventual integration into RGBDS.
- 2016-09-05: RGBGFX is [integrated](https://github.com/gbdev/rgbds/commit/c3c31138ddbd8680d4e67957e387f2816798a71b)
  into Bentley's repository.
- 2017-02-23: Bentley's repository is moved to the [rednex](https://github.com/rednex)
  organization.
- 2018-01-26: The codebase is [relicensed](https://github.com/gbdev/rgbds/issues/128)
  under the MIT license.
- 2020-09-15: The repository is [moved](https://github.com/gbdev/rgbds/issues/567)
  to the [gbdev](https://github.com/gbdev) organization.
- 2022-05-17: The [rgbds.gbdev.io](https://rgbds.gbdev.io) website for RGBDS
  documentation and downloads is published.

## 4. Acknowledgements

RGBGFX generates palettes using algorithms found in the paper
["Algorithms for the Pagination Problem, a Bin Packing with Overlapping Items"](https://arxiv.org/abs/1605.00558)
([GitHub](https://github.com/pagination-problem/pagination), MIT license),
by Aristide Grange, Imed Kacem, and Sébastien Martin.

RGBGFX's color palette was taken from [SameBoy](https://sameboy.github.io), with permission and help
by [LIJI](https://github.com/LIJI32).
