/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_OUTPUT_H
#define RGBDS_ASM_OUTPUT_H

#include <stdint.h>

#include "linkdefs.hpp"

struct Expression;
struct FileStackNode;

extern const char *objectName;

void out_RegisterNode(FileStackNode *node);
void out_ReplaceNode(FileStackNode *node);
void out_SetFileName(char *s);
void out_CreatePatch(uint32_t type, Expression const *expr, uint32_t ofs, uint32_t pcShift);
void out_CreateAssert(enum AssertionType type, Expression const *expr, char const *message, uint32_t ofs);
void out_WriteObject();

#endif // RGBDS_ASM_OUTPUT_H
