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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/macro.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/warning.h"

#include "extern/err.h"

#include "types.h"

static struct sContext *pFileStack;
static unsigned int nFileStackDepth;
unsigned int nMaxRecursionDepth;
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
static int32_t nCurrentREPTBodyFirstLine;
static int32_t nCurrentREPTBodyLastLine;

uint32_t ulMacroReturnValue;

/*
 * defines for nCurrentStatus
 */
#define STAT_isInclude		0 /* 'Normal' state as well */
#define STAT_isMacro		1
#define STAT_isMacroArg		2
#define STAT_isREPTBlock	3

/* Max context stack size */

/*
 * Context push and pop
 */
static void pushcontext(void)
{
	struct sContext **ppFileStack;

	if (++nFileStackDepth > nMaxRecursionDepth)
		fatalerror("Recursion limit (%d) exceeded", nMaxRecursionDepth);

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
		(*ppFileStack)->macroArgs = macro_GetCurrentArgs();
		(*ppFileStack)->pMacro = pCurrentMacro;
		break;
	case STAT_isInclude:
		(*ppFileStack)->pFile = pCurrentFile;
		break;
	case STAT_isREPTBlock:
		(*ppFileStack)->macroArgs = macro_GetCurrentArgs();
		(*ppFileStack)->pREPTBlock = pCurrentREPTBlock;
		(*ppFileStack)->nREPTBlockSize = nCurrentREPTBlockSize;
		(*ppFileStack)->nREPTBlockCount = nCurrentREPTBlockCount;
		(*ppFileStack)->nREPTBodyFirstLine = nCurrentREPTBodyFirstLine;
		(*ppFileStack)->nREPTBodyLastLine = nCurrentREPTBodyLastLine;
		break;
	default:
		fatalerror("%s: Internal error.", __func__);
	}
	(*ppFileStack)->uniqueID = macro_GetUniqueID();

	nLineNo = 0;
}

static int32_t popcontext(void)
{
	struct sContext *pLastFile, **ppLastFile;

	if (nCurrentStatus == STAT_isREPTBlock) {
		if (--nCurrentREPTBlockCount) {
			char *pREPTIterationWritePtr;
			unsigned long nREPTIterationNo;
			int nNbCharsWritten;
			int nNbCharsLeft;

			yy_delete_buffer(CurrentFlexHandle);
			CurrentFlexHandle =
				yy_scan_bytes(pCurrentREPTBlock,
					      nCurrentREPTBlockSize);
			yy_switch_to_buffer(CurrentFlexHandle);
			macro_SetUniqueID(nMacroCount++);

			/* Increment REPT count in file path */
			pREPTIterationWritePtr =
				strrchr(tzCurrentFileName, '~') + 1;
			nREPTIterationNo =
				strtoul(pREPTIterationWritePtr, NULL, 10);
			nNbCharsLeft = sizeof(tzCurrentFileName)
				- (pREPTIterationWritePtr - tzCurrentFileName);
			nNbCharsWritten = snprintf(pREPTIterationWritePtr,
						   nNbCharsLeft, "%lu",
						   nREPTIterationNo + 1);
			if (nNbCharsWritten >= nNbCharsLeft) {
				/*
				 * The string is probably corrupted somehow,
				 * revert the change to avoid a bad error
				 * output.
				 */
				sprintf(pREPTIterationWritePtr, "%lu",
					nREPTIterationNo);
				fatalerror("Cannot write REPT count to file path");
			}

			nLineNo = nCurrentREPTBodyFirstLine;
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
	nLineNo = nCurrentStatus == STAT_isREPTBlock ? nCurrentREPTBodyLastLine
						     : pLastFile->nLine;

	if (nCurrentStatus == STAT_isInclude)
		fclose(pCurrentFile);

	if (nCurrentStatus == STAT_isMacro
	 || nCurrentStatus == STAT_isREPTBlock)
		nLineNo++;

	CurrentFlexHandle = pLastFile->FlexHandle;
	strcpy((char *)tzCurrentFileName, (char *)pLastFile->tzFileName);

	switch (pLastFile->nStatus) {
		struct MacroArgs *args;

	case STAT_isMacroArg:
	case STAT_isMacro:
		args = macro_GetCurrentArgs();
		if (nCurrentStatus == STAT_isMacro) {
			macro_FreeArgs(args);
			free(args);
		}
		macro_UseNewArgs(pLastFile->macroArgs);
		pCurrentMacro = pLastFile->pMacro;
		break;
	case STAT_isInclude:
		pCurrentFile = pLastFile->pFile;
		break;
	case STAT_isREPTBlock:
		args = macro_GetCurrentArgs();
		if (nCurrentStatus == STAT_isMacro) {
			macro_FreeArgs(args);
			free(args);
		}
		macro_UseNewArgs(pLastFile->macroArgs);
		pCurrentREPTBlock = pLastFile->pREPTBlock;
		nCurrentREPTBlockSize = pLastFile->nREPTBlockSize;
		nCurrentREPTBlockCount = pLastFile->nREPTBlockCount;
		nCurrentREPTBodyFirstLine = pLastFile->nREPTBodyFirstLine;
		break;
	default:
		fatalerror("%s: Internal error.", __func__);
	}
	macro_SetUniqueID(pLastFile->uniqueID);

	nCurrentStatus = pLastFile->nStatus;

	nFileStackDepth--;

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

void fstk_DumpToStr(char *buf, size_t buflen)
{
	const struct sContext *pLastFile = pFileStack;
	int retcode;
	size_t len = buflen;

	while (pLastFile) {
		retcode = snprintf(&buf[buflen - len], len, "%s(%d) -> ",
				   pLastFile->tzFileName, pLastFile->nLine);
		if (retcode < 0)
			fatalerror("Failed to dump file stack to string: %s",
				   strerror(errno));
		else if (retcode >= len)
			len = 0;
		else
			len -= retcode;
		pLastFile = pLastFile->pNext;
	}

	retcode = snprintf(&buf[buflen - len], len, "%s(%d)", tzCurrentFileName,
			   nLineNo);
	if (retcode < 0)
		fatalerror("Failed to dump file stack to string: %s",
			   strerror(errno));
	else if (retcode >= len)
		len = 0;
	else
		len -= retcode;

	if (!len)
		warning(WARNING_LONG_STR, "File stack dump too long, got truncated");
}

/*
 * Dump the string expansion stack to stderr
 */
void fstk_DumpStringExpansions(void)
{
	const struct sStringExpansionPos *pExpansion = pCurrentStringExpansion;

	while (pExpansion) {
		fprintf(stderr, "while expanding symbol \"%s\"\n",
			pExpansion->tzName);
		pExpansion = pExpansion->pParent;
	}
}

/*
 * Extra includepath stuff
 */
void fstk_AddIncludePath(char *s)
{
	if (NextIncPath == MAXINCPATHS)
		fatalerror("Too many include directories passed from command line");

	// Find last occurrence of slash; is it at the end of the string?
	char const *lastSlash = strrchr(s, '/');
	char const *pattern = lastSlash && *(lastSlash + 1) == 0 ? "%s" : "%s/";

	if (snprintf(IncludePaths[NextIncPath++], _MAX_PATH, pattern,
		     s) >= _MAX_PATH)
		fatalerror("Include path too long '%s'", s);
}

static void printdep(const char *fileName)
{
	if (dependfile) {
		fprintf(dependfile, "%s: %s\n", tzTargetFileName, fileName);
		if (oGeneratePhonyDeps)
			fprintf(dependfile, "%s:\n", fileName);
	}
}

static FILE *getFile(char const *pathname)
{
	struct stat statbuf;

	if (stat(pathname, &statbuf) != 0)
		return NULL;

	/* Reject directories */
	if (S_ISDIR(statbuf.st_mode))
		return NULL;

	return fopen(pathname, "rb");
}

FILE *fstk_FindFile(char const *fname, char **incPathUsed)
{
	if (fname == NULL)
		return NULL;

	char path[_MAX_PATH];
	FILE *f = getFile(fname);

	if (f) {
		printdep(fname);
		return f;
	}

	for (size_t i = 0; i < NextIncPath; ++i) {
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

		f = getFile(path);
		if (f) {
			printdep(path);

			if (incPathUsed)
				*incPathUsed = IncludePaths[i];
			return f;
		}
	}

	errno = ENOENT;
	if (oGeneratedMissingIncludes)
		printdep(fname);
	return NULL;
}

/*
 * Set up an include file for parsing
 */
void fstk_RunInclude(char *tzFileName)
{
	char *incPathUsed = "";
	FILE *f = fstk_FindFile(tzFileName, &incPathUsed);

	if (f == NULL) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		yyerror("Unable to open included file '%s': %s", tzFileName,
			strerror(errno));
		return;
	}

	pushcontext();
	nLineNo = 1;
	nCurrentStatus = STAT_isInclude;
	snprintf(tzCurrentFileName, sizeof(tzCurrentFileName), "%s%s",
		 incPathUsed, tzFileName);
	if (CurrentOptions.verbose)
		printf("Assembling %s\n", tzCurrentFileName);
	pCurrentFile = f;
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);

	/* Dirty hack to give the INCLUDE directive a linefeed */

	yyunput('\n');
	nLineNo--;
}

/*
 * Set up a macro for parsing
 */
bool fstk_RunMacro(char *s, struct MacroArgs *args)
{
	struct sSymbol *sym = sym_FindMacro(s);
	int nPrintedChars;

	if (sym == NULL || sym->pMacro == NULL)
		return false;

	pushcontext();
	macro_SetUniqueID(nMacroCount++);
	/* Minus 1 because there is a newline at the beginning of the buffer */
	nLineNo = sym->nFileLine - 1;
	macro_UseNewArgs(args);
	nCurrentStatus = STAT_isMacro;
	nPrintedChars = snprintf(tzCurrentFileName, _MAX_PATH + 1,
				 "%s::%s", sym->tzFileName, s);
	if (nPrintedChars > _MAX_PATH) {
		popcontext();
		fatalerror("File name + macro name is too large to fit into buffer");
	}

	pCurrentMacro = sym;
	CurrentFlexHandle = yy_scan_bytes(pCurrentMacro->pMacro,
					  strlen(pCurrentMacro->pMacro));
	yy_switch_to_buffer(CurrentFlexHandle);

	return true;
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
void fstk_RunRept(uint32_t count, int32_t nReptLineNo)
{
	if (count) {
		static const char *tzReptStr = "::REPT~1";

		/* For error printing to make sense, fake nLineNo */
		nCurrentREPTBodyLastLine = nLineNo;
		nLineNo = nReptLineNo;
		pushcontext();
		macro_SetUniqueID(nMacroCount++);
		nCurrentREPTBlockCount = count;
		nCurrentStatus = STAT_isREPTBlock;
		nCurrentREPTBlockSize = ulNewMacroSize;
		pCurrentREPTBlock = tzNewMacro;
		nCurrentREPTBodyFirstLine = nReptLineNo + 1;
		nLineNo = nReptLineNo;

		if (strlen(tzCurrentFileName) + strlen(tzReptStr) > _MAX_PATH)
			fatalerror("Cannot append \"%s\" to file path",
				   tzReptStr);
		strcat(tzCurrentFileName, tzReptStr);

		CurrentFlexHandle =
			yy_scan_bytes(pCurrentREPTBlock, nCurrentREPTBlockSize);
		yy_switch_to_buffer(CurrentFlexHandle);
	}
}

/*
 * Initialize the filestack routines
 */
void fstk_Init(char *pFileName)
{
	char tzSymFileName[_MAX_PATH + 1 + 2];

	snprintf(tzSymFileName, sizeof(tzSymFileName), "\"%s\"", pFileName);
	sym_AddString("__FILE__", tzSymFileName);

	pFileStack = NULL;
	if (strcmp(pFileName, "-") == 0) {
		pCurrentFile = stdin;
	} else {
		pCurrentFile = fopen(pFileName, "rb");
		if (pCurrentFile == NULL)
			yyerror("Unable to open file '%s': %s", pFileName,
				strerror(errno));
	}
	nFileStackDepth = 0;

	nMacroCount = 0;
	nCurrentStatus = STAT_isInclude;
	snprintf(tzCurrentFileName, _MAX_PATH + 1, "%s", pFileName);
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);
	nLineNo = 1;
}
