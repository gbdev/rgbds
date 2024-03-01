/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_MACRO_H
#define RGBDS_MACRO_H

#include <stdint.h>
#include <stdlib.h>

#include "asm/warning.hpp"

#include "helpers.hpp"

struct MacroArgs;

MacroArgs *macro_GetCurrentArgs();
MacroArgs *macro_NewArgs();
void macro_AppendArg(MacroArgs *args, char *s);
void macro_UseNewArgs(MacroArgs *args);
void macro_FreeArgs(MacroArgs *args);
char const *macro_GetArg(uint32_t i);
char const *macro_GetAllArgs();

uint32_t macro_GetUniqueID();
char const *macro_GetUniqueIDStr();
void macro_SetUniqueID(uint32_t id);
uint32_t macro_UseNewUniqueID();
uint32_t macro_UndefUniqueID();
void macro_ShiftCurrentArgs(int32_t count);
uint32_t macro_NbArgs();

#endif // RGBDS_MACRO_H
