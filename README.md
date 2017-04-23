# RGBDS

RGBDS (Rednex Game Boy Development System) is a free assembler/linker package
for the Game Boy and Game Boy Color. It consists of:

  - rgbasm  (assembler)
  - rgblink (linker)
  - rgbfix  (checksum/header fixer)
  - rgbgfx  (PNG‐to‐Game Boy graphics converter)

This is a fork of the original RGBDS which aims to make the programs more like
other UNIX tools.

This toolchain is maintained on [GitHub](https://github.com/rednex/rgbds), as
well as its [documentation](https://github.com/rednex/rednex.github.io).

The documentation of this toolchain can be viewed online
[here](https://rednex.github.io/), it is generated from the man pages found in
this repository.

## 1. Installing RGBDS

### 1.1 Windows

Windows builds are available in the releases page on GitHub:

    https://github.com/rednex/rgbds/releases

Copy the `.exe` files to `C:\Windows\` or similar.

If you require the latest version in development, it should be possible to
compile RGBDS with MinGW or Cygwin by following the instructions to build it
on UNIX systems.

### 1.2 macOS

You can build RGBDS by following the instructions below. However, if you would
prefer not to build RGBDS yourself, you may also install it using
[Homebrew](http://brew.sh/).

To install the latest release, use:

```sh
brew install rgbds
```

To install RGBDS with all of the current changes in development (as seen on the
`master` branch on GitHub), use:

```sh
brew install rgbds --HEAD
```

### 1.3 Other UNIX-like systems

No official binaries of RGBDS are distributed for these systems, you must follow
the simple instructions below to compile and install it.


## 2. Building RGBDS from source

RGBDS can be built in UNIX-like systems by following the instructions below.

### 2.1 Dependencies

RGBDS requires yacc, flex, libpng and pkg-config to be installed.

On macOS, install the latter two with [Homebrew](http://brew.sh/):

```sh
brew install libpng pkg-config
```

On other Unixes, use the built-in package manager. For example, on Debian or
Ubuntu:

```sh
sudo apt-get install byacc flex pkg-config libpng-dev
```

You can test if libpng and pkg-config are installed by running
`pkg-config --cflags libpng`: if the output is a path, then you're good, and if
it outputs an error then you need to install them via a package manager.

### 2.2 Build process

To build the programs, run in your terminal:

```sh
make
```

Then, to install the compiled programs and manual pages, run (with appropriate
privileges, e.g, with `sudo`):


```sh
make install
```

After installation, you can read the manuals with the `man` command. E.g.,

```sh
man 7 rgbds
```

There are some variables in the Makefile that can be redefined by the user. The
variables described below can affect installation behavior when given on the
make command line. For example, to install RGBDS in your home directory instead
of systemwide, run the following:

```sh
make install PREFIX=$HOME
```

To do a verbose build, run:

```sh
make Q=
```

This is the complete list of user-defined variables:

- `PREFIX`: Location where RGBDS will be installed. Defaults to `/usr/local`.

- `bindir`: Location where the binaries will be installed. Defaults to `${PREFIX}/bin`.

- `mandir`: Location where the manpages will be installed. Defaults to `${PREFIX}/man`.

- `DESTDIR`: This is prepended to all paths during the installation. It is
  mainly used for packaging.

- `Q`: Whether to quiet the build or not. To make the build more verbose, clear
  this variable. Defaults to `@`.

- `STRIP`: Whether to strip the installed binaries of debug symbols or not.
  Defaults to `-s`.

- `BINMODE`: Permissions of the installed binaries. Defaults to `555`.

- `MANMODE`: Permissions of the installed manpages. Defaults to `444`.

## 3 History

- Around 1997, Carsten Sorensen (AKA SurfSmurf) writes ASMotor as a
  general-purpose assembler/linker system for DOS/Win32

- Around 1999, Justin Lloyd (AKA Otaku no Zoku) adapts ASMotor to read and
  produce GBZ80 assembly/machine code, and releases this version as RGBDS.

- 2009, Vegard Nossum adapts the code to be more UNIX-like and releases this
  version as rgbds-linux on [GitHub](https://github.com/vegard/rgbds-linux).

- 2010, Anthony J. Bentley forks that repository. The fork becomes the reference
  implementation of rgbds.

- 2017, Bentley's repository is moved to a neutral name.
