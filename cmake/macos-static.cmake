# This file is meant to be included at the project level,
# in order to generate executables compatible with old macOS versions.
# See our `macos-static` CMake preset for how it's meant to be used.

# The first of these flags ensures that the binary only uses APIs available on this old a macOS,
# and the `-arch` flags build a "fat binary" that works on both architectures.
# (Both older Intel Macs and the newer "Apple Silicon" ones.)
set("-mmacosx-version-min=10.9 -arch x86_64 -arch arm64")
set(CMAKE_C_FLAGS "${secret_sauce}" CACHE STRING "Flags used by the C compiler during all build types.")
set(CMAKE_CXX_FLAGS "${secret_sauce}" CACHE STRING "Flags used by the CXX compiler during all build types.")

# Here is a bit of context and rationale for the following actions.
# OSX has always provided zlib, so we can safely link dynamically against it.
# However, libpng is *not* provided by default, yet it might be available on the build host
# (e.g. via Homebrew); we instead link it statically.
set(PNG_SHARED OFF)
set(PNG_STATIC ON)
# This requires building it from source, which itself requires downloading it, and ignoring it on the host.
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
# But we still want to attempt linking against the system's zlib.
# (We let other deps, such as libpng, be handled normally.)
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
