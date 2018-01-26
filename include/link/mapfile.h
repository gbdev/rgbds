/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINK_MAPFILE_H
#define RGBDS_LINK_MAPFILE_H

#include <stdint.h>

void SetMapfileName(char *name);
void SetSymfileName(char *name);
void CloseMapfile(void);
void MapfileWriteSection(const struct sSection *pSect);
void MapfileInitBank(int32_t bank);
void MapfileCloseBank(int32_t slack);

#endif /* RGBDS_LINK_MAPFILE_H */
