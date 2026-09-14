# This file is meant to be included at the project level,
# in order to generate executables compatible with old macOS versions.
# See our `macos-static` CMake preset for how it's meant to be used.

# Note that targeting old enough versions of Mac OS X on recent enough versions of macOS
# triggers some poorly-tested code paths within Apple's linker, which then crashes.
# This can be worked around by using LLVM's LLD linker and passing `-fuse-ld=lld` when linking.

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.4 CACHE STRING "Minimum Mac OS X version to target for deployment (at runtime)")
# This builds a "fat binary" that works on both Apple architectures:
# older Intel x64 Macs and newer ARM "Apple Silicon" ones.
# Due to a libpng build script limitation/bug (as of 1.6.58), the native architecture has to be first...
# and since our CI builds this executable on an ARM machine, that's what we're putting first.
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64 CACHE STRING "Build architectures for Mac OS X")
# This controls the SIMD optimizations, which include architecture-specific headers that get rejected
# in a dual-arch build, thus we have to disable them.
# This shouldn't be a big deal for RGBGFX, anyway?
set(PNG_HARDWARE_OPTIMIZATIONS OFF)

# Mac OS X has always provided zlib, so we can safely link dynamically against it.
# However, libpng is *not* provided by default, so we link it statically, which requires downloading and building it from source.
set(PNG_SHARED OFF)
set(PNG_STATIC ON)
# If libpng is already available (e.g. via Homebrew), we ignore that and still build our own.
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
# But we still want to attempt linking against the system's zlib.
function(rgbds_provide_dependency method dep_name)
  if(dep_name STREQUAL "ZLIB")
    find_package(ZLIB)
    if(ZLIB_FOUND)
      FetchContent_SetPopulated(ZLIB)
    endif()
  endif()
endfunction(rgbds_provide_dependency)
cmake_language(SET_DEPENDENCY_PROVIDER rgbds_provide_dependency
               SUPPORTED_METHODS FETCHCONTENT_MAKEAVAILABLE_SERIAL)
