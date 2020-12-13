/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hashmap.h"
#include "extern/err.h"

/*
 * The lower half of the hash is used to index the "master" table,
 * the upper half is used to help resolve collisions more quickly
 */
#define UINT_BITS_(NB_BITS) uint##NB_BITS##_t
#define UINT_BITS(NB_BITS)  UINT_BITS_(NB_BITS)
typedef UINT_BITS(HASH_NB_BITS) HashType;
typedef UINT_BITS(HALF_HASH_NB_BITS) HalfHashType;

struct HashMapEntry {
	HalfHashType hash;
	char const *key;
	void *content;
	struct HashMapEntry *next;
};

#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME 16777619

/* FNV-1a hash */
static HashType hash(char const *str)
{
	HashType hash = FNV_OFFSET_BASIS;

	while (*str) {
		hash ^= (uint8_t)*str++;
		hash *= FNV_PRIME;
	}
	return hash;
}

bool hash_AddElement(HashMap map, char const *key, void *element)
{
	HashType hashedKey = hash(key);
	HalfHashType index = hashedKey;
	struct HashMapEntry *newEntry = malloc(sizeof(*newEntry));

	if (!newEntry)
		err(1, "%s: Failed to allocate new entry", __func__);

	newEntry->hash = hashedKey >> HALF_HASH_NB_BITS;
	newEntry->key = key;
	newEntry->content = element;
	newEntry->next = map[index];
	map[index] = newEntry;

	return newEntry->next != NULL;
}

bool hash_ReplaceElement(HashMap const map, char const *key, void *element)
{
	HashType hashedKey = hash(key);
	struct HashMapEntry *ptr = map[(HalfHashType)hashedKey];

	while (ptr) {
		if (hashedKey >> HALF_HASH_NB_BITS == ptr->hash
		 && !strcmp(ptr->key, key)) {
			ptr->content = element;
			return true;
		}
		ptr = ptr->next;
	}
	return false;
}

void *hash_RemoveElement(HashMap map, char const *key)
{
	HashType hashedKey = hash(key);
	struct HashMapEntry **ptr = &map[(HalfHashType)hashedKey];

	while (*ptr) {
		if (hashedKey >> HALF_HASH_NB_BITS == (*ptr)->hash
		 && !strcmp((*ptr)->key, key)) {
			struct HashMapEntry *next = (*ptr)->next;
			void *elem = (*ptr)->content;

			free(*ptr);
			*ptr = next;
			return elem;
		}
		ptr = &(*ptr)->next;
	}
	return NULL;
}

void *hash_GetElement(HashMap const map, char const *key)
{
	HashType hashedKey = hash(key);
	struct HashMapEntry *ptr = map[(HalfHashType)hashedKey];

	while (ptr) {
		if (hashedKey >> HALF_HASH_NB_BITS == ptr->hash
		 && !strcmp(ptr->key, key)) {
			return ptr->content;
		}
		ptr = ptr->next;
	}
	return NULL;
}

void hash_ForEach(HashMap const map, void (*func)(void *, void *), void *arg)
{
	for (size_t i = 0; i < HASHMAP_NB_BUCKETS; i++) {
		struct HashMapEntry *ptr = map[i];

		while (ptr) {
			func(ptr->content, arg);
			ptr = ptr->next;
		}
	}
}

void hash_EmptyMap(HashMap map, void (*callback)(void *))
{
	for (size_t i = 0; i < HASHMAP_NB_BUCKETS; i++) {
		struct HashMapEntry *ptr = map[i];

		while (ptr) {
			struct HashMapEntry *next = ptr->next;

			if (callback)
				callback(ptr->content);
			free(ptr);
			ptr = next;
		}
		map[i] = NULL;
	}
}
