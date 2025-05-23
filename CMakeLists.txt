# SPDX-License-Identifier: MIT

# 3.9 required for LTO checks
# 3.17 optional for CMAKE_CTEST_ARGUMENTS
cmake_minimum_required(VERSION 3.9..3.17 FATAL_ERROR)

project(rgbds
        LANGUAGES CXX)

include(CTest)

# get real path of source and binary directories
get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

# reject in-source builds, may conflict with Makefile
if(srcdir STREQUAL bindir)
  message("RGBDS should not be built in the source directory.")
  message("Instead, create a separate build directory and specify to CMake the path to the source directory.")
  message(FATAL_ERROR "Terminating configuration")
endif()

option(SANITIZERS "Build with sanitizers enabled" OFF) # Ignored on MSVC
option(MORE_WARNINGS "Turn on more warnings" OFF) # Ignored on MSVC

if(MSVC)
  # MSVC's own standard library triggers warning C5105,
  # "macro expansion producing 'defined' has undefined behavior".
  # Warning C5030 is about unknown attributes (`[[gnu::ATTR]]`), none of ours being load-bearing.
  # Warning C4996 is about using POSIX names, which we want to do for portability.
  # We also opt into the C++20-conformant preprocessor.
  add_compile_options(/MP /wd5105 /wd5030 /wd4996 /Zc:preprocessor)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)

  if(SANITIZERS)
    set(SAN_FLAGS /fsanitize=address)
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})
  endif()
else()
  add_compile_options(-Wall -pedantic)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # C++20 allows macros to take zero variadic arguments, but Clang (aka AppleClang on macOS)
    # does not recognize this yet.
    add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
  endif()
  if(SANITIZERS)
    set(SAN_FLAGS -fsanitize=address -fsanitize=undefined
                  -fsanitize=float-divide-by-zero)
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})
    add_definitions(-D_GLIBCXX_ASSERTIONS)
    # A non-zero optimization level is desired in debug mode, but allow overriding it nonetheless
    set(CMAKE_CXX_FLAGS_DEBUG "-g -Og -fno-omit-frame-pointer -fno-optimize-sibling-calls ${CMAKE_CXX_FLAGS_DEBUG}"
                              CACHE STRING "" FORCE)
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

# Use versioning consistent with Makefile
# the git revision is used but uses the fallback in an archive

find_program(GIT git)
if(GIT)
  execute_process(COMMAND ${GIT} --git-dir=.git -c safe.directory='*' describe --tags --dirty --always
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  message(STATUS "RGBDS version: ${GIT_REV}")
else(GIT)
  message(STATUS "Cannot determine RGBDS version (Git not installed), falling back")
endif(GIT)

find_package(PkgConfig)
if(MSVC OR NOT PKG_CONFIG_FOUND)
  # fallback to find_package
  # cmake's FindPNG is very fragile; it breaks when multiple versions are installed
  # this is most evident on macOS but can occur on Linux too
  find_package(PNG REQUIRED)
else()
  pkg_check_modules(LIBPNG REQUIRED libpng)
endif()

include_directories("${PROJECT_SOURCE_DIR}/include")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
set(CMAKE_CTEST_ARGUMENTS "--verbose")
add_subdirectory(test)

# By default, build in Release mode; Debug mode must be explicitly requested
# (You may want to augment it with the options above)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(CHECK_START "Checking if LTO is supported")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT enable_lto)
  if(enable_lto)
    message(CHECK_PASS "yes")
    set_property(TARGET rgbasm rgblink rgbfix rgbgfx PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(CHECK_FAIL "no")
  endif()
endif()

set(MANDIR "share/man")
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
  set(DEST "${MANDIR}/${SECTION}")
  install(FILES ${${SECTION}} DESTINATION ${DEST})
endforeach()
