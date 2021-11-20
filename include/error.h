/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2021, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ERROR_H
#define RGBDS_ERROR_H

#include "helpers.h"

void warn(char const *fmt, ...) format_(printf, 1, 2);
void warnx(char const *fmt, ...) format_(printf, 1, 2);

_Noreturn void err(int status, char const *fmt, ...) format_(printf, 2, 3);
_Noreturn void errx(int status, char const *fmt, ...) format_(printf, 2, 3);

#endif /* RGBDS_ERROR_H */
