/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "asm/asm.h"
#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/warning.h"

#include "extern/err.h"

#include "asmy.h"

struct sLexString {
	char *tzName;
	uint32_t nToken;
	uint32_t nNameLength;
	struct sLexString *pNext;
};

#define pLexBufferRealStart	(pCurrentBuffer->pBufferRealStart)
#define pLexBuffer		(pCurrentBuffer->pBuffer)
#define AtLineStart		(pCurrentBuffer->oAtLineStart)

#define SAFETYMARGIN		1024

#define BOM_SIZE 3

struct sLexFloat tLexFloat[32];
struct sLexString *tLexHash[LEXHASHSIZE];
YY_BUFFER_STATE pCurrentBuffer;
uint32_t nLexMaxLength; // max length of all keywords and operators

uint32_t tFloatingSecondChar[256];
uint32_t tFloatingFirstChar[256];
uint32_t tFloatingChars[256];
uint32_t nFloating;
enum eLexerState lexerstate = LEX_STATE_NORMAL;

struct sStringExpansionPos *pCurrentStringExpansion;
static unsigned int nNbStringExpansions;

/* UTF-8 byte order mark */
static const unsigned char bom[BOM_SIZE] = { 0xEF, 0xBB, 0xBF };

void upperstring(char *s)
{
	while (*s) {
		*s = toupper(*s);
		s++;
	}
}

void lowerstring(char *s)
{
	while (*s) {
		*s = tolower(*s);
		s++;
	}
}

void yyskipbytes(uint32_t count)
{
	pLexBuffer += count;
}

void yyunputbytes(uint32_t count)
{
	pLexBuffer -= count;
}

void yyunput(char c)
{
	if (pLexBuffer <= pLexBufferRealStart)
		fatalerror("Buffer safety margin exceeded");

	*(--pLexBuffer) = c;
}

void yyunputstr(const char *s)
{
	int32_t len;

	len = strlen(s);

	/*
	 * It would be undefined behavior to subtract `len` from pLexBuffer and
	 * potentially have it point outside of pLexBufferRealStart's buffer,
	 * this is why the check is done this way.
	 * Refer to https://github.com/rednex/rgbds/pull/411#discussion_r319779797
	 */
	if (pLexBuffer - pLexBufferRealStart < len)
		fatalerror("Buffer safety margin exceeded");

	pLexBuffer -= len;

	memcpy(pLexBuffer, s, len);
}

/*
 * Marks that a new string expansion with name `tzName` ends here
 * Enforces recursion depth
 */
void lex_BeginStringExpansion(const char *tzName)
{
	if (++nNbStringExpansions > nMaxRecursionDepth)
		fatalerror("Recursion limit (%d) exceeded", nMaxRecursionDepth);

	struct sStringExpansionPos *pNewStringExpansion =
		malloc(sizeof(*pNewStringExpansion));
	char *tzNewExpansionName = strdup(tzName);

	if (!pNewStringExpansion || !tzNewExpansionName)
		fatalerror("Could not allocate memory to expand '%s'",
			   tzName);

	pNewStringExpansion->tzName = tzNewExpansionName;
	pNewStringExpansion->pBuffer = pLexBufferRealStart;
	pNewStringExpansion->pBufferPos = pLexBuffer;
	pNewStringExpansion->pParent = pCurrentStringExpansion;

	pCurrentStringExpansion = pNewStringExpansion;
}

void yy_switch_to_buffer(YY_BUFFER_STATE buf)
{
	pCurrentBuffer = buf;
}

void yy_set_state(enum eLexerState i)
{
	lexerstate = i;
}

void yy_delete_buffer(YY_BUFFER_STATE buf)
{
	free(buf->pBufferStart - SAFETYMARGIN);
	free(buf);
}

/*
 * Maintains the following invariants:
 * 1. nBufferSize < capacity
 * 2. The buffer is terminated with 0
 * 3. nBufferSize is the size without the terminator
 */
static void yy_buffer_append(YY_BUFFER_STATE buf, size_t capacity, char c)
{
	assert(buf->pBufferStart[buf->nBufferSize] == 0);
	assert(buf->nBufferSize + 1 < capacity);

	buf->pBufferStart[buf->nBufferSize++] = c;
	buf->pBufferStart[buf->nBufferSize] = 0;
}

static void yy_buffer_append_newlines(YY_BUFFER_STATE buf, size_t capacity)
{
	/* Add newline if file doesn't end with one */
	if (buf->nBufferSize == 0
	 || buf->pBufferStart[buf->nBufferSize - 1] != '\n')
		yy_buffer_append(buf, capacity, '\n');

	/* Add newline if \ will eat the last newline */
	if (buf->nBufferSize >= 2) {
		size_t pos = buf->nBufferSize - 2;

		/* Skip spaces and tabs */
		while (pos > 0 && (buf->pBufferStart[pos] == ' '
				|| buf->pBufferStart[pos] == '\t'))
			pos--;

		if (buf->pBufferStart[pos] == '\\')
			yy_buffer_append(buf, capacity, '\n');
	}
}

YY_BUFFER_STATE yy_scan_bytes(char *mem, uint32_t size)
{
	YY_BUFFER_STATE pBuffer = malloc(sizeof(struct yy_buffer_state));

	if (pBuffer == NULL)
		fatalerror("%s: Out of memory!", __func__);

	size_t capacity = size + 3; /* space for 2 newlines and terminator */

	pBuffer->pBufferRealStart = malloc(capacity + SAFETYMARGIN);

	if (pBuffer->pBufferRealStart == NULL)
		fatalerror("%s: Out of memory for buffer!", __func__);

	pBuffer->pBufferStart = pBuffer->pBufferRealStart + SAFETYMARGIN;
	pBuffer->pBuffer = pBuffer->pBufferRealStart + SAFETYMARGIN;
	memcpy(pBuffer->pBuffer, mem, size);
	pBuffer->pBuffer[size] = 0;
	pBuffer->nBufferSize = size;
	yy_buffer_append_newlines(pBuffer, capacity);
	pBuffer->oAtLineStart = 1;

	return pBuffer;
}

YY_BUFFER_STATE yy_create_buffer(FILE *f)
{
	YY_BUFFER_STATE pBuffer = malloc(sizeof(struct yy_buffer_state));

	if (pBuffer == NULL)
		fatalerror("%s: Out of memory!", __func__);

	size_t size = 0, capacity = -1;
	char *buf = NULL;

	/*
	 * Check if we can get the file size without implementation-defined
	 * behavior:
	 *
	 * From ftell(3p):
	 * [On error], ftell() and ftello() shall return −1, and set errno to
	 * indicate the error.
	 *
	 * The ftell() and ftello() functions shall fail if: [...]
	 * ESPIPE The file descriptor underlying stream is associated with a
	 * pipe, FIFO, or socket.
	 *
	 * From fseek(3p):
	 * The behavior of fseek() on devices which are incapable of seeking
	 * is implementation-defined.
	 */
	if (ftell(f) != -1) {
		fseek(f, 0, SEEK_END);
		capacity = ftell(f);
		rewind(f);
	}

	// If ftell errored or the block above wasn't executed
	if (capacity == -1)
		capacity = 4096;
	// Handle 0-byte files gracefully
	else if (capacity == 0)
		capacity = 1;

	while (!feof(f)) {
		if (buf == NULL || size >= capacity) {
			if (buf)
				capacity *= 2;
			/* Give extra room for 2 newlines and terminator */
			buf = realloc(buf, capacity + SAFETYMARGIN + 3);

			if (buf == NULL)
				fatalerror("%s: Out of memory for buffer!",
					   __func__);
		}

		char *bufpos = buf + SAFETYMARGIN + size;
		size_t read_count = fread(bufpos, 1, capacity - size, f);

		if (read_count == 0 && !feof(f))
			fatalerror("%s: fread error", __func__);

		size += read_count;
	}

	pBuffer->pBufferRealStart = buf;
	pBuffer->pBufferStart = buf + SAFETYMARGIN;
	pBuffer->pBuffer = buf + SAFETYMARGIN;
	pBuffer->pBuffer[size] = 0;
	pBuffer->nBufferSize = size;

	/* This is added here to make the buffer scaling above easy to express,
	 * while taking the newline space into account
	 * for the yy_buffer_append_newlines() call below.
	 */
	capacity += 3;

	/* Skip UTF-8 byte order mark. */
	if (pBuffer->nBufferSize >= BOM_SIZE
	 && !memcmp(pBuffer->pBuffer, bom, BOM_SIZE))
		pBuffer->pBuffer += BOM_SIZE;

	/* Convert all line endings to LF and spaces */

	char *mem = pBuffer->pBuffer;
	int32_t lineCount = 0;

	while (*mem) {
		if ((mem[0] == '\\') && (mem[1] == '\"' || mem[1] == '\\')) {
			mem += 2;
		} else {
			/* LF CR and CR LF */
			if (((mem[0] == '\n') && (mem[1] == '\r'))
			 || ((mem[0] == '\r') && (mem[1] == '\n'))) {
				*mem++ = ' ';
				*mem++ = '\n';
				lineCount++;
			/* LF and CR */
			} else if ((mem[0] == '\n') || (mem[0] == '\r')) {
				*mem++ = '\n';
				lineCount++;
			} else {
				mem++;
			}
		}
	}

	if (mem != pBuffer->pBuffer + size) {
		nLineNo = lineCount + 1;
		fatalerror("Found null character");
	}

	/* Remove comments */

	mem = pBuffer->pBuffer;
	bool instring = false;

	while (*mem) {
		if (*mem == '\"')
			instring = !instring;

		if ((mem[0] == '\\') && (mem[1] == '\"' || mem[1] == '\\')) {
			mem += 2;
		} else if (instring) {
			mem++;
		} else {
			/* Comments that start with ; anywhere in a line */
			if (*mem == ';') {
				while (!((*mem == '\n') || (*mem == '\0')))
					*mem++ = ' ';
			/* Comments that start with * at the start of a line */
			} else if ((mem[0] == '\n') && (mem[1] == '*')) {
				mem++;
				while (!((*mem == '\n') || (*mem == '\0')))
					*mem++ = ' ';
			} else {
				mem++;
			}
		}
	}

	yy_buffer_append_newlines(pBuffer, capacity);
	pBuffer->oAtLineStart = 1;
	return pBuffer;
}

uint32_t lex_FloatAlloc(const struct sLexFloat *token)
{
	tLexFloat[nFloating] = *token;

	return (1 << (nFloating++));
}

/*
 * Make sure that only non-zero ASCII characters are used. Also, check if the
 * start is greater than the end of the range.
 */
bool lex_CheckCharacterRange(uint16_t start, uint16_t end)
{
	if (start > end || start < 1 || end > 127) {
		yyerror("Invalid character range (start: %u, end: %u)",
			start, end);
		return false;
	}
	return true;
}

void lex_FloatDeleteRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingChars[start] &= ~id;
			start++;
		}
	}
}

void lex_FloatAddRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingChars[start] |= id;
			start++;
		}
	}
}

void lex_FloatDeleteFirstRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingFirstChar[start] &= ~id;
			start++;
		}
	}
}

void lex_FloatAddFirstRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingFirstChar[start] |= id;
			start++;
		}
	}
}

void lex_FloatDeleteSecondRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingSecondChar[start] &= ~id;
			start++;
		}
	}
}

void lex_FloatAddSecondRange(uint32_t id, uint16_t start, uint16_t end)
{
	if (lex_CheckCharacterRange(start, end)) {
		while (start <= end) {
			tFloatingSecondChar[start] |= id;
			start++;
		}
	}
}

static struct sLexFloat *lexgetfloat(uint32_t nFloatMask)
{
	if (nFloatMask == 0)
		fatalerror("Internal error in %s", __func__);

	int32_t i = 0;

	while ((nFloatMask & 1) == 0) {
		nFloatMask >>= 1;
		i++;
	}

	return &tLexFloat[i];
}

static uint32_t lexcalchash(char *s)
{
	uint32_t hash = 0;

	while (*s)
		hash = (hash * 283) ^ toupper(*s++);

	return hash % LEXHASHSIZE;
}

void lex_Init(void)
{
	uint32_t i;

	for (i = 0; i < LEXHASHSIZE; i++)
		tLexHash[i] = NULL;

	for (i = 0; i < 256; i++) {
		tFloatingFirstChar[i] = 0;
		tFloatingSecondChar[i] = 0;
		tFloatingChars[i] = 0;
	}

	nLexMaxLength = 0;
	nFloating = 0;

	pCurrentStringExpansion = NULL;
	nNbStringExpansions = 0;
}

void lex_AddStrings(const struct sLexInitString *lex)
{
	while (lex->tzName) {
		struct sLexString **ppHash;
		uint32_t hash;

		ppHash = &tLexHash[hash = lexcalchash(lex->tzName)];
		while (*ppHash)
			ppHash = &((*ppHash)->pNext);

		*ppHash = malloc(sizeof(struct sLexString));
		if (*ppHash == NULL)
			fatalerror("Out of memory!");

		(*ppHash)->tzName = (char *)strdup(lex->tzName);
		if ((*ppHash)->tzName == NULL)
			fatalerror("Out of memory!");

		(*ppHash)->nNameLength = strlen(lex->tzName);
		(*ppHash)->nToken = lex->nToken;
		(*ppHash)->pNext = NULL;

		upperstring((*ppHash)->tzName);

		if ((*ppHash)->nNameLength > nLexMaxLength)
			nLexMaxLength = (*ppHash)->nNameLength;

		lex++;
	}
}

/*
 * Gets the "float" mask and "float" length.
 * "Float" refers to the token type of a token that is not a keyword.
 * The character classes floatingFirstChar, floatingSecondChar, and
 * floatingChars are defined separately for each token type.
 * It uses bit masks to match against a set of simple regular expressions
 * of the form /[floatingFirstChar]([floatingSecondChar][floatingChars]*)?/.
 * The token types with the longest match from the current position in the
 * buffer will have their bits set in the float mask.
 */
void yylex_GetFloatMaskAndFloatLen(uint32_t *pnFloatMask, uint32_t *pnFloatLen)
{
	/*
	 * Note that '\0' should always have a bit mask of 0 in the "floating"
	 * tables, so it doesn't need to be checked for separately.
	 */

	char *s = pLexBuffer;
	uint32_t nOldFloatMask = 0;
	uint32_t nFloatMask = tFloatingFirstChar[(uint8_t)*s];

	if (nFloatMask != 0) {
		s++;
		nOldFloatMask = nFloatMask;
		nFloatMask &= tFloatingSecondChar[(uint8_t)*s];

		while (nFloatMask != 0) {
			s++;
			nOldFloatMask = nFloatMask;
			nFloatMask &= tFloatingChars[(uint8_t)*s];
		}
	}

	*pnFloatMask = nOldFloatMask;
	*pnFloatLen = (uint32_t)(s - pLexBuffer);
}

/*
 * Gets the longest keyword/operator from the current position in the buffer.
 */
struct sLexString *yylex_GetLongestFixed(void)
{
	struct sLexString *pLongestFixed = NULL;
	char *s = pLexBuffer;
	uint32_t hash = 0;
	uint32_t length = 0;

	while (length < nLexMaxLength && *s) {
		hash = (hash * 283) ^ toupper(*s);
		s++;
		length++;

		struct sLexString *lex = tLexHash[hash % LEXHASHSIZE];

		while (lex) {
			if (lex->nNameLength == length
			 && strncasecmp(pLexBuffer, lex->tzName, length) == 0) {
				pLongestFixed = lex;
				break;
			}
			lex = lex->pNext;
		}
	}

	return pLongestFixed;
}

size_t CopyMacroArg(char *dest, size_t maxLength, char c)
{
	size_t i;
	char *s;
	int32_t argNum;

	switch (c) {
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		argNum = c - '0';
		break;
	case '@':
		argNum = -1;
		break;
	default:
		return 0;
	}

	s = sym_FindMacroArg(argNum);

	if (s == NULL)
		fatalerror("Macro argument not defined");

	for (i = 0; s[i] != 0; i++) {
		if (i >= maxLength)
			fatalerror("Macro argument too long to fit buffer");

		dest[i] = s[i];
	}

	return i;
}

static inline void yylex_StringWriteChar(char *s, size_t index, char c)
{
	if (index >= MAXSTRLEN)
		fatalerror("String too long");

	s[index] = c;
}

static inline void yylex_SymbolWriteChar(char *s, size_t index, char c)
{
	if (index >= MAXSYMLEN)
		fatalerror("Symbol too long");

	s[index] = c;
}

/*
 * Trims white space at the end of a string.
 * The index parameter is the index of the 0 at the end of the string.
 */
void yylex_TrimEnd(char *s, size_t index)
{
	int32_t i = (int32_t)index - 1;

	while ((i >= 0) && (s[i] == ' ' || s[i] == '\t')) {
		s[i] = 0;
		i--;
	}
}

size_t yylex_ReadBracketedSymbol(char *dest, size_t index)
{
	char sym[MAXSYMLEN + 1];
	char ch;
	size_t i = 0;
	size_t length, maxLength;
	const char *mode = NULL;

	for (ch = *pLexBuffer;
	     ch != '}' && ch != '"' && ch != '\n';
		 ch = *(++pLexBuffer)) {
		if (ch == '\\') {
			ch = *(++pLexBuffer);
			maxLength = MAXSYMLEN - i;
			length = CopyMacroArg(&sym[i], maxLength, ch);

			if (length != 0)
				i += length;
			else
				fatalerror("Illegal character escape '%c'", ch);
		} else if (ch == '{') {
			/* Handle nested symbols */
			++pLexBuffer;
			i += yylex_ReadBracketedSymbol(sym, i);
			--pLexBuffer;
		} else if (ch == ':' && !mode) { /* Only grab 1st colon */
			/* Use a whitelist of modes, which does prevent the
			 * use of some features such as precision,
			 * but also avoids a security flaw
			 */
			const char *acceptedModes = "bxXd";
			/* Binary isn't natively supported,
			 * so it's handled differently
			 */
			static const char * const formatSpecifiers[] = {
				"", "%x", "%X", "%d"
			};
			/* Prevent reading out of bounds! */
			const char *designatedMode;

			if (i != 1)
				fatalerror("Print types are exactly 1 character long");

			designatedMode = strchr(acceptedModes, sym[i - 1]);
			if (!designatedMode)
				fatalerror("Illegal print type '%c'",
					   sym[i - 1]);
			mode = formatSpecifiers[designatedMode - acceptedModes];
			/* Begin writing the symbol again */
			i = 0;
		} else {
			yylex_SymbolWriteChar(sym, i++, ch);
		}
	}

	/* Properly terminate the string */
	yylex_SymbolWriteChar(sym, i, 0);

	/* It's assumed we're writing to a T_STRING */
	maxLength = MAXSTRLEN - index;
	length = symvaluetostring(&dest[index], maxLength, sym, mode);

	if (*pLexBuffer == '}')
		pLexBuffer++;
	else
		fatalerror("Missing }");

	return length;
}

static void yylex_ReadQuotedString(void)
{
	size_t index = 0;
	size_t length, maxLength;

	while (*pLexBuffer != '"' && *pLexBuffer != '\n') {
		char ch = *pLexBuffer++;

		if (ch == '\\') {
			ch = *pLexBuffer++;

			switch (ch) {
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			case '\\':
				ch = '\\';
				break;
			case '"':
				ch = '"';
				break;
			case ',':
				ch = ',';
				break;
			case '{':
				ch = '{';
				break;
			case '}':
				ch = '}';
				break;
			default:
				maxLength = MAXSTRLEN - index;
				length = CopyMacroArg(&yylval.tzString[index],
						      maxLength, ch);

				if (length != 0)
					index += length;
				else
					fatalerror("Illegal character escape '%c'",
						   ch);

				ch = 0;
				break;
			}
		} else if (ch == '{') {
			// Get bracketed symbol within string.
			index += yylex_ReadBracketedSymbol(yylval.tzString,
							   index);
			ch = 0;
		}

		if (ch)
			yylex_StringWriteChar(yylval.tzString, index++, ch);
	}

	yylex_StringWriteChar(yylval.tzString, index, 0);

	if (*pLexBuffer == '"')
		pLexBuffer++;
	else
		fatalerror("Unterminated string");
}

static uint32_t yylex_NORMAL(void)
{
	struct sLexString *pLongestFixed = NULL;
	uint32_t nFloatMask, nFloatLen;
	uint32_t linestart = AtLineStart;

	AtLineStart = 0;

scanagain:
	while (*pLexBuffer == ' ' || *pLexBuffer == '\t') {
		linestart = 0;
		pLexBuffer++;
	}

	if (*pLexBuffer == 0) {
		// Reached the end of a file, macro, or rept.
		if (yywrap() == 0) {
			linestart = AtLineStart;
			AtLineStart = 0;
			goto scanagain;
		}
	}

	/* Check for line continuation character */
	if (*pLexBuffer == '\\') {
		/*
		 * Look for line continuation character after a series of
		 * spaces. This is also useful for files that use Windows line
		 * endings: "\r\n" is replaced by " \n" before the lexer has the
		 * opportunity to see it.
		 */
		if (pLexBuffer[1] == ' ' || pLexBuffer[1] == '\t') {
			pLexBuffer += 2;
			while (1) {
				if (*pLexBuffer == ' ' || *pLexBuffer == '\t') {
					pLexBuffer++;
				} else if (*pLexBuffer == '\n') {
					pLexBuffer++;
					nLineNo++;
					goto scanagain;
				} else {
					yyerror("Expected a new line after the continuation character.");
					pLexBuffer++;
				}
			}
		}

		/* Line continuation character */
		if (pLexBuffer[1] == '\n') {
			pLexBuffer += 2;
			nLineNo++;
			goto scanagain;
		}

		/*
		 * If there isn't a newline character or a space, ignore the
		 * character '\'. It will eventually be handled by other
		 * functions like PutMacroArg().
		 */
	}

	/*
	 * Try to match an identifier, macro argument (e.g. \1),
	 * or numeric literal.
	 */
	yylex_GetFloatMaskAndFloatLen(&nFloatMask, &nFloatLen);

	/* Try to match a keyword or operator. */
	pLongestFixed = yylex_GetLongestFixed();

	if (nFloatLen == 0 && pLongestFixed == NULL) {
		/*
		 * No keyword, identifier, operator, or numerical literal
		 * matches.
		 */

		if (*pLexBuffer == '"') {
			pLexBuffer++;
			yylex_ReadQuotedString();
			return T_STRING;
		} else if (*pLexBuffer == '{') {
			pLexBuffer++;
			size_t len = yylex_ReadBracketedSymbol(yylval.tzString,
							       0);
			yylval.tzString[len] = 0;
			return T_STRING;
		}

		/*
		 * It's not a keyword, operator, identifier, macro argument,
		 * numeric literal, string, or bracketed symbol, so just return
		 * the ASCII character.
		 */
		unsigned char ch = *pLexBuffer++;

		if (ch == '\n')
			AtLineStart = 1;

		/*
		 * Check for invalid unprintable characters.
		 * They may not be readily apparent in a text editor,
		 * so this is useful for identifying encoding problems.
		 */
		if (ch != 0
		 && ch != '\n'
		 && !(ch >= 0x20 && ch <= 0x7E))
			fatalerror("Found garbage character: 0x%02X", ch);

		return ch;
	}

	if (pLongestFixed == NULL || nFloatLen > pLongestFixed->nNameLength) {
		/*
		 * Longest match was an identifier, macro argument, or numeric
		 * literal.
		 */
		struct sLexFloat *token = lexgetfloat(nFloatMask);

		if (token->Callback) {
			int32_t done = token->Callback(pLexBuffer, nFloatLen);

			if (!done)
				goto scanagain;
		}

		if (token->nToken == T_ID && linestart)
			return T_LABEL;
		else
			return token->nToken;
	}

	/* Longest match was a keyword or operator. */
	pLexBuffer += pLongestFixed->nNameLength;
	yylval.nConstValue = pLongestFixed->nToken;
	return pLongestFixed->nToken;
}

static uint32_t yylex_MACROARGS(void)
{
	size_t index = 0;
	size_t length, maxLength;

	while ((*pLexBuffer == ' ') || (*pLexBuffer == '\t'))
		pLexBuffer++;

	while ((*pLexBuffer != ',') && (*pLexBuffer != '\n')) {
		char ch = *pLexBuffer++;

		if (ch == '\\') {
			ch = *pLexBuffer++;

			switch (ch) {
			case 'n':
				ch = '\n';
				break;
			case 't':
				ch = '\t';
				break;
			case '\\':
				ch = '\\';
				break;
			case '"':
				ch = '\"';
				break;
			case ',':
				ch = ',';
				break;
			case '{':
				ch = '{';
				break;
			case '}':
				ch = '}';
				break;
			case ' ':
			case '\t':
				/*
				 * Look for line continuation character after a
				 * series of spaces. This is also useful for
				 * files that use Windows line endings: "\r\n"
				 * is replaced by " \n" before the lexer has the
				 * opportunity to see it.
				 */
				while (1) {
					if (*pLexBuffer == ' '
					 || *pLexBuffer == '\t') {
						pLexBuffer++;
					} else if (*pLexBuffer == '\n') {
						pLexBuffer++;
						nLineNo++;
						ch = 0;
						break;
					} else {
						yyerror("Expected a new line after the continuation character.");
					}
				}
				break;
			case '\n':
				/* Line continuation character */
				nLineNo++;
				ch = 0;
				break;
			default:
				maxLength = MAXSTRLEN - index;
				length = CopyMacroArg(&yylval.tzString[index],
						      maxLength, ch);

				if (length != 0)
					index += length;
				else
					fatalerror("Illegal character escape '%c'",
						   ch);

				ch = 0;
				break;
			}
		} else if (ch == '{') {
			index += yylex_ReadBracketedSymbol(yylval.tzString,
							   index);
			ch = 0;
		}
		if (ch)
			yylex_StringWriteChar(yylval.tzString, index++, ch);
	}

	if (index) {
		yylex_StringWriteChar(yylval.tzString, index, 0);

		/* trim trailing white space at the end of the line */
		if (*pLexBuffer == '\n')
			yylex_TrimEnd(yylval.tzString, index);

		return T_STRING;
	} else if (*pLexBuffer == '\n') {
		pLexBuffer++;
		AtLineStart = 1;
		return '\n';
	} else if (*pLexBuffer == ',') {
		pLexBuffer++;
		return ',';
	}

	fatalerror("Internal error in %s", __func__);
}

int yylex(void)
{
	int returnedChar;

	switch (lexerstate) {
	case LEX_STATE_NORMAL:
		returnedChar = yylex_NORMAL();
		break;
	case LEX_STATE_MACROARGS:
		returnedChar = yylex_MACROARGS();
		break;
	default:
		fatalerror("%s: Internal error.", __func__);
	}

	/* Check if string expansions were fully read */
	while (pCurrentStringExpansion
	    && pCurrentStringExpansion->pBuffer == pLexBufferRealStart
	    && pCurrentStringExpansion->pBufferPos <= pLexBuffer) {
		struct sStringExpansionPos *pParent =
			pCurrentStringExpansion->pParent;
		free(pCurrentStringExpansion->tzName);
		free(pCurrentStringExpansion);

		pCurrentStringExpansion = pParent;
		nNbStringExpansions--;
	}

	return returnedChar;
}
