RGBDS
=====

RGBDS (Rednex Game Boy Development System) is a free assembler/linker package
for the Game Boy and Game Boy Color. It consists of:

- rgbasm (assembler)
- rgblink (linker)
- rgbfix (checksum/header fixer)
- rgbgfx (PNG‐to‐Game Boy graphics converter)

This is a fork of the original RGBDS which aims to make the programs more like
other UNIX tools.

This toolchain is maintained `on GitHub <https://github.com/gbdev/rgbds>`__.

The documentation of this toolchain can be viewed online `here <https://rgbds.gbdev.io/docs/>`__.
It is generated from the man pages found in this repository.
The source code of the website itself is on GitHub as well under the repo
`rgbds-www <https://github.com/gbdev/rgbds-www>`__.

If you want to contribute or maintain RGBDS, and have questions regarding the code, its
organisation, etc. you can find the maintainers `on the gbdev community channels <https://gbdev.io/chat>`__
or via mail at ``rgbds at gbdev dot io``.

1. Installing RGBDS
-------------------

The `installation procedure <https://rgbds.gbdev.io/install>`__ is available
online for various platforms. `Building from source <https://rgbds.gbdev.io/install/source>`__
is possible using ``make`` or ``cmake``; follow the link for more detailed instructions.

.. code:: sh

    make
    sudo make install

.. code:: sh

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    cmake --install build

2. RGBDS Folder Organization
----------------------------

The RGBDS source code file structure is as follows:

::

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
       └── README.rst

.. |clang-format| replace:: ``clang-format``
.. _clang-format: https://clang.llvm.org/docs/ClangFormat.html

- ``.github/`` - files and scripts related to the integration of the RGBDS codebase with
  GitHub.

  * ``scripts/`` - scripts used by workflow files.
  * ``workflows/`` - CI workflow description files.

- ``contrib/`` - scripts and other resources which may be useful to users and developers of
  RGBDS.

  * ``zsh_compl`` contains tab completion scripts for use with zsh. Put them somewhere in
    your ``fpath``, and they should auto-load.

  * ``bash_compl`` contains tab completion scripts for use with bash. Run them with ``source``
    somewhere in your ``.bashrc``, and they should load every time you open a shell.

- ``include/`` - header files for the respective source files in `src`.

- ``man/`` - manual pages.

- ``src/`` - source code of RGBDS.

  * Note that the code unique to each RGBDS tool is stored in its respective subdirectory
    (rgbasm -> ``src/asm/``, for example). ``src/extern/`` contains code imported from
    external sources.

- ``test/`` - testing framework used to verify that changes to the code don't break or
  modify the behavior of RGBDS.

- ``.clang-format`` - code style for automated C++ formatting with |clang-format|_.

- ``Dockerfile`` - defines how to build RGBDS with Docker.

3. History
----------

- 1996-10-01: Carsten Sørensen (a.k.a. SurfSmurf) releases
  `xAsm <http://otakunozoku.com/RGBDSdocs/asm.htm>`__,
  `xLink <http://otakunozoku.com/RGBDSdocs/link.htm>`__, and
  `RGBFix <http://otakunozoku.com/RGBDSdocs/fix.htm>`__,
  a Game Boy SM83 (GBZ80) assembler/linker system for DOS/Win32.

- 1997-07-03: Sørensen releases `ASMotor <http://otakunozoku.com/RGBDSdocs/geninfo.htm>`__,
  packaging the three programs together and moving towards making them a
  general-purpose target-independent system.

- 1999-08-01: Justin Lloyd (a.k.a. Otaku no Zoku) adapts ASMotor to re-focus
  on SM83 assembly/machine code, and releases this version as
  `RGBDS <http://otakunozoku.com/rednex-gameboy-development-system/>`__.

- 2009-06-11: Vegard Nossum adapts the code to be more UNIX-like and releases
  this version as `rgbds-linux <https://github.com/vegard/rgbds-linux>`__.

- 2010-01-12: Anthony J. Bentley `forks <https://github.com/bentley>`__ Nossum's
  repository. The fork becomes the reference implementation of RGBDS.

- 2015-01-18: stag019 begins implementing `rgbgfx <https://github.com/stag019/rgbgfx>`__,
  a PNG‐to‐Game Boy graphics converter, for eventual integration into RGBDS.

- 2016-09-05: rgbgfx is `integrated <https://github.com/gbdev/rgbds/commit/c3c31138ddbd8680d4e67957e387f2816798a71b>`__
  into Bentley's repository.

- 2017-02-23: Bentley's repository is moved to the `rednex <https://github.com/rednex>`__
  organization.

- 2018-01-26: The codebase is `relicensed <https://github.com/gbdev/rgbds/issues/128>`__
  under the MIT license.

- 2020-09-15: The repository is `moved <https://github.com/gbdev/rgbds/issues/567>`__
  to the `gbdev <https://github.com/gbdev>`__ organization.

- 2022-05-17: The `rgbds.gbdev.io <https://rgbds.gbdev.io>`__ website for RGBDS
  documentation and downloads is published.

4. Acknowledgements
-------------------

RGBGFX generates palettes using algorithms found in the paper
`"Algorithms for the Pagination Problem, a Bin Packing with Overlapping Items" <http://arxiv.org/abs/1605.00558>`__
(`GitHub <https://github.com/pagination-problem/pagination>`__, MIT license),
by Aristide Grange, Imed Kacem, and Sébastien Martin.

RGBGFX's color palette was taken from `SameBoy <https://sameboy.github.io>`__, with permission and help by `LIJI <https://github.com/LIJI32>`__.
