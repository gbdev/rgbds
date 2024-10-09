# SPDX-License-Identifier: MIT

OPTION(USE_NONFREE_TESTS "run tests that build nonfree codebases" ON)

if(NOT USE_NONFREE_TESTS)
  set(ONLY_FREE "--only-free")
endif()

add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)
set_target_properties(randtilegen rgbgfx_test PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gfx)

configure_file(CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake)

foreach(TARGET randtilegen rgbgfx_test)
  target_compile_definitions(${TARGET} PRIVATE ${PNG_DEFINITIONS})
  target_include_directories(${TARGET} PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(${TARGET} PRIVATE ${PNG_LIBRARIES})
endforeach()

if(UNIX)
  add_test(NAME all
           COMMAND "./run-tests.sh" ${ONLY_FREE}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()
