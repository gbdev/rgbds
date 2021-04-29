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
#include <stdbool.h>
#include <stdlib.h>

#include "asm/warning.h"

#include "helpers.h"

struct MacroArgs;
struct String;

struct MacroArgs *macro_GetCurrentArgs(void);
struct MacroArgs *macro_NewArgs(void);
void macro_AppendArg(struct MacroArgs **args, struct String *str);
void macro_UseNewArgs(struct MacroArgs *args);
void macro_FreeArgs(struct MacroArgs *args);
struct String *macro_GetArg(uint32_t i);
struct String *macro_GetAllArgs(void);

uint32_t macro_GetUniqueID(void);
struct String *macro_GetUniqueIDStr(void);
void macro_SetUniqueID(uint32_t id);
uint32_t macro_UseNewUniqueID(void);
void macro_ShiftCurrentArgs(int32_t count);
uint32_t macro_NbArgs(void);

#endif
