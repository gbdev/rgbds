// SPDX-License-Identifier: MIT

#include <errno.h>
#include <iterator> // std::size
#include <optional>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.hpp"
#include "util.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp" // options
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
		++s; // Skip a leading '-'
	}

	char c = *s++;

	while (isBlankSpace(*s)) {
		++s; // Skip leading blank spaces
	}

	switch (c) {
	case 'b':
		if (strlen(s) == 2) {
			opt_B(s);
		} else {
			error("Must specify exactly 2 characters for option 'b'");
		}
		break;

	case 'g':
		if (strlen(s) == 4) {
			opt_G(s);
		} else {
			error("Must specify exactly 4 characters for option 'g'");
		}
		break;

	case 'p':
		if (std::optional<uint64_t> padByte = parseWholeNumber(s); !padByte) {
			error("Invalid argument for option 'p'");
		} else if (*padByte > 0xFF) {
			error("Argument for option 'p' must be between 0 and 0xFF");
		} else {
			opt_P(*padByte);
		}
		break;

	case 'Q':
		if (s[0] == '.') {
			++s; // Skip leading '.'
		}
		if (std::optional<uint64_t> precision = parseWholeNumber(s); !precision) {
			error("Invalid argument for option 'Q'");
		} else if (*precision < 1 || *precision > 31) {
			error("Argument for option 'Q' must be between 1 and 31");
		} else {
			opt_Q(*precision);
		}
		break;

	case 'r':
		if (std::optional<uint64_t> maxRecursionDepth = parseWholeNumber(s); !maxRecursionDepth) {
			error("Invalid argument for option 'r'");
		} else if (errno == ERANGE) {
			error("Argument for option 'r' is out of range");
		} else {
			opt_R(*maxRecursionDepth);
		}
		break;

	case 'W':
		if (strlen(s) > 0) {
			opt_W(s);
		} else {
			error("Must specify an argument for option 'W'");
		}
		break;

	default:
		error("Unknown option '%c'", c);
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
