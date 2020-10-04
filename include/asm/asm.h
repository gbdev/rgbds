/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Contains some assembler-wide defines and externs
 */

#ifndef RGBDS_ASM_ASM_H
#define RGBDS_ASM_ASM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "asm/localasm.h"
#include "asm/symbol.h"

#define MAXMACROARGS	99999
#define MAXINCPATHS	128

extern uint32_t nTotalLines;
extern uint32_t nIFDepth;
extern struct Section *pCurrentSection;

#endif /* RGBDS_ASM_ASM_H */
