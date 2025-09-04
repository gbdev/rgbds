// SPDX-License-Identifier: MIT

#include "asm/format.hpp"

#include <algorithm>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "util.hpp" // isDigit

#include "asm/main.hpp" // options
#include "asm/warning.hpp"

static size_t parseNumber(char const *spec, size_t &value) {
	size_t i = 0;

	value = 0;
	for (; isDigit(spec[i]); ++i) {
		value = value * 10 + (spec[i] - '0');
	}

	return i;
}

size_t FormatSpec::parseSpec(char const *spec) {
	size_t i = 0;
	// <sign>
	if (char c = spec[i]; c == ' ' || c == '+') {
		++i;
		sign = c;
	}
	// <exact>
	if (spec[i] == '#') {
		++i;
		exact = true;
	}
	// <align>
	if (spec[i] == '-') {
		++i;
		alignLeft = true;
	}
	// <pad>
	if (spec[i] == '0') {
		++i;
		padZero = true;
	}
	// <width>
	if (isDigit(spec[i])) {
		i += parseNumber(&spec[i], width);
	}
	// <frac>
	if (spec[i] == '.') {
		++i;
		hasFrac = true;
		i += parseNumber(&spec[i], fracWidth);
	}
	// <prec>
	if (spec[i] == 'q') {
		++i;
		hasPrec = true;
		i += parseNumber(&spec[i], precision);
	}
	// <type>
	switch (char c = spec[i]; c) {
	case 'd':
	case 'u':
	case 'X':
	case 'x':
	case 'b':
	case 'o':
	case 'f':
	case 's':
		++i;
		type = c;
		break;
	}
	// Done parsing
	parsed = true;
	return i;
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
	if (!useType) {
		// No format was specified
		useType = 's';
	}

	if (sign) {
		error("Formatting string with sign flag '%c'", sign);
	}
	if (padZero) {
		error("Formatting string with padding flag '0'");
	}
	if (hasFrac) {
		error("Formatting string with fractional width");
	}
	if (hasPrec) {
		error("Formatting string with fractional precision");
	}
	if (useType != 's') {
		error("Formatting string as type '%c'", useType);
	}

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
	if (!useType) {
		// No format was specified; default to uppercase $hex
		useType = 'X';
		useExact = true;
	}

	if (useType != 'X' && useType != 'x' && useType != 'b' && useType != 'o' && useType != 'f'
	    && useExact) {
		error("Formatting type '%c' with exact flag '#'", useType);
	}
	if (useType != 'f' && hasFrac) {
		error("Formatting type '%c' with fractional width", useType);
	}
	if (useType != 'f' && hasPrec) {
		error("Formatting type '%c' with fractional precision", useType);
	}
	if (useType == 's') {
		error("Formatting number as type 's'");
	}

	char signChar = sign; // 0 or ' ' or '+'

	if (useType == 'd' || useType == 'f') {
		if (int32_t v = value; v < 0) {
			signChar = '-';
			if (v != INT32_MIN) { // -INT32_MIN is UB
				value = -v;
			}
		}
	}

	// The longest possible formatted number is fixed-point with 10 digits, 255 fractional digits,
	// and a precision suffix, for 270 total bytes (counting the NUL terminator).
	// (Actually 269 since a 2-digit precision cannot reach 10 integer digits.)
	// Make the buffer somewhat larger just in case.
	char valueBuf[300];
	if (useType == 'b') {
		// Special case for binary (since `snprintf` doesn't support it)

		// Buffer the digits from least to greatest
		char *ptr = valueBuf;
		do {
			*ptr++ = (value & 1) + '0';
			value >>= 1;
		} while (value);

		// Reverse the digits and terminate the string
		std::reverse(valueBuf, ptr);
		*ptr = '\0';
	} else if (useType == 'f') {
		// Special case for fixed-point (since it needs fractional part and precision)

		// Default fractional width (C++'s is 6 for "%f"; here 5 is enough for Q16.16)
		size_t useFracWidth = hasFrac ? fracWidth : 5;
		if (useFracWidth > 255) {
			error("Fractional width %zu too long, limiting to 255", useFracWidth);
			useFracWidth = 255;
		}

		// Default precision taken from default `-Q` option
		size_t defaultPrec = options.fixPrecision;
		size_t usePrec = hasPrec ? precision : defaultPrec;
		if (usePrec < 1 || usePrec > 31) {
			error(
			    "Fixed-point constant precision %zu invalid, defaulting to %zu",
			    usePrec,
			    defaultPrec
			);
			usePrec = defaultPrec;
		}

		// Floating-point formatting works for all fixed-point values
		double fval = fabs(value / pow(2.0, usePrec));
		if (int fracWidthArg = static_cast<int>(useFracWidth); useExact) {
			snprintf(valueBuf, sizeof(valueBuf), "%.*fq%zu", fracWidthArg, fval, usePrec);
		} else {
			snprintf(valueBuf, sizeof(valueBuf), "%.*f", fracWidthArg, fval);
		}
	} else {
		// `value` has already been made non-negative, so type 'd' is OK here even for `INT32_MIN`.
		// The sign will be printed later from `signChar`.
		char const *spec = useType == 'd' || useType == 'u' ? "%" PRIu32
		                   : useType == 'X'                 ? "%" PRIX32
		                   : useType == 'x'                 ? "%" PRIx32
		                   : useType == 'o'                 ? "%" PRIo32
		                                                    : "%" PRIu32;
		snprintf(valueBuf, sizeof(valueBuf), spec, value);
	}

	char prefixChar = !useExact                          ? 0
	                  : useType == 'X' || useType == 'x' ? '$'
	                  : useType == 'b'                   ? '%'
	                  : useType == 'o'                   ? '&'
	                                                     : 0;

	size_t valueLen = strlen(valueBuf);
	size_t numLen = (signChar != 0) + (prefixChar != 0) + valueLen;
	size_t totalLen = width > numLen ? width : numLen;
	size_t padLen = totalLen - numLen;

	str.reserve(str.length() + totalLen);
	if (alignLeft) {
		if (signChar) {
			str += signChar;
		}
		if (prefixChar) {
			str += prefixChar;
		}
		str.append(valueBuf);
		str.append(padLen, ' ');
	} else {
		if (padZero) {
			// sign, then prefix, then zero padding
			if (signChar) {
				str += signChar;
			}
			if (prefixChar) {
				str += prefixChar;
			}
			str.append(padLen, '0');
		} else {
			// space padding, then sign, then prefix
			str.append(padLen, ' ');
			if (signChar) {
				str += signChar;
			}
			if (prefixChar) {
				str += prefixChar;
			}
		}
		str.append(valueBuf);
	}
}
