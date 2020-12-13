/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Generic stack list implementation */
#ifndef RGBDS_STACK_LIST_H
#define RGBDS_STACK_LIST_H

struct StackList {
	char const *data;
	struct StackList *next;
};

/**
 * Pushes a string onto a stack list.
 * @warning Pushing a NULL will make `stack_Pop`'s and `stack_Top`'s returns ambiguous!
 * @param stack The StackList to push the string onto
 * @param data The string to push onto the StackList
 */
void stack_Push(struct StackList **stack, const char *data);

/**
 * Pops a string off of a stack list.
 * @param stack The StackList to pop the string off of
 * @return The popped string, or NULL if the stack is empty
 */
const char *stack_Pop(struct StackList **stack);

/**
 * Gets the string on top of a stack list.
 * @param stack The StackList to get the string on top of
 * @return The top string, or NULL if the stack is empty
 */
const char *stack_Top(struct StackList *stack);

#endif /* RGBDS_STACK_LIST_H */
