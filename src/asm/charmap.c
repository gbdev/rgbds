/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
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
#include "asm/warning.h"

#include "hashmap.h"

/*
 * Charmaps are stored using a structure known as "trie".
 * Essentially a tree, where each nodes stores a single character's worth of info:
 * whether there exists a mapping that ends at the current character,
 */
struct Charnode {
	bool isTerminal; /* Whether there exists a mapping that ends here */
	uint8_t value; /* If the above is true, its corresponding value */
	/* This MUST be indexes and not pointers, because pointers get invalidated by `realloc`!! */
	size_t next[255]; /* Indexes of where to go next, 0 = nowhere */
};

#define INITIAL_CAPACITY 32

struct Charmap {
	char *name;
	size_t usedNodes; /* How many nodes are being used */
	size_t capacity; /* How many nodes have been allocated */
	struct Charnode nodes[]; /* first node is reserved for the root node */
};

static HashMap charmaps;

static struct Charmap *currentCharmap;

struct CharmapStackEntry {
	struct Charmap *charmap;
	struct CharmapStackEntry *next;
};

struct CharmapStackEntry *charmapStack;

static inline struct Charmap *charmap_Get(const char *name)
{
	return hash_GetElement(charmaps, name);
}

static inline struct Charmap *resizeCharmap(struct Charmap *map, size_t capacity)
{
	struct Charmap *new = realloc(map, sizeof(*map) + sizeof(*map->nodes) * capacity);

	if (!new)
		fatalerror("Failed to %s charmap: %s\n",
			   map ? "create" : "resize", strerror(errno));
	new->capacity = capacity;
	return new;
}

static inline void initNode(struct Charnode *node)
{
	node->isTerminal = false;
	memset(node->next, 0, sizeof(node->next));
}

struct Charmap *charmap_New(const char *name, const char *baseName)
{
	struct Charmap *base = NULL;

	if (baseName != NULL) {
		base = charmap_Get(baseName);

		if (base == NULL)
			error("Base charmap '%s' doesn't exist\n", baseName);
	}

	struct Charmap *charmap = charmap_Get(name);

	if (charmap) {
		error("Charmap '%s' already exists\n", name);
		return charmap;
	}

	/* Init the new charmap's fields */
	if (base) {
		charmap = resizeCharmap(NULL, base->capacity);
		charmap->usedNodes = base->usedNodes;

		memcpy(charmap->nodes, base->nodes, sizeof(base->nodes[0]) * charmap->usedNodes);
	} else {
		charmap = resizeCharmap(NULL, INITIAL_CAPACITY);
		charmap->usedNodes = 1;
		initNode(&charmap->nodes[0]); /* Init the root node */
	}
	charmap->name = strdup(name);

	hash_AddElement(charmaps, charmap->name, charmap);
	currentCharmap = charmap;

	return charmap;
}

void charmap_Delete(struct Charmap *charmap)
{
	free(charmap->name);
	free(charmap);
}

void charmap_Set(const char *name)
{
	struct Charmap *charmap = charmap_Get(name);

	if (charmap == NULL)
		error("Charmap '%s' doesn't exist\n", name);
	else
		currentCharmap = charmap;
}

void charmap_Push(void)
{
	struct CharmapStackEntry *stackEntry;

	stackEntry = malloc(sizeof(*stackEntry));
	if (stackEntry == NULL)
		fatalerror("Failed to alloc charmap stack entry: %s\n", strerror(errno));

	stackEntry->charmap = currentCharmap;
	stackEntry->next = charmapStack;

	charmapStack = stackEntry;
}

void charmap_Pop(void)
{
	if (charmapStack == NULL) {
		error("No entries in the charmap stack\n");
		return;
	}

	struct CharmapStackEntry *top = charmapStack;

	currentCharmap = top->charmap;
	charmapStack = top->next;
	free(top);
}

void charmap_Add(char *mapping, uint8_t value)
{
	struct Charnode *node = &currentCharmap->nodes[0];

	for (uint8_t c; *mapping; mapping++) {
		c = *mapping - 1;

		if (node->next[c]) {
			node = &currentCharmap->nodes[node->next[c]];
		} else {
			/* Register next available node */
			node->next[c] = currentCharmap->usedNodes;
			/* If no more nodes are available, get new ones */
			if (currentCharmap->usedNodes == currentCharmap->capacity) {
				currentCharmap->capacity *= 2;
				currentCharmap = resizeCharmap(currentCharmap, currentCharmap->capacity);
				hash_ReplaceElement(charmaps, currentCharmap->name, currentCharmap);
			}

			/* Switch to and init new node */
			node = &currentCharmap->nodes[currentCharmap->usedNodes++];
			initNode(node);
		}
	}

	if (node->isTerminal)
		warning(WARNING_CHARMAP_REDEF, "Overriding charmap mapping");

	node->isTerminal = true;
	node->value = value;
}

size_t charmap_Convert(char const *input, uint8_t *output)
{
	/*
	 * The goal is to match the longest mapping possible.
	 * For that, advance through the trie with each character read.
	 * If that would lead to a dead end, rewind characters until the last match, and output.
	 * If no match, read a UTF-8 codepoint and output that.
	 */
	size_t outputLen = 0;
	struct Charnode const *node = &currentCharmap->nodes[0];
	struct Charnode const *match = NULL;
	size_t rewindDistance = 0;

	for (;;) {
		/* We still want NULs to reach the `else` path, to give a chance to rewind */
		uint8_t c = *input - 1;

		if (*input && node->next[c]) {
			input++; /* Consume that char */
			rewindDistance++;

			node = &currentCharmap->nodes[node->next[c]];
			if (node->isTerminal) {
				match = node;
				rewindDistance = 0; /* Rewind from after the match */
			}

		} else {
			input -= rewindDistance; /* Rewind */
			rewindDistance = 0;
			node = &currentCharmap->nodes[0];

			if (match) { /* Arrived at a dead end with a match found */
				*output++ = match->value;
				outputLen++;
				match = NULL; /* Reset match for next round */

			} else if (*input) { /* No match found */
				size_t codepointLen = readUTF8Char(output, input);

				input += codepointLen; /* OK because UTF-8 has no NUL in multi-byte chars */
				output += codepointLen;
				outputLen += codepointLen;
			}

			if (!*input)
				break;
		}
	}

	return outputLen;
}
