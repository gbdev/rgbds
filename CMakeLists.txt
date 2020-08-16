#
# This file is part of RGBDS.
#
# Copyright (c) 2020 RGBDS contributors.
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.0)
cmake_policy(VERSION 3.0)

project(rgbds
        LANGUAGES C)

option(DEVELOP "build in development mode" OFF)

set(DEFAULT_BUILD_TYPE "Release")
set(DEVELOP_BUILD_TYPE "Debug")
if(NOT CMAKE_BUILD_TYPE)
  if(DEVELOP)
    set(CMAKE_BUILD_TYPE "${DEVELOP_BUILD_TYPE}")
  else()
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
  endif()
endif()

# get real path of source and binary directories
get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

# reject in-source builds, may conflict with Makefile
if(srcdir STREQUAL bindir)
  message("RGBDS should not be built in the source directory.")
  message("Instead, create a separate build directory and specify to CMake the path to the source directory.")
  message(FATAL_ERROR "Terminating configuration")
endif()

find_package(PNG 1.2 REQUIRED)
find_package(BISON REQUIRED)
find_package(FLEX)

include_directories("${PROJECT_SOURCE_DIR}/include")

if(DEVELOP)
  add_definitions(-DDEVELOP)
endif()

if(MSVC)
  add_compile_options(/W1 /MP)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)
else()
  if(DEVELOP)
    add_compile_options(-Werror -Wall -Wextra -pedantic
                        -Wno-sign-compare -Wformat -Wformat-security -Wformat-overflow=2
                        -Wformat-truncation=1 -Wformat-y2k -Wswitch-enum -Wunused
                        -Wuninitialized -Wunknown-pragmas -Wstrict-overflow=5
                        -Wstringop-overflow=4 -Walloc-zero -Wduplicated-cond
                        -Wfloat-equal -Wshadow -Wcast-qual -Wcast-align -Wlogical-op
                        -Wnested-externs -Wno-aggressive-loop-optimizations -Winline
                        -Wundef -Wstrict-prototypes -Wold-style-definition)

     link_libraries(-fsanitize=shift -fsanitize=integer-divide-by-zero
                    -fsanitize=unreachable -fsanitize=vla-bound
                    -fsanitize=signed-integer-overflow -fsanitize=bounds
                    -fsanitize=object-size -fsanitize=bool -fsanitize=enum
                    -fsanitize=alignment -fsanitize=null)
  else()
    add_compile_options(-Wall -pedantic)
  endif()
endif()

# Use versioning consistent with Makefile
# the git revision is used but uses the fallback in an archive

execute_process(COMMAND git describe --tags --dirty --always
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)
string(STRIP "${GIT_REV}" GIT_REV)

add_definitions(-DBUILD_VERSION_STRING="${GIT_REV}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_subdirectory(src)
