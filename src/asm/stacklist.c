/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "asm/stacklist.h"
#include "extern/err.h"

void stack_Push(struct StackList **stack, void *data)
{
	struct StackList *top = malloc(sizeof(*top));

	if (!top)
		err(1, "%s: Failed to allocate new item", __func__);

	top->data = data;
	top->next = *stack;

	*stack = top;
}

void *stack_Pop(struct StackList **stack)
{
	if (!*stack)
		return NULL;

	void *data = (*stack)->data;

	struct StackList *top = (*stack)->next;

	free(*stack);
	*stack = top;

	return data;
}

void *stack_Top(struct StackList const *stack)
{
	return stack ? stack->data : NULL;
}

size_t stack_Size(struct StackList const *stack)
{
	size_t size;

	for (size = 0; stack; size++)
		stack = stack->next;

	return size;
}
