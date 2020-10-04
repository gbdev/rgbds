/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

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

size_t readUTF8Char(uint8_t *dest, char const *src)
{
	uint32_t state = 0;
	uint32_t codep;
	size_t i = 0;

	for (;;) {
		if (decode(&state, &codep, (uint8_t)src[i]) == 1)
			fatalerror("invalid UTF-8 character\n");

		dest[i] = src[i];
		i++;

		if (state == 0)
			return i;
	}
}
