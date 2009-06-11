/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * INCLUDES
 *
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"asm.h"
#include	"output.h"
#include	"symbol.h"
#include	"mylink.h"
#include	"main.h"
#include	"rpn.h"
#include	"fstack.h"

#define		SECTIONCHUNK	0x4000

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Internal structures
 *
 */

void    out_SetCurrentSection (struct Section *pSect);

struct	Patch
{
	char	tzFilename[_MAX_PATH + 1];
	ULONG	nLine;
	ULONG	nOffset;
	UBYTE	nType;
	ULONG	nRPNSize;
	UBYTE	*pRPN;
	struct	Patch	*pNext;
};

struct PatchSymbol
{
	ULONG	ID;
	struct	sSymbol		*pSymbol;
	struct	PatchSymbol	*pNext;
};

struct	SectionStackEntry
{
	struct	Section	*pSection;
	struct	SectionStackEntry	*pNext;
};

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * VARIABLES
 *
 */

struct	Section		*pSectionList=NULL,
			   		*pCurrentSection=NULL;
struct	PatchSymbol *pPatchSymbols=NULL;
char	tzObjectname[_MAX_PATH];
struct	SectionStackEntry	*pSectionStack=NULL;

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Section stack routines
 *
 */

void	out_PushSection( void )
{
	struct	SectionStackEntry	*pSect;

	if( (pSect=(struct SectionStackEntry *)malloc(sizeof(struct SectionStackEntry)))!=NULL )
	{
		pSect->pSection=pCurrentSection;
		pSect->pNext=pSectionStack;
		pSectionStack=pSect;
	}
	else
		fatalerror( "No memory for section stack" );
}

void	out_PopSection( void )
{
	if( pSectionStack )
	{
		struct	SectionStackEntry	*pSect;

		pSect=pSectionStack;
		out_SetCurrentSection(pSect->pSection);
		pSectionStack=pSect->pNext;
		free( pSect );
	}
	else
		fatalerror( "No entries in the section stack" );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Count the number of symbols used in this object
 *
 */

ULONG   countsymbols( void )
{
	struct	PatchSymbol	*pSym;
	ULONG	count=0;

	pSym=pPatchSymbols;

	while( pSym )
	{
		count+=1;
		pSym=pSym->pNext;
	}

	return (count);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Count the number of sections used in this object
 *
 */

ULONG   countsections( void )
{
	struct	Section	*pSect;
	ULONG	count=0;

	pSect=pSectionList;

	while( pSect )
	{
		count+=1;
		pSect=pSect->pNext;
	}

	return( count );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Count the number of patches used in this object
 *
 */

ULONG	countpatches( struct Section *pSect )
{
	struct	Patch	*pPatch;
	ULONG	r=0;

	pPatch=pSect->pPatches;
	while( pPatch )
	{
		r+=1;
		pPatch=pPatch->pNext;
	}

	return( r );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write a long to a file (little-endian)
 *
 */

void	fputlong( ULONG i, FILE * f )
{
	fputc( i, f);
	fputc( i>>8, f );
	fputc( i>>16, f );
	fputc( i>>24, f );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write a NULL-terminated string to a file
 *
 */

void	fputstring( char *s, FILE * f )
{
	while( *s )
	fputc( *s++, f );
	fputc( 0, f );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Return a sections ID
 *
 */

ULONG	getsectid( struct Section *pSect )
{
	struct	Section	*sec;
	ULONG	ID = 0;

	sec=pSectionList;

	while( sec )
	{
		if( sec==pSect)
			return( ID );
		ID+=1;
		sec=sec->pNext;
	}

	fatalerror( "INTERNAL: Unknown section" );
	return( (ULONG)-1 );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write a patch to a file
 *
 */

void    writepatch( struct Patch *pPatch, FILE * f )
{
	fputstring( pPatch->tzFilename, f );
	fputlong( pPatch->nLine, f );
	fputlong( pPatch->nOffset, f );
	fputc( pPatch->nType, f );
	fputlong( pPatch->nRPNSize, f );
	fwrite( pPatch->pRPN, 1, pPatch->nRPNSize, f );
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write a section to a file
 *
 */

void    writesection( struct Section *pSect, FILE * f )
{
	//printf( "SECTION: %s, ID: %d\n", pSect->pzName, getsectid(pSect) );

	fputlong (pSect->nPC, f);
	fputc (pSect->nType, f);
	fputlong (pSect->nOrg, f);		//      RGB1 addition

	fputlong (pSect->nBank, f);		//      RGB1 addition

	if( (pSect->nType==SECT_HOME)
	||	(pSect->nType==SECT_CODE) )
	{
		struct Patch *pPatch;

		fwrite (pSect->tData, 1, pSect->nPC, f);
		fputlong (countpatches (pSect), f);

		pPatch = pSect->pPatches;
		while (pPatch)
		{
			writepatch (pPatch, f);
			pPatch = pPatch->pNext;
		}
	}
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write a symbol to a file
 *
 */

void    writesymbol (struct sSymbol *pSym, FILE * f)
{
	char    symname[MAXSYMLEN * 2 + 1];
	ULONG   type;
	ULONG   offset;
	SLONG   sectid;

	if (pSym->nType & SYMF_IMPORT)
	{
		/* Symbol should be imported */
		strcpy (symname, pSym->tzName);
		offset=0;
		sectid=-1;
		type = SYM_IMPORT;
	}
	else if (pSym->nType & SYMF_EXPORT)
	{
		/* Symbol should be exported */
		strcpy (symname, pSym->tzName);
		type = SYM_EXPORT;
		offset = pSym->nValue;
		if (pSym->nType & SYMF_CONST)
			sectid = -1;
		else
			sectid = getsectid (pSym->pSection);
	}
	else
	{
		/* Symbol is local to this file */
		if (pSym->nType & SYMF_LOCAL)
		{
			strcpy (symname, pSym->pScope->tzName);
			strcat (symname, pSym->tzName);
		}
		else
			strcpy (symname, pSym->tzName);
		type = SYM_LOCAL;
		offset = pSym->nValue;
		sectid = getsectid (pSym->pSection);
	}

	fputstring (symname, f);
	fputc (type, f);

	if (type != SYM_IMPORT)
	{
		fputlong (sectid, f);
		fputlong (offset, f);
	}
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Add a symbol to the object
 *
 */

ULONG   addsymbol (struct sSymbol *pSym)
{
    struct PatchSymbol *pPSym,
          **ppPSym;
    ULONG   ID = 0;

    pPSym = pPatchSymbols;
    ppPSym = &(pPatchSymbols);

    while (pPSym)
    {
		if (pSym == pPSym->pSymbol)
		    return (pPSym->ID);
		ppPSym = &(pPSym->pNext);
		pPSym = pPSym->pNext;
		ID += 1;
    }

    if( (*ppPSym=pPSym=(struct PatchSymbol *)malloc(sizeof(struct PatchSymbol)))!=NULL )
    {
		pPSym->pNext = NULL;
		pPSym->pSymbol = pSym;
		pPSym->ID = ID;
		return (ID);
    }
    else
		fatalerror ("No memory for patchsymbol");

    return ((ULONG) -1);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Add all exported symbols to the object
 *
 */

void    addexports (void)
{
    int     i;

    for (i = 0; i < HASHSIZE; i += 1)
    {
		struct sSymbol *pSym;

		pSym = tHashedSymbols[i];
		while (pSym)
		{
		    if (pSym->nType & SYMF_EXPORT)
			addsymbol (pSym);
		    pSym = pSym->pNext;
		}
    }
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Allocate a new patchstructure and link it into the list
 *
 */

struct Patch *allocpatch (void)
{
    struct Patch *pPatch,
          **ppPatch;

    pPatch = pCurrentSection->pPatches;
    ppPatch = &(pCurrentSection->pPatches);

    while (pPatch)
    {
		ppPatch = &(pPatch->pNext);
		pPatch = pPatch->pNext;
    }

    if( (*ppPatch=pPatch=(struct Patch *)malloc(sizeof (struct Patch)))!=NULL )
    {
		pPatch->pNext = NULL;
		pPatch->nRPNSize = 0;
		pPatch->pRPN = NULL;
    }
    else
		fatalerror ("No memory for patch");

    return (pPatch);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Create a new patch (includes the rpn expr)
 *
 */

void    createpatch (ULONG type, struct Expression *expr)
{
    struct Patch *pPatch;
    UWORD   rpndata;
    UBYTE   rpnexpr[2048];
    char    tzSym[512];
    ULONG   rpnptr = 0,
            symptr;

    pPatch = allocpatch ();
    pPatch->nType = type;
    strcpy (pPatch->tzFilename, tzCurrentFileName);
    pPatch->nLine = nLineNo;
    pPatch->nOffset = nPC;

    while ((rpndata = rpn_PopByte (expr)) != 0xDEAD)
    {
		switch (rpndata)
		{
		    case RPN_CONST:
				rpnexpr[rpnptr++] = RPN_CONST;
				rpnexpr[rpnptr++] = rpn_PopByte (expr);
				rpnexpr[rpnptr++] = rpn_PopByte (expr);
				rpnexpr[rpnptr++] = rpn_PopByte (expr);
				rpnexpr[rpnptr++] = rpn_PopByte (expr);
				break;
		    case RPN_SYM:
				symptr = 0;
				while( (tzSym[symptr++]=rpn_PopByte(expr))!=0 );
				if (sym_isConstant (tzSym))
				{
				    ULONG   value;

				    value = sym_GetConstantValue (tzSym);
				    rpnexpr[rpnptr++] = RPN_CONST;
				    rpnexpr[rpnptr++] = value & 0xFF;
				    rpnexpr[rpnptr++] = value >> 8;
				    rpnexpr[rpnptr++] = value >> 16;
				    rpnexpr[rpnptr++] = value >> 24;
				}
				else
				{
				    symptr = addsymbol (sym_FindSymbol (tzSym));
				    rpnexpr[rpnptr++] = RPN_SYM;
				    rpnexpr[rpnptr++] = symptr & 0xFF;
				    rpnexpr[rpnptr++] = symptr >> 8;
				    rpnexpr[rpnptr++] = symptr >> 16;
				    rpnexpr[rpnptr++] = symptr >> 24;
				}
				break;
		    case RPN_BANK:
				symptr = 0;
				while( (tzSym[symptr++]=rpn_PopByte(expr))!=0 );
				symptr = addsymbol (sym_FindSymbol (tzSym));
				rpnexpr[rpnptr++] = RPN_BANK;
				rpnexpr[rpnptr++] = symptr & 0xFF;
				rpnexpr[rpnptr++] = symptr >> 8;
				rpnexpr[rpnptr++] = symptr >> 16;
				rpnexpr[rpnptr++] = symptr >> 24;
				break;
		    default:
				rpnexpr[rpnptr++] = rpndata;
				break;
		}
    }
    if( (pPatch->pRPN=(UBYTE *)malloc(rpnptr))!=NULL )
    {
		memcpy (pPatch->pRPN, rpnexpr, rpnptr);
		pPatch->nRPNSize = rpnptr;
    }
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * A quick check to see if we have an initialized section
 *
 */

void    checksection (void)
{
    if (pCurrentSection)
		return;
    else
		fatalerror ("Code generation before SECTION directive");
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 *
 */

void    checkcodesection (SLONG size)
{
    checksection ();
    if ((pCurrentSection->nType == SECT_HOME || pCurrentSection->nType == SECT_CODE)
	&& (pCurrentSection->nPC + size <= MAXSECTIONSIZE))
	{
		if( ((pCurrentSection->nPC%SECTIONCHUNK)>((pCurrentSection->nPC+size)%SECTIONCHUNK))
		&&	(pCurrentSection->nType == SECT_HOME || pCurrentSection->nType == SECT_CODE) )
		{
			if( (pCurrentSection->tData=
				(UBYTE *)realloc( pCurrentSection->tData,
				((pCurrentSection->nPC+size)/SECTIONCHUNK+1)*SECTIONCHUNK))!=NULL )
			{
				return;
			}
			else
				fatalerror( "Not enough memory to expand section" );
		}
		return;
	}
    else
		fatalerror ("Section can't contain initialized data or section limit exceeded");
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Write an objectfile
 *
 */

void    out_WriteObject (void)
{
    FILE   *f;

    addexports ();

    if( (f=fopen(tzObjectname,"wb"))!=NULL )
    {
		struct PatchSymbol *pSym;
		struct Section *pSect;

		fwrite ("RGB2", 1, 4, f);
		fputlong (countsymbols (), f);
		fputlong (countsections (), f);

		pSym = pPatchSymbols;
		while (pSym)
		{
		    writesymbol (pSym->pSymbol, f);
		    pSym = pSym->pNext;
		}

		pSect = pSectionList;
		while (pSect)
		{
		    writesection (pSect, f);
		    pSect = pSect->pNext;
		}

		fclose (f);
    }
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Prepare for pass #2
 *
 */

void    out_PrepPass2 (void)
{
    struct Section *pSect;

    pSect = pSectionList;
    while (pSect)
    {
		pSect->nPC = 0;
		pSect = pSect->pNext;
    }
    pCurrentSection = NULL;
	pSectionStack = NULL;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Set the objectfilename
 *
 */

void    out_SetFileName (char *s)
{
    strcpy (tzObjectname, s);
    printf ("Output filename %s\n", s);
    pSectionList = NULL;
    pCurrentSection = NULL;
    pPatchSymbols = NULL;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Find a section by name and type.  If it doesn't exist, create it
 *
 */

struct Section *out_FindSection (char *pzName, ULONG secttype, SLONG org, SLONG bank)
{
    struct Section *pSect,
          **ppSect;

    ppSect = &pSectionList;
    pSect = pSectionList;

    while (pSect)
    {
		if (strcmp (pzName, pSect->pzName) == 0)
		{
		    if( secttype==pSect->nType && ((ULONG)org)==pSect->nOrg && ((ULONG)bank)==pSect->nBank)
			{
				return (pSect);
			}
		    else
				fatalerror ("Section already exists but with a different type");
		}
		ppSect = &(pSect->pNext);
		pSect = pSect->pNext;
    }

    if( (*ppSect=(pSect=(struct Section *)malloc(sizeof(struct Section))))!=NULL )
    {
		if( (pSect->pzName=(char *)malloc(strlen(pzName)+1))!=NULL )
		{
		    strcpy (pSect->pzName, pzName);
		    pSect->nType = secttype;
		    pSect->nPC = 0;
		    pSect->nOrg = org;
		    pSect->nBank = bank;
		    pSect->pNext = NULL;
		    pSect->pPatches = NULL;
		    pPatchSymbols = NULL;

			if( (pSect->tData=(UBYTE *)malloc(SECTIONCHUNK))!=NULL )
			{
		    	return (pSect);
			}
		    else
				fatalerror ("Not enough memory for section");
		}
		else
		    fatalerror ("Not enough memory for sectionname");
    }
    else
		fatalerror ("Not enough memory for section");

    return (NULL);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Set the current section
 *
 */

void    out_SetCurrentSection (struct Section *pSect)
{
    pCurrentSection = pSect;
    nPC = pSect->nPC;

    pPCSymbol->nValue = nPC;
    pPCSymbol->pSection = pCurrentSection;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Set the current section by name and type
 *
 */

void    out_NewSection (char *pzName, ULONG secttype)
{
    out_SetCurrentSection (out_FindSection (pzName, secttype, -1, -1));
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Set the current section by name and type
 *
 */

void    out_NewAbsSection (char *pzName, ULONG secttype, SLONG org, SLONG bank)
{
    out_SetCurrentSection (out_FindSection (pzName, secttype, org, bank));
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output an absolute byte
 *
 */

void    out_AbsByte (int b)
{
    checkcodesection (1);
    b &= 0xFF;
    if (nPass == 2)
		pCurrentSection->tData[nPC] = b;

	pCurrentSection->nPC += 1;
    nPC += 1;
    pPCSymbol->nValue += 1;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Skip this many bytes
 *
 */

void    out_Skip (int skip)
{
    checksection ();
	if( (CurrentOptions.fillchar==-1)
	||	!((pCurrentSection->nType == SECT_HOME) || (pCurrentSection->nType == SECT_CODE)) )
	{
	    pCurrentSection->nPC += skip;
	    nPC += skip;
	    pPCSymbol->nValue += skip;
	}
	else
	{
		checkcodesection( skip );
		while( skip-- )
			out_AbsByte( CurrentOptions.fillchar );
	}
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a NULL terminated string (excluding the NULL-character)
 *
 */

void    out_String (char *s)
{
    checkcodesection (strlen (s));
    while (*s)
	out_AbsByte (*s++);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a relocatable byte.  Checking will be done to see if it
 * is an absolute value in disguise.
 *
 */

void    out_RelByte (struct Expression *expr)
{
    checkcodesection (1);
    if (rpn_isReloc (expr))
    {
		if (nPass == 2)
		{
		    pCurrentSection->tData[nPC] = 0;
		    createpatch (PATCH_BYTE, expr);
		}
		pCurrentSection->nPC += 1;
		nPC += 1;
		pPCSymbol->nValue += 1;
    }
    else
		out_AbsByte (expr->nVal);

	rpn_Reset (expr);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output an absolute word
 *
 */

void    out_AbsWord (int b)
{
    checkcodesection (2);
    b &= 0xFFFF;
    if (nPass == 2)
    {
		if( CurrentOptions.endian==ASM_LITTLE_ENDIAN )
		{
			pCurrentSection->tData[nPC] = b & 0xFF;
			pCurrentSection->tData[nPC + 1] = b >> 8;
		}
		else
		{
			//	Assume big endian
			pCurrentSection->tData[nPC] = b >> 8;
			pCurrentSection->tData[nPC+1] = b & 0xFF;
		}
    }
    pCurrentSection->nPC += 2;
    nPC += 2;
    pPCSymbol->nValue += 2;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a relocatable word.  Checking will be done to see if
 * is an absolute value in disguise.
 *
 */

void    out_RelWord (struct Expression *expr)
{
	ULONG	b;

    checkcodesection (2);
    b = expr->nVal&0xFFFF;
    if (rpn_isReloc (expr))
    {
		if (nPass == 2)
		{
			if( CurrentOptions.endian==ASM_LITTLE_ENDIAN )
			{
			    pCurrentSection->tData[nPC] = b & 0xFF;
		    	pCurrentSection->tData[nPC + 1] = b >> 8;
			    createpatch (PATCH_WORD_L,expr);
			}
			else
			{
				//	Assume big endian
		    	pCurrentSection->tData[nPC] = b >> 8;
			    pCurrentSection->tData[nPC+1] = b & 0xFF;
			    createpatch (PATCH_WORD_B,expr);
			}
		}
		pCurrentSection->nPC += 2;
		nPC += 2;
		pPCSymbol->nValue += 2;
    }
    else
		out_AbsWord (expr->nVal);
    rpn_Reset (expr);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output an absolute longword
 *
 */

void    out_AbsLong (SLONG b)
{
    checkcodesection (sizeof(SLONG));
    if (nPass == 2)
    {
		if( CurrentOptions.endian==ASM_LITTLE_ENDIAN )
		{
			pCurrentSection->tData[nPC] = b & 0xFF;
			pCurrentSection->tData[nPC + 1] = b >> 8;
			pCurrentSection->tData[nPC + 2] = b >> 16;
			pCurrentSection->tData[nPC + 3] = b >> 24;
		}
		else
		{
			//	Assume big endian
			pCurrentSection->tData[nPC] = b >> 24;
			pCurrentSection->tData[nPC+1] = b >> 16;
			pCurrentSection->tData[nPC+2] = b >> 8;
			pCurrentSection->tData[nPC+3] = b & 0xFF;
		}
    }
    pCurrentSection->nPC += 4;
    nPC += 4;
    pPCSymbol->nValue += 4;
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a relocatable longword.  Checking will be done to see if
 * is an absolute value in disguise.
 *
 */

void    out_RelLong (struct Expression *expr)
{
	SLONG	b;

    checkcodesection (4);
	b=expr->nVal;
    if (rpn_isReloc (expr))
    {
		if (nPass == 2)
		{
			if( CurrentOptions.endian==ASM_LITTLE_ENDIAN )
			{
			    pCurrentSection->tData[nPC] = b & 0xFF;
		    	pCurrentSection->tData[nPC + 1] = b >> 8;
		    	pCurrentSection->tData[nPC + 2] = b >> 16;
		    	pCurrentSection->tData[nPC + 3] = b >> 24;
			    createpatch (PATCH_LONG_L,expr);
			}
			else
			{
				//	Assume big endian
		    	pCurrentSection->tData[nPC] = b >> 24;
		    	pCurrentSection->tData[nPC+1] = b >> 16;
		    	pCurrentSection->tData[nPC+2] = b >> 8;
			    pCurrentSection->tData[nPC+3] = b & 0xFF;
			    createpatch (PATCH_LONG_B,expr);
			}
		}
		pCurrentSection->nPC += 4;
		nPC += 4;
		pPCSymbol->nValue += 4;
    }
    else
		out_AbsLong (expr->nVal);
    rpn_Reset (expr);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a PC-relative byte
 *
 */

void    out_PCRelByte (struct Expression *expr)
{
	SLONG	b=expr->nVal;

    checkcodesection (1);
    b = (b & 0xFFFF) - (nPC + 1);
    if (nPass == 2 && (b < -128 || b > 127))
		yyerror ("PC-relative value must be 8-bit");

    out_AbsByte (b);
    rpn_Reset (expr);
}

/*
 * RGBAsm - OUTPUT.C - Outputs an objectfile
 *
 * Output a binary file
 *
 */

void    out_BinaryFile (char *s)
{
    FILE   *f;

    fstk_FindFile (s);

    if( (f=fopen(s,"rb"))!=NULL )
    {
		SLONG	fsize;

		fseek (f, 0, SEEK_END);
		fsize = ftell (f);
		fseek (f, 0, SEEK_SET);

		checkcodesection (fsize);

		if (nPass == 2)
		{
		    SLONG	dest = nPC;
		    SLONG	todo = fsize;

		    while (todo--)
				pCurrentSection->tData[dest++] = fgetc (f);
		}

		pCurrentSection->nPC += fsize;
		nPC += fsize;
		pPCSymbol->nValue += fsize;
		fclose (f);
    }
    else
		fatalerror ("File not found");
}