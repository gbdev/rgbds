/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_CHARMAP_HPP
#define RGBDS_ASM_CHARMAP_HPP

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#define DEFAULT_CHARMAP_NAME "main"

bool charmap_ForEach(
    void (*mapFunc)(std::string const &), void (*charFunc)(std::string const &, uint8_t value)
);
void charmap_New(std::string const &name, std::string const *baseName);
void charmap_Set(std::string const &name);
void charmap_Push();
void charmap_Pop();
void charmap_Add(std::string const &mapping, uint8_t value);
bool charmap_HasChar(std::string const &input);
std::vector<uint8_t> charmap_Convert(std::string const &input);
size_t charmap_ConvertNext(std::string_view &input, std::vector<uint8_t> *output);

#endif // RGBDS_ASM_CHARMAP_HPP
