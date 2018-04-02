/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef EXTERN_ERR_H
#define EXTERN_ERR_H

#ifdef ERR_IN_LIBC

#include <err.h>

#else /* ERR_IN_LIBC */

#include <stdarg.h>

#include "helpers.h"

#define warn rgbds_warn
#define vwarn rgbds_vwarn
#define warnx rgbds_warnx
#define vwarnx rgbds_vwarnx

#define err rgbds_err
#define verr rgbds_verr
#define errx rgbds_errx
#define verrx rgbds_verrx

void warn(const char *fmt, ...);
void vwarn(const char *fmt, va_list ap);
void warnx(const char *fmt, ...);
void vwarnx(const char *fmt, va_list ap);

noreturn_ void err(int status, const char *fmt, ...);
noreturn_ void verr(int status, const char *fmt, va_list ap);
noreturn_ void errx(int status, const char *fmt, ...);
noreturn_ void verrx(int status, const char *fmt, va_list ap);

#endif /* ERR_IN_LIBC */

#endif /* EXTERN_ERR_H */
