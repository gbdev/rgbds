# This file declares the dependencies we use, using `FetchContent`.
# https://cmake.org/cmake.help/latest/guide/using-dependencies/index.html#downloading-and-building-from-source-with-fetchcontent
# These are kept in a separate file so that it can be hashed as a key for our CI's `actions/cache`.

FetchContent_Declare(PNG
                     URL https://download.sourceforge.net/libpng/libpng-1.6.55.tar.xz
                     URL_HASH SHA256=d925722864837ad5ae2a82070d4b2e0603dc72af44bd457c3962298258b8e82d
                     EXCLUDE_FROM_ALL # We only install the runtime dependencies, and do so separately.
                     FIND_PACKAGE_ARGS 1.5.4)

set(PNG_TESTS OFF CACHE INTERNAL "") # We do not care for these two (and they can even cause compile errors!)
set(PNG_TOOLS OFF CACHE INTERNAL "")
set(PNG_SHARED ON CACHE INTERNAL "") # Upstream seems to favour the dynamic lib over the static one?
set(PNG_STATIC OFF CACHE INTERNAL "")

FetchContent_Declare(ZLIB
                     URL https://www.zlib.net/zlib-1.3.2.tar.xz
                     URL_HASH SHA256=d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3
                     EXCLUDE_FROM_ALL # We only install the runtime dependencies, and do so separately.
# libpng documents requiring "zlib 1.0.4 or later (1.2.13 or later recommended for performance and security reasons)".
# We thus enforce 1.0.4, but note that the libpng source code mentions that "it may work with versions as old as zlib 0.95".
                     FIND_PACKAGE_ARGS 1.0.4)

set(ZLIB_BUILD_SHARED ON CACHE INTERNAL "")
set(ZLIB_BUILD_STATIC OFF CACHE INTERNAL "")
