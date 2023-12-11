# SPDX-License-Identifier: MIT

configure_file(version.cpp _version.cpp ESCAPE_QUOTES)

set(common_src
    "error.cpp"
    "extern/getopt.cpp"
    "_version.cpp"
    )

find_package(BISON 3.0.0 REQUIRED)
set(BISON_FLAGS "-Wall")
# Set some optimization flags on versions that support them
if(BISON_VERSION VERSION_GREATER_EQUAL "3.5")
  set(BISON_FLAGS "${BISON_FLAGS} -Dapi.token.raw=true")
endif()
if(BISON_VERSION VERSION_GREATER_EQUAL "3.6")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=detailed")
else()
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=verbose")
endif()
set(BISON_FLAGS "${BISON_FLAGS} -Dparse.lac=full")
set(BISON_FLAGS "${BISON_FLAGS} -Dlr.type=ielr")

BISON_TARGET(ASM_PARSER "asm/parser.y"
             "${PROJECT_SOURCE_DIR}/src/asm/parser.cpp"
             COMPILE_FLAGS "${BISON_FLAGS}"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/asm/parser.hpp"
             )

BISON_TARGET(LINKER_SCRIPT_PARSER "link/script.y"
             "${PROJECT_SOURCE_DIR}/src/link/script.cpp"
             COMPILE_FLAGS "${BISON_FLAGS}"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/link/script.hpp"
             )

set(rgbasm_src
    "${BISON_ASM_PARSER_OUTPUT_SOURCE}"
    "asm/charmap.cpp"
    "asm/fixpoint.cpp"
    "asm/format.cpp"
    "asm/fstack.cpp"
    "asm/lexer.cpp"
    "asm/macro.cpp"
    "asm/main.cpp"
    "asm/opt.cpp"
    "asm/output.cpp"
    "asm/rpn.cpp"
    "asm/section.cpp"
    "asm/symbol.cpp"
    "asm/warning.cpp"
    "extern/utf8decoder.cpp"
    "hashmap.cpp"
    "linkdefs.cpp"
    "opmath.cpp"
    "util.cpp"
    )

set(rgbfix_src
    "fix/main.cpp"
    )

set(rgbgfx_src
    "gfx/main.cpp"
    "gfx/pal_packing.cpp"
    "gfx/pal_sorting.cpp"
    "gfx/pal_spec.cpp"
    "gfx/process.cpp"
    "gfx/proto_palette.cpp"
    "gfx/reverse.cpp"
    "gfx/rgba.cpp"
    "extern/getopt.cpp"
    "error.cpp"
    )

set(rgblink_src
    "${BISON_LINKER_SCRIPT_PARSER_OUTPUT_SOURCE}"
    "link/assign.cpp"
    "link/main.cpp"
    "link/object.cpp"
    "link/output.cpp"
    "link/patch.cpp"
    "link/sdas_obj.cpp"
    "link/section.cpp"
    "link/symbol.cpp"
    "extern/utf8decoder.cpp"
    "hashmap.cpp"
    "linkdefs.cpp"
    "opmath.cpp"
    "util.cpp"
    )

foreach(PROG "asm" "fix" "gfx" "link")
  add_executable(rgb${PROG}
                 ${rgb${PROG}_src}
                 ${common_src}
                 )
  install(TARGETS rgb${PROG} RUNTIME DESTINATION bin)
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
