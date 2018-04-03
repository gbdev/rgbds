/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_SAFELIBC_H
#define RGBDS_SAFELIBC_H

#include <stdio.h>

/*
 * The functions below are designed to stop the execution of the program as soon
 * as they find any problem. Normally, libc functions return error codes that
 * have to be checked manually. In most cases, a single error means that the
 * program has to stop. The functions below act exactly like the ones of libc
 * except that they will exit the program as soon as they find an error.
 *
 * Be careful in cases like zfgetc(). If it returns EOF it will be considered an
 * error and will exit the program. If you need to use fgetc() to iterate until
 * the end of the file, just use fgetc() instead of zfgetc(). You can also
 * calculate the size of the file with zfseek() and zftell() instead.
 */

/*
 * stdlib.h
 */

void *z_malloc(const char *file_, unsigned int line_,
	       size_t size);
void z_free(const char *file_, unsigned int line_,
	    void *ptr);
void *z_calloc(const char *file_, unsigned int line_,
	       size_t nmemb, size_t size);
void *z_realloc(const char *file_, unsigned int line_,
		void *ptr, size_t size);

#define zmalloc(size) \
		z_malloc(__FILE__, __LINE__, size)
#define zfree(ptr) \
		z_free(__FILE__, __LINE__, ptr)
#define zcalloc(nmemb, size) \
		z_calloc(__FILE__, __LINE__, nmemb, size)
#define zrealloc(ptr, size) \
		z_realloc(__FILE__, __LINE__, ptr, size)

/*
 * stdio.h
 */

FILE *z_fopen(const char *file_, unsigned int line,
	      const char *pathname, const char *mode);
int z_fclose(const char *file_, unsigned int line_,
	     FILE *stream);
size_t z_fread(const char *file_, unsigned int line_,
	       void *ptr, size_t size, size_t count, FILE *stream);
size_t z_fwrite(const char *file_, unsigned int line_,
		const void *ptr, size_t size, size_t count, FILE *stream);
int z_fseek(const char *file_, unsigned int line_,
	    FILE *stream, long int offset, int origin);
long int z_ftell(const char *file_, unsigned int line_,
		 FILE *stream);
int z_fgetc(const char *file_, unsigned int line_,
	    FILE *stream);
int z_fputc(const char *file_, unsigned int line_,
	    int character, FILE *stream);
int z_fputs(const char *file_, unsigned int line_,
	    const char *str, FILE *stream);
void z_rewind(const char *file_, unsigned int line_,
	      FILE *stream);
int z_vfprintf(const char *file_, unsigned int line_,
	       FILE *stream, const char *format, va_list arg);
int z_fprintf(const char *file_, unsigned int line_,
	      FILE *stream, const char *format, ...);

#define zfopen(pathname, mode) \
		z_fopen(__FILE__, __LINE__, pathname, mode)
#define zfclose(stream) \
		z_fclose(__FILE__, __LINE__, stream)
#define zfread(ptr, size, count, stream) \
		z_fread(__FILE__, __LINE__, ptr, size, count, stream)
#define zfwrite(ptr, size, count, stream) \
		z_fwrite(__FILE__, __LINE__, ptr, size, count, stream)
#define zfseek(stream, offset, origin) \
		z_fseek(__FILE__, __LINE__, stream, offset, origin)
#define zftell(stream) \
		z_ftell(__FILE__, __LINE__, stream)
#define zfgetc(stream) \
		z_fgetc(__FILE__, __LINE__, stream)
#define zfputc(character, stream) \
		z_fputc(__FILE__, __LINE__, character, stream)
#define zfputs(str, stream) \
		z_fputs(__FILE__, __LINE__, str, stream)
#define zrewind(stream) \
		z_rewind(__FILE__, __LINE__, stream)
#define zvfprintf(stream, format, ap) \
		z_vfprintf(__FILE__, __LINE__, stream, format, ap)
#define zfprintf(stream, format, ...) \
		z_fprintf(__FILE__, __LINE__, stream, format, ##__VA_ARGS__)

#endif /* RGBDS_SAFELIBC_H */
