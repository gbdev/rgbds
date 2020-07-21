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

# Lexer is not present yet
if(False) # FLEX_FOUND
  FLEX_TARGET(Lexer "asm/lexer.l"
              "${PROJECT_SOURCE_DIR}/src/asm/lexer.c"
              )
  ADD_FLEX_BISON_DEPENDENCY(Lexer ASMy)
  set(Lexer_SOURCE "${FLEX_Lexer_OUTPUTS}")
else()
  set(Lexer_SOURCE "asm/lexer.c")
endif()

set(rgbasm_src
    "${BISON_ASMy_OUTPUT_SOURCE}"
    "${Lexer_SOURCE}"
    "asm/charmap.c"
    "asm/fstack.c"
    "asm/globlex.c"
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

if(CMAKE_VERSION VERSION_LESS 2.8.12)
  add_definitions(${PNG_DEFINITIONS})
  include_directories(${PNG_INCLUDE_DIRS})
  target_link_libraries(rgbgfx ${PNG_LIBRARIES})
else()
  target_compile_definitions(rgbgfx PRIVATE ${PNG_DEFINITIONS})
  target_include_directories(rgbgfx PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(rgbgfx PRIVATE ${PNG_LIBRARIES})
endif()

include(CheckLibraryExists)
check_library_exists("m" "sin" "" HAS_LIBM)
if(HAS_LIBM)
  if(CMAKE_VERSION VERSION_LESS 2.8.12)
    target_link_libraries(rgbasm LINK_PRIVATE "m")
  else()
    target_link_libraries(rgbasm PRIVATE "m")
  endif()
endif()
