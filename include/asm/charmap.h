/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_CHARMAP_H
#define RGBDS_ASM_CHARMAP_H

#include <stdint.h>

#define MAXCHARMAPS	512
#define CHARMAPLENGTH	16
#define MAXCHARNODES	(MAXCHARMAPS * CHARMAPLENGTH + 1)

/*
 * A node for trie structure.
 */
struct Charnode {
	uint8_t code; /* the value in a key-value pair. */
	uint8_t isCode; /* has 1 if it's a code node, not just a bridge node. */
	struct Charnode *next[256]; /* each index representing the next possible
				     * character from its current state.
				     */
};

struct Charmap {
	char name[MAXSYMLEN + 1];
	int32_t charCount; /* user-side count. */
	int32_t nodeCount; /* node-side count. */
	struct Charnode nodes[MAXCHARNODES]; /* first node is reserved for the
					      * root node in charmap.
					      */
	struct Charmap *next; /* next charmap in hash table bucket */
};

void charmap_InitMain(void);
struct Charmap *charmap_New(const char *name, const char *baseName);
void charmap_Set(const char *name);
void charmap_Push(void);
void charmap_Pop(void);
int32_t charmap_Add(char *input, uint8_t output);
int32_t charmap_Convert(char **input);

#endif /* RGBDS_ASM_CHARMAP_H */
