/* SPDX-License-Identifier: MIT */

// Contains some assembler-wide defines and externs

#ifndef RGBDS_ASM_FSTACK_HPP
#define RGBDS_ASM_FSTACK_HPP

#include <memory>
#include <optional>
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

	std::shared_ptr<FileStackNode> parent; // Pointer to parent node, for error reporting
	// Line at which the parent context was exited
	// Meaningless at the root level, but gets written to the object file anyway, so init it
	uint32_t lineNo = 0;

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

	// If true, entering this context generates a new unique ID.
	bool generatesUniqueID() const { return type == NODE_REPT || type == NODE_MACRO; }
};

#define DEFAULT_MAX_DEPTH 64
extern size_t maxRecursionDepth;

struct MacroArgs;

void fstk_DumpCurrent();
std::shared_ptr<FileStackNode> fstk_GetFileStack();
std::shared_ptr<std::string> fstk_GetUniqueIDStr();
MacroArgs *fstk_GetCurrentMacroArgs();

void fstk_AddIncludePath(std::string const &path);
void fstk_SetPreIncludeFile(std::string const &path);
std::optional<std::string> fstk_FindFile(std::string const &path);

bool yywrap();
void fstk_RunInclude(std::string const &path, bool updateStateNow);
void fstk_RunMacro(std::string const &macroName, std::shared_ptr<MacroArgs> macroArgs);
void fstk_RunRept(uint32_t count, int32_t reptLineNo, ContentSpan const &span);
void fstk_RunFor(
    std::string const &symName,
    int32_t start,
    int32_t stop,
    int32_t step,
    int32_t reptLineNo,
    ContentSpan const &span
);
void fstk_StopRept();
bool fstk_Break();

void fstk_NewRecursionDepth(size_t newDepth);
void fstk_Init(std::string const &mainPath, size_t maxDepth);

#endif // RGBDS_ASM_FSTACK_HPP
