#ifndef RGBDS_ASM_RPN_H
#define RGBDS_ASM_RPN_H

#include <stdint.h>

struct Expression {
	int32_t  nVal;
	uint8_t  tRPN[256];
	uint32_t nRPNLength;
	uint32_t nRPNOut;
	uint32_t isReloc;
	uint32_t isPCRel;
};

uint32_t rpn_isReloc(struct Expression * expr);
uint32_t rpn_isPCRelative(struct Expression * expr);
void rpn_Symbol(struct Expression * expr, char *tzSym);
void rpn_Number(struct Expression * expr, uint32_t i);
void rpn_LOGNOT(struct Expression * expr, struct Expression * src1);
void
rpn_LOGOR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGAND(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGEQU(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGGT(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGLT(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGGE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGLE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_LOGNE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_ADD(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_SUB(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_XOR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_OR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_AND(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_SHL(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_SHR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_MUL(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_DIV(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void
rpn_MOD(struct Expression * expr, struct Expression * src1,
    struct Expression * src2);
void rpn_HIGH(struct Expression * expr, struct Expression * src);
void rpn_LOW(struct Expression * expr, struct Expression * src);
void rpn_UNNEG(struct Expression * expr, struct Expression * src);
void rpn_UNNOT(struct Expression * expr, struct Expression * src);
uint16_t rpn_PopByte(struct Expression * expr);
void rpn_Bank(struct Expression * expr, char *tzSym);
void rpn_Reset(struct Expression * expr);
void rpn_CheckHRAM(struct Expression * expr, struct Expression * src1);

#endif
