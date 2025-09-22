// SPDX-License-Identifier: MIT

#include "util.hpp"

#include <errno.h>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <string.h> // strspn

#include "helpers.hpp" // assume

bool isNewline(int c) {
	return c == '\r' || c == '\n';
}

bool isBlankSpace(int c) {
	return c == ' ' || c == '\t';
}

bool isWhitespace(int c) {
	return isBlankSpace(c) || isNewline(c);
}

bool isPrintable(int c) {
	return c >= ' ' && c <= '~';
}

bool isLetter(int c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isDigit(int c) {
	return c >= '0' && c <= '9';
}

bool isBinDigit(int c) {
	return c == '0' || c == '1';
}

bool isOctDigit(int c) {
	return c >= '0' && c <= '7';
}

bool isHexDigit(int c) {
	return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

bool isAlphanumeric(int c) {
	return isLetter(c) || isDigit(c);
}

bool startsIdentifier(int c) {
	// This returns false for anonymous labels, which internally start with a '!',
	// and for section fragment literal labels, which internally start with a '$'.
	return isLetter(c) || c == '.' || c == '_';
}

bool continuesIdentifier(int c) {
	return startsIdentifier(c) || isDigit(c) || c == '#' || c == '$' || c == '@';
}

uint8_t parseHexDigit(int c) {
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else {
		assume(isDigit(c));
		return c - '0';
	}
}

// Parses a number from a string, moving the pointer to skip the parsed characters.
std::optional<uint64_t> parseNumber(char const *&str, NumberBase base) {
	// Identify the base if not specified
	// Does *not* support '+' or '-' sign prefix (unlike `strtoul` and `std::from_chars`)
	if (base == BASE_AUTO) {
		// Skips leading blank space (like `strtoul`)
		str += strspn(str, " \t");

		// Supports traditional ("0b", "0o", "0x") and RGBASM ('%', '&', '$') base prefixes
		switch (str[0]) {
		case '%':
			base = BASE_2;
			++str;
			break;
		case '&':
			base = BASE_8;
			++str;
			break;
		case '$':
			base = BASE_16;
			++str;
			break;
		case '0':
			switch (str[1]) {
			case 'B':
			case 'b':
				base = BASE_2;
				str += 2;
				break;
			case 'O':
			case 'o':
				base = BASE_8;
				str += 2;
				break;
			case 'X':
			case 'x':
				base = BASE_16;
				str += 2;
				break;
			default:
				base = BASE_10;
				break;
			}
			break;
		default:
			base = BASE_10;
			break;
		}
	}
	assume(base != BASE_AUTO);

	// Get the digit-condition function corresponding to the base
	bool (*canParseDigit)(int c) = base == BASE_2    ? isBinDigit
	                               : base == BASE_8  ? isOctDigit
	                               : base == BASE_10 ? isDigit
	                               : base == BASE_16 ? isHexDigit
	                                                 : nullptr; // LCOV_EXCL_LINE
	assume(canParseDigit != nullptr);

	char const * const startDigits = str;

	// Parse the number one digit at a time
	// Does *not* support '_' digit separators
	uint64_t result = 0;
	for (; canParseDigit(str[0]); ++str) {
		uint8_t digit = parseHexDigit(str[0]);
		if (result > (UINT64_MAX - digit) / base) {
			// Skip remaining digits and set errno = ERANGE on overflow
			while (canParseDigit(str[0])) {
				++str;
			}
			result = UINT64_MAX;
			errno = ERANGE;
			break;
		}
		result = result * base + digit;
	}

	// Return the parsed number if there were any digit characters
	if (str - startDigits == 0) {
		return std::nullopt;
	}
	return result;
}

// Parses a number from an entire string, returning nothing if there are more unparsed characters.
std::optional<uint64_t> parseWholeNumber(char const *str, NumberBase base) {
	std::optional<uint64_t> result = parseNumber(str, base);
	return str[0] == '\0' ? result : std::nullopt;
}

char const *printChar(int c) {
	// "'A'" + '\0': 4 bytes
	// "'\\n'" + '\0': 5 bytes
	// "0xFF" + '\0': 5 bytes
	static char buf[5];

	if (c == EOF) {
		return "EOF";
	}

	// Handle printable ASCII characters
	if (isPrintable(c)) {
		buf[0] = '\'';
		buf[1] = c;
		buf[2] = '\'';
		buf[3] = '\0';
		return buf;
	}

	switch (c) {
	case '\n':
		buf[2] = 'n';
		break;
	case '\r':
		buf[2] = 'r';
		break;
	case '\t':
		buf[2] = 't';
		break;
	case '\0':
		buf[2] = '0';
		break;

	default: // Print as hex
		buf[0] = '0';
		buf[1] = 'x';
		snprintf(&buf[2], 3, "%02hhX", static_cast<uint8_t>(c)); // includes the '\0'
		return buf;
	}
	buf[0] = '\'';
	buf[1] = '\\';
	buf[3] = '\'';
	buf[4] = '\0';
	return buf;
}
