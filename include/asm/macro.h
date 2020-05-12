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
#include <stdlib.h>

#include "asm/warning.h"

#include "helpers.h"

struct MacroArgs;

struct MacroArgs *macro_GetCurrentArgs(void);
struct MacroArgs *macro_NewArgs(void);
void macro_AppendArg(struct MacroArgs **args, char *s);
void macro_UseNewArgs(struct MacroArgs *args);
void macro_FreeArgs(struct MacroArgs *args);
char const *macro_GetArg(uint32_t i);

uint32_t macro_GetUniqueID(void);
char const *macro_GetUniqueIDStr(void);
void macro_SetUniqueID(uint32_t id);
void macro_ShiftCurrentArgs(void);
uint32_t macro_NbArgs(void);

#endif
