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

noreturn void fatalerror(const char *fmt, ...);
void yyerror(const char *fmt, ...);

#define	YY_FATAL_ERROR fatalerror

#ifdef	YYLMAX
#undef	YYLMAX
#endif
#define YYLMAX 65536

#endif
