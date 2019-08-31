/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_UTIL_H
#define RGBDS_UTIL_H

#include <stdint.h>

uint32_t calchash(const char *s);
int32_t readUTF8Char(char *dest, char *src);

#endif /* RGBDS_UTIL_H */
