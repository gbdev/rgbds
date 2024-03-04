/* SPDX-License-Identifier: MIT */

#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <new>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "error.hpp"
#include "platform.hpp" // S_ISDIR (stat macro)

#include "asm/fstack.hpp"
#include "asm/macro.hpp"
#include "asm/main.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

struct Context {
	FileStackNode *fileInfo;
	LexerState lexerState;
	uint32_t uniqueID;
	MacroArgs *macroArgs; // Macro args are *saved* here
	uint32_t nbReptIters;
	bool isForLoop;
	int32_t forValue;
	int32_t forStep;
	std::string forName;
};

static std::stack<Context> contextStack;
size_t maxRecursionDepth;

// The first include path for `fstk_FindFile` to try is none at all
static std::vector<std::string> includePaths = {""};

static const char *preIncludeName;

std::vector<uint32_t> &FileStackNode::iters() {
	assert(std::holds_alternative<std::vector<uint32_t>>(data));
	return std::get<std::vector<uint32_t>>(data);
}

std::vector<uint32_t> const &FileStackNode::iters() const {
	assert(std::holds_alternative<std::vector<uint32_t>>(data));
	return std::get<std::vector<uint32_t>>(data);
}

std::string &FileStackNode::name() {
	assert(std::holds_alternative<std::string>(data));
	return std::get<std::string>(data);
}

std::string const &FileStackNode::name() const {
	assert(std::holds_alternative<std::string>(data));
	return std::get<std::string>(data);
}

static const char *dumpNodeAndParents(FileStackNode const &node) {
	char const *name;

	if (node.type == NODE_REPT) {
		assert(node.parent); // REPT nodes should always have a parent
		std::vector<uint32_t> const &nodeIters = node.iters();

		name = dumpNodeAndParents(*node.parent);
		fprintf(stderr, "(%" PRIu32 ") -> %s", node.lineNo, name);
		for (uint32_t i = nodeIters.size(); i--;)
			fprintf(stderr, "::REPT~%" PRIu32, nodeIters[i]);
	} else {
		name = node.name().c_str();
		if (node.parent) {
			dumpNodeAndParents(*node.parent);
			fprintf(stderr, "(%" PRIu32 ") -> %s", node.lineNo, name);
		} else {
			fputs(name, stderr);
		}
	}
	return name;
}

void FileStackNode::dump(uint32_t curLineNo) const {
	dumpNodeAndParents(*this);
	fprintf(stderr, "(%" PRIu32 ")", curLineNo);
}

void fstk_DumpCurrent() {
	if (contextStack.empty()) {
		fputs("at top level", stderr);
		return;
	}
	contextStack.top().fileInfo->dump(lexer_GetLineNo());
}

FileStackNode *fstk_GetFileStack() {
	if (contextStack.empty())
		return nullptr;

	FileStackNode *topNode = contextStack.top().fileInfo;

	// Mark node and all of its parents as referenced if not already so they don't get freed
	for (FileStackNode *node = topNode; node && !node->referenced; node = node->parent) {
		node->ID = -1;
		node->referenced = true;
	}
	return topNode;
}

char const *fstk_GetFileName() {
	// Iterating via the nodes themselves skips nested REPTs
	FileStackNode const *node = contextStack.top().fileInfo;

	while (node->type != NODE_FILE)
		node = node->parent;
	return node->name().c_str();
}

void fstk_AddIncludePath(char const *path) {
	if (path[0] == '\0')
		return;

	std::string &str = includePaths.emplace_back(path);

	if (str.back() != '/')
		str += '/';
}

void fstk_SetPreIncludeFile(char const *path) {
	if (preIncludeName)
		warnx("Overriding pre-included filename %s", preIncludeName);
	preIncludeName = path;
	if (verbose)
		printf("Pre-included filename %s\n", preIncludeName);
}

static void printDep(char const *path) {
	if (dependfile) {
		fprintf(dependfile, "%s: %s\n", targetFileName.c_str(), path);
		if (generatePhonyDeps)
			fprintf(dependfile, "%s:\n", path);
	}
}

static bool isPathValid(char const *path) {
	struct stat statbuf;

	if (stat(path, &statbuf) != 0)
		return false;

	// Reject directories
	return !S_ISDIR(statbuf.st_mode);
}

std::string *fstk_FindFile(char const *path) {
	std::string *fullPath = new (std::nothrow) std::string();

	if (!fullPath) {
		error("Failed to allocate string during include path search: %s\n", strerror(errno));
	} else {
		for (std::string &str : includePaths) {
			*fullPath = str + path;
			if (isPathValid(fullPath->c_str())) {
				printDep(fullPath->c_str());
				return fullPath;
			}
		}
	}

	errno = ENOENT;
	if (generatedMissingIncludes)
		printDep(path);
	return nullptr;
}

bool yywrap() {
	uint32_t ifDepth = lexer_GetIFDepth();

	if (ifDepth != 0)
		fatalerror("Ended block with %" PRIu32 " unterminated IF construct%s\n", ifDepth,
		           ifDepth == 1 ? "" : "s");

	if (Context &context = contextStack.top(); context.fileInfo->type == NODE_REPT) {
		// The context is a REPT or FOR block, which may loop

		// If the node is referenced, we can't edit it; duplicate it
		if (context.fileInfo->referenced) {
			context.fileInfo = new (std::nothrow) FileStackNode(*context.fileInfo);
			if (!context.fileInfo)
				fatalerror("Failed to duplicate REPT file node: %s\n", strerror(errno));
			// Copy all info but the referencing
			context.fileInfo->referenced = false;
		}

		std::vector<uint32_t> &fileInfoIters = context.fileInfo->iters();

		// If this is a FOR, update the symbol value
		if (context.isForLoop && fileInfoIters.front() <= context.nbReptIters) {
			// Avoid arithmetic overflow runtime error
			uint32_t forValue = (uint32_t)context.forValue + (uint32_t)context.forStep;
			context.forValue = forValue <= INT32_MAX ? forValue : -(int32_t)~forValue - 1;
			Symbol *sym = sym_AddVar(context.forName.c_str(), context.forValue);

			// This error message will refer to the current iteration
			if (sym->type != SYM_VAR)
				fatalerror("Failed to update FOR symbol value\n");
		}
		// Advance to the next iteration
		fileInfoIters.front()++;
		// If this wasn't the last iteration, wrap instead of popping
		if (fileInfoIters.front() <= context.nbReptIters) {
			lexer_RestartRept(context.fileInfo->lineNo);
			context.uniqueID = macro_UseNewUniqueID();
			return false;
		}
	} else if (contextStack.size() == 1) {
		return true;
	}

	Context oldContext = contextStack.top();

	contextStack.pop();

	lexer_CleanupState(oldContext.lexerState);
	// Restore args if a macro (not REPT) saved them
	if (oldContext.fileInfo->type == NODE_MACRO)
		macro_UseNewArgs(contextStack.top().macroArgs);
	// Free the file stack node
	if (!oldContext.fileInfo->referenced)
		delete oldContext.fileInfo;

	lexer_SetState(&contextStack.top().lexerState);
	macro_SetUniqueID(contextStack.top().uniqueID);

	return false;
}

// Make sure not to switch the lexer state before calling this, so the saved line no is correct.
// BE CAREFUL! This modifies the file stack directly, you should have set up the file info first.
// Callers should set `contextStack.top().lexerState` after this so it is not `nullptr`.
static Context &newContext(FileStackNode &fileInfo) {
	if (contextStack.size() > maxRecursionDepth)
		fatalerror("Recursion limit (%zu) exceeded\n", maxRecursionDepth);

	// Save the current `\@` value, to be restored when this context ends
	contextStack.top().uniqueID = macro_GetUniqueID();

	FileStackNode *parent = contextStack.top().fileInfo;

	fileInfo.parent = parent;
	fileInfo.lineNo = lexer_GetLineNo();
	fileInfo.referenced = false;

	Context &context = contextStack.emplace();

	context.fileInfo = &fileInfo;
	context.isForLoop = false;

	return context;
}

void fstk_RunInclude(char const *path) {
	std::string *fullPath = fstk_FindFile(path);

	if (!fullPath) {
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCLUDE file '%s' (%s)\n", path, strerror(errno));
			failedOnMissingInclude = true;
		} else {
			error("Unable to open included file '%s': %s\n", path, strerror(errno));
		}
		return;
	}

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode();

	if (!fileInfo) {
		error("Failed to alloc file info for INCLUDE: %s\n", strerror(errno));
		return;
	}
	fileInfo->type = NODE_FILE;
	fileInfo->data = *fullPath;
	delete fullPath;

	uint32_t uniqueID = contextStack.top().uniqueID;
	Context &context = newContext(*fileInfo);

	if (!lexer_OpenFile(context.lexerState, fileInfo->name().c_str()))
		fatalerror("Failed to set up lexer for file include\n");
	lexer_SetStateAtEOL(&context.lexerState);
	// We're back at top-level, so most things are reset,
	// but not the unique ID, since INCLUDE may be inside a
	// MACRO or REPT/FOR loop
	context.uniqueID = uniqueID;
}

// Similar to `fstk_RunInclude`, but not subject to `-MG`, and
// calling `lexer_SetState` instead of `lexer_SetStateAtEOL`.
static void runPreIncludeFile() {
	if (!preIncludeName)
		return;

	std::string *fullPath = fstk_FindFile(preIncludeName);

	if (!fullPath) {
		error("Unable to open included file '%s': %s\n", preIncludeName, strerror(errno));
		return;
	}

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode();

	if (!fileInfo) {
		error("Failed to alloc file info for pre-include: %s\n", strerror(errno));
		return;
	}
	fileInfo->type = NODE_FILE;
	fileInfo->data = *fullPath;
	delete fullPath;

	Context &context = newContext(*fileInfo);

	if (!lexer_OpenFile(context.lexerState, fileInfo->name().c_str()))
		fatalerror("Failed to set up lexer for file include\n");
	lexer_SetState(&context.lexerState);
	// We're back at top-level, so most things are reset
	context.uniqueID = macro_UndefUniqueID();
}

void fstk_RunMacro(char const *macroName, MacroArgs &args) {
	Symbol *macro = sym_FindExactSymbol(macroName);

	if (!macro) {
		error("Macro \"%s\" not defined\n", macroName);
		return;
	}
	if (macro->type != SYM_MACRO) {
		error("\"%s\" is not a macro\n", macroName);
		return;
	}
	contextStack.top().macroArgs = macro_GetCurrentArgs();

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode();

	if (!fileInfo) {
		error("Failed to alloc file info for \"%s\": %s\n", macro->name, strerror(errno));
		return;
	}
	fileInfo->type = NODE_MACRO;
	fileInfo->data = "";

	// Print the name...
	std::string &fileInfoName = fileInfo->name();

	for (FileStackNode const *node = macro->src; node; node = node->parent) {
		if (node->type != NODE_REPT) {
			fileInfoName.append(node->name());
			break;
		}
	}
	if (macro->src->type == NODE_REPT) {
		std::vector<uint32_t> const &srcIters = macro->src->iters();

		for (uint32_t i = srcIters.size(); i--;) {
			char buf[sizeof("::REPT~4294967295")]; // UINT32_MAX

			if (sprintf(buf, "::REPT~%" PRIu32, srcIters[i]) < 0)
				fatalerror("Failed to write macro invocation info: %s\n", strerror(errno));
			fileInfoName.append(buf);
		}
	}
	fileInfoName.append("::");
	fileInfoName.append(macro->name);

	Context &context = newContext(*fileInfo);
	std::string_view *macroView = macro->getMacro();

	lexer_OpenFileView(context.lexerState, "MACRO", macroView->data(), macroView->size(),
	                   macro->fileLine);
	lexer_SetStateAtEOL(&context.lexerState);
	context.uniqueID = macro_UseNewUniqueID();
	macro_UseNewArgs(&args);
}

static bool newReptContext(int32_t reptLineNo, char const *body, size_t size) {
	uint32_t reptDepth = contextStack.top().fileInfo->type == NODE_REPT
	                         ? contextStack.top().fileInfo->iters().size()
	                         : 0;
	FileStackNode *fileInfo = new (std::nothrow) FileStackNode();

	if (!fileInfo) {
		error("Failed to alloc file info for REPT: %s\n", strerror(errno));
		return false;
	}
	fileInfo->type = NODE_REPT;
	fileInfo->data = std::vector<uint32_t>{1};
	if (reptDepth) {
		// Append all parent iter counts
		fileInfo->iters().insert(fileInfo->iters().end(),
		                         RANGE(contextStack.top().fileInfo->iters()));
	}

	Context &context = newContext(*fileInfo);

	// Correct our line number, which currently points to the `ENDR` line
	context.fileInfo->lineNo = reptLineNo;

	lexer_OpenFileView(context.lexerState, "REPT", body, size, reptLineNo);
	lexer_SetStateAtEOL(&context.lexerState);
	context.uniqueID = macro_UseNewUniqueID();
	return true;
}

void fstk_RunRept(uint32_t count, int32_t reptLineNo, char const *body, size_t size) {
	if (count == 0)
		return;
	if (!newReptContext(reptLineNo, body, size))
		return;

	contextStack.top().nbReptIters = count;
}

void fstk_RunFor(char const *symName, int32_t start, int32_t stop, int32_t step, int32_t reptLineNo,
                 char const *body, size_t size) {
	Symbol *sym = sym_AddVar(symName, start);

	if (sym->type != SYM_VAR)
		return;

	uint32_t count = 0;

	if (step > 0 && start < stop)
		count = ((int64_t)stop - start - 1) / step + 1;
	else if (step < 0 && stop < start)
		count = ((int64_t)start - stop - 1) / -(int64_t)step + 1;
	else if (step == 0)
		error("FOR cannot have a step value of 0\n");

	if ((step > 0 && start > stop) || (step < 0 && start < stop))
		warning(WARNING_BACKWARDS_FOR, "FOR goes backwards from %d to %d by %d\n", start, stop,
		        step);

	if (count == 0)
		return;
	if (!newReptContext(reptLineNo, body, size))
		return;

	Context &context = contextStack.top();

	context.nbReptIters = count;
	context.isForLoop = true;
	context.forValue = start;
	context.forStep = step;
	context.forName = symName;
}

void fstk_StopRept() {
	// Prevent more iterations
	contextStack.top().nbReptIters = 0;
}

bool fstk_Break() {
	if (contextStack.top().fileInfo->type != NODE_REPT) {
		error("BREAK can only be used inside a REPT/FOR block\n");
		return false;
	}

	fstk_StopRept();
	return true;
}

void fstk_NewRecursionDepth(size_t newDepth) {
	if (contextStack.size() > newDepth + 1)
		fatalerror("Recursion limit (%zu) exceeded\n", newDepth);
	maxRecursionDepth = newDepth;
}

void fstk_Init(char const *mainPath, size_t maxDepth) {
	Context &context = contextStack.emplace();

	if (!lexer_OpenFile(context.lexerState, mainPath))
		fatalerror("Failed to open main file\n");
	lexer_SetState(&context.lexerState);

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode();

	if (!fileInfo)
		fatalerror("Failed to allocate memory for main file info: %s\n", strerror(errno));
	fileInfo->type = NODE_FILE;
	fileInfo->data = lexer_GetFileName();
	// lineNo and nbReptIters are unused on the top-level context
	fileInfo->parent = nullptr;
	fileInfo->lineNo = 0; // This still gets written to the object file, so init it
	fileInfo->referenced = false;

	context.fileInfo = fileInfo;
	context.uniqueID = macro_UndefUniqueID();
	context.nbReptIters = 0;
	context.forValue = 0;
	context.forStep = 0;
	context.isForLoop = false;

	maxRecursionDepth = maxDepth;

	runPreIncludeFile();
}
