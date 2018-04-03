/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void *z_malloc(const char *file_, unsigned int line_,
	       size_t size)
{
	void *p = malloc(size);

	if (p == NULL) {
		fprintf(stderr, "%s:%d: malloc(%zu)\n    %s\n", file_, line_,
			size, strerror(errno));
		exit(1);
	}

	return p;
}

void z_free(const char *file_, unsigned int line_,
	    void *ptr)
{
	if (ptr == NULL) {
		fprintf(stderr, "%s:%d: free(NULL)\n", file_, line_);
		exit(1);
	}

	free(ptr);
}

void *z_calloc(const char *file_, unsigned int line_,
	       size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);

	if (p == NULL) {
		fprintf(stderr, "%s:%d: calloc(%zu, %zu)\n    %s\n", file_,
			line_, nmemb, size, strerror(errno));
		exit(1);
	}

	return p;
}

void *z_realloc(const char *file_, unsigned int line_,
		void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL) {
		fprintf(stderr, "%s:%d: realloc(%p, %zu)\n    %s\n", file_,
			line_, ptr, size, strerror(errno));
		exit(1);
	}

	return p;
}

FILE *z_fopen(const char *file_, unsigned int line_,
	      const char *pathname, const char *mode)
{
	FILE *f = fopen(pathname, mode);

	if (f == NULL) {
		fprintf(stderr, "%s:%d: fopen(\"%s\", \"%s\")\n    %s\n", file_,
			line_, pathname, mode, strerror(errno));
		exit(1);
	}

	return f;
}

int z_fclose(const char *file_, unsigned int line_,
	     FILE *stream)
{
	int ret = fclose(stream);

	if (ret == EOF) {
		fprintf(stderr, "%s:%d: fclose() = %d\n    %s\n", file_, line_,
			ret, strerror(errno));
		exit(1);
	}

	return ret;
}

size_t z_fread(const char *file_, unsigned int line_,
	       void *ptr, size_t size, size_t count, FILE *stream)
{
	size_t s = fread(ptr, size, count, stream);

	if (s != count) {
		fprintf(stderr, "%s:%d: fread(%zu, %zu) = %zu\n    %s\n", file_,
			line_, size, count, s, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return s;
}

size_t z_fwrite(const char *file_, unsigned int line_,
		const void *ptr, size_t size, size_t count, FILE *stream)
{
	size_t s = fwrite(ptr, size, count, stream);

	if (s != count) {
		fprintf(stderr, "%s:%d: fwrite(%zu, %zu) = %zu\n    %s\n",
			file_, line_, size, count, s, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return s;
}

int z_fseek(const char *file_, unsigned int line_,
	    FILE *stream, long int offset, int origin)
{
	int ret = fseek(stream, offset, origin);

	if (ret != 0) {
		fprintf(stderr, "%s:%d: fseek(%ld, %d) = %d\n    %s\n", file_,
			line_, offset, origin, ret, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

long int z_ftell(const char *file_, unsigned int line_,
		 FILE *stream)
{
	long int ret = ftell(stream);

	if (ret == -1L) {
		fprintf(stderr, "%s:%d: ftell() = %ld\n    %s\n", file_, line_,
			ret, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

int z_fgetc(const char *file_, unsigned int line_,
	    FILE *stream)
{
	int ret = fgetc(stream);

	if (ret == EOF) {
		fprintf(stderr, "%s:%d: fgetc()\n    %s\n", file_, line_,
			strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

int z_fputc(const char *file_, unsigned int line_,
	    int character, FILE *stream)
{
	int ret = fputc(character, stream);

	if (ret == EOF) {
		fprintf(stderr, "%s:%d: fputc(%d)\n    %s\n", file_, line_,
			character, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

int z_fputs(const char *file_, unsigned int line_,
	    const char *str, FILE *stream)
{
	int ret = fputs(str, stream);

	if (ret == EOF) {
		fprintf(stderr, "%s:%d: fputs(\"%s\")\n    %s\n", file_, line_,
			str, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

void z_rewind(const char *file_, unsigned int line_,
	      FILE *stream)
{
	rewind(stream);

	int ret = ferror(stream);

	if (ret != 0) {
		fprintf(stderr, "%s:%d: rewind() = %d\n    %s\n", file_, line_,
			ret, strerror(errno));
		fclose(stream);
		exit(1);
	}
}

int z_vfprintf(const char *file_, unsigned int line_,
	       FILE *stream, const char *format, va_list ap)
{
	int ret = vfprintf(stream, format, ap);

	if (ret < 0) {
		fprintf(stderr, "%s:%d: vfprintf() = %d\n    %s\n", file_,
			line_, ret, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}

int z_fprintf(const char *file_, unsigned int line_,
	      FILE *stream, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vfprintf(stream, format, ap);
	va_end(ap);

	if (ret < 0) {
		fprintf(stderr, "%s:%d: fprintf() = %d\n    %s\n", file_, line_,
			ret, strerror(errno));
		fclose(stream);
		exit(1);
	}

	return ret;
}
