/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2021, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_STRING_H
#define RGBDS_STRING_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "helpers.h"

struct String;

#define PRI_STR ".*s"
// WARNING: **DO NOT** pass any side-effecting parameters to the macros below!
#define STR_FMT(str) (int)str_Len(str), str_Chars(str)

#define MUTATE_STR(str, ...) do { \
	struct String *___orig_str = str; \
	(void)___orig_str; /* Suppress `-Wunused-variable` */ \
	str = __VA_ARGS__; \
	assert(___orig_str == str); /* This shouldn't have been reallocated */ \
} while (0)

static inline bool str_IsWhitespace(int c)
{
	return c == ' ' || c == '\t';
}

size_t str_Len(struct String const *str) attr_(pure);
void str_Trunc(struct String *str, size_t len);
char str_Index(struct String const *str, size_t i) attr_(pure);
bool str_Find(struct String const *str, char c) attr_(pure);
char const *str_Chars(struct String const *str);

// I wouldn't normally expose these, but I consider RGBDS maintainers to be responsible people
void str_SetLen(struct String *str, size_t len);
char *str_CharsMut(struct String *str);

/**
 * @param capacity The capacity to use, or 0 if unknown
 */
struct String *str_New(size_t capacity) attr_(malloc);
void str_Ref(struct String *str);
void str_Unref(struct String *str);

struct String *str_Push(struct String *str, char c) attr_(warn_unused_result);
struct String *str_Append(struct String *lhs, struct String const *rhs) attr_(warn_unused_result);
struct String *str_AppendSlice(struct String *lhs, char const *rhs, size_t len)
	attr_(warn_unused_result);

/**
 * @param capacity The minimum capacity to reserve
 */
struct String *str_Reserve(struct String *str, size_t capacity) attr_(warn_unused_result);

#endif
