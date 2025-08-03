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

struct AlignmentSpec {
	uint8_t alignment;
	uint16_t alignOfs;
};

void act_If(int32_t condition);
void act_Elif(int32_t condition);
void act_Else();
void act_Endc();

AlignmentSpec act_Alignment(int32_t alignment, int32_t alignOfs);

void act_Assert(AssertionType type, Expression const &expr, std::string const &message);
void act_StaticAssert(AssertionType type, int32_t condition, std::string const &message);

std::optional<std::string> act_ReadFile(std::string const &name, uint32_t maxLen);

uint32_t act_CharToNum(std::string const &str);
uint32_t act_StringToNum(std::vector<int32_t> const &str);

int32_t act_CharVal(std::string const &str);
int32_t act_CharVal(std::string const &str, int32_t negIdx);
uint8_t act_StringByte(std::string const &str, int32_t negIdx);

size_t act_StringLen(std::string const &str, bool printErrors);
std::string
    act_StringSlice(std::string const &str, int32_t negStart, std::optional<int32_t> negStop);
std::string act_StringSub(std::string const &str, int32_t negPos, std::optional<uint32_t> optLen);

size_t act_CharLen(std::string const &str);
std::string act_StringChar(std::string const &str, int32_t negIdx);
std::string act_CharSub(std::string const &str, int32_t negPos);
int32_t act_CharCmp(std::string_view str1, std::string_view str2);

std::string act_StringReplace(std::string_view str, std::string const &old, std::string const &rep);
std::string act_StringFormat(
    std::string const &spec, std::vector<std::variant<uint32_t, std::string>> const &args
);

std::string act_SectionName(std::string const &symName);

void act_CompoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue);

#endif // RGBDS_ASM_ACTIONS_HPP
