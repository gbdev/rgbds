// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_CHARMAP_HPP
#define RGBDS_ASM_CHARMAP_HPP

#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#define DEFAULT_CHARMAP_NAME "main"

bool charmap_ForEach(
    void (*mapFunc)(std::string const &),
    void (*charFunc)(std::string const &, std::vector<int32_t>)
);
void charmap_New(std::string const &name, std::string const *baseName);
void charmap_Set(std::string const &name);
void charmap_Push();
void charmap_Pop();
void charmap_CheckStack();
void charmap_Add(std::string const &mapping, std::vector<int32_t> &&value);
bool charmap_HasChar(std::string const &mapping);
size_t charmap_CharSize(std::string const &mapping);
std::optional<int32_t> charmap_CharValue(std::string const &mapping, size_t idx);
std::vector<int32_t> charmap_Convert(std::string const &input);
size_t charmap_ConvertNext(std::string_view &input, std::vector<int32_t> *output);
std::string charmap_Reverse(std::vector<int32_t> const &value, bool &unique);

#endif // RGBDS_ASM_CHARMAP_HPP
