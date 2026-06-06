# SPDX-License-Identifier: MIT

option(TESTS_RUN_NONFREE "Run tests that build nonfree codebases." ON)
set(TESTS_OS_NAME "" CACHE STRING "Skip running tests known to fail on this OS.")

add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)
set_target_properties(randtilegen rgbgfx_test PROPERTIES
# hack for MSVC: no-op generator expression to stop generation of "per-configuration subdirectory"
                      RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/gfx>")
foreach(prog "randtilegen" "rgbgfx_test")
  target_link_libraries(${prog} PRIVATE PNG::PNG)
  # Copy the DLLs in the output directory so the program can be run for testing without having to `install`.
  # From https://cmake.org/cmake/help/v4.3/manual/cmake-generator-expressions.7.html#genex:TARGET_RUNTIME_DLLS.
  add_custom_command(TARGET ${prog} POST_BUILD COMMENT "Copying ${prog}'s DLLs"
                     # It would be nice to symlink instead, but that only supports one file at a time.
                     COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${prog}> $<TARGET_RUNTIME_DLLS:${prog}>
                     COMMAND_EXPAND_LISTS VERBATIM)
endforeach()

set(ONLY_FREE)
if(NOT TESTS_RUN_NONFREE)
  set(ONLY_FREE "--only-free")
endif()

set(OS_NAME)
if(NOT TESTS_OS_NAME STREQUAL "")
  set(OS_NAME "--os" "${TESTS_OS_NAME}")
endif()

foreach(component "asm" "link" "fix" "gfx")
  add_test(NAME "rgb${component}"
           COMMAND bash -- test.sh
           WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${component}")
  set_tests_properties("rgb${component}" PROPERTIES REQUIRED_FILES "$<TARGET_FILE:rgb${component}>"
                                                    PROCESSORS 1
                                                    LABELS "rgb${component}")
endforeach()
set_tests_properties(rgbgfx PROPERTIES REQUIRED_FILES "$<TARGET_FILE:rgbgfx>;$<TARGET_FILE:randtilegen>;$<TARGET_FILE:rgbgfx_test>")

add_test(NAME fetch-test-deps
         COMMAND bash -- fetch-test-deps.sh ${ONLY_FREE}
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(fetch-test-deps PROPERTIES FIXTURES_SETUP "external-repos"
                                                LABELS "external")
add_test(NAME external
         COMMAND bash -- run-tests.sh --only-external ${ONLY_FREE} ${OS_NAME}
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(external PROPERTIES DEPENDS "rgbasm;rgblink;rgbfix;rgbgfx" # Only attempt building whole projects if each tool passes muster on its own.
                                         PROCESSORS 4
                                         FIXTURES_REQUIRED "external-repos"
                                         LABELS "external") # Allow filtering out external tests.
