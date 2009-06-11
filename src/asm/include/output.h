#ifndef OUTPUT_H
#define OUTPUT_H 1

#include	"types.h"

struct Section
{
	char *pzName;
	UBYTE nType;
	ULONG nPC;
	ULONG nOrg;
	ULONG nBank;
	struct Section *pNext;
	struct Patch *pPatches;
	UBYTE *tData;
};

void out_PrepPass2( void );
void out_SetFileName( char *s );
void out_NewSection (char *pzName, ULONG secttype);
void    out_NewAbsSection (char *pzName, ULONG secttype, SLONG org, SLONG bank);
void out_AbsByte( int b );
void out_RelByte( struct Expression *expr );
void out_RelWord( struct Expression *expr );
void out_PCRelByte( struct Expression *expr );
void out_WriteObject( void );
void out_Skip( int skip );
void out_BinaryFile( char *s );
void out_String( char *s );
void out_AbsLong (SLONG b);
void out_RelLong (struct Expression *expr);
void	out_PushSection( void );
void	out_PopSection( void );

#endif