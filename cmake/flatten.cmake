# This file is meant to flatten our install targets into a single top-level
# directory when packaged by CPack.

# Detect if the install is run by CPack.
if (${CMAKE_INSTALL_PREFIX} MATCHES "/_CPack_Packages/.*/(TGZ|ZIP)/")
  # Flatten the directory structure so that targets are placed at the top level.
  set(dirs "bin" "lib")
  set(files "")
  foreach(dir IN LISTS dirs)
    file(GLOB dir_files LIST_DIRECTORIES FALSE "${CMAKE_INSTALL_PREFIX}/${dir}/*")
    list(APPEND files ${dir_files})
  endforeach()
  foreach(file IN LISTS files)
    get_filename_component(filename ${file} NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E rename ${file} "${CMAKE_INSTALL_PREFIX}/${filename}")
  endforeach()
  foreach(dir IN LISTS dirs)
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}/${dir}")
  endforeach()
endif()
