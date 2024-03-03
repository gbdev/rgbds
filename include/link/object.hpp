/* SPDX-License-Identifier: MIT */

// Declarations related to processing of object (.o) files

#ifndef RGBDS_LINK_OBJECT_H
#define RGBDS_LINK_OBJECT_H

/*
 * Read an object (.o) file, and add its info to the data structures.
 * @param fileName A path to the object file to be read
 * @param i The ID of the file
 */
void obj_ReadFile(char const *fileName, unsigned int i);

/*
 * Evaluate all assertions
 */
void obj_CheckAssertions();

/*
 * Sets up object file reading
 * @param nbFiles The number of object files that will be read
 */
void obj_Setup(unsigned int nbFiles);

#endif // RGBDS_LINK_OBJECT_H
