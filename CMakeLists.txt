# SPDX-License-Identifier: MIT

# - 3.24 is required for `FetchContent`'s `find_package` integration.
#   Older versions *may* work, but we can't test them; compat patches are welcome.
# - 3.17 is required for `CHECK_*` messages to display properly, but is not essential.
# - 3.9 is required for LTO checks.
cmake_minimum_required(VERSION 3.24...4.2 FATAL_ERROR)

# Read the project version from the header (the canonical source of truth).
file(STRINGS "include/version.hpp" version_defines REGEX "^[ \t]*#define[ \t]+PACKAGE_VERSION_")
foreach(line IN LISTS version_defines)
  # We want the `CMAKE_MATCH_n` variables, so we just need to run *some* regex op.
  string(REGEX MATCH "PACKAGE_(VERSION_[^ \t]+)[ \t]+([0-9]+)" dummy "${line}")
  set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
endforeach()

project(rgbds
        VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
        LANGUAGES CXX
        DESCRIPTION "Game Boy assembly toolchain"
        HOMEPAGE_URL "https://rgbds.gbdev.io")
if(DEFINED VERSION_RC)
  string(APPEND CMAKE_PROJECT_VERSION "-rc${VERSION_RC}")
endif()

# Reject in-source builds, as they may conflict with the Makefile.
get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)
if(srcdir STREQUAL bindir)
  message(FATAL_ERROR "RGBDS should not be built in the source directory.
Instead, create a separate build directory and specify to CMake the path to the source directory.")
endif()

include(CTest) # Note: CTest only functions properly if included from the top-level CMakeLists.
include(GNUInstallDirs)

## Compiler switches.

include(CMakeDependentOption)
option(SANITIZERS "Build with sanitizers enabled" OFF)
cmake_dependent_option(MORE_WARNINGS "Turn on more warnings" OFF !MSVC OFF)

if(MSVC)
  add_compile_options(
    /wd5030 # Warning C5030 is about unknown attributes (`[[gnu::ATTR]]`), none of ours being load-bearing.
    /wd4996 # Warning C4996 is about using POSIX names, which we want to do for portability.
    /Zc:preprocessor # Opt into the C++20-conformant preprocessor.
  )
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)

  if(SANITIZERS)
    message(STATUS "ASan enabled")
    add_compile_options(/fsanitize=address) # Note that this shouldn't be passed to the linker.
  endif()
else()
  add_compile_options(-Wall -pedantic -fno-exceptions $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> -Wno-unknown-warning-option
    # C++20 allows macros to take zero variadic arguments.
    # Some versions of Clang don't recognize this, and treat them as a GNU extension.
    -Wno-gnu-zero-variadic-macro-arguments)
  if(SANITIZERS)
    message(STATUS "ASan and UBSan enabled")
    set(SAN_FLAGS -fsanitize=address -fsanitize=undefined
                  -fsanitize=float-divide-by-zero)
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})
    add_definitions(-D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
    # A non-zero optimization level is desired even in debug mode (especially for Clang),
    # and the two codegen flags improve the sanitizers' backtraces, but we want the user to
    # be able to override these easily so we put them first.
    string(PREPEND CMAKE_CXX_FLAGS_DEBUG "-Og -fno-omit-frame-pointer -fno-optimize-sibling-calls ")
  endif()

  if(MORE_WARNINGS)
    add_compile_options(-Werror -Wextra
                        -Walloc-zero -Wcast-align -Wcast-qual -Wduplicated-branches -Wduplicated-cond
                        -Wfloat-equal -Wlogical-op -Wnull-dereference -Wold-style-cast -Wshift-overflow=2
                        -Wstringop-overflow=4 -Wtrampolines -Wundef -Wuninitialized -Wunused -Wshadow
                        -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=1
                        -Wno-format-nonliteral -Wno-strict-overflow
                        -Wno-unused-but-set-variable # bison's `yynerrs_` is incremented but unused
                        -Wno-type-limits -Wno-tautological-constant-out-of-range-compare
                        -Wvla # MSVC does not support VLAs
                        -Wno-unknown-warning-option) # Clang shouldn't diagnose unknown warnings
  endif()
endif()

message(CHECK_START "Checking if LTO is supported")
include(CheckIPOSupported)
check_ipo_supported(RESULT enable_lto)
if(enable_lto)
  message(CHECK_PASS "yes (enabled only in release modes)")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
else()
  message(CHECK_FAIL "no")
endif()

# Use versioning consistent with Makefile:
# the git revision is used but uses the fallback in an archive.

message(CHECK_START "Determining RGBDS version from Git history")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
set(GIT_REV "") # This fallback is important!
find_package(Git)
list(POP_BACK CMAKE_MESSAGE_INDENT)
if(NOT Git_FOUND)
  message(CHECK_FAIL "Git not found")
else()
  execute_process(COMMAND "${GIT_EXECUTABLE}" --git-dir=.git -c safe.directory='*'
                            describe --tags --dirty --always --match "v[0-9]*"
                  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                  OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_VARIABLE git_err ERROR_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    # Note that this happens e.g. when building from a tarball, so it shouldn't fail the build!
    message(CHECK_FAIL "error ${result} from Git:")
    list(APPEND CMAKE_MESSAGE_INDENT "     ")
    message("${git_err}")
    list(POP_BACK CMAKE_MESSAGE_INDENT)
  else()
    message(CHECK_PASS "${GIT_REV}")
    if(NOT "${GIT_REV}" MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+(-rc[0-9]+)?")
      # Can't find an ancestor tag! (That passes `--match`, anyway.)
      message(WARNING "No `v*` Git tag reachable; falling back")
    elseif(NOT CMAKE_MATCH_0 STREQUAL "v${CMAKE_PROJECT_VERSION}")
      message(SEND_ERROR "\
Version mismatch! Git says ${CMAKE_MATCH_0},
          version.hpp says v${CMAKE_PROJECT_VERSION}!")
    endif()
  endif()
endif()

## Dependencies.

include(FetchContent)
include(cmake/deps.cmake)

FetchContent_MakeAvailable(ZLIB)
if(NOT DEFINED ZLIB_INCLUDE_DIRS)
  set(ZLIB_INCLUDE_DIRS "${zlib_BINARY_DIR};${zlib_SOURCE_DIR}") # libpng's `genout` script relies on this variable to be set.
endif()

FetchContent_MakeAvailable(PNG)
if(NOT TARGET PNG::PNG)
  if(PNG_SHARED)
    add_library(PNG::PNG ALIAS png_shared)
  else()
    add_library(PNG::PNG ALIAS png_static)
  endif()
endif()

## The actual stuff.
# Any compiler options that shouldn't apply to our dependencies go here.

include_directories("include")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
add_subdirectory(test)

set(man1 "man/rgbasm.1"
         "man/rgbfix.1"
         "man/rgbgfx.1"
         "man/rgblink.1")
set(man5 "man/rgbasm.5"
         "man/rgbasm-old.5"
         "man/rgblink.5"
         "man/rgbds.5")
set(man7 "man/gbz80.7"
         "man/rgbds.7")

foreach(SECTION "man1" "man5" "man7")
  install(FILES ${${SECTION}} DESTINATION "${CMAKE_INSTALL_MANDIR}/${SECTION}")
endforeach()
