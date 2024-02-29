/* SPDX-License-Identifier: MIT */

// Contains some assembler-wide defines and externs

#ifndef RGBDS_ASM_FSTACK_H
#define RGBDS_ASM_FSTACK_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

#include "asm/lexer.hpp"

#include "linkdefs.hpp"

struct FileStackNode {
	FileStackNode *parent; // Pointer to parent node, for error reporting
	// Line at which the parent context was exited; meaningless for the root level
	uint32_t lineNo;

	bool referenced; // If referenced by a Symbol, Section, or Patch's `src`, don't `delete`!
	uint32_t ID; // Set only if referenced: ID within the object file, -1 if not output yet

	enum FileStackNodeType type;
	std::variant<
		std::monostate, // Default constructed; `.type` and `.data` must be set manually
		std::vector<uint32_t>, // NODE_REPT
		std::string // NODE_FILE, NODE_MACRO
	> data;

	// REPT iteration counts since last named node, in reverse depth order
	std::vector<uint32_t> &iters();
	std::vector<uint32_t> const &iters() const;
	// File name for files, file::macro name for macros
	std::string &name();
	std::string const &name() const;
};

#define DEFAULT_MAX_DEPTH 64
extern size_t maxRecursionDepth;

struct MacroArgs;

void fstk_Dump(FileStackNode const *node, uint32_t lineNo);
void fstk_DumpCurrent(void);
FileStackNode *fstk_GetFileStack(void);
// The lifetime of the returned chars is until reaching the end of that file
char const *fstk_GetFileName(void);

void fstk_AddIncludePath(char const *s);
void fstk_SetPreIncludeFile(char const *s);
/*
 * @param path The user-provided file name
 * @return A pointer to the `new`-allocated full path, or NULL if no path worked
 */
std::string *fstk_FindFile(char const *path);

bool yywrap(void);
void fstk_RunInclude(char const *path);
void fstk_RunMacro(char const *macroName, MacroArgs *args);
void fstk_RunRept(uint32_t count, int32_t reptLineNo, char *body, size_t size);
void fstk_RunFor(char const *symName, int32_t start, int32_t stop, int32_t step,
		     int32_t reptLineNo, char *body, size_t size);
void fstk_StopRept(void);
bool fstk_Break(void);

void fstk_NewRecursionDepth(size_t newDepth);
void fstk_Init(char const *mainPath, size_t maxDepth);

#endif // RGBDS_ASM_FSTACK_H
