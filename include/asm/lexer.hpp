// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_LEXER_HPP
#define RGBDS_ASM_LEXER_HPP

#include <deque>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "asm/intern.hpp"

enum LexerMode {
	LEXER_NORMAL,
	LEXER_RAW,
	LEXER_SKIP_TO_ELIF,
	LEXER_SKIP_TO_ENDC,
	LEXER_SKIP_TO_ENDR,
	NB_LEXER_MODES
};

struct Expansion {
	std::optional<InternedStr> name;
	std::shared_ptr<std::string> contents;
	size_t offset; // Cursor into `contents`

	size_t size() const { return contents->size(); }
	bool advance(); // Increment `offset`; return whether it then exceeds `contents`
};

struct ContentSpan {
	std::shared_ptr<char[]> ptr;
	size_t size;
};

struct IfStackEntry {
	bool ranIfBlock;       // Whether an IF/ELIF/ELSE block ran already
	bool reachedElseBlock; // Whether an ELSE block ran already
};

struct LexerState {
	std::string path;

	LexerMode mode;
	bool atLineStart;
	uint32_t lineNo;
	int lastToken;
	int nextToken;

	std::deque<IfStackEntry> ifStack; // Front is the innermost `IF` block

	bool capturing;     // Whether the text being lexed should be captured
	size_t captureSize; // Amount of text captured
	std::shared_ptr<std::vector<char>> captureBuf; // Buffer to send the captured text to if set

	bool enableExpansions;
	bool enableStringExpansions;
	size_t expansionScanDistance;         // Max distance already scanned for expansions
	std::deque<Expansion> expansionStack; // Front is the innermost current expansion

	ContentSpan content; // Span of chars
	size_t offset = 0;   // Cursor into `content.ptr`

	~LexerState();

	int peekChar();
	int peekCharAhead();

	void setAsCurrentState();
	void setFileAsNextState(std::string const &filePath, bool updateStateNow);
	void setViewAsNextState(char const *name, ContentSpan const &content_, uint32_t lineNo_);

	void clear(uint32_t lineNo_);
};

void lexer_SetBinDigits(char const digits[2]);
void lexer_SetGfxDigits(char const digits[4]);

bool lexer_AtTopLevel();
void lexer_RestartRept(uint32_t lineNo);
void lexer_SetMode(LexerMode mode);
void lexer_ToggleStringExpansion(bool enable);

uint32_t lexer_GetIFDepth();
void lexer_IncIFDepth();
void lexer_DecIFDepth();
bool lexer_RanIFBlock();
bool lexer_ReachedELSEBlock();
void lexer_RunIFBlock();
void lexer_ReachELSEBlock();

void lexer_CheckRecursionDepth();
uint32_t lexer_GetLineNo();
void lexer_TraceStringExpansions();

struct Capture {
	uint32_t lineNo;
	ContentSpan span;
};

Capture lexer_CaptureRept();
Capture lexer_CaptureMacro();

#endif // RGBDS_ASM_LEXER_HPP
