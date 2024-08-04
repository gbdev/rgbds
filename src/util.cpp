/* SPDX-License-Identifier: MIT */

#include "util.hpp"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "extern/utf8decoder.hpp"

char const *printChar(int c) {
	// "'A'" + '\0': 4 bytes
	// "'\\n'" + '\0': 5 bytes
	// "0xFF" + '\0': 5 bytes
	static char buf[5];

	if (c == EOF)
		return "EOF";

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
		snprintf(&buf[2], 3, "%02hhX", (uint8_t)c); // includes the '\0'
		return buf;
	}
	buf[0] = '\'';
	buf[1] = '\\';
	buf[3] = '\'';
	buf[4] = '\0';
	return buf;
}

size_t readUTF8Char(std::vector<int32_t> *dest, char const *src) {
	uint32_t state = 0, codepoint;
	size_t i = 0;

	for (;;) {
		if (decode(&state, &codepoint, src[i]) == 1)
			return 0;

		if (dest)
			dest->push_back(src[i]);
		i++;

		if (state == 0)
			return i;
	}
}
