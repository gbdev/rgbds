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

/**
 * Registers a section for output.
 * @param section The section to add
 */
void out_AddSection(struct Section const *section);

/**
 * Writes all output (bin, sym, map) files.
 */
void out_WriteFiles(void);

#endif /* RGBDS_LINK_OUTPUT_H */
