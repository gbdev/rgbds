// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_LAYOUT_HPP
#define RGBDS_LINK_LAYOUT_HPP

#include <fstream>
#include <stdint.h>
#include <string>

#include "linkdefs.hpp"

struct LexerStackEntry {
	std::filebuf file;
	std::string path;
	uint32_t lineNo;

	explicit LexerStackEntry(std::string &&path_) : file(), path(path_), lineNo(1) {}
};

#define scriptError(context, fmt, ...) \
	::error( \
	    "%s(%" PRIu32 "): " fmt, context.path.c_str(), context.lineNo __VA_OPT__(, ) __VA_ARGS__ \
	)

LexerStackEntry &lexer_Context();
void lexer_IncludeFile(std::string &&path);
void lexer_IncLineNo();
bool lexer_Init(char const *linkerScriptName);

void layout_SetFloatingSectionType(SectionType type);
void layout_SetSectionType(SectionType type);
void layout_SetSectionType(SectionType type, uint32_t bank);
void layout_SetAddr(uint32_t addr);
void layout_MakeAddrFloating();
void layout_AlignTo(uint32_t alignment, uint32_t offset);
void layout_Pad(uint32_t length);
void layout_PlaceSection(std::string const &name, bool isOptional);

#endif // RGBDS_LINK_LAYOUT_HPP
