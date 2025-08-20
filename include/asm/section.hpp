// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_SECTION_HPP
#define RGBDS_ASM_SECTION_HPP

#include <deque>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

struct Expression;
struct FileStackNode;
struct Section;

struct Patch {
	std::shared_ptr<FileStackNode> src;
	uint32_t lineNo;
	uint32_t offset;
	Section *pcSection;
	uint32_t pcOffset;
	uint8_t type;
	std::vector<uint8_t> rpn;
};

struct Section {
	std::string name;
	SectionType type;
	SectionModifier modifier;
	std::shared_ptr<FileStackNode> src; // Where the section was defined
	uint32_t fileLine;                  // Line where the section was defined
	uint32_t size;
	uint32_t org;
	uint32_t bank;
	uint8_t align; // Exactly as specified in `ALIGN[]`
	uint16_t alignOfs;
	std::deque<Patch> patches;
	std::vector<uint8_t> data;

	uint32_t getID() const; // ID of the section in the object file (`UINT32_MAX` if none)
	bool isSizeKnown() const;
};

struct SectionSpec {
	uint32_t bank;
	uint8_t alignment;
	uint16_t alignOfs;
};

size_t sect_CountSections();
void sect_ForEach(void (*callback)(Section &));

Section *sect_FindSectionByName(std::string const &name);
void sect_NewSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
);
void sect_SetLoadSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
);
void sect_EndLoadSection(char const *cause);
void sect_CheckLoadClosed();

Section *sect_GetSymbolSection();
uint32_t sect_GetSymbolOffset();
uint32_t sect_GetOutputOffset();
std::optional<uint32_t> sect_GetOutputBank();

Patch *sect_AddOutputPatch();

uint32_t sect_GetAlignBytes(uint8_t alignment, uint16_t offset);
void sect_AlignPC(uint8_t alignment, uint16_t offset);

void sect_CheckSizes();

void sect_StartUnion();
void sect_NextUnionMember();
void sect_EndUnion();
void sect_CheckUnionClosed();

void sect_ConstByte(uint8_t byte);
void sect_ByteString(std::vector<int32_t> const &str);
void sect_WordString(std::vector<int32_t> const &str);
void sect_LongString(std::vector<int32_t> const &str);
void sect_Skip(uint32_t skip, bool ds);
void sect_RelByte(Expression const &expr, uint32_t pcShift);
void sect_RelBytes(uint32_t n, std::vector<Expression> const &exprs);
void sect_RelWord(Expression const &expr, uint32_t pcShift);
void sect_RelLong(Expression const &expr, uint32_t pcShift);
void sect_PCRelByte(Expression const &expr, uint32_t pcShift);
bool sect_BinaryFile(std::string const &name, uint32_t startPos);
bool sect_BinaryFileSlice(std::string const &name, uint32_t startPos, uint32_t length);

void sect_EndSection();
void sect_PushSection();
void sect_PopSection();
void sect_CheckStack();

std::string sect_PushSectionFragmentLiteral();

#endif // RGBDS_ASM_SECTION_HPP
