# SPDX-License-Identifier: MIT

if(DEFINED TESTS_OS_NAME)
  # Avoid re-computing the default value if the cache entry already exists.
elseif(APPLE)
  set(TESTS_OS_NAME "macos-${CMAKE_SYSTEM_VERSION}")
else()
  string(TOLOWER "${CMAKE_SYSTEM}" TESTS_OS_NAME)
endif()
set(TESTS_OS_NAME "${TESTS_OS_NAME}" CACHE STRING "Skip running tests known to fail on this OS.")

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

foreach(component "asm" "link" "fix" "gfx")
  add_test(NAME "rgb${component}"
           COMMAND bash -- test.sh
           WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${component}")
  set_tests_properties("rgb${component}" PROPERTIES REQUIRED_FILES "$<TARGET_FILE:rgb${component}>"
                                                    PROCESSORS 1
                                                    LABELS "internal;free")
endforeach()
set_tests_properties(rgbgfx PROPERTIES REQUIRED_FILES "$<TARGET_FILE:rgbgfx>;$<TARGET_FILE:randtilegen>;$<TARGET_FILE:rgbgfx_test>")

add_test(NAME fetch-free-deps
         COMMAND bash -- external/fetch-repos.sh --only-free
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(fetch-free-deps PROPERTIES FIXTURES_SETUP "free-repos"
                                                LABELS "external;free")
add_test(NAME fetch-nonfree-deps
         COMMAND bash -- external/fetch-repos.sh --only-nonfree
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(fetch-nonfree-deps PROPERTIES FIXTURES_SETUP "nonfree-repos"
                                                   LABELS "external;nonfree")

file(GLOB ext_projects CONFIGURE_DEPENDS "external/*.cfg")
foreach(cfg_file IN LISTS ext_projects)
  cmake_path(GET cfg_file STEM LAST_ONLY project) # Extract the project's name from the config file's name.

  # Parse the config file, using it to set some variables for more convenient access.
  file(STRINGS "${cfg_file}" proj_props REGEX "^[^# \t]*[^#]") # Ignore comments and empty lines.
  foreach(line IN LISTS proj_props)
    string(REGEX MATCH "^[ \t]*EXT_TEST_([A-Za-z0-9_]+)=(.+)\$" matched "${line}")
    if(NOT matched STREQUAL "")
      set(${project}_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
  endforeach()

  # Wire it up according to whether it's free (as in speech) or not.
  if(${project}_IS_NONFREE)
    set(freedom "nonfree")
  else()
    set(freedom "free")
  endif()

  add_test(NAME "${project}"
           COMMAND bash -- external/test.sh ${project}
           WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(${project} PROPERTIES DEPENDS "rgbasm;rgblink;rgbfix;rgbgfx" # Only attempt external tests after all individual tool tests.
                                             PROCESSORS 4
                                             LABELS "external;${freedom}"
                                             FIXTURES_REQUIRED "${freedom}-repos")
endforeach()

# gb-starter kit fails with any `make` on Windows: https://codeberg.org/ISSOtm/gb-starter-kit/issues/1
# gb-starter-kit fails with macOS/BSD `make`: https://codeberg.org/ISSOtm/gb-starter-kit/issues/29
if(TESTS_OS_NAME MATCHES "^(windows|macos|[^-]*bsd)(-.*)?")
  set_tests_properties(gb-starter-kit PROPERTIES DISABLED TRUE)
endif()
