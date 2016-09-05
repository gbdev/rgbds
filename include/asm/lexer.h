#ifndef RGBDS_ASM_LEXER_H
#define RGBDS_ASM_LEXER_H

#include <stdio.h>

#include "types.h"

#define LEXHASHSIZE (1 << 11)
#define MAXSTRLEN 255

struct sLexInitString {
	char *tzName;
	ULONG nToken;
};

struct sLexFloat {
	ULONG(*Callback) (char *s, ULONG size);
	ULONG nToken;
};

struct yy_buffer_state {
	char *pBufferRealStart; // actual starting address
	char *pBufferStart; // address where the data is initially written
	                    // after the "safety margin"
	char *pBuffer;
	ULONG nBufferSize;
	ULONG oAtLineStart;
};

enum eLexerState {
	LEX_STATE_NORMAL,
	LEX_STATE_MACROARGS
};
#define INITIAL			0
#define macroarg		3

typedef struct yy_buffer_state *YY_BUFFER_STATE;

extern void yy_set_state(enum eLexerState i);
extern YY_BUFFER_STATE yy_create_buffer(FILE * f);
extern YY_BUFFER_STATE yy_scan_bytes(char *mem, ULONG size);
extern void yy_delete_buffer(YY_BUFFER_STATE);
extern void yy_switch_to_buffer(YY_BUFFER_STATE);
extern ULONG lex_FloatAlloc(struct sLexFloat * tok);
extern void lex_FloatAddRange(ULONG id, UWORD start, UWORD end);
extern void lex_FloatDeleteRange(ULONG id, UWORD start, UWORD end);
extern void lex_FloatAddFirstRange(ULONG id, UWORD start, UWORD end);
extern void lex_FloatDeleteFirstRange(ULONG id, UWORD start, UWORD end);
extern void lex_FloatAddSecondRange(ULONG id, UWORD start, UWORD end);
extern void lex_FloatDeleteSecondRange(ULONG id, UWORD start, UWORD end);
extern void lex_Init(void);
extern void lex_AddStrings(struct sLexInitString * lex);
extern void lex_SetBuffer(char *buffer, ULONG len);
extern ULONG yylex(void);
extern void yyunput(char c);
extern void yyunputstr(char *s);
extern void yyskipbytes(ULONG count);
extern void yyunputbytes(ULONG count);

extern YY_BUFFER_STATE pCurrentBuffer;

extern void upperstring(char *s);
extern void lowerstring(char *s);

#endif
