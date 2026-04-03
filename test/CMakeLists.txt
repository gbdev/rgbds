# SPDX-License-Identifier: MIT

option(TESTS_RUN_NONFREE "Run tests that build nonfree codebases." ON)
option(TESTS_RUN_EXTERNAL "Run tests that build external codebases." ON)
set(TESTS_OS_NAME "" CACHE STRING "Skip running tests known to fail on this OS.")

add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)
set_target_properties(randtilegen rgbgfx_test PROPERTIES
# hack for MSVC: no-op generator expression to stop generation of "per-configuration subdirectory"
                      RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_SOURCE_DIR}/gfx>)
foreach(prog  randtilegen rgbgfx_test)
  target_link_libraries(${prog} PRIVATE PNG::PNG)
  # Copy the DLLs in the output directory so the program can be run for testing without having to `install`.
  # From https://cmake.org/cmake/help/v4.3/manual/cmake-generator-expressions.7.html#genex:TARGET_RUNTIME_DLLS.
  add_custom_command(TARGET ${prog} POST_BUILD COMMENT "Copying gfx test programs' DLLs"
                     # It would be nice to symlink instead, but that only supports one file at a time.
                     COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${prog}> $<TARGET_RUNTIME_DLLS:${prog}>
                     # FIXME: this would be nice (so DLLs get removed by `make clean`), but CMake complains about the target not existing...
                     #BYPRODUCTS $<LIST:TRANSFORM,$<PATH:GET_FILENAME,$<TARGET_RUNTIME_DLLS:${prog}>>,PREPEND,$<TARGET_FILE_DIR:${prog}>/>
                     COMMAND_EXPAND_LISTS VERBATIM)
endforeach()

set(ONLY_FREE)
if(NOT TESTS_RUN_NONFREE)
  set(ONLY_FREE "--only-free")
endif()

set(ONLY_INTERNAL)
if(NOT TESTS_RUN_EXTERNAL)
  set(ONLY_INTERNAL "--only-internal")
endif()

set(OS_NAME)
if(NOT TESTS_OS_NAME STREQUAL "")
  set(OS_NAME "--os" "${TESTS_OS_NAME}")
endif()

configure_file(CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake @ONLY)

add_test(NAME all
         COMMAND ./run-tests.sh ${ONLY_FREE} ${ONLY_INTERNAL} ${OS_NAME}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
