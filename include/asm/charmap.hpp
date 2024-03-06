/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_CHARMAP_H
#define RGBDS_ASM_CHARMAP_H

#include <stdint.h>
#include <vector>

#define DEFAULT_CHARMAP_NAME "main"

void charmap_New(char const *name, char const *baseName);
void charmap_Set(char const *name);
void charmap_Push();
void charmap_Pop();
void charmap_Add(char const *mapping, uint8_t value);
bool charmap_HasChar(char const *input);
void charmap_Convert(char const *input, std::vector<uint8_t> &output);
size_t charmap_ConvertNext(char const *&input, std::vector<uint8_t> *output);

#endif // RGBDS_ASM_CHARMAP_H
