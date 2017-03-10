#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "link/assign.h"
#include "link/mylink.h"
#include "link/symbol.h"
#include "link/main.h"

struct sSection *pCurrentSection;
SLONG rpnstack[256];
SLONG rpnp;
SLONG nPC;

void 
rpnpush(SLONG i)
{
	rpnstack[rpnp++] = i;
}

SLONG 
rpnpop(void)
{
	return (rpnstack[--rpnp]);
}

SLONG 
getsymvalue(SLONG symid)
{
	switch (pCurrentSection->tSymbols[symid]->Type) {
		case SYM_IMPORT:
		return (sym_GetValue(pCurrentSection->tSymbols[symid]->pzName));
		break;
	case SYM_EXPORT:
	case SYM_LOCAL:
		{
			if (strcmp
			    (pCurrentSection->tSymbols[symid]->pzName,
				"@") == 0) {
				return (nPC);
			} else
				return (pCurrentSection->tSymbols[symid]->
				    nOffset +
				    pCurrentSection->tSymbols[symid]->
				    pSection->nOrg);
		}
	default:
		break;
	}
	errx(1, "*INTERNAL* UNKNOWN SYMBOL TYPE");
}

SLONG 
getsymbank(SLONG symid)
{
	SLONG nBank;

	switch (pCurrentSection->tSymbols[symid]->Type) {
	case SYM_IMPORT:
		nBank = sym_GetBank(pCurrentSection->tSymbols[symid]->pzName);
		break;
	case SYM_EXPORT:
	case SYM_LOCAL:
		nBank = pCurrentSection->tSymbols[symid]->pSection->nBank;
		break;
	default:
		errx(1, "*INTERNAL* UNKNOWN SYMBOL TYPE");
	}

	if (nBank == BANK_WRAM0 || nBank == BANK_OAM) return 0;
	if (nBank >= BANK_WRAMX && nBank < (BANK_WRAMX + BANK_COUNT_WRAMX))
		return nBank - BANK_WRAMX + 1;
	if (nBank >= BANK_VRAM && nBank < (BANK_VRAM + BANK_COUNT_VRAM))
		return nBank - BANK_VRAM;
	if (nBank >= BANK_SRAM && nBank < (BANK_SRAM + BANK_COUNT_SRAM))
		return nBank - BANK_SRAM;

	return nBank;
}

SLONG 
calcrpn(struct sPatch * pPatch)
{
	SLONG t, size;
	UBYTE *rpn;

	rpnp = 0;

	size = pPatch->nRPNSize;
	rpn = pPatch->pRPN;
	pPatch->oRelocPatch = 0;

	while (size > 0) {
		size -= 1;
		switch (*rpn++) {
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
			rpnpush(rpnpop() ^ 0xFFFFFFFF);
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
		case RPN_PCEZP:
			t = rpnpop();
			rpnpush(t & 0xFF);
			if (t < 0x2000 || t > 0x20FF) {
				errx(1,
				    "%s(%ld) : Value must be in the ZP area",
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
		case RPN_BANK:
			/* symbol */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(getsymbank(t));
			size -= 4;
			break;
		case RPN_RANGECHECK:
			{
				SLONG low, high;

				low = (*rpn++);
				low |= (*rpn++) << 8;
				low |= (*rpn++) << 16;
				low |= (*rpn++) << 24;
				high = (*rpn++);
				high |= (*rpn++) << 8;
				high |= (*rpn++) << 16;
				high |= (*rpn++) << 24;
				t = rpnpop();
				if (t < low || t > high) {
					errx(1,
					    "%s(%ld) : Value must be in the range [%ld;%ld]",
					    pPatch->pzFilename,
					    pPatch->nLineNo, low, high);
				}
				rpnpush(t);
				size -= 8;
				break;
			}
		}
	}
	return (rpnpop());
}

void 
Patch(void)
{
	struct sSection *pSect;

	pSect = pSections;
	while (pSect) {
		struct sPatch *pPatch;

		pCurrentSection = pSect;
		pPatch = pSect->pPatches;
		while (pPatch) {
			SLONG t;

			nPC = pSect->nOrg + pPatch->nOffset;
			t = calcrpn(pPatch);
			switch (pPatch->Type) {
			case PATCH_BYTE:
				if (t >= -128 && t <= 255) {
					t &= 0xFF;
					pSect->pData[pPatch->nOffset] =
					    (UBYTE) t;
				} else {
					errx(1,
					    "%s(%ld) : Value must be 8-bit",
					    pPatch->pzFilename,
					    pPatch->nLineNo);
				}
				break;
			case PATCH_WORD_L:
			case PATCH_WORD_B:
				if (t >= -32768 && t <= 65535) {
					t &= 0xFFFF;
					if (pPatch->Type == PATCH_WORD_L) {
						pSect->pData[pPatch->nOffset] =
						    t & 0xFF;
						pSect->pData[pPatch->nOffset +
						    1] =
						    (t >> 8) & 0xFF;
					} else {
						//Assume big endian
						    pSect->pData[pPatch->nOffset] =
						    (t >> 8) & 0xFF;
						pSect->pData[pPatch->nOffset +
						    1] = t & 0xFF;
					}
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
			case PATCH_LONG_B:
				pSect->pData[pPatch->nOffset + 0] =
				    (t >> 24) & 0xFF;
				pSect->pData[pPatch->nOffset + 1] =
				    (t >> 16) & 0xFF;
				pSect->pData[pPatch->nOffset + 2] =
				    (t >> 8) & 0xFF;
				pSect->pData[pPatch->nOffset + 3] = t & 0xFF;
				break;
			}

			pPatch = pPatch->pNext;
		}

		pSect = pSect->pNext;
	}
}
