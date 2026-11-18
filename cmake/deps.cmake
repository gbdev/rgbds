# This file declares the dependencies we use, using `FetchContent`.
# https://cmake.org/cmake.help/latest/guide/using-dependencies/index.html#downloading-and-building-from-source-with-fetchcontent
# These are kept in a separate file so that it can be hashed as a key for our CI's `actions/cache`.

FetchContent_Declare(PNG
                     URL https://downloads.sourceforge.net/project/libpng/libpng16/1.6.59/libpng-1.6.59.tar.xz
                     URL_HASH SHA256=d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389
                     EXCLUDE_FROM_ALL # We only install the runtime dependencies, and do so separately.
                     FIND_PACKAGE_ARGS 1.5.4)

set(PNG_TESTS OFF CACHE INTERNAL "") # We do not need libpng's tests or tools
set(PNG_TOOLS OFF CACHE INTERNAL "") # (and they can even cause compile errors!)
set(PNG_SHARED OFF CACHE INTERNAL "")
set(PNG_STATIC ON CACHE INTERNAL "")

FetchContent_Declare(ZLIB
                     URL https://www.zlib.net/zlib-1.4.tar.xz
                     URL_HASH SHA256=4f11c512053d7bab8d9e9b89bde3575add3e4c50a383b42ef6cb837035fc5a7c
                     EXCLUDE_FROM_ALL # We only install the runtime dependencies, and do so separately.
# libpng documents requiring "zlib 1.0.4 or later (1.2.13 or later recommended for performance and security reasons)".
# We thus enforce 1.0.4, but note that the libpng source code mentions that "it may work with versions as old as zlib 0.95".
                     FIND_PACKAGE_ARGS 1.0.4)

set(ZLIB_BUILD_SHARED OFF CACHE INTERNAL "")
set(ZLIB_BUILD_STATIC ON CACHE INTERNAL "")
