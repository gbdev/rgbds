/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_LEXER_H
#define RGBDS_ASM_LEXER_H

#include <deque>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#include "platform.hpp" // SSIZE_MAX

#define LEXER_BUF_SIZE 42 // TODO: determine a sane value for this
// The buffer needs to be large enough for the maximum `peekInternal` lookahead distance
static_assert(LEXER_BUF_SIZE > 1, "Lexer buffer size is too small");
// This caps the size of buffer reads, and according to POSIX, passing more than SSIZE_MAX is UB
static_assert(LEXER_BUF_SIZE <= SSIZE_MAX, "Lexer buffer size is too large");

enum LexerMode {
	LEXER_NORMAL,
	LEXER_RAW,
	LEXER_SKIP_TO_ELIF,
	LEXER_SKIP_TO_ENDC,
	LEXER_SKIP_TO_ENDR,
	NB_LEXER_MODES
};

struct Expansion {
	std::optional<std::string> name;
	std::shared_ptr<std::string> contents;
	size_t offset; // Cursor into the contents

	size_t size() const { return contents->size(); }
};

struct IfStackEntry {
	bool ranIfBlock;       // Whether an IF/ELIF/ELSE block ran already
	bool reachedElseBlock; // Whether an ELSE block ran already
};

struct MmappedLexerState {
	char *ptr;
	size_t size;
	size_t offset;
	bool isReferenced; // If a macro in this file requires not unmapping it
};

struct ViewedLexerState {
	char const *ptr;
	size_t size;
	size_t offset;
};

struct BufferedLexerState {
	int fd;
	size_t index;             // Read index into the buffer
	char buf[LEXER_BUF_SIZE]; // Circular buffer
	size_t nbChars;           // Number of "fresh" chars in the buffer
};

struct LexerState {
	std::string path;

	LexerMode mode;
	bool atLineStart;
	uint32_t lineNo;
	uint32_t colNo;
	int lastToken;

	std::deque<IfStackEntry> ifStack;

	bool capturing;                // Whether the text being lexed should be captured
	size_t captureSize;            // Amount of text captured
	std::vector<char> *captureBuf; // Buffer to send the captured text to if non-null

	bool disableMacroArgs;
	bool disableInterpolation;
	size_t macroArgScanDistance; // Max distance already scanned for macro args
	bool expandStrings;
	std::deque<Expansion> expansions; // Front is the innermost current expansion

	std::variant<std::monostate, MmappedLexerState, ViewedLexerState, BufferedLexerState> content;

	LexerState() = default;
	LexerState(LexerState &&) = default;
	LexerState(LexerState const &) = delete;

	// This destructor unmaps or closes the content file if applicable.
	// As such, lexer states should not be copyable.
	~LexerState();

	LexerState &operator=(LexerState &&) = default;
	LexerState &operator=(LexerState const &) = delete;

	void setAsCurrentState();
	bool setFileAsNextState(std::string const &filePath, bool updateStateNow);
	void setViewAsNextState(char const *filePath, char const *buf, size_t size, uint32_t lineNo_);

	void clear(uint32_t lineNo_);
};

extern char binDigits[2];
extern char gfxDigits[4];

static inline void lexer_SetBinDigits(char const digits[2]) {
	binDigits[0] = digits[0];
	binDigits[1] = digits[1];
}

static inline void lexer_SetGfxDigits(char const digits[4]) {
	gfxDigits[0] = digits[0];
	gfxDigits[1] = digits[1];
	gfxDigits[2] = digits[2];
	gfxDigits[3] = digits[3];
}

void lexer_RestartRept(uint32_t lineNo);
void lexer_Init();
void lexer_SetMode(LexerMode mode);
void lexer_ToggleStringExpansion(bool enable);

uint32_t lexer_GetIFDepth();
void lexer_IncIFDepth();
void lexer_DecIFDepth();
bool lexer_RanIFBlock();
bool lexer_ReachedELSEBlock();
void lexer_RunIFBlock();
void lexer_ReachELSEBlock();

struct CaptureBody {
	uint32_t lineNo;
	char const *body;
	size_t size;

	void startCapture();
	void endCapture();
};

void lexer_CheckRecursionDepth();
uint32_t lexer_GetLineNo();
uint32_t lexer_GetColNo();
void lexer_DumpStringExpansions();
CaptureBody lexer_CaptureRept();
CaptureBody lexer_CaptureMacroBody();

#endif // RGBDS_ASM_LEXER_H
