#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "asm/asm.h"
#include "asm/lexer.h"
#include "types.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/fstack.h"
#include "extern/err.h"

#include "asmy.h"

struct sLexString {
	char *tzName;
	ULONG nToken;
	ULONG nNameLength;
	struct sLexString *pNext;
};
#define pLexBufferRealStart (pCurrentBuffer->pBufferRealStart)
#define pLexBuffer		    (pCurrentBuffer->pBuffer)
#define AtLineStart	        (pCurrentBuffer->oAtLineStart)

#define SAFETYMARGIN	1024

extern size_t symvaluetostring(char *dest, size_t maxLength, char *sym);

struct sLexFloat tLexFloat[32];
struct sLexString *tLexHash[LEXHASHSIZE];
YY_BUFFER_STATE pCurrentBuffer;
ULONG nLexMaxLength; // max length of all keywords and operators

ULONG tFloatingSecondChar[256];
ULONG tFloatingFirstChar[256];
ULONG tFloatingChars[256];
ULONG nFloating;
enum eLexerState lexerstate = LEX_STATE_NORMAL;

void 
upperstring(char *s)
{
	while (*s) {
		*s = toupper(*s);
		s += 1;
	}
}

void 
lowerstring(char *s)
{
	while (*s) {
		*s = tolower(*s);
		s += 1;
	}
}

void 
yyskipbytes(ULONG count)
{
	pLexBuffer += count;
}

void 
yyunputbytes(ULONG count)
{
	pLexBuffer -= count;
}

void 
yyunput(char c)
{
	if (pLexBuffer <= pLexBufferRealStart)
		fatalerror("Buffer safety margin exceeded");

	*(--pLexBuffer) = c;
}

void 
yyunputstr(char *s)
{
	int i, len;

	len = strlen(s);

	if (pLexBuffer - len < pLexBufferRealStart)
		fatalerror("Buffer safety margin exceeded");

	for (i = len - 1; i >= 0; i--)
		*(--pLexBuffer) = s[i];
}

void 
yy_switch_to_buffer(YY_BUFFER_STATE buf)
{
	pCurrentBuffer = buf;
}

void 
yy_set_state(enum eLexerState i)
{
	lexerstate = i;
}

void 
yy_delete_buffer(YY_BUFFER_STATE buf)
{
	free(buf->pBufferStart - SAFETYMARGIN);
	free(buf);
}

YY_BUFFER_STATE 
yy_scan_bytes(char *mem, ULONG size)
{
	YY_BUFFER_STATE pBuffer;

	if ((pBuffer = malloc(sizeof(struct yy_buffer_state))) != NULL) {
		if ((pBuffer->pBufferRealStart =
		    malloc(size + 1 + SAFETYMARGIN)) != NULL) {
			pBuffer->pBufferStart = pBuffer->pBufferRealStart + SAFETYMARGIN;
			pBuffer->pBuffer = pBuffer->pBufferRealStart + SAFETYMARGIN;
			memcpy(pBuffer->pBuffer, mem, size);
			pBuffer->nBufferSize = size;
			pBuffer->oAtLineStart = 1;
			pBuffer->pBuffer[size] = 0;
			return (pBuffer);
		}
	}
	fatalerror("Out of memory!");
	return (NULL);
}

YY_BUFFER_STATE 
yy_create_buffer(FILE * f)
{
	YY_BUFFER_STATE pBuffer;

	if ((pBuffer = malloc(sizeof(struct yy_buffer_state))) != NULL) {
		ULONG size;

		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);

		if ((pBuffer->pBufferRealStart =
			malloc(size + 2 + SAFETYMARGIN)) != NULL) {
			char *mem;
			ULONG instring = 0;

			pBuffer->pBufferStart = pBuffer->pBufferRealStart + SAFETYMARGIN;
			pBuffer->pBuffer = pBuffer->pBufferRealStart + SAFETYMARGIN;

			size = fread(pBuffer->pBuffer, sizeof(UBYTE), size, f);

			pBuffer->pBuffer[size] = '\n';
			pBuffer->pBuffer[size + 1] = 0;
			pBuffer->nBufferSize = size + 1;

			mem = pBuffer->pBuffer;

			while (*mem) {
				if (*mem == '\"')
					instring = 1 - instring;

				if (mem[0] == '\\' &&
				    (mem[1] == '\"' || mem[1] == '\\')) {
					mem += 2;
				} else if (instring) {
					mem += 1;
				} else {
					if ((mem[0] == 10 && mem[1] == 13)
					 || (mem[0] == 13 && mem[1] == 10)) {
						mem[0] = ' ';
						mem[1] = '\n';
						mem += 2;
					} else if (mem[0] == 10 || mem[0] == 13) {
						mem[0] = '\n';
						mem += 1;
					} else if (mem[0] == '\n' && mem[1] == '*') {
						mem += 1;
						while (!(*mem == '\n' || *mem == '\0'))
							*mem++ = ' ';
					} else if (*mem == ';') {
						while (!(*mem == '\n' || *mem == '\0'))
							*mem++ = ' ';
					} else
						mem += 1;
				}
			}

			pBuffer->oAtLineStart = 1;
			return (pBuffer);
		}
	}
	fatalerror("Out of memory!");
	return (NULL);
}

ULONG
lex_FloatAlloc(struct sLexFloat *token)
{
	tLexFloat[nFloating] = *token;

	return (1 << (nFloating++));
}

/*
 * Make sure that only non-zero ASCII characters are used. Also, check if the
 * start is greater than the end of the range.
 */
void
lex_CheckCharacterRange(UWORD start, UWORD end)
{
	if (start > end || start < 1 || end > 127) {
		errx(1, "Invalid character range (start: %u, end: %u)",
		        start, end);
	}
}

void 
lex_FloatDeleteRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingChars[start] &= ~id;
		start += 1;
	}
}

void 
lex_FloatAddRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingChars[start] |= id;
		start += 1;
	}
}

void 
lex_FloatDeleteFirstRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingFirstChar[start] &= ~id;
		start += 1;
	}
}

void 
lex_FloatAddFirstRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingFirstChar[start] |= id;
		start += 1;
	}
}

void 
lex_FloatDeleteSecondRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingSecondChar[start] &= ~id;
		start += 1;
	}
}

void 
lex_FloatAddSecondRange(ULONG id, UWORD start, UWORD end)
{
	lex_CheckCharacterRange(start, end);

	while (start <= end) {
		tFloatingSecondChar[start] |= id;
		start += 1;
	}
}

struct sLexFloat *
lexgetfloat(ULONG nFloatMask)
{
	if (nFloatMask == 0) {
		fatalerror("Internal error in lexgetfloat");
	}

	int i = 0;

	while ((nFloatMask & 1) == 0) {
		nFloatMask >>= 1;
		i++;
	}

	return (&tLexFloat[i]);
}

ULONG 
lexcalchash(char *s)
{
	ULONG hash = 0;

	while (*s) {
		hash = (hash * 283) ^ toupper(*s++);
	}

	return (hash % LEXHASHSIZE);
}

void 
lex_Init(void)
{
	ULONG i;

	for (i = 0; i < LEXHASHSIZE; i++) {
		tLexHash[i] = NULL;
	}

	for (i = 0; i < 256; i++) {
		tFloatingFirstChar[i] = 0;
		tFloatingSecondChar[i] = 0;
		tFloatingChars[i] = 0;
	}

	nLexMaxLength = 0;
	nFloating = 0;
}

void 
lex_AddStrings(struct sLexInitString * lex)
{
	while (lex->tzName) {
		struct sLexString **ppHash;
		ULONG hash;

		ppHash = &tLexHash[hash = lexcalchash(lex->tzName)];
		while (*ppHash)
			ppHash = &((*ppHash)->pNext);

		if (((*ppHash) = malloc(sizeof(struct sLexString))) != NULL) {
			if (((*ppHash)->tzName =
				(char *) strdup(lex->tzName)) != NULL) {
				(*ppHash)->nNameLength = strlen(lex->tzName);
				(*ppHash)->nToken = lex->nToken;
				(*ppHash)->pNext = NULL;

				upperstring((*ppHash)->tzName);

				if ((*ppHash)->nNameLength > nLexMaxLength)
					nLexMaxLength = (*ppHash)->nNameLength;

			} else
				fatalerror("Out of memory!");
		} else
			fatalerror("Out of memory!");

		lex += 1;
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
void
yylex_GetFloatMaskAndFloatLen(ULONG *pnFloatMask, ULONG *pnFloatLen)
{
	// Note that '\0' should always have a bit mask of 0 in the "floating"
	// tables, so it doesn't need to be checked for separately.

	char *s = pLexBuffer;
	ULONG nOldFloatMask = 0;
	ULONG nFloatMask = tFloatingFirstChar[(int)*s];

	if (nFloatMask != 0) {
		s++;
		nOldFloatMask = nFloatMask;
		nFloatMask &= tFloatingSecondChar[(int)*s];

		while (nFloatMask != 0) {
			s++;
			nOldFloatMask = nFloatMask;
			nFloatMask &= tFloatingChars[(int)*s];
		}
	}

	*pnFloatMask = nOldFloatMask;
	*pnFloatLen = (ULONG)(s - pLexBuffer);
}

/*
 * Gets the longest keyword/operator from the current position in the buffer.
 */
struct sLexString *
yylex_GetLongestFixed()
{
	struct sLexString *pLongestFixed = NULL;
	char *s = pLexBuffer;
	ULONG hash = 0;
	ULONG length = 0;

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

size_t
CopyMacroArg(char *dest, size_t maxLength, char c)
{
	int i;
	char *s;
	int argNum;
	
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

	if ((s = sym_FindMacroArg(argNum)) == NULL)
		fatalerror("Macro argument not defined");

	for (i = 0; s[i] != 0; i++) {
		if (i >= maxLength) {
			fatalerror("Macro argument too long to fit buffer");
		}
		dest[i] = s[i];
	}

	return i;
}

static inline void
yylex_StringWriteChar(char *s, size_t index, char c)
{
	if (index >= MAXSTRLEN) {
		fatalerror("String too long");
	}

	s[index] = c;
}

static inline void
yylex_SymbolWriteChar(char *s, size_t index, char c)
{
	if (index >= MAXSYMLEN) {
		fatalerror("Symbol too long");
	}

	s[index] = c;
}

/*
 * Trims white space at the end of a string.
 * The index parameter is the index of the 0 at the end of the string.
 */
void yylex_TrimEnd(char *s, size_t index)
{
	int i;

	for (i = (int)index - 1; i >= 0 && (s[i] == ' ' || s[i] == '\t'); i--)
		s[i] = 0;
}

size_t
yylex_ReadBracketedSymbol(char *dest, size_t index)
{
	char sym[MAXSYMLEN + 1];
	char ch;
	size_t i = 0;
	size_t length, maxLength;

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
		} else
			yylex_SymbolWriteChar(sym, i++, ch);
	}

	yylex_SymbolWriteChar(sym, i, 0);

	maxLength = MAXSTRLEN - index; // it's assumed we're writing to a T_STRING
	length = symvaluetostring(&dest[index], maxLength, sym);

	if (*pLexBuffer == '}')
		pLexBuffer++;
	else
		yyerror("Missing }");

	return length;
}

void
yylex_ReadQuotedString()
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
			case 't':
				ch = '\t';
				break;
			case '\\':
				ch = '\\';
				break;
			case '"':
				ch = '"';
				break;
			default:
				maxLength = MAXSTRLEN - index;
				length = CopyMacroArg(&yylval.tzString[index], maxLength, ch);

				if (length != 0)
					index += length;
				else
					fatalerror("Illegal character escape '%c'", ch);

				ch = 0;
				break;
			}
		} else if (ch == '{') {
			// Get bracketed symbol within string.
			index += yylex_ReadBracketedSymbol(yylval.tzString, index);
			ch = 0;
		}

		if (ch)
			yylex_StringWriteChar(yylval.tzString, index++, ch);
	}

	yylex_StringWriteChar(yylval.tzString, index, 0);

	if (*pLexBuffer == '"')
		pLexBuffer++;
	else
		yyerror("Unterminated string");
}

ULONG
yylex_NORMAL()
{
	struct sLexString *pLongestFixed = NULL;
	ULONG nFloatMask, nFloatLen;
	ULONG linestart = AtLineStart;

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

	// Try to match an identifier, macro argument (e.g. \1),
	// or numeric literal.
	yylex_GetFloatMaskAndFloatLen(&nFloatMask, &nFloatLen);

	// Try to match a keyword or operator.
	pLongestFixed = yylex_GetLongestFixed();

	if (nFloatLen == 0 && pLongestFixed == NULL) {
		// No keyword, identifier, operator, or numerical literal matches.

		if (*pLexBuffer == '"') {
			pLexBuffer++;
			yylex_ReadQuotedString();
			return T_STRING;
		} else if (*pLexBuffer == '{') {
			pLexBuffer++;
			yylex_ReadBracketedSymbol(yylval.tzString, 0);
			return T_STRING;
		} else {
			// It's not a keyword, operator, identifier, macro argument,
			// numeric literal, string, or bracketed symbol, so just return
			// the ASCII character.
			if (*pLexBuffer == '\n')
				AtLineStart = 1;

			return *pLexBuffer++;
		}
	}

	if (pLongestFixed == NULL || nFloatLen > pLongestFixed->nNameLength) {
		// Longest match was an identifier, macro argument, or numeric literal.
		struct sLexFloat *token = lexgetfloat(nFloatMask);

		if (token->Callback) {
			int done = token->Callback(pLexBuffer, nFloatLen);
			if (!done)
				goto scanagain;
		}

		pLexBuffer += nFloatLen;

		if (token->nToken == T_ID && linestart) {
			return T_LABEL;
		} else {
			return token->nToken;
		}
	}

	// Longest match was a keyword or operator.
	pLexBuffer += pLongestFixed->nNameLength;
	return pLongestFixed->nToken;
}

ULONG
yylex_MACROARGS()
{
	size_t index = 0;
	size_t length, maxLength;

	while (*pLexBuffer == ' ' || *pLexBuffer == '\t') {
		pLexBuffer++;
	}

	while (*pLexBuffer != ',' && (*pLexBuffer != '\n')) {
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
			default:
				maxLength = MAXSTRLEN - index;
				length = CopyMacroArg(&yylval.tzString[index], maxLength, ch);

				if (length != 0)
					index += length;
				else
					fatalerror("Illegal character escape '%c'", ch);
				
				ch = 0;
				break;
			}
		} else if (ch == '{') {
			index += yylex_ReadBracketedSymbol(yylval.tzString, index);
			ch = 0;
		}
		if (ch)
			yylex_StringWriteChar(yylval.tzString, index++, ch);
	}

	if (index) {
		yylex_StringWriteChar(yylval.tzString, index, 0);

		// trim trailing white space at the end of the line
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

	fatalerror("Internal error in yylex_MACROARGS");
	return 0;
}

ULONG 
yylex(void)
{
	switch (lexerstate) {
	case LEX_STATE_NORMAL:
		return yylex_NORMAL();
	case LEX_STATE_MACROARGS:
		return yylex_MACROARGS();
	}

	fatalerror("Internal error in yylex");
	return 0;
}
