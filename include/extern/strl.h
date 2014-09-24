#ifndef STRL_H
#define STRL_H

#ifdef STRL_IN_LIBC
#include <string.h>
#else
#define strlcpy rgbds_strlcpy
#define strlcat rgbds_strlcat
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
#endif

#endif
