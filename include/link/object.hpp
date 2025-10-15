// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_OBJECT_HPP
#define RGBDS_LINK_OBJECT_HPP

#include <stddef.h>
#include <string>

// Read an object (.o) file, and add its info to the data structures.
void obj_ReadFile(std::string const &filePath, size_t fileID);

// Sets up object file reading
void obj_Setup(size_t nbFiles);

#endif // RGBDS_LINK_OBJECT_HPP
