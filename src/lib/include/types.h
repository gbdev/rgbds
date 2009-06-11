#ifndef TYPES_H
#define TYPES_H 1

#if	defined(AMIGA) || defined(__GNUC__)
#define	_MAX_PATH	512
#endif

typedef unsigned char	UBYTE;
typedef signed char		SBYTE;
typedef unsigned short	UWORD;
typedef signed short	SWORD;
typedef unsigned long	ULONG;
typedef signed long		SLONG;
typedef signed char		BBOOL;

#endif