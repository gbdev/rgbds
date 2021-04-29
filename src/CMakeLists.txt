#
# This file is part of RGBDS.
#
# Copyright (c) 2020 RGBDS contributors.
#
# SPDX-License-Identifier: MIT
#

set(common_src
    "extern/err.c"
    "extern/getopt.c"
    "version.c"
    )

find_package(PkgConfig)
if(MSVC OR NOT PKG_CONFIG_FOUND)
  # fallback to find_package
  find_package(PNG REQUIRED)
else()
  pkg_check_modules(LIBPNG REQUIRED libpng)
endif()

find_package(BISON REQUIRED)
set(BISON_FLAGS "-Wall")
# Set sompe optimization flags on versions that support them
if(BISON_VERSION VERSION_GREATER_EQUAL "3.5")
  set(BISON_FLAGS "${BISON_FLAGS} -Dapi.token.raw=true")
endif()
if(BISON_VERSION VERSION_GREATER_EQUAL "3.6")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=detailed")
elseif(BISON_VERSION VERSION_GREATER_EQUAL "3.0")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=verbose")
endif()
if(BISON_VERSION VERSION_GREATER_EQUAL "3.0")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.lac=full")
  set(BISON_FLAGS "${BISON_FLAGS} -Dlr.type=ielr")
endif()
BISON_TARGET(PARSER "asm/parser.y"
             "${PROJECT_SOURCE_DIR}/src/asm/parser.c"
             COMPILE_FLAGS "${BISON_FLAGS}"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/asm/parser.h"
             )

set(rgbasm_src
    "${BISON_PARSER_OUTPUT_SOURCE}"
    "asm/charmap.c"
    "asm/fixpoint.c"
    "asm/format.c"
    "asm/fstack.c"
    "asm/lexer.c"
    "asm/macro.c"
    "asm/main.c"
    "asm/opt.c"
    "asm/output.c"
    "asm/rpn.c"
    "asm/section.c"
    "asm/string.c"
    "asm/symbol.c"
    "asm/util.c"
    "asm/warning.c"
    "extern/utf8decoder.c"
    "hashmap.c"
    "linkdefs.c"
    "opmath.c"
    )

set(rgbfix_src
    "fix/main.c"
    )

set(rgbgfx_src
    "gfx/gb.c"
    "gfx/main.c"
    "gfx/makepng.c"
    )

set(rgblink_src
    "link/assign.c"
    "link/main.c"
    "link/object.c"
    "link/output.c"
    "link/patch.c"
    "link/script.c"
    "link/section.c"
    "link/symbol.c"
    "hashmap.c"
    "linkdefs.c"
    "opmath.c"
    )

foreach(PROG "asm" "fix" "gfx" "link")
  add_executable(rgb${PROG}
                 ${rgb${PROG}_src}
                 ${common_src}
                 )
  install(TARGETS rgb${PROG} RUNTIME DESTINATION bin)
endforeach()

set(MANDIR "share/man")
set(man1 "asm/rgbasm.1"
         "fix/rgbfix.1"
         "gfx/rgbgfx.1"
         "link/rgblink.1")
set(man5 "asm/rgbasm.5"
         "link/rgblink.5"
         "rgbds.5")
set(man7 "gbz80.7"
         "rgbds.7")

foreach(SECTION "man1" "man5" "man7")
  set(DEST "${MANDIR}/${SECTION}")
  install(FILES ${${SECTION}} DESTINATION ${DEST})
endforeach()

if(LIBPNG_FOUND) # pkg-config
  target_include_directories(rgbgfx PRIVATE ${LIBPNG_INCLUDE_DIRS})
  target_link_directories(rgbgfx PRIVATE ${LIBPNG_LIBRARY_DIRS})
  target_link_libraries(rgbgfx PRIVATE ${LIBPNG_LIBRARIES})
else()
  target_compile_definitions(rgbgfx PRIVATE ${PNG_DEFINITIONS})
  target_include_directories(rgbgfx PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(rgbgfx PRIVATE ${PNG_LIBRARIES})
endif()

include(CheckLibraryExists)
check_library_exists("m" "sin" "" HAS_LIBM)
if(HAS_LIBM)
  target_link_libraries(rgbasm PRIVATE "m")
endif()
