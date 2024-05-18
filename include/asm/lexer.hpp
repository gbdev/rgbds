/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_LEXER_HPP
#define RGBDS_ASM_LEXER_HPP

#include <deque>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#include "platform.hpp" // SSIZE_MAX

// This value is a compromise between `LexerState` allocation performance when `mmap` works, and
// buffering performance when it doesn't/can't (e.g. when piping a file into RGBASM).
#define LEXER_BUF_SIZE 64
// The buffer needs to be large enough for the maximum `lexerState->peek()` lookahead distance
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
	size_t offset; // Cursor into `contents`

	size_t size() const { return contents->size(); }
	bool advance(); // Increment `offset`; return whether it then exceeds `contents`
};

struct ContentSpan {
	std::shared_ptr<char[]> ptr;
	size_t size;
};

struct ViewedContent {
	ContentSpan span;  // Span of chars
	size_t offset = 0; // Cursor into `span.ptr`

	ViewedContent(ContentSpan const &span_) : span(span_) {}
	ViewedContent(std::shared_ptr<char[]> ptr, size_t size) : span({.ptr = ptr, .size = size}) {}

	std::shared_ptr<char[]> makeSharedContentPtr() const {
		return std::shared_ptr<char[]>(span.ptr, &span.ptr[offset]);
	}
};

struct BufferedContent {
	int fd;                        // File from which to read chars
	char buf[LEXER_BUF_SIZE] = {}; // Circular buffer of chars
	size_t offset = 0;             // Cursor into `buf`
	size_t size = 0;               // Number of "fresh" chars in `buf`

	BufferedContent(int fd_) : fd(fd_) {}
	~BufferedContent();

	void advance(); // Increment `offset` circularly, decrement `size`
	void refill();  // Read from `fd` to fill `buf`

private:
	size_t readMore(size_t startIndex, size_t nbChars);
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
	uint32_t colNo;
	int lastToken;

	std::deque<IfStackEntry> ifStack;

	bool capturing;     // Whether the text being lexed should be captured
	size_t captureSize; // Amount of text captured
	std::shared_ptr<std::vector<char>> captureBuf; // Buffer to send the captured text to if set

	bool disableMacroArgs;
	bool disableInterpolation;
	size_t macroArgScanDistance; // Max distance already scanned for macro args
	bool expandStrings;
	std::deque<Expansion> expansions; // Front is the innermost current expansion

	std::variant<std::monostate, ViewedContent, BufferedContent> content;

	~LexerState();

	int peekChar();
	int peekCharAhead();

	std::shared_ptr<char[]> makeSharedCaptureBufPtr() const {
		return std::shared_ptr<char[]>(captureBuf, captureBuf->data());
	}

	void setAsCurrentState();
	bool setFileAsNextState(std::string const &filePath, bool updateStateNow);
	void setViewAsNextState(char const *name, ContentSpan const &span, uint32_t lineNo_);

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

void lexer_CheckRecursionDepth();
uint32_t lexer_GetLineNo();
uint32_t lexer_GetColNo();
void lexer_DumpStringExpansions();

struct Capture {
	uint32_t lineNo;
	ContentSpan span;
};

Capture lexer_CaptureRept();
Capture lexer_CaptureMacro();

#endif // RGBDS_ASM_LEXER_HPP
