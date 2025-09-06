# RGBDS Architecture

## Folder Organization

The RGBDS source code file structure is as follows:

```
rgbds/
├── .github/
│   ├── scripts/
│   │   └── ...
│   └── workflows/
│       └── ...
├── contrib/
│   ├── bash_compl/
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
│   ├── bison.sh
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

- **`.github/`:**  
  Files related to the integration of the RGBDS codebase with GitHub features.
  * **`scripts/`:**  
    Scripts used by GitHub Actions workflow files.
  * **`workflows/`:**  
    GitHub Actions CI workflow description files. Used for automated testing, deployment, etc.
- **`contrib/`:**  
  Scripts and other resources which may be useful to RGBDS users and developers.
  * **`bash_compl/`:**  
    Tab completion scripts for use with `bash`. Run them with `source` somewhere in your `.bashrc`, and they should auto-load when you open a shell.
  * **`zsh_compl/`:**  
    Tab completion scripts for use with `zsh`. Put them somewhere in your `fpath`, and they should auto-load when you open a shell.
- **`include/`:**  
  Header files for the respective source files in `src`.
- **`man/`:**  
  Manual pages to be read with `man`, written in the [`mandoc`](https://mandoc.bsd.lv) dialect.
- **`src/`:**  
  Source code of RGBDS.
  * **`asm/`:**  
    Source code of RGBASM.
  * **`extern/`:**  
    Source code copied from external sources.
  * **`fix/`:**  
    Source code of RGBFIX.
  * **`gfx/`:**  
    Source code of RGBGFX.
  * **`link/`:**  
    Source code of RGBLINK.
  * **`CMakeLists.txt`:**  
    Defines how to build individual RGBDS programs with CMake, including the source files that each program depends on.
  * **`bison.sh`:**  
    Script used to run the Bison parser generator with the latest flags that the user's version supports.
- **`test/`:**  
  Testing framework used to verify that changes to the code don't break or modify the behavior of RGBDS.
  * **`fetch-test-deps.sh`:**  
    Script used to fetch dependencies for building external repositories. `fetch-test-deps.sh --help` describes its options.
  * **`run-tests.sh`:**  
    Script used to run tests, including internal test cases and external repositories. `run-tests.sh --help` describes its options.
- **`.clang-format`:**  
  Code style for automated C++ formatting with [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html) (for which we define the shortcut `make format`).
- **`.clang-tidy`:**  
  Configuration for C++ static analysis with [`clang-tidy`](https://clang.llvm.org/extra/clang-tidy/) (for which we define the shortcut `make tidy`).
- **`CMakeLists.txt`:**  
  Defines how to build RGBDS with CMake.
- **`compile_flags.txt`:**  
  Compiler flags for `clang-tidy`.
- **`Dockerfile`:**  
  Defines how to build RGBDS with Docker.
- **`Makefile`:**  
  Defines how to build RGBDS with `make`, including the source files that each program depends on.

## RGBDS

These files are in the `src/` directory. They apply to more than one program, usually all four of them.

- **`backtrace.cpp`:**  
  Generic printing of location backtraces for RGBASM and RGBLINK. Allows configuring backtrace styles with a command-line flag (conventionally `-B/--backtrace`). Renders warnings in yellow, errors in red, and locations in cyan.
- **`diagnostics.cpp`:**  
  Generic warning/error diagnostic support for all programs. Allows command-line flags (conventionally `-W`) to have `no-`, `error=`, or `no-error=` prefixes, `=` level suffixes; allows "meta" flags to affect groups of individual flags; and counts how many total errors there have been. Every program has its own `warning.cpp` file that uses this.
- **`linkdefs.cpp`:**  
  Constants, data, and functions related to RGBDS object files, which are used for RGBASM output and RGBLINK input.  
  This file defines two *global* variables, `sectionTypeInfo` (metadata about each section type) and `sectionModNames` (names of section modifiers, for error reporting). RGBLINK may change some values in `sectionTypeInfo` depending on its command-line options.
- **`opmath.cpp`:**  
  Functions for mathematical operations in RGBASM and RGBLINK that aren't trivially equivalent to built-in C++ ones.
- **`style.cpp`:**  
  Generic printing of cross-platform colored or bold text. Obeys the `FORCE_COLOR` or `NO_COLOR` envionment variables, and allows configuring with a command-line flag (conventionally `--color`).
- **`usage.cpp`:**  
  Generic printing of usage information. Renders headings in green, flags in cyan, and URLs in blue. Every program has its own `main.cpp` file that uses this.
- **`util.cpp`:**  
  Utility functions applicable to most programs, mostly dealing with text strings.
- **`verbosity.cpp`:**  
  Generic printing of messages conditionally at different verbosity levels. Allows configuring with a command-line flag (conventionally `-v/--verbose`).
- **`version.cpp`:**  
  RGBDS version number and string for all the programs.

## External

These source files are in the `src/extern/` directory. They have been copied from external authors and adapted for use with RGBDS.

- **`getopt.cpp`:**  
  Functions for parsing command-line options, including conventional single-dash and double-dash options.  
  This file defines some *global* `musl_opt*` variables, including `musl_optarg` (the argument given after an option flag) and `musl_optind` (the index of the next option in `argv`). Copied from [musl libc](https://musl.libc.org/).
- **`utf8decoder.cpp`:**  
  Function for decoding UTF-8 bytes into Unicode code points. Copied from [Björn Höhrmann](https://bjoern.hoehrmann.de/utf-8/decoder/dfa/).

## RGBASM

- **`actions.cpp`:**  
  Actions taken by the assembly language parser, to avoid large amounts of code going in the parser.y file.
- **`charmap.cpp`:**  
  Functions and data related to charmaps.  
  This file *owns* the `Charmap`s in its `charmaps` collection. It also maintains a static `currentCharmap` pointer, and a `charmapStack` stack of pointers to `Charmap`s within `charmaps` (which is affected by `PUSHC` and `POPC` directives).
- **`fixpoint.cpp`:**  
  Functions for fixed-point math.
- **`format.cpp`:**  
  `FormatSpec` methods for parsing and applying format specs, as used by `{interpolations}` and `STRFMT`.
- **`fstack.cpp`:**  
  Functions and data related to "fstack" nodes (the contents of top-level or `INCLUDE`d files, macro expansions, or `REPT`/`FOR` loop iterations) and their "contexts" (metadata that is only relevant while a node's content is being lexed and parsed).  
  This file *owns* the `Context`s in its `contextStack` collection. Each of those `Context`s *owns* its `LexerState`, and *refers* to its `FileStackNode`, `uniqueIDStr`, and `macroArgs`. and  Each `FileStackNode` also *references* its `parent`.
- **`lexer.cpp`:**  
  Functions and data related to [lexing](https://en.wikipedia.org/wiki/Lexical_analysis) assembly source code into tokens, which can then be parsed.  
  This file maintains static `lexerState` and `lexerStateEOL` pointers to `LexerState`s from the `Context`s in `fstack.cpp`.  
  Each `LexerState` *owns* its `content` and its `expansions`' content. Each `Expansion` (the contents of an `{interpolation}` or macro argument) in turn *owns* its `contents`.  
  The lexer and parser and interdependent: when the parser reaches certain tokens, it changes the lexer's mode, which affects how characters get lexed into tokens. For example, when the parser reaches a macro name, it changes the lexer to "raw" mode, which lexes the rest of the line as a sequence of string arguments to the macro.
- **`macro.cpp`:**  
  `MacroArgs` methods related to macro arguments. Each `MacroArgs` *references* its arguments' contents.
- **`main.cpp`:**  
  The `main` function for running RGBASM, including the intial handling of command-line options.  
  This file defines a *global* `options` variable with the parsed CLI options.
- **`opt.cpp`:**  
  Functions for parsing options specified by `OPT` or by certain command-line options.  
  This file *owns* the `OptStackEntry`s in its `stack` collection (which is affected by `PUSHO` and `POPO` directives).
- **`output.cpp`:**  
  Functions and data related to outputting object files (with `-o/--output`) and state files (with `-s/--state`).  
  This file *owns* its `assertions` (created by `ASSERT` and `STATIC_ASSERT` directives).  Every assertion gets output in the object file.
  This file also *references* some `fileStackNodes`, and maintains static pointers to `Symbol`s in `objectSymbols`. Only the "registered" symbols and fstack nodes get output in the object file. The `fileStackNodes` and `objectSymbols` collections keep track of which nodes and symbols have been registered for output.
- **`parser.y`:**  
  Grammar for the RGBASM assembly language, which Bison preprocesses into an [LR(1) parser](https://en.wikipedia.org/wiki/Canonical_LR_parser).  
  The Bison-generated parser calls `yylex` (defined in `lexer.cpp`) to get the next token, and calls `yywrap` (defined in `fstack.cpp`) when the current context is out of tokens and returns `EOF`.
- **`rpn.cpp`:**  
  `Expression` methods and data related to "[RPN](https://en.wikipedia.org/wiki/Reverse_Polish_notation)" expressions. When a numeric expression is parsed, if its value cannot be calculated at assembly time, it is built up into a buffer of RPN-encoded operations to do so at link time by RGBLINK.
- **`section.cpp`:**  
  Functions and data related to `SECTION`s.  
  This file *owns* the `Section`s in its `sections` collection. It also maintains various static pointers to those sections, including the `currentSection`, `currentLoadSection`, and `sectionStack` (which is affected by `PUSHS` and `POPS` directives). (Note that sections cannot be deleted.)
- **`symbol.cpp`:**  
  Functions and data related to symbols (labels, constants, variables, string constants, macros, etc).  
  This file *owns* the `Symbol`s in its `symbols` collection, and the various built-in ones outside that collection (`PCSymbol` for "`@`", `NARGSymbol` for "`_NARG`", etc). It also maintains a static `purgedSymbols` collection to remember which symbol names have been `PURGE`d from `symbols`, for error reporting purposes.
- **`warning.cpp`:**  
  Functions and data for warning and error output.  
  This file defines a *global* `warnings` variable using the `diagnostics.cpp` code for RGBASM-specific warning flags.

## RGBFIX

- **`fix.cpp`:**  
  Functions for fixing the ROM header.
- **`main.cpp`:**  
  The `main` function for running RGBFIX, including the intial handling of command-line options.  
  This file defines a *global* `options` variable with the parsed CLI options.
- **`mbc.cpp`:**  
  Functions and data related to [MBCs](https://gbdev.io/pandocs/MBCs.html), including the names of known MBC values.
- **`warning.cpp`:**  
  Functions and data for warning and error output.  
  This file defines a *global* `warnings` variable using the `diagnostics.cpp` code for RGBFIX-specific warning flags.

## RGBGFX

- **`color_set.cpp`:**  
  `ColorSet` methods for creating and comparing sets of colors. A color set includes the unique colors used by a single tile, and these sets are then packed into palettes.
- **`main.cpp`:**  
  The `main` function for running RGBGFX, including the intial handling of command-line options.  
  This file defines a *global* `options` variable with the parsed CLI options.
- **`pal_packing.cpp`:**  
  Functions for packing color sets into palettes. This is done with an ["overload-and-remove" heuristic](https://arxiv.org/abs/1605.00558) for a pagination algorithm.
- **`pal_sorting.cpp`:**  
  Functions for sorting colors within palettes, which works differently for grayscale, RGB, or idnexed-color palettes.
- **`pal_spec.cpp`:**  
  Functions for parsing various formats of palette specifications (from `-c/--colors`).
- **`png.cpp`:**  
  `Png` methods for reading PNG image files, standardizing them to 8-bit RGBA pixels while also reading their indexed palette if there is one.
- **`process.cpp`:**  
  Functions related to generating and outputting files (tile data, palettes, tilemap, attribute map, and/or palette map).
- **`reverse.cpp`:**  
  Functions related to reverse-generating RGBGFX outputs into a PNG file (for `-r/--reverse`).
- **`rgba.cpp`:**  
  `Rgba` methods related to RGBA colors and their 8-bit or 5-bit representations.
- **`warning.cpp`:**  
  Functions and data for warning and error output.  
  This file defines a *global* `warnings` variable using the `diagnostics.cpp` code for RGBGFX-specific warning flags.

## RGBLINK

- **`assign.cpp`:**  
  Functions and data for assigning `SECTION`s to specific banks and addresses.  
  This file *owns* the `memory` table of free space: each section type is associated with a list of each bank's free address ranges, which are allocated to sections using a [first-fit decreasing](https://en.wikipedia.org/wiki/Bin_packing_problem#First-fit_algorithm) bin-packing algorithm.
- **`fstack.cpp`:**  
  Functions related to "fstack" nodes (the contents of top-level or `INCLUDE`d files, macro expansions, or `REPT`/`FOR` loop iterations) read from the object files. At link time, these nodes are only needed for printing of location backtraces.
- **`layout.cpp`:**  
  Actions taken by the linker script parser, to avoid large amounts of code going in the script.y file.  
  This file maintains some static data about the current bank and address layout, which get checked and updated for consistency as the linker script is parsed.
- **`lexer.cpp`:**  
  Functions and data related to [lexing](https://en.wikipedia.org/wiki/Lexical_analysis) linker script files into tokens, which can then be parsed.  
  This file *owns* the `LexerStackEntry`s in its `lexerStack` collection. Each of those `LexerStackEntry`s *owns* its `file`. The stack is updated as linker scripts can `INCLUDE` other linker script pieces.  
  The linker script lexer is simpler than the RGBASM one, and does not have modes.
- **`main.cpp`:**  
  The `main` function for running RGBLINK, including the intial handling of command-line options.  
  This file defines a *global* `options` variable with the parsed CLI options.
- **`object.cpp`:**  
  Functions and data for reading object files generated by RGBASM.  
  This file *owns* the `Symbol`s in its `symbolLists` collection, and the `FileStackNode`s in its `nodes` collection.
- **`output.cpp`:**  
  Functions and data related to outputting ROM files (with `-o/--output`), symbol files (with `-n/--sym`), and map files (with `-m/--map`).  
  This file *references* some `Symbol`s and `Section`s, in collections that keep them sorted by address and name, which allows the symbol and map output to be in order.
- **`patch.cpp`:**  
  Functions and data related to "[RPN](https://en.wikipedia.org/wiki/Reverse_Polish_notation)" expression patches read from the object files, including the ones for `ASSERT` conditions. After sections have been assigned specific locations, the RPN patches can have their values calculated and applied to the ROM.  
  This file *owns* the `Assertion`s in its `assertions` collection, and the `RPNStackEntry`s in its `rpnStack` collection.
- **`script.y`:**  
  Grammar for the linker script language, which Bison preprocesses into an [LR(1) parser](https://en.wikipedia.org/wiki/Canonical_LR_parser).  
  The Bison-generated parser calls `yylex` (defined in `lexer.cpp`) to get the next token, and calls `yywrap` (also defined in `lexer.cpp`) when the current context is out of tokens and returns `EOF`.
- **`sdas_obj.cpp`:**  
  Functions and data for reading object files generated by [GBDK with SDCC](https://gbdk.org/). RGBLINK support for these object files is incomplete.
- **`section.cpp`:**  
  Functions and data related to `SECTION`s read from the object files.  
  This file *owns* the `Section`s in its `sections` collection.
- **`symbol.cpp`:**  
  Functions and data related to symbols read from the object files.  
  This file *references* the `Symbol`s in its `symbols` and `localSymbols` collections, which allow accessing symbols by name.
- **`warning.cpp`:**  
  Functions and data for warning and error output.  
  This file defines a *global* `warnings` variable using the `diagnostics.cpp` code for RGBLINK-specific warning flags.
