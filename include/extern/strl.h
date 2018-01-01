#ifndef EXTERN_STRL_H
#define EXTERN_STRL_H

#ifdef STRL_IN_LIBC

#include <string.h>

#else /* STRL_IN_LIBC */

#define strlcpy rgbds_strlcpy
#define strlcat rgbds_strlcat
size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);

#endif /* STRL_IN_LIBC */

#endif /* EXTERN_STRL_H */
