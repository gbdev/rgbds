/* SPDX-License-Identifier: MIT */

#include "asm/format.hpp"

#include <algorithm>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fixpoint.hpp"
#include "asm/warning.hpp"

void FormatSpec::useCharacter(int c) {
	if (state == FORMAT_INVALID)
		return;

	switch (c) {
	// sign
	case ' ':
	case '+':
		if (state > FORMAT_SIGN)
			goto invalid;
		state = FORMAT_EXACT;
		sign = c;
		break;

	// exact
	case '#':
		if (state > FORMAT_EXACT)
			goto invalid;
		state = FORMAT_ALIGN;
		exact = true;
		break;

	// align
	case '-':
		if (state > FORMAT_ALIGN)
			goto invalid;
		state = FORMAT_WIDTH;
		alignLeft = true;
		break;

	// pad, width, and prec values
	case '0':
		if (state < FORMAT_WIDTH)
			padZero = true;
		[[fallthrough]];
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (state < FORMAT_WIDTH) {
			state = FORMAT_WIDTH;
			width = c - '0';
		} else if (state == FORMAT_WIDTH) {
			width = width * 10 + (c - '0');
		} else if (state == FORMAT_FRAC) {
			fracWidth = fracWidth * 10 + (c - '0');
		} else if (state == FORMAT_PREC) {
			precision = precision * 10 + (c - '0');
		} else {
			goto invalid;
		}
		break;

	// width
	case '.':
		if (state > FORMAT_WIDTH)
			goto invalid;
		state = FORMAT_FRAC;
		hasFrac = true;
		break;

	// prec
	case 'q':
		if (state > FORMAT_PREC)
			goto invalid;
		state = FORMAT_PREC;
		hasPrec = true;
		break;

	// type
	case 'd':
	case 'u':
	case 'X':
	case 'x':
	case 'b':
	case 'o':
	case 'f':
	case 's':
		if (state >= FORMAT_DONE)
			goto invalid;
		state = FORMAT_DONE;
		valid = true;
		type = c;
		break;

	default:
invalid:
		state = FORMAT_INVALID;
		valid = false;
	}
}

void FormatSpec::finishCharacters() {
	if (!isValid())
		state = FORMAT_INVALID;
}

static std::string escapeString(std::string const &str) {
	std::string escaped;
	for (char c : str) {
		// Escape characters that need escaping
		switch (c) {
		case '\n':
			escaped += "\\n";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\t':
			escaped += "\\t";
			break;
		case '\0':
			escaped += "\\0";
			break;
		case '\\':
		case '"':
		case '{':
			escaped += '\\';
			[[fallthrough]];
		default:
			escaped += c;
			break;
		}
	}
	return escaped;
}

void FormatSpec::appendString(std::string &str, std::string const &value) const {
	int useType = type;
	if (isEmpty()) {
		// No format was specified
		useType = 's';
	}

	if (sign)
		error("Formatting string with sign flag '%c'\n", sign);
	if (padZero)
		error("Formatting string with padding flag '0'\n");
	if (hasFrac)
		error("Formatting string with fractional width\n");
	if (hasPrec)
		error("Formatting string with fractional precision\n");
	if (useType != 's')
		error("Formatting string as type '%c'\n", useType);

	std::string useValue = exact ? escapeString(value) : value;
	size_t valueLen = useValue.length();
	size_t totalLen = width > valueLen ? width : valueLen;
	size_t padLen = totalLen - valueLen;

	str.reserve(str.length() + totalLen);
	if (alignLeft) {
		str.append(useValue);
		str.append(padLen, ' ');
	} else {
		str.append(padLen, ' ');
		str.append(useValue);
	}
}

void FormatSpec::appendNumber(std::string &str, uint32_t value) const {
	int useType = type;
	bool useExact = exact;
	if (isEmpty()) {
		// No format was specified; default to uppercase $hex
		useType = 'X';
		useExact = true;
	}

	if (useType != 'X' && useType != 'x' && useType != 'b' && useType != 'o' && useType != 'f' && useExact)
		error("Formatting type '%c' with exact flag '#'\n", useType);
	if (useType != 'f' && hasFrac)
		error("Formatting type '%c' with fractional width\n", useType);
	if (useType != 'f' && hasPrec)
		error("Formatting type '%c' with fractional precision\n", useType);
	if (useType == 's')
		error("Formatting number as type 's'\n");

	char signChar = sign; // 0 or ' ' or '+'

	if (useType == 'd' || useType == 'f') {
		if (int32_t v = value; v < 0) {
			signChar = '-';
			if (v != INT32_MIN)
				value = -v;
		}
	}

	char prefixChar = !useExact        ? 0
	                  : useType == 'X' ? '$'
	                  : useType == 'x' ? '$'
	                  : useType == 'b' ? '%'
	                  : useType == 'o' ? '&'
	                                   : 0;

	char valueBuf[262]; // Max 5 digits + decimal + 255 fraction digits + terminator

	if (useType == 'b') {
		// Special case for binary
		char *ptr = valueBuf;

		do {
			*ptr++ = (value & 1) + '0';
			value >>= 1;
		} while (value);

		// Reverse the digits
		std::reverse(valueBuf, ptr);

		*ptr = '\0';
	} else if (useType == 'f') {
		// Special case for fixed-point

		// Default fractional width (C++'s is 6 for "%f"; here 5 is enough for Q16.16)
		size_t useFracWidth = hasFrac ? fracWidth : 5;
		if (useFracWidth > 255) {
			error("Fractional width %zu too long, limiting to 255\n", useFracWidth);
			useFracWidth = 255;
		}

		size_t defaultPrec = fix_Precision();
		size_t usePrec = hasPrec ? precision : defaultPrec;
		if (usePrec < 1 || usePrec > 31) {
			error(
			    "Fixed-point constant precision %zu invalid, defaulting to %zu\n",
			    usePrec,
			    defaultPrec
			);
			usePrec = defaultPrec;
		}

		double fval = fabs(value / pow(2.0, usePrec));
		if (useExact)
			snprintf(valueBuf, sizeof(valueBuf), "%.*fq%zu", (int)useFracWidth, fval, usePrec);
		else
			snprintf(valueBuf, sizeof(valueBuf), "%.*f", (int)useFracWidth, fval);
	} else if (useType == 'd') {
		// Decimal numbers may be formatted with a '-' sign by `snprintf`, so `abs` prevents that,
		// with a special case for `INT32_MIN` since `labs(INT32_MIN)` is UB. The sign will be
		// printed later from `signChar`.
		uint32_t uval = value != (uint32_t)INT32_MIN ? labs((int32_t)value) : value;
		snprintf(valueBuf, sizeof(valueBuf), "%" PRIu32, uval);
	} else {
		char const *spec = useType == 'u'   ? "%" PRIu32
		                   : useType == 'X' ? "%" PRIX32
		                   : useType == 'x' ? "%" PRIx32
		                   : useType == 'o' ? "%" PRIo32
		                                    : "%" PRIu32;

		snprintf(valueBuf, sizeof(valueBuf), spec, value);
	}

	size_t valueLen = strlen(valueBuf);
	size_t numLen = (signChar != 0) + (prefixChar != 0) + valueLen;
	size_t totalLen = width > numLen ? width : numLen;
	size_t padLen = totalLen - numLen;

	str.reserve(str.length() + totalLen);
	if (alignLeft) {
		if (signChar)
			str += signChar;
		if (prefixChar)
			str += prefixChar;
		str.append(valueBuf);
		str.append(padLen, ' ');
	} else {
		if (padZero) {
			// sign, then prefix, then zero padding
			if (signChar)
				str += signChar;
			if (prefixChar)
				str += prefixChar;
			str.append(padLen, '0');
		} else {
			// space padding, then sign, then prefix
			str.append(padLen, ' ');
			if (signChar)
				str += signChar;
			if (prefixChar)
				str += prefixChar;
		}
		str.append(valueBuf);
	}
}
