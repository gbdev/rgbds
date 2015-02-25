#ifndef EXTERN_REALLOCARRAY_H
#define EXTERN_REALLOCARRAY_H

#ifdef REALLOCARRAY_IN_LIBC
#include <stdlib.h>
#else

#define reallocarray rgbds_reallocarray

void *reallocarray(void *, size_t, size_t);

#endif

#endif
