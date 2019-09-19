/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Outputting the result of linking */
#ifndef RGBDS_LINK_OUTPUT_H
#define RGBDS_LINK_OUTPUT_H

#include <stdint.h>

#include "link/section.h"

void out_AddSection(struct Section const *section);

void out_WriteFiles(void);

#endif /* RGBDS_LINK_OUTPUT_H */
