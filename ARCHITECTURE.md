# RGBDS Architecture

## Folder Organization

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
├── compile_flags.txt
├── Dockerfile
└── Makefile
```

- `.github/` - files and scripts related to the integration of the RGBDS codebase with
  GitHub.
  * `scripts/` - scripts used by workflow files.
  * `workflows/` - CI workflow description files.
- `contrib/` - scripts and other resources which may be useful to users and developers of
  RGBDS.
  * `zsh_compl` - contains tab completion scripts for use with zsh. Put them somewhere in
    your `fpath`, and they should auto-load.
  * `bash_compl` - contains tab completion scripts for use with bash. Run them with `source`
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
- `CMakeLists.txt` - defines how to build RGBDS with CMake.
- `compile_flags.txt` - compiler flags for C++ static analysis with
  [`clang-tidy`](https://clang.llvm.org/extra/clang-tidy/).
- `Dockerfile` - defines how to build RGBDS with Docker.
- `Makefile` - defines how to build RGBDS with `make`.
