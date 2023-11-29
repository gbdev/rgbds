/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_CHARMAP_H
#define RGBDS_ASM_CHARMAP_H

#include <stdbool.h>
#include <stdint.h>

#define DEFAULT_CHARMAP_NAME "main"

struct Charmap *charmap_New(char const *name, char const *baseName);
void charmap_Set(char const *name);
void charmap_Push(void);
void charmap_Pop(void);
void charmap_Add(char *mapping, uint8_t value);
bool charmap_HasChar(char const *input);
size_t charmap_Convert(char const *input, uint8_t *output);
size_t charmap_ConvertNext(char const **input, uint8_t **output);

#endif // RGBDS_ASM_CHARMAP_H
