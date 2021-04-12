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

If you want to contribute or maintain RGBDS, and have questions regarding the code, its organisation, etc. you can find me `on GBDev <https://gbdev.io/chat>`__ or via mail at ``rgbds at eldred dot fr``.

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

The RGBDS source code file structure somewhat resembles the following:

::

       .
       ├── .github/
       │   ├── actions/
       │   │   └── ...
       │   └── workflows/
       │       └── ...
       ├── contrib/
       │   ├── zsh_compl/
       │   │   └── ...
       │   └── ...
       ├── include/
       │   └── ...
       ├── src/
       │   ├── asm/
       │   │   └── ...
       │   ├── extern/
       │   │   └── ...
       │   ├── fix/
       │   │   └── ...
       │   ├── gfx/
       │   │   └── ...
       │   ├── link/
       │   │   └── ...
       │   ├── CMakeLists.txt
       │   └── ...
       ├── test/
       │   ├── ...
       │   └── run-tests.sh
       ├── CMakeLists.txt
       ├── Makefile
       └── README.rst

- ``.github/`` - files and scripts related to the integration of the RGBDS codebase with
  GitHub.

- ``contrib/`` - scripts and other resources which may be useful to users and developers of
  RGBDS.

  * ``zsh_compl`` contains tab completion scripts for use with zsh. Put them somewhere in your ``fpath``, and they should auto-load.

- ``include/`` - header files for each respective C files in `src`.

- ``src/`` - source code and manual pages for RGBDS.

  * Note that the code unique to each RGBDS tool is stored in its respective subdirectory
    (rgbasm -> ``src/asm/``, for example). ``src/extern/`` contains code imported from external sources.

- ``test/`` - testing framework used to verify that changes to the code don't break or modify the behavior of RGBDS.

3. History
----------

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
