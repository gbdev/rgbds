/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_OUTPUT_H
#define RGBDS_ASM_OUTPUT_H

#include <stdint.h>

#include "linkdefs.h"

struct Expression;

extern char *tzObjectname;
extern struct Section *pSectionList, *pCurrentSection;

void out_SetFileName(char *s);
void out_CreatePatch(uint32_t type, struct Expression *expr);
void out_WriteObject(void);

#endif /* RGBDS_ASM_OUTPUT_H */
