/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"

#include "link/assign.h"
#include "link/main.h"
#include "link/mylink.h"
#include "link/symbol.h"

static struct sSection *pCurrentSection;
static int32_t rpnstack[256];
static int32_t rpnp;
int32_t nPC;

static void rpnpush(int32_t i)
{
	rpnstack[rpnp] = i;
	rpnp++;
}

static int32_t rpnpop(void)
{
	rpnp--;
	return rpnstack[rpnp];
}

static int32_t getsymvalue(int32_t symid)
{
	const struct sSymbol *tSymbol = pCurrentSection->tSymbols[symid];

	switch (tSymbol->Type) {
	case SYM_IMPORT:
		return sym_GetValue(tSymbol->pzName);

	case SYM_EXPORT:
	case SYM_LOCAL:
		if (strcmp(tSymbol->pzName, "@") == 0)
			return nPC;

		return tSymbol->nOffset + tSymbol->pSection->nOrg;

	default:
		break;
	}

	errx(1, "%s: Unknown symbol type", __func__);
}

static int32_t getrealbankfrominternalbank(int32_t n)
{
	if (BankIndexIsWRAM0(n) || BankIndexIsROM0(n) ||
	    BankIndexIsOAM(n)   || BankIndexIsHRAM(n)) {
		return 0;
	} else if (BankIndexIsROMX(n)) {
		return n - BANK_INDEX_ROMX + 1;
	} else if (BankIndexIsWRAMX(n)) {
		return n - BANK_INDEX_WRAMX + 1;
	} else if (BankIndexIsVRAM(n)) {
		return n - BANK_INDEX_VRAM;
	} else if (BankIndexIsSRAM(n)) {
		return n - BANK_INDEX_SRAM;
	}

	return n;
}

static int32_t getsymbank(int32_t symid)
{
	int32_t nBank;
	const struct sSymbol *tSymbol = pCurrentSection->tSymbols[symid];

	switch (tSymbol->Type) {
	case SYM_IMPORT:
		nBank = sym_GetBank(tSymbol->pzName);
		break;
	case SYM_EXPORT:
	case SYM_LOCAL:
		nBank = tSymbol->pSection->nBank;
		break;
	default:
		errx(1, "%s: Unknown symbol type", __func__);
	}

	return getrealbankfrominternalbank(nBank);
}

int32_t calcrpn(struct sPatch *pPatch)
{
	int32_t t, size;
	uint8_t *rpn;
	uint8_t rpn_cmd;
	int32_t nBank;

	rpnp = 0;

	size = pPatch->nRPNSize;
	rpn = pPatch->pRPN;
	pPatch->oRelocPatch = 0;

	while (size > 0) {
		size -= 1;
		rpn_cmd = *rpn++;

		switch (rpn_cmd) {
		case RPN_ADD:
			rpnpush(rpnpop() + rpnpop());
			break;
		case RPN_SUB:
			t = rpnpop();
			rpnpush(rpnpop() - t);
			break;
		case RPN_MUL:
			rpnpush(rpnpop() * rpnpop());
			break;
		case RPN_DIV:
			t = rpnpop();
			rpnpush(rpnpop() / t);
			break;
		case RPN_MOD:
			t = rpnpop();
			rpnpush(rpnpop() % t);
			break;
		case RPN_UNSUB:
			rpnpush(-rpnpop());
			break;
		case RPN_OR:
			rpnpush(rpnpop() | rpnpop());
			break;
		case RPN_AND:
			rpnpush(rpnpop() & rpnpop());
			break;
		case RPN_XOR:
			rpnpush(rpnpop() ^ rpnpop());
			break;
		case RPN_UNNOT:
			rpnpush(~rpnpop());
			break;
		case RPN_LOGAND:
			rpnpush(rpnpop() && rpnpop());
			break;
		case RPN_LOGOR:
			rpnpush(rpnpop() || rpnpop());
			break;
		case RPN_LOGUNNOT:
			rpnpush(!rpnpop());
			break;
		case RPN_LOGEQ:
			rpnpush(rpnpop() == rpnpop());
			break;
		case RPN_LOGNE:
			rpnpush(rpnpop() != rpnpop());
			break;
		case RPN_LOGGT:
			t = rpnpop();
			rpnpush(rpnpop() > t);
			break;
		case RPN_LOGLT:
			t = rpnpop();
			rpnpush(rpnpop() < t);
			break;
		case RPN_LOGGE:
			t = rpnpop();
			rpnpush(rpnpop() >= t);
			break;
		case RPN_LOGLE:
			t = rpnpop();
			rpnpush(rpnpop() <= t);
			break;
		case RPN_SHL:
			t = rpnpop();
			rpnpush(rpnpop() << t);
			break;
		case RPN_SHR:
			t = rpnpop();
			rpnpush(rpnpop() >> t);
			break;
		case RPN_HRAM:
			t = rpnpop();
			rpnpush(t & 0xFF);
			if (t < 0 || (t > 0xFF && t < 0xFF00) || t > 0xFFFF) {
				errx(1,
				     "%s(%ld) : Value must be in the HRAM area",
				     pPatch->pzFilename, pPatch->nLineNo);
			}
			break;
		case RPN_CONST:
			/* constant */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(t);
			size -= 4;
			break;
		case RPN_SYM:
			/* symbol */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(getsymvalue(t));
			pPatch->oRelocPatch |= (getsymbank(t) != -1);
			size -= 4;
			break;
		case RPN_BANK_SYM:
			/* symbol */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(getsymbank(t));
			size -= 4;
			break;
		case RPN_BANK_SECT:
		{
			char *name = (char *)rpn;

			struct sSection *pSection = GetSectionByName(name);

			if (pSection == NULL) {
				errx(1, "Requested BANK() of section \"%s\", which was not found.\n",
				     name);
			}

			nBank = pSection->nBank;
			rpnpush(getrealbankfrominternalbank(nBank));

			int len = strlen(name);

			size -= len + 1;
			rpn += len + 1;
			break;
		}
		case RPN_BANK_SELF:
			nBank = pCurrentSection->nBank;
			rpnpush(getrealbankfrominternalbank(nBank));
			break;
		default:
			errx(1, "%s: Invalid command %d\n", __func__,
			     rpn_cmd);
			break;
		}
	}
	return rpnpop();
}

void Patch(void)
{
	struct sSection *pSect;

	pSect = pSections;
	while (pSect) {
		struct sPatch *pPatch;

		pCurrentSection = pSect;
		pPatch = pSect->pPatches;
		while (pPatch) {
			int32_t t;
			int32_t nPatchOrg;

			nPC = pSect->nOrg + pPatch->nOffset;
			t = calcrpn(pPatch);
			switch (pPatch->Type) {
			case PATCH_BYTE:
				if (t >= -128 && t <= 255) {
					t &= 0xFF;
					pSect->pData[pPatch->nOffset] =
						(uint8_t)t;
				} else {
					errx(1,
					     "%s(%ld) : Value must be 8-bit",
					     pPatch->pzFilename,
					     pPatch->nLineNo);
				}
				break;
			case PATCH_WORD_L:
				if (t >= -32768 && t <= 65535) {
					t &= 0xFFFF;
					pSect->pData[pPatch->nOffset] =
						t & 0xFF;
					pSect->pData[pPatch->nOffset + 1] =
						(t >> 8) & 0xFF;
				} else {
					errx(1,
					     "%s(%ld) : Value must be 16-bit",
					     pPatch->pzFilename,
					     pPatch->nLineNo);
				}
				break;
			case PATCH_LONG_L:
				pSect->pData[pPatch->nOffset + 0] = t & 0xFF;
				pSect->pData[pPatch->nOffset + 1] =
					(t >> 8) & 0xFF;
				pSect->pData[pPatch->nOffset + 2] =
					(t >> 16) & 0xFF;
				pSect->pData[pPatch->nOffset + 3] =
					(t >> 24) & 0xFF;
				break;
			case PATCH_BYTE_JR:
				/* Calculate absolute address of the patch */
				nPatchOrg = pSect->nOrg + pPatch->nOffset;

				/* t contains the destination of the jump */
				t = (int16_t)((t & 0xFFFF) - (nPatchOrg + 1));

				if (t >= -128 && t <= 127) {
					t &= 0xFF;
					pSect->pData[pPatch->nOffset] =
						(uint8_t)t;
				} else {
					errx(1,
					     "%s(%ld) : Value must be 8-bit",
					     pPatch->pzFilename,
					     pPatch->nLineNo);
				}
				break;
			default:
				errx(1, "%s: Internal error.", __func__);
			}

			pPatch = pPatch->pNext;
		}

		pSect = pSect->pNext;
	}
}
