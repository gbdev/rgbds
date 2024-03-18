/* SPDX-License-Identifier: MIT */

// Contains some assembler-wide defines and externs

#ifndef RGBDS_ASM_FSTACK_H
#define RGBDS_ASM_FSTACK_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

#include "linkdefs.hpp"

#include "asm/lexer.hpp"

struct FileStackNode {
	FileStackNodeType type;
	std::variant<
	    std::vector<uint32_t>, // NODE_REPT
	    std::string            // NODE_FILE, NODE_MACRO
	    >
	    data;

	FileStackNode *parent; // Pointer to parent node, for error reporting
	// Line at which the parent context was exited; meaningless for the root level
	uint32_t lineNo;

	// If referenced by a Symbol, Section, or Patch's `src`, don't `delete`!
	bool referenced = false;
	// Set only if referenced: ID within the object file, -1 if not output yet
	uint32_t ID = -1;

	// REPT iteration counts since last named node, in reverse depth order
	std::vector<uint32_t> &iters();
	std::vector<uint32_t> const &iters() const;
	// File name for files, file::macro name for macros
	std::string &name();
	std::string const &name() const;

	FileStackNode(FileStackNodeType type_, std::variant<std::vector<uint32_t>, std::string> data_)
	    : type(type_), data(data_){};

	std::string const &dump(uint32_t curLineNo) const;
};

#define DEFAULT_MAX_DEPTH 64
extern size_t maxRecursionDepth;

struct MacroArgs;

void fstk_DumpCurrent();
FileStackNode *fstk_GetFileStack();

void fstk_AddIncludePath(std::string const &path);
void fstk_SetPreIncludeFile(std::string const &path);
std::optional<std::string> fstk_FindFile(std::string const &path);

bool yywrap();
void fstk_RunInclude(std::string const &path);
void fstk_RunMacro(std::string const &macroName, MacroArgs &args);
void fstk_RunRept(uint32_t count, int32_t reptLineNo, char const *body, size_t size);
void fstk_RunFor(
    std::string const &symName,
    int32_t start,
    int32_t stop,
    int32_t step,
    int32_t reptLineNo,
    char const *body,
    size_t size
);
void fstk_StopRept();
bool fstk_Break();

void fstk_NewRecursionDepth(size_t newDepth);
void fstk_Init(std::string const &mainPath, size_t maxDepth);

#endif // RGBDS_ASM_FSTACK_H
