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
#include <string.h>
#include <assert.h>

#include "stacklist.h"
#include "extern/err.h"

void stack_Push(struct StackList **stack, const char *data)
{
	struct StackList *top = malloc(sizeof(*top));

	if (!top)
		err(1, "%s: Failed to allocate new item", __func__);

	top->data = data;
	top->next = *stack;

	*stack = top;
}

const char *stack_Pop(struct StackList **stack)
{
	if (!*stack)
		return NULL;

	const char *data = (*stack)->data;

	struct StackList *top = (*stack)->next;

	free(*stack);
	*stack = top;

	return data;
}

const char *stack_Top(struct StackList *stack)
{
	return stack ? stack->data : NULL;
}
