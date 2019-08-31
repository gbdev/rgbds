/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/util.h"

#define CHARMAP_HASH_SIZE (1 << 9)

struct CharmapStackEntry {
	struct Charmap *charmap;
	struct CharmapStackEntry *next;
};

static struct Charmap *tHashedCharmaps[CHARMAP_HASH_SIZE];

static struct Charmap *mainCharmap;
static struct Charmap *currentCharmap;

struct CharmapStackEntry *charmapStack;

static void warnSectionCharmap(void)
{
	static bool warned = false;

	if (warned)
		return;

	warning("Using 'charmap' within a section when the current charmap is 'main' is deprecated");
	warned = true;
}

static uint32_t charmap_CalcHash(const char *s)
{
	return calchash(s) % CHARMAP_HASH_SIZE;
}

static struct Charmap **charmap_Get(const char *name)
{
	struct Charmap **ppCharmap = &tHashedCharmaps[charmap_CalcHash(name)];

	while (*ppCharmap != NULL && strcmp((*ppCharmap)->name, name))
		ppCharmap = &(*ppCharmap)->next;

	return ppCharmap;
}

static void CopyNode(struct Charmap *dest,
		     const struct Charmap *src,
		     int nodeIdx)
{
	dest->nodes[nodeIdx].code = src->nodes[nodeIdx].code;
	dest->nodes[nodeIdx].isCode = src->nodes[nodeIdx].isCode;
	for (int i = 0; i < 256; i++)
		if (src->nodes[nodeIdx].next[i])
			dest->nodes[nodeIdx].next[i] = dest->nodes +
				(src->nodes[nodeIdx].next[i] - src->nodes);
}

struct Charmap *charmap_New(const char *name, const char *baseName)
{
	struct Charmap *pBase = NULL;

	if (baseName != NULL) {
		struct Charmap **ppBase = charmap_Get(baseName);

		if (*ppBase == NULL) {
			yyerror("Base charmap '%s' doesn't exist", baseName);
			return NULL;
		}

		pBase = *ppBase;
	}

	struct Charmap **ppCharmap = charmap_Get(name);

	if (*ppCharmap != NULL) {
		yyerror("Charmap '%s' already exists", name);
		return NULL;
	}

	*ppCharmap = calloc(1, sizeof(struct Charmap));

	if (*ppCharmap == NULL)
		fatalerror("Not enough memory for charmap");

	struct Charmap *pCharmap = *ppCharmap;

	snprintf(pCharmap->name, sizeof(pCharmap->name), "%s", name);

	if (pBase != NULL) {
		pCharmap->charCount = pBase->charCount;
		pCharmap->nodeCount = pBase->nodeCount;

		for (int i = 0; i < MAXCHARNODES; i++)
			CopyNode(pCharmap, pBase, i);
	}

	currentCharmap = pCharmap;

	return pCharmap;
}

void charmap_Set(const char *name)
{
	struct Charmap **ppCharmap = charmap_Get(name);

	if (*ppCharmap == NULL) {
		yyerror("Charmap '%s' doesn't exist", name);
		return;
	}

	currentCharmap = *ppCharmap;
}

void charmap_Push(void)
{
	struct CharmapStackEntry *stackEntry;

	stackEntry = malloc(sizeof(struct CharmapStackEntry));
	if (stackEntry == NULL)
		fatalerror("No memory for charmap stack");

	stackEntry->charmap = currentCharmap;
	stackEntry->next = charmapStack;

	charmapStack = stackEntry;
}

void charmap_Pop(void)
{
	if (charmapStack == NULL)
		fatalerror("No entries in the charmap stack");

	struct CharmapStackEntry *top = charmapStack;

	currentCharmap = top->charmap;
	charmapStack = top->next;
	free(top);
}

void charmap_InitMain(void)
{
	mainCharmap = charmap_New("main", NULL);
}

int32_t charmap_Add(char *input, uint8_t output)
{
	int32_t i;
	uint8_t v;

	struct Charmap 	*charmap;
	struct Charnode	*curr_node, *temp_node;

	/*
	 * If the user tries to define a character mapping inside a section
	 * and the current global charmap is the "main" one, then a local
	 * section charmap will be created or modified instead of the global
	 * one. In other words, the local section charmap can override the
	 * main global one, but not the others.
	 */
	if (pCurrentSection && currentCharmap == mainCharmap) {
		warnSectionCharmap();
		if (pCurrentSection->charmap) {
			charmap = pCurrentSection->charmap;
		} else {
			charmap = calloc(1, sizeof(struct Charmap));
			if (charmap == NULL)
				fatalerror("Not enough memory for charmap");
			pCurrentSection->charmap = charmap;
		}
	} else {
		charmap = currentCharmap;
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

	/*
	 * If there is a local section charmap and the current global charmap
	 * is the "main" one, the local one is used. Otherwise, the global
	 * one is used. In other words, the local section charmap can override
	 * the main global one, but not the others.
	 */
	if (pCurrentSection &&
	    pCurrentSection->charmap &&
	    currentCharmap == mainCharmap)
		charmap = pCurrentSection->charmap;
	else
		charmap = currentCharmap;

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
