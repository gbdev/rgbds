#ifndef EXTERN_ERR_H
#define EXTERN_ERR_H

#ifdef ERR_IN_LIBC
#include <err.h>
#else

#include <stdarg.h>
#include "extern/stdnoreturn.h"

#define warn rgbds_warn
#define vwarn rgbds_vwarn
#define warnx rgbds_warnx
#define vwarnx rgbds_vwarnx

#define err rgbds_err
#define verr rgbds_verr
#define errx rgbds_errx
#define verrx rgbds_verrx

void warn(const char *, ...);
void vwarn(const char *, va_list);
void warnx(const char *, ...);
void vwarnx(const char *, va_list);

noreturn void err(int, const char *, ...);
noreturn void verr(int, const char *, va_list);
noreturn void errx(int, const char *, ...);
noreturn void verrx(int, const char *, va_list);

#endif

#endif
