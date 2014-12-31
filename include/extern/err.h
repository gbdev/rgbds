/*
 * Copyright © 2005-2013 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef EXTERN_ERR_H
#define EXTERN_ERR_H

#ifdef ERR_IN_LIBC
#include <err.h>
#else

#include <stdarg.h>
#include "stdnoreturn.h"

#define warn rgbds_warn
#define vwarn rgbds_vwarn
#define warnx rgbds_warnx
#define vwarnx rgbds_vwarnx

#define err rgbds_err
#define verr rgbds_verr
#define errx rgbds_errx
#define verrx rgbds_verrx

#ifdef __cplusplus
extern "C" {
#endif

void warn(const char *, ...);
void vwarn(const char *, va_list);
void warnx(const char *, ...);
void vwarnx(const char *, va_list);

noreturn void err(int, const char *, ...);
noreturn void verr(int, const char *, va_list);
noreturn void errx(int, const char *, ...);
noreturn void verrx(int, const char *, va_list);

#ifdef __cplusplus
}
#endif

#endif

#endif
