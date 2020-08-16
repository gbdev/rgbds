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

BISON_TARGET(ASMy "asm/asmy.y"
             "${PROJECT_SOURCE_DIR}/src/asm/asmy.c"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/asm/asmy.h"
             )

set(rgbasm_src
    "${BISON_ASMy_OUTPUT_SOURCE}"
    "asm/charmap.c"
    "asm/fstack.c"
    "asm/globlex.c"
    "asm/lexer.c"
    "asm/macro.c"
    "asm/main.c"
    "asm/math.c"
    "asm/output.c"
    "asm/rpn.c"
    "asm/section.c"
    "asm/symbol.c"
    "asm/util.c"
    "asm/warning.c"
    "extern/utf8decoder.c"
    "hashmap.c"
    "linkdefs.c"
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
    )

foreach(PROG "asm" "fix" "gfx" "link")
  add_executable(rgb${PROG}
                 ${rgb${PROG}_src}
                 ${common_src}
                 )
  install(TARGETS rgb${PROG} RUNTIME DESTINATION bin)
endforeach()

target_compile_definitions(rgbgfx PRIVATE ${PNG_DEFINITIONS})
target_include_directories(rgbgfx PRIVATE ${PNG_INCLUDE_DIRS})
target_link_libraries(rgbgfx PRIVATE ${PNG_LIBRARIES})

include(CheckLibraryExists)
check_library_exists("m" "sin" "" HAS_LIBM)
if(HAS_LIBM)
  target_link_libraries(rgbasm PRIVATE "m")
endif()
