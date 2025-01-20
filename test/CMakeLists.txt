# SPDX-License-Identifier: MIT

option(USE_NONFREE_TESTS "run tests that build nonfree codebases" ON)
option(USE_EXTERNAL_TESTS "run tests that build external codebases" ON)

if(NOT USE_NONFREE_TESTS)
  set(ONLY_FREE "--only-free")
endif()
if(NOT USE_EXTERNAL_TESTS)
  set(ONLY_INTERNAL "--only-internal")
endif()

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

add_test(NAME all
         COMMAND ./run-tests.sh ${ONLY_FREE} ${ONLY_INTERNAL}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
