
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "link/main.h"
#include "link/script.h"
#include "link/section.h"

#include "extern/err.h"

FILE *linkerScript;

static inline bool isWhiteSpace(int c)
{
	return c == ' ' || c == '\t';
}

static inline bool isNewline(int c)
{
	return c == '\r' || c == '\n';
}

/**
 * Try parsing a number, in base 16 if it begins with a dollar,
 * in base 10 otherwise
 * @param str The number to parse
 * @param number A pointer where the number will be written to
 * @return True if parsing was successful, false otherwise
 */
static bool tryParseNumber(char const *str, uint32_t *number)
{
	static char const digits[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F'
	};
	uint8_t base = 10;

	if (*str == '$') {
		str++;
		base = 16;
	}

	/* An empty string is not a number */
	if (!*str)
		return false;

	*number = 0;
	do {
		char chr = toupper(*str++);
		uint8_t digit = 0;

		while (digit < base) {
			if (chr == digits[digit])
				break;
			digit++;
		}
		if (digit == base)
			return false;
		*number = *number * base + digit;
	} while (*str);

	return true;
}

enum LinkerScriptTokenType {
	TOKEN_NEWLINE,
	TOKEN_COMMAND,
	TOKEN_BANK,
	TOKEN_NUMBER,
	TOKEN_SECTION,
	TOKEN_EOF,

	TOKEN_INVALID
};

enum LinkerScriptCommand {
	COMMAND_ORG,
	COMMAND_ALIGN,

	COMMAND_INVALID
};

struct LinkerScriptToken {
	enum LinkerScriptTokenType type;
	union LinkerScriptTokenAttr {
		enum LinkerScriptCommand command;
		enum SectionType secttype;
		uint32_t number;
		char *string;
	} attr;
};

static char const * const commands[] = {
	[COMMAND_ORG] = "ORG",
	[COMMAND_ALIGN] = "ALIGN"
};

static uint32_t lineNo;

static int readChar(FILE *file)
{
	int curchar = getc_unlocked(file);

	if (curchar == EOF && ferror(file))
		err(1, "%s: Unexpected error reading linker script", __func__);
	return curchar;
}

static struct LinkerScriptToken const *nextToken(void)
{
	static struct LinkerScriptToken token;
	int curchar;

	/* If the token has a string, make sure to avoid leaking it */
	if (token.type == TOKEN_SECTION)
		free(token.attr.string);

	/* Skip initial whitespace... */
	do
		curchar = readChar(linkerScript);
	while (isWhiteSpace(curchar));

	/* If this is a comment, skip to the end of the line */
	if (curchar == ';') {
		do
			curchar = readChar(linkerScript);
		while (!isNewline(curchar) && curchar != EOF);
	}

	if (curchar == EOF) {
		token.type = TOKEN_EOF;
	} else if (isNewline(curchar)) {
		/* If we have a newline char, this is a newline token */
		token.type = TOKEN_NEWLINE;

		/* FIXME: This works with CRLF newlines, but not CR-only */
		if (curchar == '\r')
			readChar(linkerScript); /* Read and discard LF */
	} else if (curchar == '"') {
		/* If we have a string start, this is a section name */
		token.type = TOKEN_SECTION;
		token.attr.string = NULL; /* Force initial alloc */

		size_t size = 0;
		size_t capacity = 16; /* Half of the default capacity */

		do {
			curchar = readChar(linkerScript);
			if (curchar == EOF || isNewline(curchar))
				errx(1, "Line %u: Unterminated string", lineNo);
			else if (curchar == '"')
				/* Quotes force a string termination */
				curchar = '\0';

			if (size >= capacity || token.attr.string == NULL) {
				capacity *= 2;
				token.attr.string = realloc(token.attr.string,
							    capacity);
				if (!token.attr.string)
					err(1, "%s: Failed to allocate memory for section name",
					    __func__);
			}
			token.attr.string[size++] = curchar;
		} while (curchar);
	} else {
		/* This is either a number, command or bank, that is: a word */
		char *str = NULL;
		size_t size = 0;
		size_t capacity = 8; /* Half of the default capacity */

		for (;;) {
			if (size >= capacity || str == NULL) {
				capacity *= 2;
				str = realloc(str, capacity);
				if (!str)
					err(1, "%s: Failed to allocate memory for token",
					    __func__);
			}
			str[size] = toupper(curchar);
			size++;

			if (!curchar)
				break;

			curchar = readChar(linkerScript);
			/* Whitespace, a newline or a comment end the token */
			if (isWhiteSpace(curchar) || isNewline(curchar)
			 || curchar == ';') {
				ungetc(curchar, linkerScript);
				curchar = '\0';
			}
		}

		token.type = TOKEN_INVALID;
		for (enum LinkerScriptCommand i = 0; i < COMMAND_INVALID; i++) {
			if (!strcmp(commands[i], str)) {
				token.type = TOKEN_COMMAND;
				token.attr.command = i;
				break;
			}
		}

		if (token.type == TOKEN_INVALID) {
			for (enum SectionType type = 0; type < SECTTYPE_INVALID;
			     type++) {
				if (!strcmp(typeNames[type], str)) {
					token.type = TOKEN_BANK;
					token.attr.secttype = type;
					break;
				}
			}
		}

		if (token.type == TOKEN_INVALID) {
			/* None of the strings matched, do we have a number? */
			if (tryParseNumber(str, &token.attr.number))
				token.type = TOKEN_NUMBER;
			else
				errx(1, "Unknown token \"%s\" on linker script line %u",
				     str, lineNo);
		}

		free(str);
	}

	return &token;
}

static void processCommand(enum LinkerScriptCommand command, uint16_t arg,
			   uint16_t *pc)
{
	switch (command) {
	case COMMAND_INVALID:
		trap_;

	case COMMAND_ORG:
		*pc = arg;
		break;

	case COMMAND_ALIGN:
		if (arg >= 16)
			arg = 0;
		else
			arg = (*pc + (1 << arg) - 1) & ~((1 << arg) - 1);
	}

	if (arg < *pc)
		errx(1, "Linkerscript line %u: `%s` cannot be used to go backwards",
		     lineNo, commands[command]);
	*pc = arg;
}

enum LinkerScriptParserState {
	PARSER_FIRSTTIME,
	PARSER_LINESTART,
	PARSER_LINEEND
};

/* Part of internal state, but has data that needs to be freed */
static uint16_t *curaddr[SECTTYPE_INVALID];

/* Put as global to ensure it's initialized only once */
static enum LinkerScriptParserState parserState = PARSER_FIRSTTIME;

struct SectionPlacement *script_NextSection(void)
{
	static struct SectionPlacement section;
	static enum SectionType type;
	static uint32_t bank;
	static uint32_t bankID;

	if (parserState == PARSER_FIRSTTIME) {
		lineNo = 1;

		/* Init PC for all banks */
		for (enum SectionType i = 0; i < SECTTYPE_INVALID; i++) {
			curaddr[i] = malloc(sizeof(*curaddr[i]) * nbbanks(i));
			for (uint32_t b = 0; b < nbbanks(i); b++)
				curaddr[i][b] = startaddr[i];
		}

		type = SECTTYPE_INVALID;

		parserState = PARSER_LINESTART;
	}

	for (;;) {
		struct LinkerScriptToken const *token = nextToken();
		enum LinkerScriptTokenType tokType;
		union LinkerScriptTokenAttr attr;
		bool hasArg;
		uint32_t arg;

		if (type != SECTTYPE_INVALID) {
			if (curaddr[type][bankID] > endaddr(type) + 1)
				errx(1, "Linkerscript line %u: PC overflowed (%u > %u)",
				     lineNo, curaddr[type][bankID],
				     endaddr(type));
			if (curaddr[type][bankID] < startaddr[type])
				errx(1, "Linkerscript line %u: PC underflowed (%u < %u)",
				     lineNo, curaddr[type][bankID],
				     startaddr[type]);
		}

		switch (parserState) {
		case PARSER_FIRSTTIME:
			trap_;

		case PARSER_LINESTART:
			switch (token->type) {
			case TOKEN_INVALID:
				trap_;

			case TOKEN_EOF:
				return NULL;

			case TOKEN_NUMBER:
				errx(1, "Linkerscript line %u: stray number",
				     lineNo);

			case TOKEN_NEWLINE:
				lineNo++;
				break;

			case TOKEN_SECTION:
				parserState = PARSER_LINEEND;

				if (type == SECTTYPE_INVALID)
					errx(1, "Linkerscript line %u: Didn't specify a location before the section",
					     lineNo);

				section.section =
					sect_GetSection(token->attr.string);
				section.org = curaddr[type][bankID];
				section.bank = bank;

				curaddr[type][bankID] += section.section->size;
				return &section;

			case TOKEN_COMMAND:
			case TOKEN_BANK:
				tokType = token->type;
				attr = token->attr;

				token = nextToken();
				hasArg = token->type == TOKEN_NUMBER;
				/*
				 * Leaving `arg` uninitialized when `!hasArg`
				 * causes GCC to warn about its use as an
				 * argument to `processCommand`. This cannot
				 * happen because `hasArg` has to be true, but
				 * silence the warning anyways.
				 * I dislike doing this because it could swallow
				 * actual errors, but I don't have a choice.
				 */
				arg = hasArg ? token->attr.number : 0;

				if (tokType == TOKEN_COMMAND) {
					if (type == SECTTYPE_INVALID)
						errx(1, "Linkerscript line %u: Didn't specify a location before the command",
						     lineNo);
					if (!hasArg)
						errx(1, "Linkerscript line %u: Command specified without an argument",
						     lineNo);

					processCommand(attr.command, arg,
						       &curaddr[type][bankID]);
				} else { /* TOKEN_BANK */
					type = attr.secttype;
					/*
					 * If there's only one bank,
					 * specifying the number is optional.
					 */
					if (!hasArg && nbbanks(type) != 1)
						errx(1, "Linkerscript line %u: Didn't specify a bank number",
						     lineNo);
					else if (!hasArg)
						arg = bankranges[type][0];
					else if (arg < bankranges[type][0])
						errx(1, "Linkerscript line %u: specified bank number is too low (%u < %u)",
						     lineNo, arg,
						     bankranges[type][0]);
					else if (arg > bankranges[type][1])
						errx(1, "Linkerscript line %u: specified bank number is too high (%u > %u)",
						     lineNo, arg,
						     bankranges[type][1]);
					bank = arg;
					bankID = arg - bankranges[type][0];
				}

				/* If we read a token we shouldn't have... */
				if (token->type != TOKEN_NUMBER)
					goto lineend;
				break;
			}
			break;

		case PARSER_LINEEND:
lineend:
			if (token->type == TOKEN_EOF)
				return NULL;
			else if (token->type != TOKEN_NEWLINE)
				errx(1, "Linkerscript line %u: Unexpected token at the end",
				     lineNo);
			lineNo++;
			parserState = PARSER_LINESTART;
			break;
		}
	}
}

void script_Cleanup(void)
{
	for (enum SectionType type = 0; type < SECTTYPE_INVALID; type++)
		free(curaddr[type]);
}
