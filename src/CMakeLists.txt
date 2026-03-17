# SPDX-License-Identifier: MIT

add_library(common OBJECT
    "extern/getopt.cpp"
    "cli.cpp"
    "diagnostics.cpp"
    "style.cpp"
    "usage.cpp"
    "util.cpp"
    "version.cpp"
)
target_compile_definitions(common PRIVATE "BUILD_VERSION_STRING=\"${GIT_REV}\"")

find_package(BISON 3.0.0 REQUIRED)
set(BISON_FLAGS "-Wall -Dlr.type=ielr")
# Set some flags on versions that support them
if(BISON_VERSION VERSION_GREATER_EQUAL "3.5")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.lac=full -Dapi.token.raw=true -Wdangling-alias")
endif()
if(BISON_VERSION VERSION_GREATER_EQUAL "3.6")
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=detailed")
else()
  set(BISON_FLAGS "${BISON_FLAGS} -Dparse.error=verbose")
endif()
if(BISON_VERSION VERSION_GREATER_EQUAL "3.7")
  set(BISON_FLAGS "${BISON_FLAGS} -Wcounterexamples")
endif()

BISON_TARGET(ASM_PARSER "asm/parser.y"
             "${PROJECT_SOURCE_DIR}/src/asm/parser.cpp"
             COMPILE_FLAGS "${BISON_FLAGS}"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/asm/parser.hpp"
)
add_executable(rgbasm $<TARGET_OBJECTS:common>
    "${BISON_ASM_PARSER_OUTPUT_SOURCE}"
    "asm/actions.cpp"
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
    "backtrace.cpp"
    "linkdefs.cpp"
    "opmath.cpp"
    "verbosity.cpp"
)

BISON_TARGET(LINKER_SCRIPT_PARSER "link/script.y"
             "${PROJECT_SOURCE_DIR}/src/link/script.cpp"
             COMPILE_FLAGS "${BISON_FLAGS}"
             DEFINES_FILE "${PROJECT_SOURCE_DIR}/src/link/script.hpp"
)
add_executable(rgblink $<TARGET_OBJECTS:common>
    "${BISON_LINKER_SCRIPT_PARSER_OUTPUT_SOURCE}"
    "link/assign.cpp"
    "link/fstack.cpp"
    "link/lexer.cpp"
    "link/layout.cpp"
    "link/main.cpp"
    "link/object.cpp"
    "link/output.cpp"
    "link/patch.cpp"
    "link/sdas_obj.cpp"
    "link/section.cpp"
    "link/symbol.cpp"
    "link/warning.cpp"
    "extern/utf8decoder.cpp"
    "backtrace.cpp"
    "linkdefs.cpp"
    "opmath.cpp"
    "verbosity.cpp"
)

add_executable(rgbfix $<TARGET_OBJECTS:common>
    "fix/fix.cpp"
    "fix/main.cpp"
    "fix/mbc.cpp"
    "fix/warning.cpp"
)

add_executable(rgbgfx $<TARGET_OBJECTS:common>
    "gfx/color_set.cpp"
    "gfx/main.cpp"
    "gfx/pal_packing.cpp"
    "gfx/pal_sorting.cpp"
    "gfx/pal_spec.cpp"
    "gfx/palette.cpp"
    "gfx/png.cpp"
    "gfx/process.cpp"
    "gfx/reverse.cpp"
    "gfx/rgba.cpp"
    "gfx/warning.cpp"
    "verbosity.cpp"
)

install(TARGETS rgbasm rgblink rgbfix rgbgfx RUNTIME)
# Tests expect the binaries to end up in-source; this is more acceptable than the entire artifact dir.
set_target_properties(rgbasm rgblink rgbfix rgbgfx PROPERTIES
# The generator expression (even if a no-op) stops muti-config generators using a of "per-configuration subdirectory".
                      RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}>)
target_link_libraries(rgbgfx PRIVATE PNG::PNG)
