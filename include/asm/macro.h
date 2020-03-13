/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_MACRO_H
#define RGBDS_MACRO_H

#include <stdint.h>

#include "helpers.h"

void macro_AddNewArg(char const *s);
void macro_SaveCurrentArgs(char *save[]);
void macro_RestoreCurrentArgs(char *save[]);
void macro_UseNewArgs(void);
char *macro_FindArg(int32_t i);
void macro_UseCurrentArgs(void);
void macro_SetArgID(uint32_t nMacroCount);
void macro_ShiftCurrentArgs(void);
uint32_t macro_NbArgs(void);

void macro_Init(void);

#endif
