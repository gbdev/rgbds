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

This toolchain is maintained on `GitHub <https://github.com/rednex/rgbds>`__.

The documentation of this toolchain can be viewed online
`here <https://rgbds.gbdev.io/docs/>`__, it is generated from the man pages
found in this repository.

1. Installing RGBDS
-------------------

The `installation procedure <https://rgbds.gbdev.io/install>`__ is available
online for various platforms. `Building from source <https://rgbds.gbdev.io/install/source>`__
is possible using ``make`` or ``cmake``; follow the link for more detailed instructions.

.. code:: sh

    make
    sudo make install

.. code:: sh

    cmake -S . -B build
    cmake --build build
    cmake --install build

2. History
---------

- Around 1997, Carsten Sørensen (AKA SurfSmurf) writes ASMotor as a
  general-purpose assembler/linker system for DOS/Win32

- Around 1999, Justin Lloyd (AKA Otaku no Zoku) adapts ASMotor to read and
  produce GBZ80 assembly/machine code, and releases this version as RGBDS.

- 2009, Vegard Nossum adapts the code to be more UNIX-like and releases
  this version as rgbds-linux on
  `GitHub <https://github.com/vegard/rgbds-linux>`__.

- 2010, Anthony J. Bentley forks that repository. The fork becomes the reference
  implementation of rgbds.

- 2017, Bentley's repository is moved to a neutral name.

- 2018, codebase relicensed under the MIT license.

- 2020, repository is moved to the `gbdev <https://github.com/gbdev>`__ organisation. The `rgbds.gbdev.io <https://rgbds.gbdev.io>`__ website serving documentation and downloads is created.
