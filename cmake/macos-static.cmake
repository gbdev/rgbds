# This file is meant to be included at the project level,
# in order to generate executables compatible with old macOS versions.
# See our `macos-static` CMake preset for how it's meant to be used.

# The `-mmacosx-version-min=10.9` flag ensures that the binary only uses APIs available on Mac OS X 10.9 Mavericks.
# The `-arch` flags build a "fat binary" that works on both Apple architectures:
# older Intel x64 Macs and newer ARM "Apple Silicon" ones.
set("-mmacosx-version-min=10.9 -arch x86_64 -arch arm64")
set(CMAKE_C_FLAGS "${secret_sauce}" CACHE STRING "Flags used by the C compiler during all build types.")
set(CMAKE_CXX_FLAGS "${secret_sauce}" CACHE STRING "Flags used by the CXX compiler during all build types.")

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
