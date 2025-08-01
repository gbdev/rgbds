// SPDX-License-Identifier: MIT

#include "util.hpp"

#include <algorithm>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

bool startsIdentifier(int c) {
	// This returns false for anonymous labels, which internally start with a '!',
	// and for section fragment literal labels, which internally start with a '$'.
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '_';
}

bool continuesIdentifier(int c) {
	return startsIdentifier(c) || (c >= '0' && c <= '9') || c == '#' || c == '$' || c == '@';
}

char const *printChar(int c) {
	// "'A'" + '\0': 4 bytes
	// "'\\n'" + '\0': 5 bytes
	// "0xFF" + '\0': 5 bytes
	static char buf[5];

	if (c == EOF) {
		return "EOF";
	}

	if (isprint(c)) {
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

// FNV-1a hash of an uppercased string
size_t Uppercase::operator()(std::string const &str) const {
	size_t hash = 0x811C9DC5;
	for (char const &c : str) {
		hash = (hash ^ toupper(c)) * 16777619;
	}
	return hash;
}

// Compare two strings without case-sensitivity (by converting to uppercase)
bool Uppercase::operator()(std::string const &str1, std::string const &str2) const {
	return std::equal(RANGE(str1), RANGE(str2), [](char c1, char c2) {
		return toupper(c1) == toupper(c2);
	});
}
