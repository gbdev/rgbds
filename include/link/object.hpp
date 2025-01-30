// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_OBJECT_HPP
#define RGBDS_LINK_OBJECT_HPP

// Read an object (.o) file, and add its info to the data structures.
void obj_ReadFile(char const *fileName, unsigned int fileID);

// Sets up object file reading
void obj_Setup(unsigned int nbFiles);

#endif // RGBDS_LINK_OBJECT_HPP
