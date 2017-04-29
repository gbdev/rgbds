#ifndef	RGBDS_MAIN_H
#define	RGBDS_MAIN_H

#include <stdbool.h>
#include "extern/stdnoreturn.h"

struct sOptions {
	char gbgfx[4];
	char binary[2];
	SLONG fillchar;
	bool verbose;
	bool haltnop;
	bool exportall;
	bool warnings; /* true to enable warnings, false to disable them. */
	    //-1 == random
};

extern char *tzNewMacro;
extern ULONG ulNewMacroSize;
extern SLONG nGBGfxID;
extern SLONG nBinaryID;

extern struct sOptions DefaultOptions;
extern struct sOptions CurrentOptions;
extern void opt_Push(void);
extern void opt_Pop(void);
extern void opt_Parse(char *s);

/*
 * Used for errors that compromise the whole assembly process by affecting the
 * folliwing code, potencially making the assembler generate errors caused by
 * the first one and unrelated to the code that the assembler complains about.
 * It is also used when the assembler goes into an invalid state (for example,
 * when it fails to allocate memory).
 */
noreturn void fatalerror(const char *fmt, ...);
/*
 * Used for errors that make it impossible to assemble correctly, but don't
 * affect the following code. The code will fail to assemble but the user will
 * get a list of all errors at the end, making it easier to fix all of them at
 * once.
 */
void yyerror(const char *fmt, ...);
/*
 * Used to warn the user about problems that don't prevent the generation of
 * valid code.
 */
void warning(const char *fmt, ...);

#define	YY_FATAL_ERROR fatalerror

#ifdef	YYLMAX
#undef	YYLMAX
#endif
#define YYLMAX 65536

#endif
