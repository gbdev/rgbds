/*
 * RGBAsm - SYMBOL.C - Symboltable and macroargs stuff
 *
 * INCLUDES
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "asm.h"
#include "symbol.h"
#include "main.h"
#include "mymath.h"
#include "output.h"

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * VARIABLES
 *
 */

struct sSymbol *tHashedSymbols[HASHSIZE];
struct sSymbol *pScope = NULL;
struct sSymbol *pPCSymbol = NULL;
struct sSymbol *p_NARGSymbol = NULL;
struct sSymbol *p__LINE__Symbol = NULL;
char   *currentmacroargs[MAXMACROARGS + 1];
char   *newmacroargs[MAXMACROARGS + 1];
char	SavedTIME[256];
char	SavedDATE[256];

SLONG   Callback_NARG (struct sSymbol *sym)
{
    ULONG   i = 0;

    sym = sym;
    while (currentmacroargs[i] && i < MAXMACROARGS)
		i += 1;

    return (i);
}

SLONG   Callback__LINE__ (struct sSymbol *sym)
{
	sym=sym;
    return (nLineNo);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Get the nValue field of a symbol
 *
 */

SLONG   getvaluefield (struct sSymbol *sym)
{
    if (sym->Callback)
    {
		return (sym->Callback (sym));
    }
    else
		return (sym->nValue);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Calculate the hash value for a string
 *
 */

ULONG	calchash (char *s)
{
   ULONG	hash = 0;

    while (*s != 0)
		hash += (*s++);

    return (hash % HASHSIZE);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Create a new symbol by name
 *
 */

struct sSymbol *createsymbol (char *s)
{
    struct sSymbol **ppsym;
    ULONG     hash;

    hash = calchash (s);
    ppsym = &(tHashedSymbols[hash]);

    while ((*ppsym) != NULL)
		ppsym = &((*ppsym)->pNext);

    if( ((*ppsym)=(struct sSymbol *)malloc(sizeof (struct sSymbol)))!=NULL )
    {
	    strcpy ((*ppsym)->tzName, s);
		(*ppsym)->nValue = 0;
		(*ppsym)->nType = 0;
		(*ppsym)->pScope = NULL;
		(*ppsym)->pNext = NULL;
		(*ppsym)->pMacro = NULL;
		(*ppsym)->pSection = NULL;
		(*ppsym)->Callback = NULL;
		return (*ppsym);
    }
    else
    {
		fatalerror ("No memory for symbol");
		return (NULL);
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Find a symbol by name and scope
 *
 */

struct sSymbol *findsymbol (char *s, struct sSymbol *scope)
{
    struct sSymbol **ppsym;
    SLONG     hash;

    hash = calchash (s);
    ppsym = &(tHashedSymbols[hash]);

    while ((*ppsym) != NULL)
    {
		if( (strcmp (s, (*ppsym)->tzName) == 0)
		&&	((*ppsym)->pScope == scope) )
		{
		    return (*ppsym);
		}
		else
		    ppsym = &((*ppsym)->pNext);
    }
    return (NULL);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Find the pointer to a symbol by name and scope
 *
 */

struct sSymbol **findpsymbol (char *s, struct sSymbol *scope)
{
    struct sSymbol **ppsym;
    SLONG     hash;

    hash = calchash (s);
    ppsym = &(tHashedSymbols[hash]);

    while ((*ppsym) != NULL)
    {
		if( (strcmp (s, (*ppsym)->tzName) == 0)
		&&	((*ppsym)->pScope == scope) )
		{
		    return (ppsym);
		}
		else
		    ppsym = &((*ppsym)->pNext);
    }
    return (NULL);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Find a symbol by name and scope
 *
 */

struct sSymbol *sym_FindSymbol (char *tzName)
{
    struct sSymbol *pscope;

    if (*tzName == '.')
		pscope = pScope;
    else
		pscope = NULL;

    return (findsymbol (tzName, pscope));
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Purge a symbol
 *
 */

void	sym_Purge( char *tzName )
{
	struct	sSymbol	**ppSym;
    struct	sSymbol	*pscope;

    if (*tzName == '.')
		pscope = pScope;
    else
		pscope = NULL;

    ppSym=findpsymbol( tzName, pscope );

	if( ppSym )
	{
		struct	sSymbol	*pSym;

		pSym=*ppSym;
		*ppSym=pSym->pNext;

		if( pSym->pMacro )
			free( pSym->pMacro );

		free( pSym );
	}
	else
	{
		sprintf( temptext, "'%s' not defined", tzName );
		yyerror( temptext );
	}
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Determine if a symbol has been defined
 *
 */

ULONG   sym_isConstDefined (char *tzName)
{
    struct sSymbol *psym,
           *pscope;

    if (*tzName == '.')
		pscope = pScope;
    else
		pscope = NULL;

    psym = findsymbol (tzName, pscope);

	if( psym
	&&	(psym->nType & SYMF_DEFINED) )
	{
		if( psym->nType & (SYMF_EQU|SYMF_SET|SYMF_MACRO|SYMF_STRING) )
		{
			return (1);
		}
		else
		{
			sprintf( temptext, "'%s' is not allowed as argument to the DEF function", tzName );
			fatalerror( temptext );
		}
	}
	return (0);
}

ULONG   sym_isDefined (char *tzName)
{
    struct sSymbol *psym,
           *pscope;

    if (*tzName == '.')
		pscope = pScope;
    else
		pscope = NULL;

    psym = findsymbol (tzName, pscope);

	if (psym && (psym->nType & SYMF_DEFINED))
		return (1);
    else
		return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Determine if the symbol is a constant
 *
 */

ULONG   sym_isConstant (char *s)
{
    struct sSymbol *psym,
           *pscope;

    if (*s == '.')
		pscope = pScope;
    else
		pscope = NULL;

    if( (psym=findsymbol(s,pscope))!=NULL )
    {
		if (psym->nType & SYMF_CONST)
		    return (1);
    }

    return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Get a string equate's value
 *
 */

char   *sym_GetStringValue (char *tzSym)
{
    struct sSymbol *pSym;

    if( (pSym=sym_FindSymbol(tzSym))!=NULL )
		return (pSym->pMacro);
    else
    {
		sprintf (temptext, "Stringsymbol '%s' not defined", tzSym);
		yyerror (temptext);
    }

    return (NULL);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Return a constant symbols value
 *
 */

ULONG   sym_GetConstantValue (char *s)
{
    struct sSymbol *psym,
           *pscope;

    if (*s == '.')
		pscope = pScope;
    else
		pscope = NULL;

    if( (psym=findsymbol(s,pscope))!=NULL )
    {
		if (psym->nType & SYMF_CONST)
		    return (getvaluefield (psym));
		else
		{
		    fatalerror ("Expression must have a constant value");
		}
    }
    else
    {
		sprintf (temptext, "'%s' not defined", s);
		yyerror (temptext);
    }

    return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Return a symbols value... "estimated" if not defined yet
 *
 */

ULONG   sym_GetValue (char *s)
{
    struct sSymbol *psym,
           *pscope;

    if (*s == '.')
		pscope = pScope;
    else
		pscope = NULL;

    if( (psym=findsymbol(s,pscope))!=NULL )
    {
		if( psym->nType & SYMF_DEFINED )
		{
			if( psym->nType & (SYMF_MACRO|SYMF_STRING) )
			{
				sprintf (temptext, "'%s' is a macro or string symbol", s);
				yyerror (temptext);
			}
		    return (getvaluefield (psym));
		}
		else
		{
		    if ((nPass == 1) || (psym->nType & SYMF_IMPORT))
		    {
				/* 0x80 seems like a good default value... */
				return (0x80);
		    }
		    else
		    {
				sprintf (temptext, "'%s' not defined", s);
				yyerror (temptext);
		    }
		}
    }
    else
    {
		if (nPass == 1)
		{
		    createsymbol (s);
		    return (0x80);
		}
		else
		{
		    sprintf (temptext, "'%s' not defined", s);
		    yyerror (temptext);
		}
    }

    return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Return a defined symbols value... aborts if not defined yet
 *
 */

ULONG   sym_GetDefinedValue (char *s)
{
    struct sSymbol *psym,
           *pscope;

    if (*s == '.')
		pscope = pScope;
    else
		pscope = NULL;

    if( (psym=findsymbol(s,pscope))!=NULL )
    {
		if( (psym->nType & SYMF_DEFINED) )
		{
			if( psym->nType & (SYMF_MACRO|SYMF_STRING) )
			{
			    sprintf (temptext, "'%s' is a macro or string symbol", s);
			    yyerror (temptext);
			}

			return (getvaluefield (psym));
		}
		else
		{
		    sprintf (temptext, "'%s' not defined", s);
		    yyerror (temptext);
		}
    }
    else
    {
		sprintf (temptext, "'%s' not defined", s);
		yyerror (temptext);
    }

    return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Macro argument stuff
 *
 */

void    sym_ShiftCurrentMacroArgs (void)
{
    SLONG     i;

    free (currentmacroargs[0]);
    for (i = 0; i < MAXMACROARGS - 1; i += 1)
    {
		currentmacroargs[i] = currentmacroargs[i + 1];
    }
    currentmacroargs[MAXMACROARGS - 1] = NULL;
}

char   *sym_FindMacroArg (SLONG i)
{
    if (i == -1)
		i = MAXMACROARGS + 1;

    return (currentmacroargs[i - 1]);
}

void    sym_UseNewMacroArgs (void)
{
    SLONG     i;

    for (i = 0; i <= MAXMACROARGS; i += 1)
    {
		currentmacroargs[i] = newmacroargs[i];
		newmacroargs[i] = NULL;
    }
}

void    sym_SaveCurrentMacroArgs (char *save[])
{
    SLONG     i;

    for (i = 0; i <= MAXMACROARGS; i += 1)
		save[i] = currentmacroargs[i];
}

void    sym_RestoreCurrentMacroArgs (char *save[])
{
    SLONG     i;

    for (i = 0; i <= MAXMACROARGS; i += 1)
		currentmacroargs[i] = save[i];
}

void    sym_FreeCurrentMacroArgs (void)
{
    SLONG     i;

    for (i = 0; i <= MAXMACROARGS; i += 1)
    {
		free (currentmacroargs[i]);
		currentmacroargs[i] = NULL;
    }
}

void    sym_AddNewMacroArg (char *s)
{
    SLONG     i = 0;

    while (i < MAXMACROARGS && newmacroargs[i] != NULL)
		i += 1;

    if (i < MAXMACROARGS)
    {
		if (s)
		    newmacroargs[i] = strdup (s);
		else
		    newmacroargs[i] = NULL;
    }
    else
		yyerror ("A maximum of 9 arguments allowed");
}

void    sym_SetMacroArgID (ULONG nMacroCount)
{
    char    s[256];

    sprintf(s, "_%ld", nMacroCount);
    newmacroargs[MAXMACROARGS] = strdup (s);
}

void    sym_UseCurrentMacroArgs (void)
{
    SLONG     i;

    for (i = 1; i <= MAXMACROARGS; i += 1)
		sym_AddNewMacroArg (sym_FindMacroArg (i));
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Find a macro by name
 *
 */

struct sSymbol *sym_FindMacro (char *s)
{
    return (findsymbol (s, NULL));
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Add an equated symbol
 *
 */

void    sym_AddEqu (char *tzSym, SLONG value)
{
    if( (nPass==1)
		|| ((nPass==2)&&(sym_isDefined(tzSym)==0))	)
    {
		/* only add equated symbols in pass 1 */
		struct sSymbol *nsym;

		if( (nsym=findsymbol(tzSym,NULL))!=NULL )
		{
		    if (nsym->nType & SYMF_DEFINED)
		    {
				sprintf (temptext, "'%s' already defined", tzSym);
				yyerror (temptext);
		    }
		}
		else
		    nsym = createsymbol (tzSym);

		if (nsym)
		{
		    nsym->nValue = value;
		    nsym->nType |= SYMF_EQU | SYMF_DEFINED | SYMF_CONST;
		    nsym->pScope = NULL;
		}
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Add a string equated symbol
 *
 */

void    sym_AddString (char *tzSym, char *tzValue)
{
    struct sSymbol *nsym;

    if( (nsym=findsymbol(tzSym,NULL))!=NULL )
    {
		if (nsym->nType & SYMF_DEFINED)
		{
		    sprintf (temptext, "'%s' already defined", tzSym);
		    yyerror (temptext);
		}
    }
    else
		nsym = createsymbol (tzSym);

    if (nsym)
    {
		if( (nsym->pMacro=(char *)malloc(strlen(tzValue)+1))!=NULL )
		    strcpy (nsym->pMacro, tzValue);
		else
		    fatalerror ("No memory for stringequate");
		nsym->nType |= SYMF_STRING | SYMF_DEFINED;
		nsym->ulMacroSize = strlen( tzValue );
		nsym->pScope = NULL;
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * check if symbol is a string equated symbol
 *
 */

ULONG	sym_isString (char *tzSym)
{
    struct sSymbol *pSym;

    if( (pSym=findsymbol(tzSym,NULL))!=NULL )
    {
		if (pSym->nType & SYMF_STRING)
		    return (1);
    }
    return (0);
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Alter a SET symbols value
 *
 */

void    sym_AddSet (char *tzSym, SLONG value)
{
    struct sSymbol *nsym;

    if( (nsym=findsymbol(tzSym,NULL))!=NULL )
    {
    }
    else
		nsym = createsymbol (tzSym);

    if (nsym)
    {
		nsym->nValue = value;
		nsym->nType |= SYMF_SET | SYMF_DEFINED | SYMF_CONST;
		nsym->pScope = NULL;
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Add a local (.name) relocatable symbol
 *
 */

void    sym_AddLocalReloc (char *tzSym)
{
    if( (nPass==1)
		|| ((nPass==2)&&(sym_isDefined(tzSym)==0)) )
    {
		/* only add local reloc symbols in pass 1 */
		struct sSymbol *nsym;

		if (pScope)
		{
		    if( (nsym=findsymbol(tzSym,pScope))!=NULL )
		    {
				if (nsym->nType & SYMF_DEFINED)
				{
				    sprintf (temptext, "'%s' already defined", tzSym);
				    yyerror (temptext);
				}
		    }
		    else
				nsym = createsymbol (tzSym);

		    if (nsym)
		    {
				nsym->nValue = nPC;
				nsym->nType |= SYMF_RELOC | SYMF_LOCAL | SYMF_DEFINED;
				nsym->pScope = pScope;
				nsym->pSection = pCurrentSection;
		    }
		}
		else
		    fatalerror ("Local label in main scope");
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Add a relocatable symbol
 *
 */

void    sym_AddReloc (char *tzSym)
{
    if( (nPass==1)
	||	((nPass==2)&&(sym_isDefined(tzSym)==0)) )
    {
		/* only add reloc symbols in pass 1 */
		struct sSymbol *nsym;

		if( (nsym=findsymbol(tzSym,NULL))!=NULL )
		{
		    if (nsym->nType & SYMF_DEFINED)
		    {
				sprintf (temptext, "'%s' already defined", tzSym);
				yyerror (temptext);
		    }
		}
		else
		    nsym = createsymbol (tzSym);

		if (nsym)
		{
		    nsym->nValue = nPC;
		    nsym->nType |= SYMF_RELOC | SYMF_DEFINED;
		    nsym->pScope = NULL;
		    nsym->pSection = pCurrentSection;
		}
    }
    pScope = findsymbol (tzSym, NULL);

}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Export a symbol
 *
 */

void    sym_Export (char *tzSym)
{
    if (nPass == 1)
    {
		/* only export symbols in pass 1 */
		struct sSymbol *nsym;

		if ((nsym = findsymbol (tzSym, 0)) == NULL)
			nsym = createsymbol (tzSym);

		if (nsym)
			nsym->nType |= SYMF_EXPORT;
	}
	else
	{
		struct sSymbol *nsym;

		if( (nsym=findsymbol(tzSym,0))!=NULL )
		{
		    if (nsym->nType & SYMF_DEFINED)
				return;
		}
		sprintf (temptext, "'%s' not defined", tzSym);
		yyerror (temptext);
    }

}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Import a symbol
 *
 */

void    sym_Import (char *tzSym)
{
    if (nPass == 1)
    {
		/* only import symbols in pass 1 */
		struct sSymbol *nsym;

		if (findsymbol (tzSym, NULL))
		{
		    sprintf (temptext, "'%s' already defined", tzSym);
		    yyerror (temptext);
		}

		if( (nsym=createsymbol(tzSym))!=NULL )
		    nsym->nType |= SYMF_IMPORT;
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Globalize a symbol (export if defined, import if not)
 *
 */

void    sym_Global (char *tzSym)
{
    if (nPass == 2)
    {
		/* only globalize symbols in pass 2 */
		struct sSymbol *nsym;

		nsym = findsymbol (tzSym, 0);

		if ((nsym == NULL) || ((nsym->nType & SYMF_DEFINED) == 0))
		{
		    if (nsym == NULL)
				nsym = createsymbol (tzSym);

		    if (nsym)
				nsym->nType |= SYMF_IMPORT;
		}
		else
		{
		    if (nsym)
				nsym->nType |= SYMF_EXPORT;
		}
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Add a macro definition
 *
 */

void    sym_AddMacro (char *tzSym)
{
    if( (nPass==1)
	||	((nPass==2)&&(sym_isDefined(tzSym)==0)) )
    {
		/* only add macros in pass 1 */
		struct sSymbol *nsym;

		if( (nsym=findsymbol(tzSym,NULL))!=NULL )
		{
		    if (nsym->nType & SYMF_DEFINED)
		    {
				sprintf (temptext, "'%s' already defined", tzSym);
				yyerror (temptext);
		    }
		}
		else
		    nsym = createsymbol (tzSym);

		if (nsym)
		{
		    nsym->nValue = nPC;
		    nsym->nType |= SYMF_MACRO | SYMF_DEFINED;
		    nsym->pScope = NULL;
		    nsym->ulMacroSize = ulNewMacroSize;
		    nsym->pMacro = tzNewMacro;
		}
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Prepare for pass #1
 *
 */

void    sym_PrepPass1 (void)
{
    sym_Init ();
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Prepare for pass #2
 *
 */

void    sym_PrepPass2 (void)
{
    SLONG     i;

    for (i = 0; i < HASHSIZE; i += 1)
    {
		struct sSymbol **ppSym = &(tHashedSymbols[i]);

		while (*ppSym)
		{
		    if ((*ppSym)->nType & (SYMF_SET | SYMF_STRING | SYMF_EQU))
		    {
				struct sSymbol *pTemp;

				pTemp = (*ppSym)->pNext;
				free (*ppSym);
				*ppSym = pTemp;
		    }
		    else
				ppSym = &((*ppSym)->pNext);
		}
    }
    pScope = NULL;
    pPCSymbol->nValue = 0;

	sym_AddString ("__TIME__", SavedTIME);
	sym_AddString ("__DATE__", SavedDATE);
	sym_AddSet( "_RS", 0 );
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * Initialise the symboltable
 *
 */

void    sym_Init (void)
{
	SLONG	i;
	time_t	tod;

	for (i = 0; i < MAXMACROARGS; i += 1)
	{
		currentmacroargs[i] = NULL;
		newmacroargs[i] = NULL;
	}

	for (i = 0; i < HASHSIZE; i += 1)
	tHashedSymbols[i] = NULL;

	sym_AddReloc ("@");
	pPCSymbol = findsymbol ("@", NULL);
	sym_AddEqu ("_NARG", 0);
	p_NARGSymbol = findsymbol ("_NARG", NULL);
	p_NARGSymbol->Callback = Callback_NARG;
	sym_AddEqu ("__LINE__", 0);
	p__LINE__Symbol = findsymbol ("__LINE__", NULL);
	p__LINE__Symbol->Callback = Callback__LINE__;

	sym_AddEqu( "__ASM__", (SLONG) (atof (ASM_VERSION) * 65536));
	sym_AddSet( "_RS", 0 );

	if( time(&tod)!=-1 )
	{
		struct	tm	*tptr;

		tptr=localtime( &tod );
		strftime( SavedTIME, sizeof(SavedTIME), "%H:%M:%S", tptr );
		strftime( SavedDATE, sizeof(SavedDATE), "%d %B %Y", tptr );
		sym_AddString ("__TIME__", SavedTIME);
		sym_AddString ("__DATE__", SavedDATE);
	}

	pScope = NULL;

	math_DefinePI ();

}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * DEBUG: Print the symbol table
 *
 */

void    sym_PrintSymbolTable (void)
{
    ULONG     i;

    for (i = 0; i < HASHSIZE; i += 1)
    {
		struct sSymbol *sym = tHashedSymbols[i];

		if (sym != NULL)
		    printf ("\nHashTable #%ld:\n", i);

		while (sym != NULL)
		{
		    if (sym->nType & SYMF_LOCAL)
				printf ("LOCAL : '%s%s' - %08lX\n", sym->pScope->tzName, sym->tzName, getvaluefield (sym));
		    else if (sym->nType & (SYMF_MACRO | SYMF_STRING))
		    {
				ULONG     i = 0;

				printf ("MACRO : '%s'\n\"", sym->tzName);
				while (i < sym->ulMacroSize)
				{
				    if (sym->pMacro[i] == '\n')
				    {
						printf ("\n");
						i += 1;
				    }
				    else
						printf ("%c", sym->pMacro[i++]);
				}
				printf ("\"\n");
		    }
		    else if (sym->nType & SYMF_EXPORT)
				printf ("EXPORT: '%s' - %08lX\n", sym->tzName, getvaluefield (sym));
		    else if (sym->nType & SYMF_IMPORT)
				printf ("IMPORT: '%s'\n", sym->tzName);
		    else if (sym->nType & SYMF_EQU)
				printf ("EQU   : '%s' - %08lX\n", sym->tzName, getvaluefield (sym));
		    else if (sym->nType & SYMF_SET)
				printf ("SET   : '%s' - %08lX\n", sym->tzName, getvaluefield (sym));
		    else
				printf ("SYMBOL: '%s' - %08lX\n", sym->tzName, getvaluefield (sym));

		    sym = sym->pNext;
		}
    }
}

/*
 * RGBAsm - SYMBOL.C - Symboltable stuff
 *
 * DEBUG: Dump the macroargs
 *
 */

void    sym_DumpMacroArgs (void)
{
    ULONG     i;

    for (i = 0; i < MAXMACROARGS; i += 1)
		printf ("CurrentArg%ld: %s\n", i + 1, currentmacroargs[i]);
}