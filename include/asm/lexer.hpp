/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_LEXER_H
#define RGBDS_ASM_LEXER_H

#include <deque>

#include "platform.hpp" // SSIZE_MAX

#define MAXSTRLEN 255

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
	char *name;
	union {
		char const *unowned;
		char *owned;
	} contents;
	size_t size; // Length of the contents
	size_t offset; // Cursor into the contents
	bool owned; // Whether or not to free contents when this expansion is freed
};

struct IfStackEntry {
	bool ranIfBlock; // Whether an IF/ELIF/ELSE block ran already
	bool reachedElseBlock; // Whether an ELSE block ran already
};

struct MmappedLexerState {
	char *ptr; // Technically `const` during the lexer's execution
	size_t size;
	size_t offset;
	bool isReferenced; // If a macro in this file requires not unmapping it
};

struct BufferedLexerState {
	int fd;
	size_t index; // Read index into the buffer
	char buf[LEXER_BUF_SIZE]; // Circular buffer
	size_t nbChars; // Number of "fresh" chars in the buffer
};

struct LexerState {
	char const *path;

	// mmap()-dependent IO state
	bool isMmapped;
	union {
		MmappedLexerState mmap; // If mmap()ed
		BufferedLexerState cbuf; // Otherwise
	};

	// Common state
	bool isFile;

	enum LexerMode mode;
	bool atLineStart;
	uint32_t lineNo;
	uint32_t colNo;
	int lastToken;

	std::deque<IfStackEntry> ifStack;

	bool capturing; // Whether the text being lexed should be captured
	size_t captureSize; // Amount of text captured
	char *captureBuf; // Buffer to send the captured text to if non-null
	size_t captureCapacity; // Size of the buffer above

	bool disableMacroArgs;
	bool disableInterpolation;
	size_t macroArgScanDistance; // Max distance already scanned for macro args
	bool expandStrings;
	std::deque<Expansion> expansions; // Front is the innermost current expansion
};

extern LexerState *lexerState;
extern LexerState *lexerStateEOL;

static inline void lexer_SetState(LexerState *state)
{
	lexerState = state;
}

static inline void lexer_SetStateAtEOL(LexerState *state)
{
	lexerStateEOL = state;
}

extern char binDigits[2];
extern char gfxDigits[4];

static inline void lexer_SetBinDigits(char const digits[2])
{
	binDigits[0] = digits[0];
	binDigits[1] = digits[1];
}

static inline void lexer_SetGfxDigits(char const digits[4])
{
	gfxDigits[0] = digits[0];
	gfxDigits[1] = digits[1];
	gfxDigits[2] = digits[2];
	gfxDigits[3] = digits[3];
}

// `path` is referenced, but not held onto..!
bool lexer_OpenFile(LexerState &state, char const *path);
void lexer_OpenFileView(LexerState &state, char const *path, char *buf, size_t size, uint32_t lineNo);
void lexer_RestartRept(uint32_t lineNo);
void lexer_CleanupState(LexerState &state);
void lexer_Init(void);
void lexer_SetMode(enum LexerMode mode);
void lexer_ToggleStringExpansion(bool enable);

uint32_t lexer_GetIFDepth(void);
void lexer_IncIFDepth(void);
void lexer_DecIFDepth(void);
bool lexer_RanIFBlock(void);
bool lexer_ReachedELSEBlock(void);
void lexer_RunIFBlock(void);
void lexer_ReachELSEBlock(void);

struct CaptureBody {
	uint32_t lineNo;
	char *body;
	size_t size;
};

void lexer_CheckRecursionDepth(void);
char const *lexer_GetFileName(void);
uint32_t lexer_GetLineNo(void);
uint32_t lexer_GetColNo(void);
void lexer_DumpStringExpansions(void);
int yylex(void);
bool lexer_CaptureRept(CaptureBody *capture);
bool lexer_CaptureMacroBody(CaptureBody *capture);

struct AlignmentSpec {
	uint8_t alignment;
	uint16_t alignOfs;
};

#endif // RGBDS_ASM_LEXER_H
