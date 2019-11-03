/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Declarations related to processing of object (.o) files */

#ifndef RGBDS_LINK_OBJECT_H
#define RGBDS_LINK_OBJECT_H

/**
 * Read an object (.o) file, and add its info to the data structures.
 * @param fileName A path to the object file to be read
 */
void obj_ReadFile(char const *fileName);

/**
 * Perform validation on the object files' contents
 */
void obj_DoSanityChecks(void);

/**
 * `free`s all object memory that was allocated.
 */
void obj_Cleanup(void);

#endif /* RGBDS_LINK_OBJECT_H */
