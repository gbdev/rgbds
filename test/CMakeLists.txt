# SPDX-License-Identifier: MIT

add_executable(randtilegen gfx/randtilegen.cpp)
add_executable(rgbgfx_test gfx/rgbgfx_test.cpp)

install(TARGETS randtilegen rgbgfx_test
        DESTINATION ${rgbds_SOURCE_DIR}/test/gfx
        COMPONENT "Test support programs"
        EXCLUDE_FROM_ALL
        )

foreach(TARGET randtilegen rgbgfx_test)
  target_compile_definitions(${TARGET} PRIVATE ${PNG_DEFINITIONS})
  target_include_directories(${TARGET} PRIVATE ${PNG_INCLUDE_DIRS})
  target_link_libraries(${TARGET} PRIVATE ${PNG_LIBRARIES})
endforeach()
