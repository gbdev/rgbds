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
readUTF8Char(char *destination, char *source)
{
	int size;
	UBYTE first;
	first = source[0];

	if(first >= 0xFC)
	{
		size = 6;
	}
	else if(first >= 0xF8)
	{
		size = 5;
	}
	else if(first >= 0xF0)
	{
		size = 4;
	}
	else if(first >= 0xE0)
	{
		size = 3;
	}
	else if(first >= 0xC0)
	{
		size = 2;
	}
	else if(first != '\0')
	{
		size = 1;
	}
	else
	{
		size = 0;
	}
	strncpy(destination, source, size);
	destination[size] = 0;
	return size;
}

int
charmap_Add(char *input, UBYTE output)
{
	int i, input_length;
	char temp1i[CHARMAPLENGTH + 1], temp2i[CHARMAPLENGTH + 1], temp1o = 0, temp2o = 0;

	struct Charmap *charmap;

	if(pCurrentSection)
	{
		if(pCurrentSection -> charmap)
		{
			charmap = pCurrentSection -> charmap;
		}
		else
		{
			if((charmap = (struct Charmap *) calloc(1, sizeof(struct Charmap))) == NULL)
			{
				fatalerror("Not enough memory for charmap");
			}
			pCurrentSection -> charmap = charmap;
		}
	}
	else
	{
		charmap = &globalCharmap;
	}

	if(nPass == 2)
	{
		return charmap -> count;
	}

	if(charmap -> count > MAXCHARMAPS || strlen(input) > CHARMAPLENGTH)
	{
		return -1;
	}

	input_length = strlen(input);
	if(input_length > 1)
	{
		i = 0;
		while(i < charmap -> count + 1)
		{
			if(input_length > strlen(charmap -> input[i]))
			{
				memcpy(temp1i, charmap -> input[i], CHARMAPLENGTH + 1);
				memcpy(charmap -> input[i], input, input_length);
				temp1o = charmap -> output[i];
				charmap -> output[i] = output;
				i++;
				break;
			}
			i++;
		}
		while(i < charmap -> count + 1)
		{
			memcpy(temp2i, charmap -> input[i], CHARMAPLENGTH + 1);
			memcpy(charmap -> input[i], temp1i, CHARMAPLENGTH + 1);
			memcpy(temp1i, temp2i, CHARMAPLENGTH + 1);
			temp2o = charmap -> output[i];
			charmap -> output[i] = temp1o;
			temp1o = temp2o;
			i++;
		}
		memcpy(charmap -> input[charmap -> count + 1], temp1i, CHARMAPLENGTH + 1);
		charmap -> output[charmap -> count + 1] = temp1o;
	}
	else
	{
		memcpy(charmap -> input[charmap -> count], input, input_length);
		charmap -> output[charmap -> count] = output;
	}
	return ++charmap -> count;
}

int 
charmap_Convert(char **input)
{
	struct Charmap *charmap;

	char outchar[CHARMAPLENGTH + 1];
	char *buffer;
	int i, j, length;

	if(pCurrentSection && pCurrentSection -> charmap)
	{
		charmap = pCurrentSection -> charmap;
	}
	else
	{
		charmap = &globalCharmap;
	}

	if((buffer = (char *) malloc(strlen(*input))) == NULL)
	{
		fatalerror("Not enough memory for buffer");
	}

	length = 0;
	while(**input)
	{
		j = 0;
		for(i = 0; i < charmap -> count; i++)
		{
			j = strlen(charmap -> input[i]);
			if(memcmp(*input, charmap -> input[i], j) == 0)
			{
				outchar[0] = charmap -> output[i];
				outchar[1] = 0;
				break;
			}
			j = 0;
		}
		if(!j)
		{
			j = readUTF8Char(outchar, *input);
		}
		if(!outchar[0])
		{
			buffer[length++] = 0;
		}
		else
		{
			for(i = 0; outchar[i]; i++)
			{
				buffer[length++] = outchar[i];
			}
		}
		*input += j;
	}
	*input = buffer;
	return length;
}

