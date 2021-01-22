
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/lexer.h"
#include "asm/section.h"
#include "asm/warning.h"

struct OptStackEntry {
	char binary[2];
	char gbgfx[4];
	int32_t fillByte;
	struct OptStackEntry *next;
};

static struct OptStackEntry *stack = NULL;

void opt_B(char chars[2])
{
	lexer_SetBinDigits(chars);
}

void opt_G(char chars[4])
{
	lexer_SetGfxDigits(chars);
}

void opt_P(uint8_t fill)
{
	fillByte = fill;
}

void opt_Parse(char *s)
{
	switch (s[0]) {
	case 'b':
		if (strlen(&s[1]) == 2)
			opt_B(&s[1]);
		else
			error("Must specify exactly 2 characters for option 'b'\n");
		break;

	case 'g':
		if (strlen(&s[1]) == 4)
			opt_G(&s[1]);
		else
			error("Must specify exactly 4 characters for option 'g'\n");
		break;

	case 'p':
		if (strlen(&s[1]) <= 2) {
			int result;
			unsigned int fillchar;

			result = sscanf(&s[1], "%x", &fillchar);
			if (result != EOF && result != 1)
				error("Invalid argument for option 'p'\n");
			else
				opt_P(fillchar);
		} else {
			error("Invalid argument for option 'p'\n");
		}
		break;

	default:
		error("Unknown option '%c'\n", s[0]);
		break;
	}
}

void opt_Push(void)
{
	struct OptStackEntry *entry = malloc(sizeof(*entry));

	if (entry == NULL)
		fatalerror("Failed to alloc option stack entry: %s\n", strerror(errno));

	// Both of these pulled from lexer.h
	entry->binary[0] = binDigits[0];
	entry->binary[1] = binDigits[1];

	entry->gbgfx[0] = gfxDigits[0];
	entry->gbgfx[1] = gfxDigits[1];
	entry->gbgfx[2] = gfxDigits[2];
	entry->gbgfx[3] = gfxDigits[3];

	entry->fillByte = fillByte; // Pulled from section.h

	entry->next = stack;
	stack = entry;
}

void opt_Pop(void)
{
	if (stack == NULL) {
		error("No entries in the option stack\n");
		return;
	}

	struct OptStackEntry *entry = stack;

	opt_B(entry->binary);
	opt_G(entry->gbgfx);
	opt_P(entry->fillByte);
	stack = entry->next;
	free(entry);
}
