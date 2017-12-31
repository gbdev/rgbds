#ifndef RGBDS_ASM_LEXER_H
#define RGBDS_ASM_LEXER_H

#include <stdint.h>
#include <stdio.h>

#define LEXHASHSIZE (1 << 11)
#define MAXSTRLEN 255

struct sLexInitString {
	char *tzName;
	uint32_t nToken;
};

struct sLexFloat {
	uint32_t(*Callback) (char *s, uint32_t size);
	uint32_t nToken;
};

struct yy_buffer_state {
	char *pBufferRealStart; // actual starting address
	char *pBufferStart; // address where the data is initially written
	                    // after the "safety margin"
	char *pBuffer;
	uint32_t nBufferSize;
	uint32_t oAtLineStart;
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
extern YY_BUFFER_STATE yy_scan_bytes(char *mem, uint32_t size);
extern void yy_delete_buffer(YY_BUFFER_STATE);
extern void yy_switch_to_buffer(YY_BUFFER_STATE);
extern uint32_t lex_FloatAlloc(struct sLexFloat * tok);
extern void lex_FloatAddRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_FloatDeleteRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_FloatAddFirstRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_FloatDeleteFirstRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_FloatAddSecondRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_FloatDeleteSecondRange(uint32_t id, uint16_t start, uint16_t end);
extern void lex_Init(void);
extern void lex_AddStrings(struct sLexInitString * lex);
extern void lex_SetBuffer(char *buffer, uint32_t len);
extern uint32_t yylex(void);
extern void yyunput(char c);
extern void yyunputstr(char *s);
extern void yyskipbytes(uint32_t count);
extern void yyunputbytes(uint32_t count);

extern YY_BUFFER_STATE pCurrentBuffer;

extern void upperstring(char *s);
extern void lowerstring(char *s);

#endif
