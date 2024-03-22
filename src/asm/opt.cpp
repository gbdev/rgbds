/* SPDX-License-Identifier: MIT */

#include <ctype.h>
#include <errno.h>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fixpoint.hpp"
#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"
#include "asm/section.hpp"
#include "asm/warning.hpp"

static constexpr size_t numWarningStates = sizeof(warningStates);

struct OptStackEntry {
	char binary[2];
	char gbgfx[4];
	uint8_t fixPrecision;
	uint8_t fillByte;
	bool warningsAreErrors;
	size_t maxRecursionDepth;
	WarningState warningStates[numWarningStates];
};

static std::stack<OptStackEntry> stack;

void opt_B(char const chars[2]) {
	lexer_SetBinDigits(chars);
}

void opt_G(char const chars[4]) {
	lexer_SetGfxDigits(chars);
}

void opt_P(uint8_t padByte) {
	fillByte = padByte;
}

void opt_Q(uint8_t precision) {
	fixPrecision = precision;
}

void opt_R(size_t newDepth) {
	fstk_NewRecursionDepth(newDepth);
	lexer_CheckRecursionDepth();
}

void opt_W(char const *flag) {
	processWarningFlag(flag);
}

void opt_Parse(char const *s) {
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
			unsigned int padByte;

			result = sscanf(&s[1], "%x", &padByte);
			if (result != 1)
				error("Invalid argument for option 'p'\n");
			else if (padByte > 0xFF)
				error("Argument for option 'p' must be between 0 and 0xFF\n");
			else
				opt_P(padByte);
		} else {
			error("Invalid argument for option 'p'\n");
		}
		break;

		char const *precisionArg;
	case 'Q':
		precisionArg = &s[1];
		if (precisionArg[0] == '.')
			precisionArg++;
		if (strlen(precisionArg) <= 2) {
			int result;
			unsigned int precision;

			result = sscanf(precisionArg, "%u", &precision);
			if (result != 1)
				error("Invalid argument for option 'Q'\n");
			else if (precision < 1 || precision > 31)
				error("Argument for option 'Q' must be between 1 and 31\n");
			else
				opt_Q(precision);
		} else {
			error("Invalid argument for option 'Q'\n");
		}
		break;

	case 'r': {
		++s; // Skip 'r'
		while (isblank(*s))
			++s; // Skip leading whitespace

		if (s[0] == '\0') {
			error("Missing argument to option 'r'\n");
			break;
		}

		char *endptr;
		unsigned long newDepth = strtoul(s, &endptr, 10);

		if (*endptr != '\0') {
			error("Invalid argument to option 'r' (\"%s\")\n", s);
		} else if (errno == ERANGE) {
			error("Argument to 'r' is out of range (\"%s\")\n", s);
		} else {
			opt_R(newDepth);
		}
		break;
	}

	case 'W':
		if (strlen(&s[1]) > 0)
			opt_W(&s[1]);
		else
			error("Must specify an argument for option 'W'\n");
		break;

	default:
		error("Unknown option '%c'\n", s[0]);
		break;
	}
}

void opt_Push() {
	OptStackEntry entry;

	// Both of these are pulled from lexer.hpp
	entry.binary[0] = binDigits[0];
	entry.binary[1] = binDigits[1];

	entry.gbgfx[0] = gfxDigits[0];
	entry.gbgfx[1] = gfxDigits[1];
	entry.gbgfx[2] = gfxDigits[2];
	entry.gbgfx[3] = gfxDigits[3];

	entry.fixPrecision = fixPrecision; // Pulled from fixpoint.hpp

	entry.fillByte = fillByte; // Pulled from section.hpp

	// Both of these pulled from warning.hpp
	entry.warningsAreErrors = warningsAreErrors;
	memcpy(entry.warningStates, warningStates, numWarningStates);

	entry.maxRecursionDepth = maxRecursionDepth; // Pulled from fstack.h

	stack.push(entry);
}

void opt_Pop() {
	if (stack.empty()) {
		error("No entries in the option stack\n");
		return;
	}

	OptStackEntry entry = stack.top();
	stack.pop();

	opt_B(entry.binary);
	opt_G(entry.gbgfx);
	opt_P(entry.fillByte);
	opt_Q(entry.fixPrecision);
	opt_R(entry.maxRecursionDepth);

	// opt_W does not apply a whole warning state; it processes one flag string
	warningsAreErrors = entry.warningsAreErrors;
	memcpy(warningStates, entry.warningStates, numWarningStates);
}
