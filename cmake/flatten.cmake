# This file is meant to flatten our install targets into a single top-level
# directory when packaged by CPack.

# Detect if the install is run by CPack.
if (${CMAKE_INSTALL_PREFIX} MATCHES "/_CPack_Packages/.*/(TGZ|ZIP)/")
  # Flatten the directory structure so that targets are placed at the top level.
  file(GLOB binaries LIST_DIRECTORIES FALSE "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/*")
  foreach(file IN LISTS binaries)
    get_filename_component(filename ${file} NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E rename ${file} "${CMAKE_INSTALL_PREFIX}/${filename}")
  endforeach()
  execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
endif()
