/*	fstack.h
 *
 *	Contains some assembler-wide defines and externs
 *
 *	Copyright 1997 Carsten Sorensen
 *
 */

#ifndef FSTACK_H
#define FSTACK_H

#include "asm.h"
#include "types.h"
#include "lexer.h"

struct sContext
{
   YY_BUFFER_STATE		FlexHandle;
   struct	sSymbol		*pMacro;
   struct	sContext	*pNext;
   char		tzFileName[_MAX_PATH+1];
   char		*tzMacroArgs[MAXMACROARGS+1];
   SLONG	nLine;
   ULONG	nStatus;
   FILE		*pFile;
   char		*pREPTBlock;
   ULONG	nREPTBlockCount;
   ULONG	nREPTBlockSize;
};

extern	ULONG	fstk_RunInclude( char *s );
extern	void	fstk_RunMacroArg( SLONG s );
extern	ULONG	fstk_Init( char *s );
extern	void	fstk_Dump( void );
extern	void	fstk_AddIncludePath( char *s );
extern	ULONG	fstk_RunMacro( char *s );
extern	void	fstk_RunRept( ULONG count );
extern	void	fstk_FindFile( char *s );

extern	int		yywrap( void );

#endif
