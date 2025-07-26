// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_ACTIONS_HPP
#define RGBDS_ASM_ACTIONS_HPP

#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "asm/output.hpp" // AssertionType
#include "asm/rpn.hpp"    // RPNCommand

std::optional<std::string> act_ReadFile(std::string const &name, uint32_t maxLen);

uint32_t act_StringToNum(std::vector<int32_t> const &str);

size_t act_StringLen(std::string const &str, bool printErrors);
std::string act_StringSlice(std::string const &str, uint32_t start, uint32_t stop);
std::string act_StringSub(std::string const &str, uint32_t pos, uint32_t len);

size_t act_CharLen(std::string const &str);
std::string act_StringChar(std::string const &str, uint32_t idx);
std::string act_CharSub(std::string const &str, uint32_t pos);
int32_t act_CharCmp(std::string_view str1, std::string_view str2);

uint32_t act_AdjustNegativeIndex(int32_t idx, size_t len, char const *functionName);
uint32_t act_AdjustNegativePos(int32_t pos, size_t len, char const *functionName);

std::string act_StringReplace(std::string_view str, std::string const &old, std::string const &rep);
std::string act_StringFormat(
    std::string const &spec, std::vector<std::variant<uint32_t, std::string>> const &args
);

void act_CompoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue);

void act_FailAssert(AssertionType type);
void act_FailAssertMsg(AssertionType type, std::string const &message);

#endif // RGBDS_ASM_ACTIONS_HPP
