/*
 * FileStack routines
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/symbol.h"
#include "asm/fstack.h"
#include "types.h"
#include "asm/main.h"
#include "asm/lexer.h"
#include "extern/err.h"
#include "extern/strl.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

struct sContext *pFileStack;
struct sSymbol *pCurrentMacro;
YY_BUFFER_STATE CurrentFlexHandle;
FILE *pCurrentFile;
ULONG nCurrentStatus;
char tzCurrentFileName[_MAX_PATH + 1];
char IncludePaths[MAXINCPATHS][_MAX_PATH + 1];
SLONG NextIncPath = 0;
ULONG nMacroCount;

char *pCurrentREPTBlock;
ULONG nCurrentREPTBlockSize;
ULONG nCurrentREPTBlockCount;

ULONG ulMacroReturnValue;

/*
 * defines for nCurrentStatus
 */
#define STAT_isInclude 		0
#define STAT_isMacro	 		1
#define STAT_isMacroArg		2
#define STAT_isREPTBlock	3

/*
 * Context push and pop
 */
void 
pushcontext(void)
{
	struct sContext **ppFileStack;

	ppFileStack = &pFileStack;
	while (*ppFileStack)
		ppFileStack = &((*ppFileStack)->pNext);

	if ((*ppFileStack = malloc(sizeof(struct sContext))) != NULL) {
		(*ppFileStack)->FlexHandle = CurrentFlexHandle;
		(*ppFileStack)->pNext = NULL;
		strcpy((char *) (*ppFileStack)->tzFileName,
		    (char *) tzCurrentFileName);
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
			(*ppFileStack)->nREPTBlockCount =
			    nCurrentREPTBlockCount;
			break;
		}
		nLineNo = 0;
	} else
		fatalerror("No memory for context");
}

int 
popcontext(void)
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
			return (0);
		}
	}
	if ((pLastFile = pFileStack) != NULL) {
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
		strcpy((char *) tzCurrentFileName,
		    (char *) pLastFile->tzFileName);
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
		}

		free(*ppLastFile);
		*ppLastFile = NULL;
		yy_switch_to_buffer(CurrentFlexHandle);
		return (0);
	} else
		return (1);
}

int 
yywrap(void)
{
	return (popcontext());
}

/*
 * Dump the context stack to stderr
 */
void 
fstk_Dump(void)
{
	struct sContext *pLastFile;

	pLastFile = pFileStack;

	while (pLastFile) {
		fprintf(stderr, "%s(%ld) -> ", pLastFile->tzFileName,
		    pLastFile->nLine);
		pLastFile = pLastFile->pNext;
	}

	fprintf(stderr, "%s(%ld)", tzCurrentFileName, nLineNo);
}

/*
 * Extra includepath stuff
 */
void 
fstk_AddIncludePath(char *s)
{
	if (NextIncPath == MAXINCPATHS) {
		fatalerror("Too many include directories passed from command line");
		return;
	}

	if (strlcpy(IncludePaths[NextIncPath++], s, _MAX_PATH) >= _MAX_PATH) {
		fatalerror("Include path too long '%s'",s);
		return;
	}
}

FILE *
fstk_FindFile(char *fname)
{
	char path[PATH_MAX];
	int i;
	FILE *f;

	if ((f = fopen(fname, "rb")) != NULL || errno != ENOENT) {
		return f;
	}

	for (i = 0; i < NextIncPath; ++i) {
		if (strlcpy(path, IncludePaths[i], sizeof path) >= 
		    sizeof path) {
			continue;
		}
		if (strlcat(path, fname, sizeof path) >= sizeof path) {
			continue;
		}

		if ((f = fopen(path, "rb")) != NULL || errno != ENOENT) {
			return f;
		}
	}

	errno = ENOENT;
	return NULL;
}

/*
 * Set up an include file for parsing
 */
void
fstk_RunInclude(char *tzFileName)
{
	FILE *f;

	f = fstk_FindFile(tzFileName);

	if (f == NULL) {
		err(1, "Unable to open included file '%s'",
		    tzFileName);
	}

	pushcontext();
	nLineNo = 1;
	nCurrentStatus = STAT_isInclude;
	strcpy(tzCurrentFileName, tzFileName);
	pCurrentFile = f;
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);

	//Dirty hack to give the INCLUDE directive a linefeed

	yyunput('\n');
	nLineNo -= 1;
}

/*
 * Set up a macro for parsing
 */
ULONG 
fstk_RunMacro(char *s)
{
	struct sSymbol *sym;

	if ((sym = sym_FindMacro(s)) != NULL) {
		pushcontext();
		sym_SetMacroArgID(nMacroCount++);
		nLineNo = -1;
		sym_UseNewMacroArgs();
		nCurrentStatus = STAT_isMacro;
		strcpy(tzCurrentFileName, s);
		if (sym->pMacro == NULL)
			return 0;
		pCurrentMacro = sym;
		CurrentFlexHandle =
		    yy_scan_bytes(pCurrentMacro->pMacro,
		    strlen(pCurrentMacro->pMacro));
		yy_switch_to_buffer(CurrentFlexHandle);
		return (1);
	} else
		return (0);
}

/*
 * Set up a macroargument for parsing
 */
void 
fstk_RunMacroArg(SLONG s)
{
	char *sym;

	if (s == '@')
		s = -1;
	else
		s -= '0';

	if ((sym = sym_FindMacroArg(s)) != NULL) {
		pushcontext();
		nCurrentStatus = STAT_isMacroArg;
		sprintf(tzCurrentFileName, "%c", (UBYTE) s);
		CurrentFlexHandle = yy_scan_bytes(sym, strlen(sym));
		yy_switch_to_buffer(CurrentFlexHandle);
	} else
		fatalerror("No such macroargument");
}

/*
 * Set up a stringequate for parsing
 */
void 
fstk_RunString(char *s)
{
	struct sSymbol *pSym;

	if ((pSym = sym_FindSymbol(s)) != NULL) {
		pushcontext();
		nCurrentStatus = STAT_isMacroArg;
		strcpy(tzCurrentFileName, s);
		CurrentFlexHandle =
		    yy_scan_bytes(pSym->pMacro, strlen(pSym->pMacro));
		yy_switch_to_buffer(CurrentFlexHandle);
	} else
		yyerror("No such string symbol '%s'", s);
}

/*
 * Set up a repeat block for parsing
 */
void 
fstk_RunRept(ULONG count)
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
void
fstk_Init(char *s)
{
	char tzFileName[_MAX_PATH + 1];

	sym_AddString("__FILE__", s);

	strcpy(tzFileName, s);
	pFileStack = NULL;
	pCurrentFile = fopen(tzFileName, "rb");
	if (pCurrentFile == NULL) {
		err(1, "Unable to open file '%s'", tzFileName);
	}

	nMacroCount = 0;
	nCurrentStatus = STAT_isInclude;
	strcpy(tzCurrentFileName, tzFileName);
	CurrentFlexHandle = yy_create_buffer(pCurrentFile);
	yy_switch_to_buffer(CurrentFlexHandle);
	nLineNo = 1;
}
