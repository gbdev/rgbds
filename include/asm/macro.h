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

void sym_AddNewMacroArg(char const *s);
void sym_SaveCurrentMacroArgs(char *save[]);
void sym_RestoreCurrentMacroArgs(char *save[]);
void sym_UseNewMacroArgs(void);
char *sym_FindMacroArg(int32_t i);
void sym_UseCurrentMacroArgs(void);
void sym_SetMacroArgID(uint32_t nMacroCount);
void sym_ShiftCurrentMacroArgs(void);
uint32_t sym_NbMacroArgs(void);

void macro_Init(void);

#endif
