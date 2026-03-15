# SPDX-License-Identifier: MIT
#
# CTest-based test suite for RGBDS.
#
# Each test case is registered as an individual CTest test, enabling:
#   - Parallel execution: ctest -j$(nproc)
#   - Filtering:          ctest -R "asm/charmap"  or  ctest -L gfx
#   - Ordering:           Smoke tests run first via FIXTURES
#   - Minimal output:     CTest only shows failures by default
#
# The original test.sh scripts are preserved for Makefile-based builds.

# Allow arbitrary characters (e.g. #) in test names
cmake_policy(SET CMP0110 NEW)

option(USE_NONFREE_TESTS "Run tests that build nonfree codebases" ON)
option(USE_EXTERNAL_TESTS "Run tests that build external codebases" ON)

if(NOT USE_NONFREE_TESTS)
  set(ONLY_FREE "--only-free")
endif()
if(NOT USE_EXTERNAL_TESTS)
  set(ONLY_INTERNAL "--only-internal")
endif()
if(DEFINED OS)
  set(OS_NAME "--os" "${OS}")
endif()

# =========================================================================
# Build test helper executables (gfx suite)
# =========================================================================
add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)
set_target_properties(randtilegen rgbgfx_test PROPERTIES
# hack for MSVC: no-op generator expression to stop generation of "per-configuration subdirectory"
                      RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_SOURCE_DIR}/gfx>)

configure_file(CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake)

foreach(TARGET randtilegen rgbgfx_test)
  if(LIBPNG_FOUND) # pkg-config
    target_include_directories(${TARGET} PRIVATE ${LIBPNG_INCLUDE_DIRS})
    target_link_directories(${TARGET} PRIVATE ${LIBPNG_LIBRARY_DIRS})
    target_link_libraries(${TARGET} PRIVATE ${LIBPNG_LIBRARIES})
  else()
    target_compile_definitions(${TARGET} PRIVATE ${PNG_DEFINITIONS})
    target_include_directories(${TARGET} PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(${TARGET} PRIVATE ${PNG_LIBRARIES})
  endif()
endforeach()

# =========================================================================
# Helpers
# =========================================================================
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Default per-test timeout (seconds)
set(INTERNAL_TEST_TIMEOUT 60)
set(EXTERNAL_TEST_TIMEOUT 600)

# Macro: register a test that runs a per-suite runner script.
# Sets TEST_SRCDIR so the runner can find test data.
macro(add_rgbds_test TEST_NAME WORKING_DIR)
  add_test(NAME "${TEST_NAME}"
           COMMAND ${ARGN}
           WORKING_DIRECTORY "${WORKING_DIR}")
  set_tests_properties("${TEST_NAME}" PROPERTIES
    ENVIRONMENT "TEST_SRCDIR=${WORKING_DIR}"
    TIMEOUT ${INTERNAL_TEST_TIMEOUT}
  )
endmacro()

# =========================================================================
# ASM TESTS
# =========================================================================

# --- Simple .asm tests ---
file(GLOB _asm_tests RELATIVE "${TEST_DIR}/asm" "${TEST_DIR}/asm/*.asm")
set(_all_asm_tests "")
foreach(_test_file ${_asm_tests})
  string(REGEX REPLACE "\\.asm$" "" _test_name "${_test_file}")
  set(_full_name "asm/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/asm"
    bash run-one-test.sh simple "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "asm;internal")
  list(APPEND _all_asm_tests "${_full_name}")
endforeach()

# notexist.asm — intentionally missing file test (not caught by glob)
add_rgbds_test("asm/notexist" "${TEST_DIR}/asm"
  bash run-one-test.sh simple notexist)
set_tests_properties("asm/notexist" PROPERTIES LABELS "asm;internal")
list(APPEND _all_asm_tests "asm/notexist")

# --- CLI tests ---
file(GLOB _asm_cli_tests RELATIVE "${TEST_DIR}/asm/cli" "${TEST_DIR}/asm/cli/*.flags")
foreach(_test_file ${_asm_cli_tests})
  string(REGEX REPLACE "\\.flags$" "" _test_name "${_test_file}")
  set(_full_name "asm/cli/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/asm"
    bash run-one-test.sh cli "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "asm;internal")
  list(APPEND _all_asm_tests "${_full_name}")
endforeach()

# --- Dependency tracking tests ---
foreach(_dep_test
    continues-after-missing-include
    exits-after-missing-include
    continues-after-missing-preinclude
    exits-after-missing-preinclude)
  set(_full_name "asm/dep/${_dep_test}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/asm"
    bash run-one-test.sh dep "${_dep_test}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "asm;internal")
  list(APPEND _all_asm_tests "${_full_name}")
endforeach()

# --- State-file test ---
add_rgbds_test("asm/state-file" "${TEST_DIR}/asm"
  bash run-one-test.sh state state-file)
set_tests_properties("asm/state-file" PROPERTIES LABELS "asm;internal")
list(APPEND _all_asm_tests "asm/state-file")

# =========================================================================
# LINK TESTS
# =========================================================================

set(_all_link_tests "")

# --- Simple .asm tests (generic loop) ---
file(GLOB _link_simple_tests RELATIVE "${TEST_DIR}/link" "${TEST_DIR}/link/*.asm")
foreach(_test_file ${_link_simple_tests})
  string(REGEX REPLACE "\\.asm$" "" _test_name "${_test_file}")
  set(_full_name "link/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/link"
    bash run-one-test.sh simple "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "link;internal")
  list(APPEND _all_link_tests "${_full_name}")
endforeach()

# --- Special sub-directory tests ---
# Tests with sub-sub-directories (fragment-align/*)
file(GLOB _fragment_align_tests RELATIVE "${TEST_DIR}/link"
     "${TEST_DIR}/link/fragment-align/*")
foreach(_test_dir ${_fragment_align_tests})
  set(_full_name "link/${_test_dir}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/link"
    bash run-one-test.sh special "${_test_dir}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "link;internal")
  list(APPEND _all_link_tests "${_full_name}")
endforeach()

# Flat special tests (single sub-directory)
foreach(_special_test
    bank-const
    constant-parent
    export-all
    fragment-literals
    high-low
    load-fragment/base
    load-fragment/multiple-objects
    load-fragment/section-fragment
    map-file
    overlay/smaller
    overlay/unfixed
    overlay/tiny
    pipeline
    rept-trace
    same-consts
    scramble-invalid
    scramble-romx
    script-include
    sdcc/good
    sdcc/no-script
    section-conflict/different-mod
    section-fragment/good
    section-fragment/jr-offset
    section-fragment/jr-offset-load
    section-normal/same-name
    section-union/compat
    section-union/good
    section-union/same-export
    section-union/same-label
    symbols/conflict
    symbols/good
    symbols/unknown
    truncation/level1
    truncation/level2)
  set(_full_name "link/${_special_test}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/link"
    bash run-one-test.sh special "${_special_test}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "link;internal")
  list(APPEND _all_link_tests "${_full_name}")
endforeach()

# --- Error tests (section-union/*.asm and section-fragment/*.asm) ---
file(GLOB _link_su_error_tests RELATIVE "${TEST_DIR}/link"
     "${TEST_DIR}/link/section-union/*.asm")
file(GLOB _link_sf_error_tests RELATIVE "${TEST_DIR}/link"
     "${TEST_DIR}/link/section-fragment/*.asm")
foreach(_test_file ${_link_su_error_tests} ${_link_sf_error_tests})
  string(REGEX REPLACE "\\.asm$" "" _test_name "${_test_file}")
  set(_full_name "link/error/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/link"
    bash run-one-test.sh error "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "link;internal")
  list(APPEND _all_link_tests "${_full_name}")
endforeach()

# =========================================================================
# FIX TESTS
# =========================================================================

set(_all_fix_tests "")

# --- Normal .flags tests ---
file(GLOB _fix_tests RELATIVE "${TEST_DIR}/fix" "${TEST_DIR}/fix/*.flags")
foreach(_test_file ${_fix_tests})
  string(REGEX REPLACE "\\.flags$" "" _test_name "${_test_file}")
  set(_full_name "fix/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/fix"
    bash run-one-test.sh normal "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "fix;internal")
  list(APPEND _all_fix_tests "${_full_name}")
endforeach()

# --- Special tests ---
foreach(_special no-exist no-input multiple-to-one)
  set(_full_name "fix/${_special}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/fix"
    bash run-one-test.sh special "${_special}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "fix;internal")
  list(APPEND _all_fix_tests "${_full_name}")
endforeach()

# --- Padding tests (10 iterations with deterministic byte values) ---
foreach(_i RANGE 0 9)
  math(EXPR _padding "${_i} * 25 + 7")  # deterministic spread: 7, 32, 57, ...
  set(_full_name "fix/padding-${_i}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/fix"
    bash run-one-test.sh padding padding "${_padding}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "fix;internal")
  list(APPEND _all_fix_tests "${_full_name}")
endforeach()

# =========================================================================
# GFX TESTS
# =========================================================================

set(_all_gfx_tests "")

# --- PNG tests ---
file(GLOB _gfx_png_files "${TEST_DIR}/gfx/*.png")
foreach(_png_file ${_gfx_png_files})
  get_filename_component(_fname "${_png_file}" NAME)

  # Skip result.png, *.pal.png, and out*.png (transient seed test output)
  if(_fname STREQUAL "result.png")
    continue()
  endif()
  string(FIND "${_fname}" ".pal.png" _pal_pos)
  if(NOT _pal_pos EQUAL -1)
    continue()
  endif()
  string(REGEX MATCH "^out[0-9]*\\.png$" _out_match "${_fname}")
  if(_out_match)
    continue()
  endif()

  string(REGEX REPLACE "\\.png$" "" _test_name "${_fname}")
  set(_full_name "gfx/${_test_name}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/gfx"
    bash run-one-test.sh png "${_test_name}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "gfx;internal")
  list(APPEND _all_gfx_tests "${_full_name}")
endforeach()

# --- Seed tests ---
file(GLOB _gfx_seed_files RELATIVE "${TEST_DIR}/gfx" "${TEST_DIR}/gfx/seed*.bin")
foreach(_seed_file ${_gfx_seed_files})
  set(_full_name "gfx/${_seed_file}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/gfx"
    bash run-one-test.sh seed "${_seed_file}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "gfx;internal")
  list(APPEND _all_gfx_tests "${_full_name}")
endforeach()

# --- Reverse tests ---
file(GLOB _gfx_reverse_files "${TEST_DIR}/gfx/*.[12]bpp")
foreach(_rev_file ${_gfx_reverse_files})
  get_filename_component(_fname "${_rev_file}" NAME)

  # Skip result.* and *.out.*
  string(FIND "${_fname}" "result." _res_pos)
  if(NOT _res_pos EQUAL -1)
    continue()
  endif()
  string(FIND "${_fname}" ".out." _out_pos)
  if(NOT _out_pos EQUAL -1)
    continue()
  endif()

  set(_full_name "gfx/reverse/${_fname}")
  add_rgbds_test("${_full_name}" "${TEST_DIR}/gfx"
    bash run-one-test.sh reverse "${_fname}")
  set_tests_properties("${_full_name}" PROPERTIES LABELS "gfx;internal")
  list(APPEND _all_gfx_tests "${_full_name}")
endforeach()

# --- Write-stdout test ---
add_rgbds_test("gfx/write-stdout" "${TEST_DIR}/gfx"
  bash run-one-test.sh stdout write_stdout)
set_tests_properties("gfx/write-stdout" PROPERTIES LABELS "gfx;internal")
list(APPEND _all_gfx_tests "gfx/write-stdout")

# =========================================================================
# EXTERNAL / DOWNSTREAM TESTS
# =========================================================================

set(_all_external_tests "")

if(USE_EXTERNAL_TESTS)
  set(_external_projects ucity libbet SameBoy gb-starter-kit)
  if(USE_NONFREE_TESTS)
    list(PREPEND _external_projects pokecrystal pokered LADX-Disassembly)
  endif()

  foreach(_project ${_external_projects})
    set(_full_name "external/${_project}")
    add_test(NAME "${_full_name}"
             COMMAND bash run-one-external.sh "${_project}"
             WORKING_DIRECTORY "${TEST_DIR}")
    set_tests_properties("${_full_name}" PROPERTIES
      ENVIRONMENT "TEST_SRCDIR=${TEST_DIR};OS_NAME=${OS}"
      TIMEOUT ${EXTERNAL_TEST_TIMEOUT}
      LABELS "external"
    )
    list(APPEND _all_external_tests "${_full_name}")
  endforeach()
endif()

# =========================================================================
# FIXTURES — ordering / dependency control
# =========================================================================

# Pick one smoke test per suite — if it fails, the rest of that suite
# is skipped (CTest FIXTURES_REQUIRED).

# ASM smoke: use "assert" if available as a basic sanity test
if("asm/assert" IN_LIST _all_asm_tests)
  set(_asm_smoke "asm/assert")
elseif(_all_asm_tests)
  list(GET _all_asm_tests 0 _asm_smoke)
else()
  set(_asm_smoke "")
endif()

if(_asm_smoke)
  set_tests_properties("${_asm_smoke}" PROPERTIES FIXTURES_SETUP ASM_SMOKE)
  foreach(_t ${_all_asm_tests})
    if(NOT "${_t}" STREQUAL "${_asm_smoke}")
      set_tests_properties("${_t}" PROPERTIES FIXTURES_REQUIRED ASM_SMOKE)
    endif()
  endforeach()
endif()

# Slow ASM tests that need a longer timeout
if("asm/section-unsigned-overflow" IN_LIST _all_asm_tests)
  set_tests_properties("asm/section-unsigned-overflow" PROPERTIES TIMEOUT 300)
endif()

# LINK smoke
if(_all_link_tests)
  list(GET _all_link_tests 0 _link_smoke)
  set_tests_properties("${_link_smoke}" PROPERTIES FIXTURES_SETUP LINK_SMOKE)
  foreach(_t ${_all_link_tests})
    if(NOT "${_t}" STREQUAL "${_link_smoke}")
      set_tests_properties("${_t}" PROPERTIES FIXTURES_REQUIRED LINK_SMOKE)
    endif()
  endforeach()
endif()

# FIX smoke
if(_all_fix_tests)
  list(GET _all_fix_tests 0 _fix_smoke)
  set_tests_properties("${_fix_smoke}" PROPERTIES FIXTURES_SETUP FIX_SMOKE)
  foreach(_t ${_all_fix_tests})
    if(NOT "${_t}" STREQUAL "${_fix_smoke}")
      set_tests_properties("${_t}" PROPERTIES FIXTURES_REQUIRED FIX_SMOKE)
    endif()
  endforeach()
endif()

# GFX smoke
if(_all_gfx_tests)
  list(GET _all_gfx_tests 0 _gfx_smoke)
  set_tests_properties("${_gfx_smoke}" PROPERTIES FIXTURES_SETUP GFX_SMOKE)
  foreach(_t ${_all_gfx_tests})
    if(NOT "${_t}" STREQUAL "${_gfx_smoke}")
      set_tests_properties("${_t}" PROPERTIES FIXTURES_REQUIRED GFX_SMOKE)
    endif()
  endforeach()
endif()

# External tests require all smoke fixtures to pass
if(_all_external_tests)
  set(_required_fixtures "")
  if(_asm_smoke)
    list(APPEND _required_fixtures "ASM_SMOKE")
  endif()
  if(_all_link_tests)
    list(APPEND _required_fixtures "LINK_SMOKE")
  endif()
  if(_all_fix_tests)
    list(APPEND _required_fixtures "FIX_SMOKE")
  endif()
  if(_all_gfx_tests)
    list(APPEND _required_fixtures "GFX_SMOKE")
  endif()
  if(_required_fixtures)
    foreach(_t ${_all_external_tests})
      set_tests_properties("${_t}" PROPERTIES
        FIXTURES_REQUIRED "${_required_fixtures}")
    endforeach()
  endif()
endif()
