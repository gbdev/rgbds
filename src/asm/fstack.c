/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * FileStack routines
 */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/symbol.h"

#include "extern/err.h"

#include "types.h"

static struct sContext *pFileStack;
static struct sSymbol *pCurrentMacro;
static YY_BUFFER_STATE CurrentFlexHandle;
static FILE *pCurrentFile;
static uint32_t nCurrentStatus;
char tzCurrentFileName[_MAX_PATH + 1];
static char IncludePaths[MAXINCPATHS][_MAX_PATH + 1];
static int32_t NextIncPath;
static uint32_t nMacroCount;

static char *pCurrentREPTBlock;
static uint32_t nCurrentREPTBlockSize;
static uint32_t nCurrentREPTBlockCount;

uint32_t ulMacroReturnValue;

/*
 * defines for nCurrentStatus
 */
#define STAT_isInclude		0 /* 'Normal' state as well */
#define STAT_isMacro		1
#define STAT_isMacroArg		2
#define STAT_isREPTBlock	3

/*
 * Context push and pop
 */
static void pushcontext(void)
{
	struct sContext **ppFileStack;

	ppFileStack = &pFileStack;
	while (*ppFileStack)
		ppFileStack = &((*ppFileStack)->pNext);

	*ppFileStack = malloc(sizeof(struct sContext));

	if (*ppFileStack == NULL)
		fatalerror("No memory for context");

	(*ppFileStack)->FlexHandle = CurrentFlexHandle;
	(*ppFileStack)->pNext = NULL;
	strcpy((char *)(*ppFileStack)->tzFileName, (char *)tzCurrentFileName);
	(*ppFileStack)->nLine = nLineNo;

	switch ((*ppFileStack)->nStatus = nCurrentStatus) {
	case STAT_isMacroArg:
	case STAT_isMacro:
		sym_SaveCurrentMacroArgs((*ppFileStack)->tzMacroArgs);
		(*ppFileStack)->pMacro = pCurrentMacro;
		break;
	case STAT_isInclude:
		(*ppFileStack)->pFile = pCurrentFile;
		break;
	case STAT_isREPTBlock:
		sym_SaveCurrentMacroArgs((*ppFileStack)->tzMacroArgs);
		(*ppFileStack)->pREPTBlock = pCurrentREPTBlock;
		(*ppFileStack)->nREPTBlockSize = nCurrentREPTBlockSize;
		(*ppFileStack)->nREPTBlockCount = nCurrentREPTBlockCount;
		break;
	default:
		fatalerror("%s: Internal error.", __func__);
	}

	nLineNo = 0;
}

static int32_t popcontext(void)
{
	struct sContext *pLastFile, **ppLastFile;

	if (nCurrentStatus == STAT_isREPTBlock) {
		if (--nCurrentREPTBlockCount) {
			yy_delete_buffer(CurrentFlexHandle);
			CurrentFlexHandle =
				yy_scan_bytes(pCurrentREPTBlock,
					      nCurrentREPTBlockSize);
			yy_switch_to_buffer(CurrentFlexHandle);
			sym_UseCurrentMacroArgs();
			sym_SetMacroArgID(nMacroCount++);
			sym_UseNewMacroArgs();
			return 0;
		}
	}

	pLastFile = pFileStack;
	if (pLastFile == NULL)
		return 1;

	ppLastFile = &pFileStack;
	while (pLastFile->pNext) {
		ppLastFile = &(pLastFile->pNext);
		pLastFile = *ppLastFile;
	}

	yy_delete_buffer(CurrentFlexHandle);
	nLineNo = pLastFile->nLine;

	if (nCurrentStatus == STAT_isInclude)
		fclose(pCurrentFile);

	if (nCurrentStatus == STAT_isMacro) {
		sym_FreeCurrentMacroArgs();
		nLineNo += 1;
	}

	if (nCurrentStatus == STAT_isREPTBlock)
		nLineNo += 1;

	CurrentFlexHandle = pLastFile->FlexHandle;
	strcpy((char *)tzCurrentFileName, (char *)pLastFile->tzFileName);

	switch (nCurrentStatus = pLastFile->nStatus) {
	case STAT_isMacroArg:
	case STAT_isMacro:
		sym_RestoreCurrentMacroArgs(pLastFile->tzMacroArgs);
		pCurrentMacro = pLastFile->pMacro;
		break;
	case STAT_isInclude:
		pCurrentFile = pLastFile->pFile;
		break;
	case STAT_isREPTBlock:
		sym_RestoreCurrentMacroArgs(pLastFile->tzMacroArgs);
		pCurrentREPTBlock = pLastFile->pREPTBlock;
		nCurrentREPTBlockSize = pLastFile->nREPTBlockSize;
		nCurrentREPTBlockCount = pLastFile->nREPTBlockCount;
		break;
	default:
		fatalerror("%s: Internal error.", __func__);
	}

	free(*ppLastFile);
	*ppLastFile = NULL;
	yy_switch_to_buffer(CurrentFlexHandle);
	return 0;
}

int32_t fstk_GetLine(void)
{
	struct sContext *pLastFile, **ppLastFile;

	switch (nCurrentStatus) {
	case STAT_isInclude:
		/* This is the normal mode, also used when including a file. */
		return nLineNo;
	case STAT_isMacro:
		break; /* Peek top file of the stack */
	case STAT_isMacroArg:
		return nLineNo; /* ??? */
	case STAT_isREPTBlock:
		break; /* Peek top file of the stack */
	default:
		fatalerror("%s: Internal error.", __func__);
	}

	pLastFile = pFileStack;

	if (pLastFile != NULL) {
		ppLastFile = &pFileStack;
		while (pLastFile->pNext) {
			ppLastFile = &(pLastFile->pNext);
			pLastFile = *ppLastFile;
		}
		return pLastFile->nLine;
	}

	/*
	 * This is only reached if the lexer is in REPT or MACRO mode but there
	 * are no saved contexts with the origin of said REPT or MACRO.
	 */
	fatalerror("%s: Internal error.", __func__);
}

int yywrap(void)
{
	return popcontext();
}

/*
 * Dump the context stack to stderr
 */
void fstk_Dump(void)
{
	const struct sContext *pLastFile;

	pLastFile = pFileStack;

	while (pLastFile) {
		fprintf(stderr, "%s(%d) -> ", pLastFile->tzFileName,
			pLastFile->nLine);
		pLastFile = pLastFile->pNext;
	}

	fprintf(stderr, "%s(%d)", tzCurrentFileName, nLineNo);
}

/*
 * Extra includepath stuff
 */
void fstk_AddIncludePath(char *s)
{
	if (NextIncPath == MAXINCPATHS)
		fatalerror("Too many include directories passed from command line");

	if (snprintf(IncludePaths[NextIncPath++], _MAX_PATH, "%s", s) >= _MAX_PATH)
		fatalerror("Include path too long '%s'", s);
}

FILE *fstk_FindFile(char *fname)
{
	char path[_MAX_PATH];
	int32_t i;
	FILE *f;

	if (fname == NULL)
		return NULL;

	f = fopen(fname, "rb");

	if (f != NULL || errno != ENOENT) {
		if (dependfile)
			fprintf(dependfile, "%s: %s\n", tzObjectname, fname);

		return f;
	}

	for (i = 0; i < NextIncPath; ++i) {
		/*
		 * The function snprintf() does not write more than `size` bytes
		 * (including the terminating null byte ('\0')).  If the output
		 * was truncated due to this limit, the return value is the
		 * number of characters (excluding the terminating null byte)
		 * which would have been written to the final string if enough
		 * space had been available. Thus, a return value of `size` or
		 * more means that the output was truncated.
		 */
		int fullpathlen = snprintf(path, sizeof(path), "%s%s",
					   IncludePaths[i], fname);

		if (fullpathlen >= (int)sizeof(path))
			continue;

		f = fopen(path, "rb");

		if (f != NULL || errno != ENOENT) {
			if (dependfile) {
				fprintf(dependfile, "%s: %s\n", tzObjectname,
					path);
			}
			return f;
		}
	}

	errno = ENOENT;
	return NULL;
}

/*
 * Set up an include file for parsing
 */
void fstk_RunInclude(char *tzFileName)
{
	FILE *f = fstk_FindFile(tzFileName);

	if (f == NULL)
		err(1, "Unable to open included file '%s'", tzFileName);

	pushcontext();
	nLineNo = 1;
	nCurrentStatus = STAT_isInclude;
	strcpy(tzCurrentFileName, tzFileName);
	pCurrentFile = f;
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);

	/* Dirty hack to give the INCLUDE directive a linefeed */

	yyunput('\n');
	nLineNo -= 1;
}

/*
 * Set up a macro for parsing
 */
uint32_t fstk_RunMacro(char *s)
{
	struct sSymbol *sym = sym_FindMacro(s);

	if (sym == NULL)
		return 0;

	pushcontext();
	sym_SetMacroArgID(nMacroCount++);
	nLineNo = -1;
	sym_UseNewMacroArgs();
	nCurrentStatus = STAT_isMacro;
	strcpy(tzCurrentFileName, s);

	if (sym->pMacro == NULL)
		return 0;

	pCurrentMacro = sym;
	CurrentFlexHandle = yy_scan_bytes(pCurrentMacro->pMacro,
					  strlen(pCurrentMacro->pMacro));
	yy_switch_to_buffer(CurrentFlexHandle);

	return 1;
}

/*
 * Set up a macroargument for parsing
 */
void fstk_RunMacroArg(int32_t s)
{
	char *sym;

	if (s == '@')
		s = -1;
	else
		s -= '0';

	sym = sym_FindMacroArg(s);

	if (sym == NULL)
		fatalerror("No such macroargument");

	pushcontext();
	nCurrentStatus = STAT_isMacroArg;
	snprintf(tzCurrentFileName, _MAX_PATH + 1, "%c", (uint8_t)s);
	CurrentFlexHandle = yy_scan_bytes(sym, strlen(sym));
	yy_switch_to_buffer(CurrentFlexHandle);
}

/*
 * Set up a stringequate for parsing
 */
void fstk_RunString(char *s)
{
	const struct sSymbol *pSym = sym_FindSymbol(s);

	if (pSym != NULL) {
		pushcontext();
		nCurrentStatus = STAT_isMacroArg;
		strcpy(tzCurrentFileName, s);
		CurrentFlexHandle =
			yy_scan_bytes(pSym->pMacro, strlen(pSym->pMacro));
		yy_switch_to_buffer(CurrentFlexHandle);
	} else {
		yyerror("No such string symbol '%s'", s);
	}
}

/*
 * Set up a repeat block for parsing
 */
void fstk_RunRept(uint32_t count)
{
	if (count) {
		pushcontext();
		sym_UseCurrentMacroArgs();
		sym_SetMacroArgID(nMacroCount++);
		sym_UseNewMacroArgs();
		nCurrentREPTBlockCount = count;
		nCurrentStatus = STAT_isREPTBlock;
		nCurrentREPTBlockSize = ulNewMacroSize;
		pCurrentREPTBlock = tzNewMacro;
		CurrentFlexHandle =
			yy_scan_bytes(pCurrentREPTBlock, nCurrentREPTBlockSize);
		yy_switch_to_buffer(CurrentFlexHandle);
	}
}

/*
 * Initialize the filestack routines
 */
void fstk_Init(char *s)
{
	char tzFileName[_MAX_PATH + 1];
	char tzSymFileName[_MAX_PATH + 1 + 2];

	snprintf(tzSymFileName, sizeof(tzSymFileName), "\"%s\"", s);
	sym_AddString("__FILE__", tzSymFileName);

	strcpy(tzFileName, s);
	pFileStack = NULL;
	pCurrentFile = fopen(tzFileName, "rb");
	if (pCurrentFile == NULL)
		err(1, "Unable to open file '%s'", tzFileName);

	nMacroCount = 0;
	nCurrentStatus = STAT_isInclude;
	snprintf(tzCurrentFileName, _MAX_PATH + 1, "%s", tzFileName);
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);
	nLineNo = 1;
}
