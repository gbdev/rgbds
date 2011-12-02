#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/types.h"
#include "lib/libwrap.h"

SLONG 
file_Length(FILE * f)
{
	ULONG r, p;

	p = ftell(f);
	fseek(f, 0, SEEK_END);
	r = ftell(f);
	fseek(f, p, SEEK_SET);

	return (r);
}

SLONG 
file_ReadASCIIz(char *b, FILE * f)
{
	SLONG r = 0;

	while ((*b++ = fgetc(f)) != 0)
		r += 1;

	return (r + 1);
}

void 
file_WriteASCIIz(char *b, FILE * f)
{
	while (*b)
		fputc(*b++, f);

	fputc(0, f);
}

UWORD 
file_ReadWord(FILE * f)
{
	UWORD r;

	r = fgetc(f);
	r |= fgetc(f) << 8;

	return (r);
}

void 
file_WriteWord(UWORD w, FILE * f)
{
	fputc(w, f);
	fputc(w >> 8, f);
}

ULONG 
file_ReadLong(FILE * f)
{
	ULONG r;

	r = fgetc(f);
	r |= fgetc(f) << 8;
	r |= fgetc(f) << 16;
	r |= fgetc(f) << 24;

	return (r);
}

void 
file_WriteLong(UWORD w, FILE * f)
{
	fputc(w, f);
	fputc(w >> 8, f);
	fputc(w >> 16, f);
	fputc(w >> 24, f);
}

sLibrary *
lib_ReadLib0(FILE * f, SLONG size)
{
	if (size) {
		sLibrary *l = NULL, *first = NULL;

		while (size > 0) {
			if (l == NULL) {
				l = malloc(sizeof *l);
				if (!l) {
					fprintf(stderr, "Out of memory\n");
					exit(1);
				}

				first = l;
			} else {
				l->pNext = malloc(sizeof *l->pNext);
				if (!l->pNext) {
					fprintf(stderr, "Out of memory\n");
					exit(1);
				}

				l = l->pNext;
			}

			size -= file_ReadASCIIz(l->tName, f);
			l->uwTime = file_ReadWord(f);
			size -= 2;
			l->uwDate = file_ReadWord(f);
			size -= 2;
			l->nByteLength = file_ReadLong(f);
			size -= 4;
			if ((l->pData = malloc(l->nByteLength))) {
				fread(l->pData, sizeof(UBYTE), l->nByteLength,
				    f);
				size -= l->nByteLength;
			} else {
				fprintf(stderr, "Out of memory\n");
				exit(1);
			}

			l->pNext = NULL;
		}
		return (first);
	}
	return (NULL);
}

sLibrary *
lib_Read(char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "rb"))) {
		SLONG size;
		char ID[5];

		size = file_Length(f);
		if (size == 0) {
			fclose(f);
			return (NULL);
		}
		fread(ID, sizeof(char), 4, f);
		ID[4] = 0;
		size -= 4;

		if (strcmp(ID, "XLB0") == 0) {
			sLibrary *r;

			r = lib_ReadLib0(f, size);
			fclose(f);
			printf("Library '%s' opened\n", filename);
			return (r);
		} else {
			fclose(f);
			fprintf(stderr, "Not a valid xLib library\n");
			exit(1);
		}
	} else {
		printf
		    ("Library '%s' not found, it will be created if necessary\n",
		    filename);
		return (NULL);
	}
}

BBOOL 
lib_Write(sLibrary * lib, char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "wb"))) {
		fwrite("XLB0", sizeof(char), 4, f);
		while (lib) {
			file_WriteASCIIz(lib->tName, f);
			file_WriteWord(lib->uwTime, f);
			file_WriteWord(lib->uwDate, f);
			file_WriteLong(lib->nByteLength, f);
			fwrite(lib->pData, sizeof(UBYTE), lib->nByteLength, f);
			lib = lib->pNext;
		}

		fclose(f);
		printf("Library '%s' closed\n", filename);
		return (1);
	}
	return (0);
}

sLibrary *
lib_Find(sLibrary * lib, char *filename)
{
	if (strlen(filename) >= MAXNAMELENGTH) {
		fprintf(stderr, "Module name too long: %s\n", filename);
		exit(1);
	}

	while (lib) {
		if (strcmp(lib->tName, filename) == 0)
			break;

		lib = lib->pNext;
	}

	return (lib);
}

sLibrary *
lib_AddReplace(sLibrary * lib, char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "rb"))) {
		sLibrary *module;

		if (strlen(filename) >= MAXNAMELENGTH) {
			fprintf(stderr, "Module name too long: %s\n",
			    filename);
			exit(1);
		}

		if ((module = lib_Find(lib, filename)) == NULL) {
			module = malloc(sizeof *module);
			if (!module) {
				fprintf(stderr, "Out of memory\n");
				exit(1);
			}

			module->pNext = lib;
			lib = module;
		} else {
			/* Module already exists */
			free(module->pData);
		}

		module->nByteLength = file_Length(f);
		strcpy(module->tName, filename);
		module->pData = malloc(module->nByteLength);
		if (!module->pData) {
			fprintf(stderr, "Out of memory\n");
			exit(1);
		}

		fread(module->pData, sizeof(UBYTE), module->nByteLength, f);

		printf("Added module '%s'\n", filename);

		fclose(f);
	}
	return (lib);
}

sLibrary *
lib_DeleteModule(sLibrary * lib, char *filename)
{
	sLibrary **pp, **first;
	BBOOL found = 0;

	pp = &lib;
	first = pp;

	if (strlen(filename) >= MAXNAMELENGTH) {
		fprintf(stderr, "Module name too long: %s\n", filename);
		exit(1);
	}

	while ((*pp) && (!found)) {
		if (strcmp((*pp)->tName, filename) == 0) {
			sLibrary *t;

			t = *pp;

			if (t->pData)
				free(t->pData);

			*pp = t->pNext;

			free(t);
			found = 1;
		}
		pp = &((*pp)->pNext);
	}

	if (!found) {
		fprintf(stderr, "Module not found\n");
		exit(1);
	} else
		printf("Module '%s' deleted from library\n", filename);

	return (*first);
}

void 
lib_Free(sLibrary * lib)
{
	while (lib) {
		sLibrary *l;

		if (lib->pData)
			free(lib->pData);

		l = lib;
		lib = lib->pNext;
		free(l);
	}
}
