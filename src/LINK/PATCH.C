#include <stdio.h>
#include <string.h>

#include "mylink.h"
#include "symbol.h"
#include "main.h"

struct sSection *pCurrentSection;
SLONG	rpnstack[256];
SLONG	rpnp;
SLONG	nPC;

void rpnpush( SLONG i )
{
	rpnstack[rpnp++]=i;
}

SLONG rpnpop( void )
{
	return( rpnstack[--rpnp] );
}

SLONG getsymvalue( SLONG symid )
{
	switch( pCurrentSection->tSymbols[symid]->Type )
	{
		case SYM_IMPORT:
			return( sym_GetValue(pCurrentSection->tSymbols[symid]->pzName) );
			break;
		case SYM_EXPORT:
		case SYM_LOCAL:
			{
				if( strcmp(pCurrentSection->tSymbols[symid]->pzName,"@")==0 )
				{
					return( nPC );
				}
				else
					return( pCurrentSection->tSymbols[symid]->nOffset+pCurrentSection->tSymbols[symid]->pSection->nOrg );
			}
		default:
			break;
	}
	fatalerror( "*INTERNAL* UNKNOWN SYMBOL TYPE" );
	return( 0 );
}

SLONG getsymbank( SLONG symid )
{
	switch( pCurrentSection->tSymbols[symid]->Type )
	{
		case SYM_IMPORT:
			return( sym_GetBank(pCurrentSection->tSymbols[symid]->pzName) );
			break;
		case SYM_EXPORT:
		case SYM_LOCAL:
			return( pCurrentSection->tSymbols[symid]->pSection->nBank );
			//return( pCurrentSection->nBank );
		default:
			break;
	}
	fatalerror( "*INTERNAL* UNKNOWN SYMBOL TYPE" );
	return( 0 );
}

SLONG	calcrpn( struct sPatch *pPatch )
{
	SLONG	t, size;
	UBYTE	*rpn;

	rpnp=0;

	size=pPatch->nRPNSize;
	rpn=pPatch->pRPN;
	pPatch->oRelocPatch=0;

	while( size>0 )
	{
		size-=1;
		switch( *rpn++ )
		{
			case RPN_ADD:
				rpnpush( rpnpop()+rpnpop() );
				break;
			case RPN_SUB:
				t=rpnpop();
				rpnpush( rpnpop()-t );
				break;
			case RPN_MUL:
				rpnpush( rpnpop()*rpnpop() );
				break;
			case RPN_DIV:
				t=rpnpop();
				rpnpush( rpnpop()/t );
				break;
			case RPN_MOD:
				t=rpnpop();
				rpnpush( rpnpop()%t );
				break;
			case RPN_UNSUB:
				rpnpush( -rpnpop() );
				break;
			case RPN_OR:
				rpnpush( rpnpop()|rpnpop() );
				break;
			case RPN_AND:
				rpnpush( rpnpop()&rpnpop() );
				break;
			case RPN_XOR:
				rpnpush( rpnpop()^rpnpop() );
				break;
			case RPN_UNNOT:
				rpnpush( rpnpop()^0xFFFFFFFF );
				break;
			case RPN_LOGAND:
				rpnpush( rpnpop()&&rpnpop() );
				break;
			case RPN_LOGOR:
				rpnpush( rpnpop()||rpnpop() );
				break;
			case RPN_LOGUNNOT:
				rpnpush( !rpnpop() );
				break;
			case RPN_LOGEQ:
				rpnpush( rpnpop()==rpnpop() );
				break;
			case RPN_LOGNE:
				rpnpush( rpnpop()!=rpnpop() );
				break;
			case RPN_LOGGT:
				t=rpnpop();
				rpnpush( rpnpop()>t );
				break;
			case RPN_LOGLT:
				t=rpnpop();
				rpnpush( rpnpop()<t );
				break;
			case RPN_LOGGE:
				t=rpnpop();
				rpnpush( rpnpop()>=t );
				break;
			case RPN_LOGLE:
				t=rpnpop();
				rpnpush( rpnpop()<=t );
				break;
			case RPN_SHL:
				t=rpnpop();
				rpnpush( rpnpop()<<t );
				break;
			case RPN_SHR:
				t=rpnpop();
				rpnpush( rpnpop()>>t );
				break;
			case RPN_HRAM:
				t=rpnpop();
				rpnpush(t&0xFF);
				if( t<0 || (t>0xFF && t<0xFF00) || t>0xFFFF )
				{
					sprintf( temptext, "%s(%d) : Value must be in the HRAM area", pPatch->pzFilename, pPatch->nLineNo );
					fatalerror( temptext );
				}
				break;
			case RPN_PCEZP:
				t=rpnpop();
				rpnpush(t&0xFF);
				if( t<0x2000 || t>0x20FF )
				{
					sprintf( temptext, "%s(%d) : Value must be in the ZP area", pPatch->pzFilename, pPatch->nLineNo );
					fatalerror( temptext );
				}
				break;
			case RPN_CONST:
				/* constant */
				t=(*rpn++);
				t|=(*rpn++)<<8;
				t|=(*rpn++)<<16;
				t|=(*rpn++)<<24;
				rpnpush( t );
				size-=4;
				break;
			case RPN_SYM:
				/* symbol */
				t=(*rpn++);
				t|=(*rpn++)<<8;
				t|=(*rpn++)<<16;
				t|=(*rpn++)<<24;
				rpnpush( getsymvalue(t) );
				pPatch->oRelocPatch|=(getsymbank(t)!=-1);
				size-=4;
				break;
			case RPN_BANK:
				/* symbol */
				t=(*rpn++);
				t|=(*rpn++)<<8;
				t|=(*rpn++)<<16;
				t|=(*rpn++)<<24;
				rpnpush( getsymbank(t) );
				size-=4;
				break;
			case RPN_RANGECHECK:
			{
				SLONG	low,
						high;

				low =(*rpn++);
				low|=(*rpn++)<<8;
				low|=(*rpn++)<<16;
				low|=(*rpn++)<<24;
				high =(*rpn++);
				high|=(*rpn++)<<8;
				high|=(*rpn++)<<16;
				high|=(*rpn++)<<24;
				t=rpnpop();
				if( t<low || t>high  )
				{
					sprintf( temptext, "%s(%d) : Value must be in the range [%d;%d]", pPatch->pzFilename, pPatch->nLineNo, low, high );
					fatalerror( temptext );
				}
				rpnpush(t);
				size-=8;
				break;
			}
		}
	}
	return( rpnpop() );
}

void Patch( void )
{
	struct sSection *pSect;

	pSect=pSections;
	while( pSect )
	{
		struct sPatch *pPatch;

		pCurrentSection=pSect;
		pPatch=pSect->pPatches;
		while( pPatch )
		{
			SLONG t;

			nPC=pSect->nOrg+pPatch->nOffset;
			t=calcrpn( pPatch );
			switch( pPatch->Type )
			{
				case PATCH_BYTE:
					if( t>=-128 && t<=255 )
					{
						t&=0xFF;
						pSect->pData[pPatch->nOffset]=(UBYTE)t;
					}
					else
					{
						sprintf( temptext, "%s(%d) : Value must be 8-bit\n", pPatch->pzFilename, pPatch->nLineNo );
						fatalerror( temptext );
					}
					break;
				case PATCH_WORD_L:
				case PATCH_WORD_B:
					if( t>=-32768 && t<=65535 )
					{
						t&=0xFFFF;
						if( pPatch->Type==PATCH_WORD_L )
						{
							pSect->pData[pPatch->nOffset]=t&0xFF;
							pSect->pData[pPatch->nOffset+1]=(t>>8)&0xFF;
						}
						else
						{
							//	Assume big endian
							pSect->pData[pPatch->nOffset]=(t>>8)&0xFF;
							pSect->pData[pPatch->nOffset+1]=t&0xFF;
						}
					}
					else
					{
						sprintf( temptext, "%s(%d) : Value must be 16-bit\n", pPatch->pzFilename, pPatch->nLineNo );
						fatalerror( temptext );
					}
					break;
				case PATCH_LONG_L:
					pSect->pData[pPatch->nOffset+0]=t&0xFF;
					pSect->pData[pPatch->nOffset+1]=(t>>8)&0xFF;
					pSect->pData[pPatch->nOffset+2]=(t>>16)&0xFF;
					pSect->pData[pPatch->nOffset+3]=(t>>24)&0xFF;
					break;
				case PATCH_LONG_B:
					pSect->pData[pPatch->nOffset+0]=(t>>24)&0xFF;
					pSect->pData[pPatch->nOffset+1]=(t>>16)&0xFF;
					pSect->pData[pPatch->nOffset+2]=(t>>8)&0xFF;
					pSect->pData[pPatch->nOffset+3]=t&0xFF;
					break;
			}

			pPatch=pPatch->pNext;
		}

		pSect=pSect->pNext;
	}
}