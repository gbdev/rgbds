/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/main.h"
#include "asm/output.h"

#include "extern/utf8decoder.h"

struct Charmap globalCharmap = {0};

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

int32_t charmap_Add(char *input, uint8_t output)
{
	int32_t i;
	size_t input_length;
	char temp1i[CHARMAPLENGTH + 1], temp2i[CHARMAPLENGTH + 1];
	char temp1o = 0, temp2o = 0;

	struct Charmap *charmap;

	if (pCurrentSection) {
		if (pCurrentSection->charmap) {
			charmap = pCurrentSection->charmap;
		} else {
			charmap = calloc(1, sizeof(struct Charmap));
			if (charmap == NULL)
				fatalerror("Not enough memory for charmap");

			pCurrentSection->charmap = charmap;
		}
	} else {
		charmap = &globalCharmap;
	}

	if (nPass == 2)
		return charmap->count;

	if (charmap->count > MAXCHARMAPS || strlen(input) > CHARMAPLENGTH)
		return -1;

	input_length = strlen(input);
	if (input_length > 1) {
		i = 0;
		while (i < charmap->count + 1) {
			if (input_length > strlen(charmap->input[i])) {
				memcpy(temp1i, charmap->input[i],
				       CHARMAPLENGTH + 1);
				memcpy(charmap->input[i], input, input_length);
				temp1o = charmap->output[i];
				charmap->output[i] = output;
				i++;
				break;
			}
			i++;
		}
		while (i < charmap->count + 1) {
			memcpy(temp2i, charmap->input[i], CHARMAPLENGTH + 1);
			memcpy(charmap->input[i], temp1i, CHARMAPLENGTH + 1);
			memcpy(temp1i, temp2i, CHARMAPLENGTH + 1);
			temp2o = charmap->output[i];
			charmap->output[i] = temp1o;
			temp1o = temp2o;
			i++;
		}
		memcpy(charmap->input[charmap->count + 1], temp1i,
		       CHARMAPLENGTH + 1);
		charmap->output[charmap->count + 1] = temp1o;
	} else {
		memcpy(charmap->input[charmap->count], input, input_length);
		charmap->output[charmap->count] = output;
	}
	return ++charmap->count;
}

int32_t charmap_Convert(char **input)
{
	struct Charmap *charmap;

	char outchar[CHARMAPLENGTH + 1];
	char *buffer;
	int32_t i, j, length;

	if (pCurrentSection && pCurrentSection->charmap)
		charmap = pCurrentSection->charmap;
	else
		charmap = &globalCharmap;

	buffer = malloc(strlen(*input));
	if (buffer == NULL)
		fatalerror("Not enough memory for buffer");

	length = 0;
	while (**input) {
		j = 0;
		for (i = 0; i < charmap->count; i++) {
			j = strlen(charmap->input[i]);
			if (memcmp(*input, charmap->input[i], j) == 0) {
				outchar[0] = charmap->output[i];
				outchar[1] = 0;
				break;
			}
			j = 0;
		}

		if (!j)
			j = readUTF8Char(outchar, *input);

		if (!outchar[0]) {
			buffer[length++] = 0;
		} else {
			for (i = 0; outchar[i]; i++)
				buffer[length++] = outchar[i];
		}
		*input += j;
	}
	*input = buffer;
	return length;
}
