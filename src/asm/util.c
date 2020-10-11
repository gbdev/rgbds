/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <stdint.h>

#include "asm/main.h"
#include "asm/util.h"
#include "asm/warning.h"

#include "extern/utf8decoder.h"

/*
 * Calculate the hash value for a string
 */
uint32_t calchash(const char *s)
{
	uint32_t hash = 5381;

	while (*s != 0)
		hash = (hash * 33) ^ (*s++);

	return hash;
}

char const *print(int c)
{
	static char buf[5]; /* '\xNN' + '\0' */

	if (c == EOF)
		return "EOF";

	if (isprint(c)) {
		buf[0] = c;
		buf[1] = '\0';
		return buf;
	}

	buf[0] = '\\';
	switch (c) {
	case '\n':
		buf[1] = 'n';
		break;
	case '\r':
		buf[1] = 'r';
		break;
	case '\t':
		buf[1] = 't';
		break;

	default: /* Print as hex */
		buf[1] = 'x';
		sprintf(&buf[2], "%02hhx", c);
		return buf;
	}
	buf[2] = '\0';
	return buf;
}

size_t readUTF8Char(uint8_t *dest, char const *src)
{
	uint32_t state = 0;
	uint32_t codep;
	size_t i = 0;

	for (;;) {
		if (decode(&state, &codep, src[i]) == 1)
			fatalerror("invalid UTF-8 character\n");

		dest[i] = src[i];
		i++;

		if (state == 0)
			return i;
	}
}
