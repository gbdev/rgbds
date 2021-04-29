/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2021, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "asm/string.h"

#include "helpers.h"

// A ref-counted string
struct String {
	size_t refs;
	size_t capacity;
	size_t len;
	char chars[];
};

size_t str_Len(struct String const *str) attr_(pure)
{
	return str->len;
}

void str_Trunc(struct String *str, size_t len)
{
	assert(len <= str->len);

	str->len = len;
}

char str_Index(struct String const *str, size_t i) attr_(pure)
{
	return str->chars[i];
}

bool str_Find(struct String const *str, char c) attr_(pure)
{
	for (size_t i = 0; i < str->len; i++) {
		if (str->chars[i] == c)
			return true;
	}

	return false;
}

char const *str_Chars(struct String const *str) attr_(pure)
{
	return str->chars;
}

char *str_CharsMut(struct String *str) attr_(pure)
{
	return str->chars;
}

struct String *str_New(size_t capacity) attr_(malloc)
{
	if (capacity == 0)
		capacity = 32;

	struct String *str = malloc(sizeof(*str) + capacity);

	if (!str)
		return NULL;

	str->refs = 1;
	str->capacity = capacity;
	str->len = 0;
	return str;
}

void str_Ref(struct String *str)
{
	assert(str->refs < SIZE_MAX);

	str->refs++;
}

void str_Unref(struct String *str)
{
	assert(str->refs > 0);

	str->refs--;

	if (!str->refs)
		free(str);
}

static bool doubleCapacity(struct String *str) attr_(warn_unused_result)
{
	assert(str->capacity > 0);

	if (str->capacity == SIZE_MAX) {
		errno = ERANGE;
		return false;
	} else if (str->capacity > SIZE_MAX / 2) {
		str->capacity = SIZE_MAX;
	} else {
		str->capacity *= 2;
	}

	return true;
}

struct String *str_Push(struct String *str, char c) attr_(warn_unused_result)
{
	assert(str->len <= str->capacity);

	if (str->len == str->capacity) {
		if (!doubleCapacity(str))
			return NULL;
		str = realloc(str, sizeof(*str) + str->capacity);
		if (!str)
			return NULL;
	}

	str->chars[str->len++] = c;
	return str;
}

struct String *str_AppendSlice(struct String *lhs, char const *rhs, size_t len)
	attr_(warn_unused_result)
{
	assert(lhs->len <= lhs->capacity);

	// Avoid overflow
	if (lhs->len > INT_MAX - len) {
		errno = ERANGE;
		return NULL;
	}

	// If the combined len is larger than the capacity, grow lhs
	if (lhs->len + len > lhs->capacity) {
		if (!doubleCapacity(lhs))
			return NULL;
		if (lhs->capacity < lhs->len)
			lhs->capacity = lhs->len;
		lhs = realloc(lhs, sizeof(*lhs) + lhs->capacity);
		if (!lhs)
			return NULL;
	}

	// Copy rhs
	memcpy(&lhs->chars[lhs->len], rhs, len);
	lhs->len += len;

	return lhs;
}

struct String *str_Append(struct String *lhs, struct String const *rhs) attr_(warn_unused_result)
{
	return str_AppendSlice(lhs, rhs->chars, rhs->len);
}

struct String *str_Reserve(struct String *str, size_t capacity) attr_(warn_unused_result)
{
	if (str->capacity < capacity) {
		str->capacity = capacity;
		str = realloc(str, sizeof(*str) + str->capacity);
	}
	return str;
}
