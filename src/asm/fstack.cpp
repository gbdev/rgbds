/* SPDX-License-Identifier: MIT */

#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <new>
#include <optional>
#include <stack>
#include <stdlib.h>
#include <string_view>

#include "error.hpp"
#include "helpers.hpp"
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
	uint32_t nbReptIters = 0;
	bool isForLoop = false;
	int32_t forValue = 0;
	int32_t forStep = 0;
	std::string forName;
};

static std::stack<Context> contextStack;
size_t maxRecursionDepth;

// The first include path for `fstk_FindFile` to try is none at all
static std::vector<std::string> includePaths = {""};

static std::string preIncludeName;

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

std::string const &FileStackNode::dump(uint32_t curLineNo) const {
	Visitor visitor{
	    [this](std::vector<uint32_t> const &iters) -> std::string const & {
		    assert(this->parent); // REPT nodes use their parent's name
		    std::string const &lastName = this->parent->dump(this->lineNo);
		    fprintf(stderr, " -> %s", lastName.c_str());
		    for (uint32_t i = iters.size(); i--;)
			    fprintf(stderr, "::REPT~%" PRIu32, iters[i]);
		    return lastName;
	    },
	    [this](std::string const &name) -> std::string const & {
		    if (this->parent) {
			    this->parent->dump(this->lineNo);
			    fprintf(stderr, " -> %s", name.c_str());
		    } else {
			    fputs(name.c_str(), stderr);
		    }
		    return name;
	    },
	};
	std::string const &topName = std::visit(visitor, data);
	fprintf(stderr, "(%" PRIu32 ")", curLineNo);
	return topName;
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
		node->referenced = true;
		node->ID = -1;
	}
	return topNode;
}

void fstk_AddIncludePath(std::string const &path) {
	if (path.empty())
		return;

	std::string &includePath = includePaths.emplace_back(path);

	if (includePath.back() != '/')
		includePath += '/';
}

void fstk_SetPreIncludeFile(std::string const &path) {
	if (!preIncludeName.empty())
		warnx("Overriding pre-included filename %s", preIncludeName.c_str());
	preIncludeName = path;
	if (verbose)
		printf("Pre-included filename %s\n", preIncludeName.c_str());
}

static void printDep(std::string const &path) {
	if (dependfile) {
		fprintf(dependfile, "%s: %s\n", targetFileName.c_str(), path.c_str());
		if (generatePhonyDeps)
			fprintf(dependfile, "%s:\n", path.c_str());
	}
}

static bool isPathValid(std::string const &path) {
	struct stat statbuf;

	if (stat(path.c_str(), &statbuf) != 0)
		return false;

	// Reject directories
	return !S_ISDIR(statbuf.st_mode);
}

std::optional<std::string> fstk_FindFile(std::string const &path) {
	for (std::string &str : includePaths) {
		std::string fullPath = str + path;
		if (isPathValid(fullPath)) {
			printDep(fullPath);
			return fullPath;
		}
	}

	errno = ENOENT;
	if (generatedMissingIncludes)
		printDep(path);
	return std::nullopt;
}

bool yywrap() {
	uint32_t ifDepth = lexer_GetIFDepth();

	if (ifDepth != 0)
		fatalerror(
		    "Ended block with %" PRIu32 " unterminated IF construct%s\n",
		    ifDepth,
		    ifDepth == 1 ? "" : "s"
		);

	if (Context &context = contextStack.top(); context.fileInfo->type == NODE_REPT) {
		// The context is a REPT or FOR block, which may loop

		// If the node is referenced, we can't edit it; duplicate it
		if (context.fileInfo->referenced) {
			context.fileInfo = new (std::nothrow) FileStackNode(*context.fileInfo);
			if (!context.fileInfo)
				fatalerror("Failed to duplicate REPT file node: %s\n", strerror(errno));
			// Copy all info but the referencing
			context.fileInfo->referenced = false;
			context.fileInfo->ID = -1;
		}

		std::vector<uint32_t> &fileInfoIters = context.fileInfo->iters();

		// If this is a FOR, update the symbol value
		if (context.isForLoop && fileInfoIters.front() <= context.nbReptIters) {
			// Avoid arithmetic overflow runtime error
			uint32_t forValue = (uint32_t)context.forValue + (uint32_t)context.forStep;
			context.forValue = forValue <= INT32_MAX ? forValue : -(int32_t)~forValue - 1;
			Symbol *sym = sym_AddVar(context.forName, context.forValue);

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

	fileInfo.parent = contextStack.top().fileInfo;
	fileInfo.lineNo = lexer_GetLineNo();

	Context &context = contextStack.emplace();
	context.fileInfo = &fileInfo;
	return context;
}

void fstk_RunInclude(std::string const &path) {
	std::optional<std::string> fullPath = fstk_FindFile(path);
	if (!fullPath) {
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCLUDE file '%s' (%s)\n", path.c_str(), strerror(errno));
			failedOnMissingInclude = true;
		} else {
			error("Unable to open included file '%s': %s\n", path.c_str(), strerror(errno));
		}
		return;
	}

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode(NODE_FILE, *fullPath);
	if (!fileInfo) {
		error("Failed to alloc file info for INCLUDE: %s\n", strerror(errno));
		return;
	}

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
	if (preIncludeName.empty())
		return;

	std::optional<std::string> fullPath = fstk_FindFile(preIncludeName);
	if (!fullPath) {
		error("Unable to open included file '%s': %s\n", preIncludeName.c_str(), strerror(errno));
		return;
	}

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode(NODE_FILE, *fullPath);
	if (!fileInfo) {
		error("Failed to alloc file info for pre-include: %s\n", strerror(errno));
		return;
	}

	Context &context = newContext(*fileInfo);
	if (!lexer_OpenFile(context.lexerState, fileInfo->name().c_str()))
		fatalerror("Failed to set up lexer for file include\n");
	lexer_SetState(&context.lexerState);
	// We're back at top-level, so most things are reset
	context.uniqueID = macro_UndefUniqueID();
}

void fstk_RunMacro(std::string const &macroName, MacroArgs &args) {
	Symbol *macro = sym_FindExactSymbol(macroName);

	if (!macro) {
		error("Macro \"%s\" not defined\n", macroName.c_str());
		return;
	}
	if (macro->type != SYM_MACRO) {
		error("\"%s\" is not a macro\n", macroName.c_str());
		return;
	}
	contextStack.top().macroArgs = macro_GetCurrentArgs();

	FileStackNode *fileInfo = new (std::nothrow) FileStackNode(NODE_MACRO, "");
	if (!fileInfo) {
		error("Failed to alloc file info for \"%s\": %s\n", macro->name.c_str(), strerror(errno));
		return;
	}

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
			fileInfoName.append("::REPT~");
			fileInfoName.append(std::to_string(srcIters[i]));
		}
	}
	fileInfoName.append("::");
	fileInfoName.append(macro->name);

	Context &context = newContext(*fileInfo);
	std::string_view *macroView = macro->getMacro();
	lexer_OpenFileView(
	    context.lexerState, "MACRO", macroView->data(), macroView->size(), macro->fileLine
	);
	lexer_SetStateAtEOL(&context.lexerState);
	context.uniqueID = macro_UseNewUniqueID();
	macro_UseNewArgs(&args);
}

static bool newReptContext(int32_t reptLineNo, char const *body, size_t size) {
	FileStackNode *fileInfo = new (std::nothrow) FileStackNode(NODE_REPT, std::vector<uint32_t>{1});
	if (!fileInfo) {
		error("Failed to alloc file info for REPT: %s\n", strerror(errno));
		return false;
	}

	if (contextStack.top().fileInfo->type == NODE_REPT
	    && !contextStack.top().fileInfo->iters().empty()) {
		// Append all parent iter counts
		fileInfo->iters().insert(
		    fileInfo->iters().end(), RANGE(contextStack.top().fileInfo->iters())
		);
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

void fstk_RunFor(
    std::string const &symName,
    int32_t start,
    int32_t stop,
    int32_t step,
    int32_t reptLineNo,
    char const *body,
    size_t size
) {
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
		warning(
		    WARNING_BACKWARDS_FOR, "FOR goes backwards from %d to %d by %d\n", start, stop, step
		);

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

void fstk_Init(std::string const &mainPath, size_t maxDepth) {
	Context &context = contextStack.emplace();
	if (!lexer_OpenFile(context.lexerState, mainPath.c_str()))
		fatalerror("Failed to open main file\n");
	lexer_SetState(&context.lexerState);

	context.fileInfo = new (std::nothrow) FileStackNode(NODE_FILE, lexer_GetFileName());
	if (!context.fileInfo)
		fatalerror("Failed to allocate memory for main file info: %s\n", strerror(errno));
	// lineNo and nbReptIters are unused on the top-level context
	context.fileInfo->parent = nullptr;
	context.fileInfo->lineNo = 0; // This still gets written to the object file, so init it

	context.uniqueID = macro_UndefUniqueID();

	maxRecursionDepth = maxDepth;

	runPreIncludeFile();
}
