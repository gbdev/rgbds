/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_OUTPUT_HPP
#define RGBDS_ASM_OUTPUT_HPP

#include <memory>
#include <stdint.h>
#include <string>

#include "linkdefs.hpp"

struct Expression;
struct FileStackNode;

extern std::string objectName;

void out_RegisterNode(std::shared_ptr<FileStackNode> node);
void out_SetFileName(std::string const &name);
void out_CreatePatch(uint32_t type, Expression const &expr, uint32_t ofs, uint32_t pcShift);
void out_CreateAssert(
    AssertionType type, Expression const &expr, std::string const &message, uint32_t ofs
);
void out_WriteObject();

#endif // RGBDS_ASM_OUTPUT_HPP
