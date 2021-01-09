#
# This file is part of RGBDS.
#
# Copyright (c) 2020 RGBDS contributors.
#
# SPDX-License-Identifier: MIT
#

# 3.9 required for LTO checks
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(rgbds
        LANGUAGES C)

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
option(TRACE_PARSER "Trace parser execution" OFF)
option(TRACE_LEXER "Trace lexer execution" OFF)

if(MSVC)
  # MSVC's standard library triggers warning C5105,
  # "macro expansion producing 'defined' has undefined behavior"
  add_compile_options(/std:c11 /W1 /MP /wd5105)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -pedantic)
  if(SANITIZERS)
    set(SAN_FLAGS -fsanitize=shift -fsanitize=integer-divide-by-zero
                  -fsanitize=unreachable -fsanitize=vla-bound
                  -fsanitize=signed-integer-overflow -fsanitize=bounds
                  -fsanitize=object-size -fsanitize=bool -fsanitize=enum
                  -fsanitize=alignment -fsanitize=null)
    add_compile_options(${SAN_FLAGS})
    link_libraries(${SAN_FLAGS})
  endif()

  if(MORE_WARNINGS)
    add_compile_options(-Werror -Wextra -Wno-type-limits
                        -Wno-sign-compare -Wvla -Wformat -Wformat-security -Wformat-overflow=2
                        -Wformat-truncation=1 -Wformat-y2k -Wswitch-enum -Wunused
                        -Wuninitialized -Wunknown-pragmas -Wstrict-overflow=5
                        -Wstringop-overflow=4 -Walloc-zero -Wduplicated-cond
                        -Wfloat-equal -Wshadow -Wcast-qual -Wcast-align -Wlogical-op
                        -Wnested-externs -Wno-aggressive-loop-optimizations -Winline
                        -Wundef -Wstrict-prototypes -Wold-style-definition)
  endif()
endif()

# Use versioning consistent with Makefile
# the git revision is used but uses the fallback in an archive

execute_process(COMMAND git describe --tags --dirty --always
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)
string(STRIP "${GIT_REV}" GIT_REV)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_definitions(-DBUILD_VERSION_STRING="${GIT_REV}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_subdirectory(src)

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

if(TRACE_PARSER)
  target_compile_definitions(rgbasm PRIVATE -DYYDEBUG)
endif()

if(TRACE_LEXER)
  target_compile_definitions(rgbasm PRIVATE -DLEXER_DEBUG)
endif()
