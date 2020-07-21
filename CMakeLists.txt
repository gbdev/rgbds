#
# This file is part of RGBDS.
#
# Copyright (c) 2020 RGBDS contributors.
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 2.8.8)

cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

set(RGBDS_VER 0.4.1)
set(RGBDS_DESC "Rednex Game Boy Development System")

if(CMAKE_VERSION VERSION_LESS 3.0)
  project(rgbds C)
  set(PROJECT_VERSION "${RGBDS_VER}")
else()
  if(CMAKE_VERSION VERSION_LESS 3.9)
    project(rgbds VERSION "${RGBDS_VER}"
                  LANGUAGES C)
  else()
    project(rgbds VERSION "${RGBDS_VER}"
                  DESCRIPTION "${RGBDS_DESC}"
                  LANGUAGES C)
  endif()
endif()

if(CMAKE_VERSION VERSION_LESS 3.9)
  set(PROJECT_DESCRIPTION "${RGBDS_DESC}")
endif()

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
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

if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W1 /MP -D_CRT_SECURE_NO_WARNINGS")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")
endif()

if(CMAKE_VERSION VERSION_LESS 3.12)
  add_definitions(-DBUILD_VERSION_STRING="${PROJECT_VERSION}")
else()
  add_compile_definitions(BUILD_VERSION_STRING="${PROJECT_VERSION}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_subdirectory(src)
