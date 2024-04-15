/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_OBJECT_HPP
#define RGBDS_LINK_OBJECT_HPP

/*
 * Read an object (.o) file, and add its info to the data structures.
 * @param fileName A path to the object file to be read
 * @param i The ID of the file
 */
void obj_ReadFile(char const *fileName, unsigned int i);

/*
 * Sets up object file reading
 * @param nbFiles The number of object files that will be read
 */
void obj_Setup(unsigned int nbFiles);

#endif // RGBDS_LINK_OBJECT_HPP
