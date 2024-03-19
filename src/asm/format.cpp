/* SPDX-License-Identifier: MIT */

#include "asm/format.hpp"

#include <algorithm>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fixpoint.hpp"
#include "asm/lexer.hpp" // MAXSTRLEN
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
		state = FORMAT_PREFIX;
		sign = c;
		break;

	// prefix
	case '#':
		if (state > FORMAT_PREFIX)
			goto invalid;
		state = FORMAT_ALIGN;
		prefix = true;
		break;

	// align
	case '-':
		if (state > FORMAT_ALIGN)
			goto invalid;
		state = FORMAT_WIDTH;
		alignLeft = true;
		break;

	// pad and width
	case '0':
		if (state < FORMAT_WIDTH)
			padZero = true;
		// fallthrough
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
		} else {
			goto invalid;
		}
		break;

	case '.':
		if (state > FORMAT_WIDTH)
			goto invalid;
		state = FORMAT_FRAC;
		hasFrac = true;
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

std::string FormatSpec::formatString(std::string const &value) {
	if (isEmpty()) {
		// No format was specified
		type = 's';
	}

	if (sign)
		error("Formatting string with sign flag '%c'\n", sign);
	if (prefix)
		error("Formatting string with prefix flag '#'\n");
	if (padZero)
		error("Formatting string with padding flag '0'\n");
	if (hasFrac)
		error("Formatting string with fractional width\n");
	if (type != 's')
		error("Formatting string as type '%c'\n", type);

	size_t valueLen = value.length();
	size_t totalLen = width > valueLen ? width : valueLen;
	size_t padLen = totalLen - valueLen;

	std::string str;
	str.reserve(totalLen);
	if (alignLeft) {
		str.append(value);
		str.append(padLen, ' ');
	} else {
		str.append(padLen, ' ');
		str.append(value);
	}

	if (str.length() > MAXSTRLEN) {
		error("Formatted string value too long\n");
		str.resize(MAXSTRLEN);
	}

	return str;
}

std::string FormatSpec::formatNumber(uint32_t value) {
	if (isEmpty()) {
		// No format was specified; default to uppercase $hex
		type = 'X';
		prefix = true;
	}

	if (type != 'X' && type != 'x' && type != 'b' && type != 'o' && prefix)
		error("Formatting type '%c' with prefix flag '#'\n", type);
	if (type != 'f' && hasFrac)
		error("Formatting type '%c' with fractional width\n", type);
	if (type == 's')
		error("Formatting number as type 's'\n");

	char signChar = sign; // 0 or ' ' or '+'

	if (type == 'd' || type == 'f') {
		int32_t v = value;

		if (v < 0 && v != INT32_MIN) {
			signChar = '-';
			value = -v;
		}
	}

	char prefixChar = !prefix       ? 0
	                  : type == 'X' ? '$'
	                  : type == 'x' ? '$'
	                  : type == 'b' ? '%'
	                  : type == 'o' ? '&'
	                                : 0;

	char valueBuf[262]; // Max 5 digits + decimal + 255 fraction digits + terminator

	if (type == 'b') {
		// Special case for binary
		char *ptr = valueBuf;

		do {
			*ptr++ = (value & 1) + '0';
			value >>= 1;
		} while (value);

		// Reverse the digits
		std::reverse(valueBuf, ptr);

		*ptr = '\0';
	} else if (type == 'f') {
		// Special case for fixed-point

		// Default fractional width (C++'s is 6 for "%f"; here 5 is enough for Q16.16)
		size_t cappedFracWidth = hasFrac ? fracWidth : 5;

		if (cappedFracWidth > 255) {
			error("Fractional width %zu too long, limiting to 255\n", cappedFracWidth);
			cappedFracWidth = 255;
		}

		snprintf(
		    valueBuf, sizeof(valueBuf), "%.*f", (int)cappedFracWidth, value / fix_PrecisionFactor()
		);
	} else {
		char const *spec = type == 'd'   ? "%" PRId32
		                   : type == 'u' ? "%" PRIu32
		                   : type == 'X' ? "%" PRIX32
		                   : type == 'x' ? "%" PRIx32
		                   : type == 'o' ? "%" PRIo32
		                                 : "%" PRId32;

		snprintf(valueBuf, sizeof(valueBuf), spec, value);
	}

	size_t valueLen = strlen(valueBuf);
	size_t numLen = (signChar != 0) + (prefixChar != 0) + valueLen;
	size_t totalLen = width > numLen ? width : numLen;
	size_t padLen = totalLen - numLen;

	std::string str;
	str.reserve(totalLen);
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

	if (str.length() > MAXSTRLEN) {
		error("Formatted numeric value too long\n");
		str.resize(MAXSTRLEN);
	}

	return str;
}
