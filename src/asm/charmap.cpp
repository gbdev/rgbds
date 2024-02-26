/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "asm/charmap.hpp"
#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/warning.hpp"

#include "hashmap.hpp"
#include "util.hpp"

// Charmaps are stored using a structure known as "trie".
// Essentially a tree, where each nodes stores a single character's worth of info:
// whether there exists a mapping that ends at the current character,
struct Charnode {
	bool isTerminal; // Whether there exists a mapping that ends here
	uint8_t value; // If the above is true, its corresponding value
	// This MUST be indexes and not pointers, because pointers get invalidated by `realloc`!
	size_t next[255]; // Indexes of where to go next, 0 = nowhere
};

struct Charmap {
	char *name;
	std::vector<struct Charnode> *nodes; // first node is reserved for the root node
};

static HashMap charmaps;

// Store pointers to hashmap nodes, so that there is only one pointer to the memory block
// that gets reallocated.
static struct Charmap **currentCharmap;

struct CharmapStackEntry {
	struct Charmap **charmap;
	struct CharmapStackEntry *next;
};

struct CharmapStackEntry *charmapStack;

static struct Charmap *charmap_Get(char const *name)
{
	return (struct Charmap *)hash_GetElement(charmaps, name);
}

static void initNode(struct Charnode *node)
{
	node->isTerminal = false;
	memset(node->next, 0, sizeof(node->next));
}

struct Charmap *charmap_New(char const *name, char const *baseName)
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

	// Init the new charmap's fields
	charmap = (struct Charmap *)malloc(sizeof(*charmap));
	if (charmap)
		charmap->nodes = new(std::nothrow) std::vector<struct Charnode>();
	if (!charmap || !charmap->nodes)
		fatalerror("Failed to create charmap: %s\n", strerror(errno));

	if (base) {
		*charmap->nodes = *base->nodes; // Copies `base->nodes`
	} else {
		charmap->nodes->emplace_back();
		initNode(&charmap->nodes->back()); // Init the root node
	}
	charmap->name = strdup(name);

	currentCharmap = (struct Charmap **)hash_AddElement(charmaps, charmap->name, charmap);

	return charmap;
}

static void freeCharmap(void *_charmap, void *)
{
	struct Charmap *charmap = (struct Charmap *)_charmap;

	free(charmap->name);
	delete charmap->nodes;
	free(charmap);
}

void charmap_Cleanup(void)
{
	hash_ForEach(charmaps, freeCharmap, NULL);
}

void charmap_Set(char const *name)
{
	struct Charmap **charmap = (struct Charmap **)hash_GetNode(charmaps, name);

	if (charmap == NULL)
		error("Charmap '%s' doesn't exist\n", name);
	else
		currentCharmap = charmap;
}

void charmap_Push(void)
{
	struct CharmapStackEntry *stackEntry = (struct CharmapStackEntry *)malloc(sizeof(*stackEntry));

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
	struct Charmap *charmap = *currentCharmap;
	struct Charnode *node = &charmap->nodes->front();

	for (uint8_t c; *mapping; mapping++) {
		c = *mapping - 1;

		if (node->next[c]) {
			node = &(*charmap->nodes)[node->next[c]];
		} else {
			// Register next available node
			node->next[c] = charmap->nodes->size();

			// Switch to and init new node
			node = &charmap->nodes->emplace_back();
			initNode(node);
		}
	}

	if (node->isTerminal)
		warning(WARNING_CHARMAP_REDEF, "Overriding charmap mapping\n");

	node->isTerminal = true;
	node->value = value;
}

bool charmap_HasChar(char const *input)
{
	struct Charmap const *charmap = *currentCharmap;
	struct Charnode const *node = &charmap->nodes->front();

	for (; *input; input++) {
		size_t next = node->next[(uint8_t)*input - 1];

		if (!next)
			return false;
		node = &(*charmap->nodes)[next];
	}

	return node->isTerminal;
}

void charmap_Convert(char const *input, std::vector<uint8_t> &output)
{
	while (charmap_ConvertNext(&input, &output))
		;
}

size_t charmap_ConvertNext(char const **input, std::vector<uint8_t> *output)
{
	// The goal is to match the longest mapping possible.
	// For that, advance through the trie with each character read.
	// If that would lead to a dead end, rewind characters until the last match, and output.
	// If no match, read a UTF-8 codepoint and output that.
	struct Charmap const *charmap = *currentCharmap;
	struct Charnode const *node = &charmap->nodes->front();
	struct Charnode const *match = NULL;
	size_t rewindDistance = 0;

	for (;;) {
		uint8_t c = (uint8_t)**input - 1;

		if (**input && node->next[c]) {
			// Consume that char
			(*input)++;
			rewindDistance++;

			// Advance to next node (index starts at 1)
			node = &(*charmap->nodes)[node->next[c]];
			if (node->isTerminal) {
				// This node matches, register it
				match = node;
				rewindDistance = 0; // If no longer match is found, rewind here
			}

		} else {
			// We are at a dead end (either because we reached the end of input, or of
			// the trie), so rewind up to the last match, and output.
			*input -= rewindDistance; // This will rewind all the way if no match found

			if (match) { // A match was found, use it
				if (output)
					output->push_back(match->value);

				return 1;

			} else if (**input) { // No match found, but there is some input left
				int firstChar = **input;
				// This will write the codepoint's value to `output`, little-endian
				size_t codepointLen = readUTF8Char(output, *input);

				if (codepointLen == 0)
					error("Input string is not valid UTF-8\n");

				// OK because UTF-8 has no NUL in multi-byte chars
				*input += codepointLen;

				// Warn if this character is not mapped but any others are
				if (charmap->nodes->size() > 1)
					warning(WARNING_UNMAPPED_CHAR_1,
						"Unmapped character %s\n", printChar(firstChar));
				else if (strcmp(charmap->name, DEFAULT_CHARMAP_NAME))
					warning(WARNING_UNMAPPED_CHAR_2,
						"Unmapped character %s not in " DEFAULT_CHARMAP_NAME
						" charmap\n", printChar(firstChar));

				return codepointLen;

			} else { // End of input
				return 0;
			}
		}
	}
}
