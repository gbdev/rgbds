/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2015-2018, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef EXTERN_REALLOCARRAY_H
#define EXTERN_REALLOCARRAY_H

#ifdef REALLOCARRAY_IN_LIBC

#include <stdlib.h>

#else /* REALLOCARRAY_IN_LIBC */

#define reallocarray rgbds_reallocarray
void *reallocarray(void *, size_t, size_t);

#endif /* REALLOCARRAY_IN_LIBC */

#endif /* EXTERN_REALLOCARRAY_H */
