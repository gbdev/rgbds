#include	"asm.h"
#include	"lexer.h"
#include	"types.h"
#include	"main.h"
#include	"rpn.h"
#include	"asmy.h"
#include	"fstack.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

struct sLexString
{
	char	*tzName;
	ULONG	nToken;
	ULONG	nNameLength;
	struct	sLexString	*pNext;
};

#define	pLexBuffer		(pCurrentBuffer->pBuffer)
#define	nLexBufferLeng	(pCurrentBuffer->nBufferSize)

#define	SAFETYMARGIN	1024

extern ULONG symvaluetostring (char *dest, char *s);

struct	sLexFloat	tLexFloat[32];
struct	sLexString	*tLexHash[LEXHASHSIZE];
YY_BUFFER_STATE	pCurrentBuffer;
ULONG	yyleng;
ULONG	nLexMaxLeng;

ULONG	tFloatingSecondChar[256];
ULONG	tFloatingFirstChar[256];
ULONG	tFloatingChars[256];
ULONG	nFloating;
enum	eLexerState	lexerstate=LEX_STATE_NORMAL;

#define	AtLineStart	pCurrentBuffer->oAtLineStart

#ifdef	__GNUC__
void	strupr( char *s )
{
	while( *s )
	{
		*s=toupper(*s);
		s+=1;
	}
}

void	strlwr( char *s )
{
	while( *s )
	{
		*s=tolower(*s);
		s+=1;
	}
}

#endif
void	yyskipbytes( ULONG count )
{
	pLexBuffer+=count;
}

void	yyunputbytes( ULONG count )
{
	pLexBuffer-=count;
}

void	yyunput( char c )
{
	*(--pLexBuffer)=c;
}

void	yyunputstr( char *s )
{
	SLONG	i;

	i=strlen(s)-1;

	while( i>=0 )
		yyunput( s[i--] );
}

void	yy_switch_to_buffer( YY_BUFFER_STATE buf )
{
	pCurrentBuffer=buf;
}

void	yy_set_state( enum eLexerState i )
{
	lexerstate=i;
}

void	yy_delete_buffer( YY_BUFFER_STATE buf )
{
	free( buf->pBufferStart-SAFETYMARGIN );
	free( buf );
}

YY_BUFFER_STATE	yy_scan_bytes( char *mem, ULONG size )
{
	YY_BUFFER_STATE	pBuffer;

	if( (pBuffer=(YY_BUFFER_STATE)malloc(sizeof(struct yy_buffer_state)))!=NULL )
	{
		if( (pBuffer->pBuffer=pBuffer->pBufferStart=(char *)malloc(size+1+SAFETYMARGIN))!=NULL )
		{
			pBuffer->pBuffer+=SAFETYMARGIN;
			pBuffer->pBufferStart+=SAFETYMARGIN;
			memcpy( pBuffer->pBuffer, mem, size );
			pBuffer->nBufferSize=size;
			pBuffer->oAtLineStart=1;
			pBuffer->pBuffer[size]=0;
			return( pBuffer );
		}
	}

	fatalerror( "Out of memory!" );
	return( NULL );
}

YY_BUFFER_STATE	yy_create_buffer( FILE *f )
{
	YY_BUFFER_STATE pBuffer;

	if( (pBuffer=(YY_BUFFER_STATE)malloc(sizeof(struct yy_buffer_state)))!=NULL )
	{
	    ULONG   size;

		fseek (f, 0, SEEK_END);
		size = ftell (f);
		fseek (f, 0, SEEK_SET);

		if( (pBuffer->pBuffer=pBuffer->pBufferStart=(char *)malloc(size+2+SAFETYMARGIN))!=NULL )
		{
			char	*mem;
			ULONG   instring = 0;

			pBuffer->pBuffer+=SAFETYMARGIN;
			pBuffer->pBufferStart+=SAFETYMARGIN;

			size=fread( pBuffer->pBuffer, sizeof (UBYTE), size, f );

			pBuffer->pBuffer[size]='\n';
			pBuffer->pBuffer[size+1]=0;
			pBuffer->nBufferSize=size+1;

			mem=pBuffer->pBuffer;

			while( *mem )
			{
				if( *mem=='\"' )
					instring=1-instring;

				if( instring )
				{
					mem+=1;
				}
				else
				{
					if( (mem[0]==10 && mem[1]==13)
					||	(mem[0]==13 && mem[1]==10) )
					{
						mem[0]=' ';
						mem[1]='\n';
						mem+=2;
					}
					else if( mem[0]==10 || mem[0]==13 )
					{
						mem[0]='\n';
						mem+=1;
					}
					else if( mem[0]=='\n' && mem[1]=='*' )
					{
						mem+=1;
						while( !(*mem == '\n' || *mem == '\0') )
							*mem++=' ';
					}
					else if( *mem==';' )
					{
						while( !(*mem == '\n' || *mem == '\0') )
							*mem++=' ';
					}
					else
						mem+=1;
				}
			}

			pBuffer->oAtLineStart=1;
			return( pBuffer );
		}
	}

	fatalerror( "Out of memory!" );
	return( NULL );
}

ULONG	lex_FloatAlloc( struct sLexFloat *tok )
{
	tLexFloat[nFloating]=(*tok);

	return( 1<<(nFloating++) );
}

void	lex_FloatDeleteRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingChars[start]&=~id;
		start+=1;
    }
}

void	lex_FloatAddRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingChars[start]|=id;
		start+=1;
    }
}

void	lex_FloatDeleteFirstRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingFirstChar[start]&=~id;
		start+=1;
    }
}

void	lex_FloatAddFirstRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingFirstChar[start]|=id;
		start+=1;
    }
}

void	lex_FloatDeleteSecondRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingSecondChar[start]&=~id;
		start+=1;
    }
}

void	lex_FloatAddSecondRange( ULONG id, UWORD start, UWORD end )
{
    while( start<=end )
    {
		tFloatingSecondChar[start]|=id;
		start+=1;
    }
}

struct	sLexFloat	*lexgetfloat( ULONG id )
{
    ULONG   r=0,
            mask=1;

    if( id==0 )
	return( NULL );

    while( (id&mask)==0 )
    {
		mask<<=1;
		r+=1;
    }

    return( &tLexFloat[r] );
}

ULONG	lexcalchash( char *s )
{
    ULONG	r=0;

    while( *s )
    {
		r=((r<<1)+(toupper(*s)))%LEXHASHSIZE;
		s+=1;
    }

    return( r );
}

void	lex_Init( void )
{
	ULONG   i;

	for( i=0; i<LEXHASHSIZE; i+=1)
	{
		tLexHash[i]=NULL;
	}

	for( i=0; i<256; i+=1 )
	{
		tFloatingFirstChar[i]=0;
		tFloatingSecondChar[i]=0;
		tFloatingChars[i]=0;
	}

	nLexMaxLeng=0;
	nFloating=0;
}

void	lex_AddStrings( struct sLexInitString *lex )
{
    while( lex->tzName )
    {
		struct sLexString 	**ppHash;
	    ULONG				hash;

		ppHash = &tLexHash[hash=lexcalchash (lex->tzName)];
		while (*ppHash)
		    ppHash = &((*ppHash)->pNext);

//   printf( "%s has hashvalue %d\n", lex->tzName, hash );

		if( ((*ppHash)=(struct sLexString *)malloc(sizeof(struct sLexString)))!=NULL )
		{
		    if( ((*ppHash)->tzName=(char *)strdup(lex->tzName))!=NULL )
		    {
				(*ppHash)->nNameLength = strlen (lex->tzName);
				(*ppHash)->nToken = lex->nToken;
				(*ppHash)->pNext = NULL;

				strupr ((*ppHash)->tzName);

				if ((*ppHash)->nNameLength > nLexMaxLeng)
				    nLexMaxLeng = (*ppHash)->nNameLength;

		    }
		    else
				fatalerror ("Out of memory!");
		}
		else
		    fatalerror ("Out of memory!");

		lex += 1;
    }
}

ULONG   yylex (void)
{
	ULONG	hash,
			maxlen;
	char	*s;
	struct sLexString *pLongestFixed = NULL;
	ULONG   nFloatMask,
	        nOldFloatMask,
	        nFloatLen;
	ULONG   linestart = AtLineStart;

	switch( lexerstate )
	{
		case LEX_STATE_NORMAL:
			AtLineStart = 0;

		  scanagain:

		    while (*pLexBuffer == ' ' || *pLexBuffer == '\t')
		    {
				linestart = 0;
				pLexBuffer += 1;
		    }

		    if (*pLexBuffer == 0)
		    {
				if (yywrap () == 0)
				{
				    linestart = AtLineStart;
				    AtLineStart = 0;
				    goto scanagain;
				}
		    }

		    s = pLexBuffer;
		    nOldFloatMask = nFloatLen = 0;
		    nFloatMask = tFloatingFirstChar[*s++];
		    while (nFloatMask && nFloatLen < nLexBufferLeng)
		    {
				nFloatLen += 1;
				nOldFloatMask = nFloatMask;
				if (nFloatLen == 1)
				    nFloatMask &= tFloatingSecondChar[*s++];
				else
				    nFloatMask &= tFloatingChars[*s++];
		    }

		    maxlen = nLexBufferLeng;
		    if (nLexMaxLeng < maxlen)
				maxlen = nLexMaxLeng;

		    yyleng = 0;
		    hash = 0;
		    s = pLexBuffer;
		    while (yyleng < nLexMaxLeng)
		    {
				yyleng += 1;
				hash = ((hash << 1) + (toupper (*s))) % LEXHASHSIZE;
				s+=1;
				if (tLexHash[hash])
				{
				    struct sLexString *lex;

				    lex = tLexHash[hash];
				    while (lex)
				    {
						if (lex->nNameLength == yyleng)
						{
						    if (strnicmp (pLexBuffer, lex->tzName, yyleng) == 0)
						    {
								pLongestFixed = lex;
						    }
						}
						lex = lex->pNext;
				    }
				}

		    }

		    if (nFloatLen == 0 && pLongestFixed == NULL)
		    {
				if (*pLexBuffer == '"')
				{
				    ULONG   index = 0;

				    pLexBuffer += 1;
				    while ((*pLexBuffer != '"') && (*pLexBuffer != '\n'))
				    {
						char    ch,
						       *marg;

						if ((ch = *pLexBuffer++) == '\\')
						{
						    switch (ch = (*pLexBuffer++))
						    {
								case 'n':
								    ch = '\n';
								    break;
								case 't':
								    ch = '\t';
								    break;
								case '0':
								case '1':
								case '2':
								case '3':
								case '4':
								case '5':
								case '6':
								case '7':
								case '8':
								case '9':
								    if( (marg=sym_FindMacroArg(ch-'0'))!=NULL )
								    {
										while (*marg)
										    yylval.tzString[index++] = *marg++;
										ch = 0;
								    }
								    break;
								case '@':
								    if( (marg=sym_FindMacroArg(-1))!=NULL )
								    {
										while (*marg)
										    yylval.tzString[index++] = *marg++;
										ch = 0;
								    }
								    break;
						    }
						}
						else if (ch == '{')
						{
						    char    sym[MAXSYMLEN];
						    int     i = 0;

						    while ((*pLexBuffer != '}') && (*pLexBuffer != '"') && (*pLexBuffer != '\n'))
						    {
								if ((ch = *pLexBuffer++) == '\\')
								{
								    switch (ch = (*pLexBuffer++))
								    {
										case '0':
										case '1':
										case '2':
										case '3':
										case '4':
										case '5':
										case '6':
										case '7':
										case '8':
										case '9':
										    if( (marg=sym_FindMacroArg(ch-'0'))!=NULL )
										    {
												while (*marg)
												    sym[i++] = *marg++;
												ch = 0;
										    }
										    break;
										case '@':
										    if( (marg=sym_FindMacroArg(-1))!=NULL )
										    {
												while (*marg)
												    sym[i++] = *marg++;
												ch = 0;
										    }
										    break;
								    }
								}
								else
									sym[i++] = ch;
						    }

						    sym[i] = 0;
						    index += symvaluetostring (&yylval.tzString[index], sym);
						    if (*pLexBuffer == '}')
								pLexBuffer += 1;
						    else
								yyerror ("Missing }");
						    ch = 0;
						}

						if (ch)
						    yylval.tzString[index++] = ch;
				    }

				    yylval.tzString[index++] = 0;

				    if (*pLexBuffer == '\n')
						yyerror ("Unterminated string");
				    else
						pLexBuffer += 1;

				    return (T_STRING);
				}
				else if (*pLexBuffer == '{')
				{
				    char    sym[MAXSYMLEN],
							ch,
							*marg;
				    int     i = 0;

				    pLexBuffer += 1;

				    while ((*pLexBuffer != '}') && (*pLexBuffer != '\n'))
				    {
						if ((ch = *pLexBuffer++) == '\\')
						{
							switch (ch = (*pLexBuffer++))
							{
								case '0':
								case '1':
								case '2':
								case '3':
								case '4':
								case '5':
								case '6':
								case '7':
								case '8':
								case '9':
									if( (marg=sym_FindMacroArg(ch-'0'))!=NULL )
									{
										while (*marg)
											sym[i++] = *marg++;
										ch = 0;
									}
									break;
								case '@':
									if( (marg=sym_FindMacroArg(-1))!=NULL )
									{
										while (*marg)
											sym[i++] = *marg++;
										ch = 0;
									}
									break;
							}
						}
						else
							sym[i++] = ch;
				    }
				    sym[i] = 0;
				    symvaluetostring (yylval.tzString, sym);
				    if (*pLexBuffer == '}')
						pLexBuffer += 1;
				    else
						yyerror ("Missing }");

				    return (T_STRING);
				}
				else
				{
				    if (*pLexBuffer == '\n')
						AtLineStart = 1;

				    yyleng = 1;
				    return (*pLexBuffer++);
				}
		    }

		    if (nFloatLen == 0)
		    {
				yyleng = pLongestFixed->nNameLength;
				pLexBuffer += yyleng;
				return (pLongestFixed->nToken);
		    }

		    if (pLongestFixed == NULL)
		    {
				struct sLexFloat *tok;

				tok = lexgetfloat (nOldFloatMask);
				yyleng = nFloatLen;
				if (tok->Callback)
				{
				    if (tok->Callback (pLexBuffer, yyleng) == 0)
						goto scanagain;
				}

				if (tok->nToken == T_ID && linestart)
				{
				    pLexBuffer += yyleng;
				    return (T_LABEL);
				}
				else
				{
				    pLexBuffer += yyleng;
				    return (tok->nToken);
				}
		    }

		    if (nFloatLen > pLongestFixed->nNameLength)
		    {
				struct sLexFloat *tok;

				tok = lexgetfloat (nOldFloatMask);
				yyleng = nFloatLen;
				if (tok->Callback)
				{
				    if (tok->Callback (pLexBuffer, yyleng) == 0)
						goto scanagain;
				}

				if (tok->nToken == T_ID && linestart)
				{
				    pLexBuffer += yyleng;
				    return (T_LABEL);
				}
				else
				{
				    pLexBuffer += yyleng;
				    return (tok->nToken);
				}
		    }
		    else
		    {
				yyleng = pLongestFixed->nNameLength;
				pLexBuffer += yyleng;
				return (pLongestFixed->nToken);
		    }
			break;

		case LEX_STATE_MACROARGS:
			{
				ULONG   index = 0;

			    while (*pLexBuffer == ' ' || *pLexBuffer == '\t')
			    {
					linestart = 0;
					pLexBuffer += 1;
			    }

				while(	(*pLexBuffer != ',')
				&&		(*pLexBuffer != '\n') )
				{
					char    ch,
							*marg;

					if ((ch = *pLexBuffer++) == '\\')
					{
						switch (ch = (*pLexBuffer++))
						{
							case 'n':
								ch = '\n';
								break;
							case 't':
								ch = '\t';
								break;
							case '0':
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
							case '8':
							case '9':
								if( (marg=sym_FindMacroArg(ch-'0'))!=NULL )
								{
									while (*marg)
										yylval.tzString[index++] = *marg++;
									ch = 0;
								}
								break;
							case '@':
								if( (marg=sym_FindMacroArg(-1))!=NULL )
								{
									while (*marg)
										yylval.tzString[index++] = *marg++;
									ch = 0;
								}
								break;
						}
					}
					else if (ch == '{')
					{
						char    sym[MAXSYMLEN];
						int     i = 0;

						while ((*pLexBuffer != '}') && (*pLexBuffer != '"') && (*pLexBuffer != '\n'))
						{
							if ((ch = *pLexBuffer++) == '\\')
							{
								switch (ch = (*pLexBuffer++))
								{
									case '0':
									case '1':
									case '2':
									case '3':
									case '4':
									case '5':
									case '6':
									case '7':
									case '8':
									case '9':
										if( (marg=sym_FindMacroArg(ch-'0'))!=NULL )
										{
											while (*marg)
												sym[i++] = *marg++;
											ch = 0;
										}
										break;
									case '@':
										if( (marg=sym_FindMacroArg(-1))!=NULL )
										{
											while (*marg)
												sym[i++] = *marg++;
											ch = 0;
										}
										break;
								}
							}
							else
								sym[i++] = ch;
						}
						sym[i] = 0;
						index += symvaluetostring (&yylval.tzString[index], sym);
						if (*pLexBuffer == '}')
							pLexBuffer += 1;
						else
							yyerror ("Missing }");
						ch = 0;
					}

					if (ch)
						yylval.tzString[index++] = ch;
				}

				if( index )
				{
					yyleng=index;
					yylval.tzString[index] = 0;
					if( *pLexBuffer=='\n' )
					{
						while( yylval.tzString[--index]==' ' )
						{
							yylval.tzString[index]=0;
							yyleng-=1;
						}
					}
					return (T_STRING);
				}
				else if( *pLexBuffer=='\n' )
				{
					pLexBuffer+=1;
					AtLineStart = 1;
				    yyleng = 1;
					return( '\n' );
				}
				else if( *pLexBuffer==',' )
				{
					pLexBuffer+=1;
					yyleng = 1;
					return( ',' );
				}
				else
				{
					yyerror( "INTERNAL ERROR IN YYLEX" );
					return( 0 );
				}
			}

			break;
	}

	yyerror( "INTERNAL ERROR IN YYLEX" );
	return( 0 );
}