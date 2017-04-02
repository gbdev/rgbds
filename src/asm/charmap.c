/*
 * UTF-8 decoder copyright © 2008–2009 Björn Höhrmann <bjoern@hoehrmann.de>
 * http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdint.h>

static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

uint32_t
decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != 0) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state*16 + type];
  return *state;
}

/*
 * Copyright © 2013 stag019 <stag019@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/main.h"
#include "asm/output.h"

struct Charmap globalCharmap = {0};

extern struct Section *pCurrentSection;

int
readUTF8Char(char *dest, char *src)
{
	uint32_t state;
	uint32_t codep;
	int i;

	for (i = 0, state = 0;; i++) {
		if (decode(&state, &codep, (uint8_t)src[i]) == 1) {
			fatalerror("invalid UTF-8 character");
		}

		dest[i] = src[i];

		i++;
		if (state == 0) {
			dest[i] = '\0';
			return i;
		}
		dest[i] = src[i];
	}
}

int
charmap_Add(char *input, UBYTE output)
{
	int i;
	size_t input_length;
	char temp1i[CHARMAPLENGTH + 1], temp2i[CHARMAPLENGTH + 1], temp1o = 0,
	    temp2o = 0;

	struct Charmap *charmap;

	if (pCurrentSection) {
		if (pCurrentSection->charmap) {
			charmap = pCurrentSection->charmap;
		} else {
			if ((charmap = calloc(1, sizeof(struct Charmap))) ==
			    NULL) {
				fatalerror("Not enough memory for charmap");
			}
			pCurrentSection->charmap = charmap;
		}
	} else {
		charmap = &globalCharmap;
	}

	if (nPass == 2) {
		return charmap->count;
	}

	if (charmap->count > MAXCHARMAPS || strlen(input) > CHARMAPLENGTH) {
		return -1;
	}

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

int
charmap_Convert(char **input)
{
	struct Charmap *charmap;

	char outchar[CHARMAPLENGTH + 1];
	char *buffer;
	int i, j, length;

	if (pCurrentSection && pCurrentSection->charmap) {
		charmap = pCurrentSection->charmap;
	} else {
		charmap = &globalCharmap;
	}

	if ((buffer = malloc(strlen(*input))) == NULL) {
		fatalerror("Not enough memory for buffer");
	}

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
		if (!j) {
			j = readUTF8Char(outchar, *input);
		}
		if (!outchar[0]) {
			buffer[length++] = 0;
		} else {
			for (i = 0; outchar[i]; i++) {
				buffer[length++] = outchar[i];
			}
		}
		*input += j;
	}
	*input = buffer;
	return length;
}
