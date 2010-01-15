#ifndef	ASMOTOR_MAIN_H
#define	ASMOTOR_MAIN_H

struct sOptions {
	char gbgfx[4];
	char binary[2];
	SLONG fillchar;
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

void fatalerror(char *s);
void yyerror(char *s);

extern char temptext[1024];

#define	YY_FATAL_ERROR fatalerror

#ifdef	YYLMAX
#undef	YYLMAX
#endif
#define YYLMAX 65536

#endif
