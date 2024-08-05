/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_OUTPUT_HPP
#define RGBDS_ASM_OUTPUT_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "linkdefs.hpp"

struct Expression;
struct FileStackNode;

enum StateFeature {
    STATE_EQU,
    STATE_VAR,
    STATE_EQUS,
    STATE_CHAR,
    STATE_MACRO,
    NB_STATE_FEATURES
};

extern std::string objectFileName;

void out_RegisterNode(std::shared_ptr<FileStackNode> node);
void out_SetFileName(std::string const &name);
void out_CreatePatch(uint32_t type, Expression const &expr, uint32_t ofs, uint32_t pcShift);
void out_CreateAssert(
    AssertionType type, Expression const &expr, std::string const &message, uint32_t ofs
);
void out_WriteObject();
void out_WriteState(std::string name, std::vector<StateFeature> const &features);

#endif // RGBDS_ASM_OUTPUT_HPP
