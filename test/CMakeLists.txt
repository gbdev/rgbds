# SPDX-License-Identifier: MIT

option(TESTS_RUN_NONFREE "Run tests that build nonfree codebases." ON)
option(TESTS_RUN_EXTERNAL "Run tests that build external codebases." ON)
set(TESTS_OS_NAME "" CACHE STRING "Skip running tests known to fail on this OS.")

add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)
set_target_properties(randtilegen rgbgfx_test PROPERTIES
# hack for MSVC: no-op generator expression to stop generation of "per-configuration subdirectory"
                      RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_SOURCE_DIR}/gfx>)

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
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
