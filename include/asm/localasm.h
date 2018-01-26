/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_LOCALASM_H
#define RGBDS_ASM_LOCALASM_H

/*
 * GB Z80 instruction groups
 *
 * n3 = 3-bit
 * n  = 8-bit
 * nn = 16-bit
 *
 * ADC  A,n         :   0xCE
 * ADC  A,r         :   0x88|r
 * ADD  A,n         :   0xC6
 * ADD  A,r         :   0x80|r
 * ADD  HL,ss       :   0x09|(ss<<4)
 * ADD  SP,n        :   0xE8
 * AND  A,n         :   0xE6
 * AND  A,r         :   0xA0|r
 * BIT  n3,r        :   0xCB 0x40|(n3<<3)|r
 * CALL cc,nn       :   0xC4|(cc<<3)
 * CALL nn          :   0xCD
 * CCF              :   0x3F
 * CP   A,n         :   0xFE
 * CP   A,r         :   0xB8|r
 * CPL              :   0x2F
 * DAA              :   0x27
 * DEC  r           :   0x05|(r<<3)
 * DEC  ss          :   0x0B|(ss<<4)
 * DI               :   0xF3
 * EI               :   0xFB
 * HALT             :   0x76
 * INC  r           :   0x04|(r<<3)
 * INC  ss          :   0x03|(ss<<4)
 * JP   HL          :   0xE9
 * JP   cc,nn       :   0xC2|(cc<<3)
 * JP   nn          :   0xC3|(cc<<3)
 * JR   n           :   0x18
 * JR   cc,n        :   0x20|(cc<<3)
 * LD   (nn),SP     :   0x08
 * LD   ($FF00+C),A :   0xE2
 * LD   ($FF00+n),A :   0xE0
 * LD   (nn),A      :   0xEA
 * LD   (rr),A      :   0x02|(rr<<4) // HL+ and HL- included
 * LD   A,($FF00+C) :   0xF2
 * LD   A,($FF00+n) :   0xF0
 * LD   A,(nn)      :   0xFA
 * LD   A,(rr)      :   0x0A|(rr<<4) // HL+ and HL- included
 * LD   HL,SP+n     :   0xF8
 * LD   SP,HL       :   0xF9
 * LD   r,n         :   0x06|(r<<3)
 * LD   r,r'        :   0x40|(r<<3)|r' // NOTE: LD (HL),(HL) not allowed
 * LD   ss,nn       :   0x01|(ss<<4)
 * NOP              :   0x00
 * OR   A,n         :   0xF6
 * OR   A,r         :   0xB0|r
 * POP  tt          :   0xC1|(tt<<4)
 * PUSH tt          :   0xC5|(tt<<4)
 * RES  n3,r        :   0xCB 0x80|(n3<<3)|r
 * RET              :   0xC9
 * RET  cc          :   0xC0|(cc<<3)
 * RETI             :   0xD9
 * RL   r           :   0xCB 0x10|r
 * RLA              :   0x17
 * RLC  r           :   0xCB 0x00|r
 * RLCA             :   0x07
 * RR   r           :   0xCB 0x18|r
 * RRA              :   0x1F
 * RRC  r           :   0xCB 0x08|r
 * RRCA             :   0x0F
 * RST  n           :   0xC7|n
 * SBC  A,n         :   0xDE
 * SBC  A,r         :   0x98|r
 * SCF              :   0x37
 * SET  n3,r        :   0xCB 0xC0|(n8<<3)|r
 * SLA  r           :   0xCB 0x20|r
 * SRA  r           :   0xCB 0x28|r
 * SRL  r           :   0xCB 0x38|r
 * STOP             :   0x10 0x00
 * SUB  A,n         :   0xD6
 * SUB  A,r         :   0x90|r
 * SWAP r           :   0xCB 0x30|r
 * XOR  A,n         :   0xEE
 * XOR  A,r         :   0xA8|r
 */

/* "r" defs */
enum {
	REG_B = 0,
	REG_C,
	REG_D,
	REG_E,
	REG_H,
	REG_L,
	REG_HL_IND,
	REG_A
};

/* "rr" defs */
enum {
	REG_BC_IND = 0,
	REG_DE_IND,
	REG_HL_INDINC,
	REG_HL_INDDEC,
};

/* "ss" defs (SP) and "tt" defs (AF) */
enum {
	REG_BC = 0,
	REG_DE = 1,
	REG_HL = 2,
	REG_SP = 3,
	REG_AF = 3
};

/* "cc" defs */
enum {
	CC_NZ = 0,
	CC_Z,
	CC_NC,
	CC_C
};

#endif /* RGBDS_ASM_LOCALASM_H */
