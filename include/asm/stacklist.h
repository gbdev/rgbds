/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Generic stack list of strings */
#ifndef RGBDS_STACK_LIST_H
#define RGBDS_STACK_LIST_H

/* NULL is an empty stack list */
struct StackList {
	void *data;
	struct StackList *next;
};

/* @warning Pushing a NULL will make `stack_Pop`'s and `stack_Top`'s returns ambiguous! */
void stack_Push(struct StackList **stack, void *data);
/* @return The popped string or NULL if the stack was empty */
void *stack_Pop(struct StackList **stack);
/* @return The top string or NULL if the stack is empty */
void *stack_Top(struct StackList const *stack);
size_t stack_Size(struct StackList const *stack);

#endif /* RGBDS_STACK_LIST_H */
