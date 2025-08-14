// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.hpp" // assume
#include "util.hpp"    // isBlankSpace

#include "asm/fixpoint.hpp"
#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp" // options
#include "asm/section.hpp"
#include "asm/warning.hpp"

struct OptStackEntry {
	char binDigits[2];
	char gfxDigits[4];
	uint8_t fixPrecision;
	uint8_t padByte;
	size_t maxRecursionDepth;
	DiagnosticsState<WarningID> warningStates;
};

static std::stack<OptStackEntry> stack;

void opt_B(char const binDigits[2]) {
	lexer_SetBinDigits(binDigits);
}

void opt_G(char const gfxDigits[4]) {
	lexer_SetGfxDigits(gfxDigits);
}

void opt_P(uint8_t padByte) {
	options.padByte = padByte;
}

void opt_Q(uint8_t fixPrecision) {
	options.fixPrecision = fixPrecision;
}

void opt_R(size_t maxRecursionDepth) {
	fstk_NewRecursionDepth(maxRecursionDepth);
	lexer_CheckRecursionDepth();
}

void opt_W(char const *flag) {
	warnings.processWarningFlag(flag);
}

void opt_Parse(char const *s) {
	if (s[0] == '-') {
		++s;
	}
	switch (s[0]) {
	case 'b':
		if (strlen(&s[1]) == 2) {
			opt_B(&s[1]);
		} else {
			error("Must specify exactly 2 characters for option 'b'");
		}
		break;

	case 'g':
		if (strlen(&s[1]) == 4) {
			opt_G(&s[1]);
		} else {
			error("Must specify exactly 4 characters for option 'g'");
		}
		break;

	case 'p':
		if (strlen(&s[1]) <= 2) {
			int result;
			unsigned int padByte;

			result = sscanf(&s[1], "%x", &padByte);
			if (result != 1) {
				error("Invalid argument for option 'p'");
			} else {
				// Two characters cannot be scanned as a hex number greater than 0xFF
				assume(padByte <= 0xFF);
				opt_P(padByte);
			}
		} else {
			error("Invalid argument for option 'p'");
		}
		break;

		char const *precisionArg;
	case 'Q':
		precisionArg = &s[1];
		if (precisionArg[0] == '.') {
			++precisionArg;
		}
		if (strlen(precisionArg) <= 2) {
			int result;
			unsigned int fixPrecision;

			result = sscanf(precisionArg, "%u", &fixPrecision);
			if (result != 1) {
				error("Invalid argument for option 'Q'");
			} else if (fixPrecision < 1 || fixPrecision > 31) {
				error("Argument for option 'Q' must be between 1 and 31");
			} else {
				opt_Q(fixPrecision);
			}
		} else {
			error("Invalid argument for option 'Q'");
		}
		break;

	case 'r': {
		++s; // Skip 'r'
		while (isBlankSpace(*s)) {
			++s; // Skip leading blank spaces
		}

		if (s[0] == '\0') {
			error("Missing argument for option 'r'");
			break;
		}

		char *endptr;
		unsigned long maxRecursionDepth = strtoul(s, &endptr, 10);

		if (*endptr != '\0') {
			error("Invalid argument for option 'r' (\"%s\")", s);
		} else if (errno == ERANGE) {
			error("Argument for option 'r' is out of range (\"%s\")", s);
		} else {
			opt_R(maxRecursionDepth);
		}
		break;
	}

	case 'W':
		if (strlen(&s[1]) > 0) {
			opt_W(&s[1]);
		} else {
			error("Must specify an argument for option 'W'");
		}
		break;

	default:
		error("Unknown option '%c'", s[0]);
		break;
	}
}

void opt_Push() {
	OptStackEntry entry;

	memcpy(entry.binDigits, options.binDigits, std::size(options.binDigits));
	memcpy(entry.gfxDigits, options.gfxDigits, std::size(options.gfxDigits));
	entry.padByte = options.padByte;
	entry.fixPrecision = options.fixPrecision;
	entry.maxRecursionDepth = options.maxRecursionDepth;
	entry.warningStates = warnings.state;

	stack.push(entry);
}

void opt_Pop() {
	if (stack.empty()) {
		error("No entries in the option stack");
		return;
	}

	OptStackEntry entry = stack.top();
	stack.pop();

	opt_B(entry.binDigits);
	opt_G(entry.gfxDigits);
	opt_P(entry.padByte);
	opt_Q(entry.fixPrecision);
	opt_R(entry.maxRecursionDepth);

	// `opt_W` does not apply a whole warning state; it processes one flag string
	warnings.state = entry.warningStates;
}

void opt_CheckStack() {
	if (!stack.empty()) {
		warning(WARNING_UNMATCHED_DIRECTIVE, "`PUSHO` without corresponding `POPO`");
	}
}
