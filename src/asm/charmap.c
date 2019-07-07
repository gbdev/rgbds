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
	uint8_t v;

	struct Charmap 	*charmap;
	struct Charnode	*curr_node, *temp_node;

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

	if (charmap->charCount >= MAXCHARMAPS || strlen(input) > CHARMAPLENGTH)
		return -1;

	curr_node = &charmap->nodes[0];

	for (i = 0; (v = (uint8_t)input[i]); i++) {
		if (curr_node->next[v]) {
			curr_node = curr_node->next[v];
		} else {
			temp_node = &charmap->nodes[charmap->nodeCount + 1];

			curr_node->next[v] = temp_node;
			curr_node = temp_node;

			++charmap->nodeCount;
		}
	}

	/* prevent duplicated keys by accepting only first key-value pair.  */
	if (curr_node->isCode)
		return charmap->charCount;

	curr_node->code = output;
	curr_node->isCode = 1;

	return ++charmap->charCount;
}

int32_t charmap_Convert(char **input)
{
	struct Charmap 	*charmap;
	struct Charnode	*charnode;

	char *output;
	char outchar[8];

	int32_t i, match, length;
	uint8_t v, foundCode;

	if (pCurrentSection && pCurrentSection->charmap)
		charmap = pCurrentSection->charmap;
	else
		charmap = &globalCharmap;

	output = malloc(strlen(*input));
	if (output == NULL)
		fatalerror("Not enough memory for buffer");

	length = 0;

	while (**input) {
		charnode = &charmap->nodes[0];

		/*
		 * find the longest valid match which has been registered in charmap.
		 * note that there could be either multiple matches or no match.
		 * and it possibly takes the longest match between them,
		 * which means that it ignores partial matches shorter than the longest one.
		*/
		for (i = match = 0; (v = (*input)[i]);) {
			if (!charnode->next[v])
				break;

			charnode = charnode->next[v];
			i++;

			if (charnode->isCode) {
				match = i;
				foundCode = charnode->code;
			}
		}

		if (match) {
			output[length] = foundCode;

			length += 1;
		} else {
			/*
			 * put a utf-8 character
			 * if failed to find a match.
			 */
			match = readUTF8Char(outchar, *input);

			if (match) {
				memcpy(output + length, *input, match);
			} else {
				output[length] = 0;
				match = 1;
			}

			length += match;
		}

		*input += match;
	}

	*input = output;

	return length;
}
