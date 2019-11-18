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

int32_t readUTF8Char(char *dest, char *src)
{
	uint32_t state;
	uint32_t codep;
	int32_t i;

	for (i = 0, state = 0;; i++) {
		if (decode(&state, &codep, (uint8_t)src[i]) == 1)
			fatalerror("invalid UTF-8 character");

		dest[i] = src[i];

		if (state == 0) {
			dest[++i] = '\0';
			return i;
		}
	}
}
