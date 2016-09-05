#ifndef RGBDS_SYMBOL_H
#define RGBDS_SYMBOL_H

#include "types.h"

#define HASHSIZE (1 << 16)
#define MAXSYMLEN 256

struct sSymbol {
	char tzName[MAXSYMLEN + 1];
	SLONG nValue;
	ULONG nType;
	struct sSymbol *pScope;
	struct sSymbol *pNext;
	struct Section *pSection;
	ULONG ulMacroSize;
	char *pMacro;
	     SLONG(*Callback) (struct sSymbol *);
};
#define SYMF_RELOC		0x001	/* symbol will be reloc'ed during
					 * linking, it's absolute value is
					 * unknown */
#define SYMF_EQU		0x002	/* symbol is defined using EQU, will
					 * not be changed during linking */
#define SYMF_SET		0x004	/* symbol is (re)defined using SET,
					 * will not be changed during linking */
#define SYMF_EXPORT		0x008	/* symbol should be exported */
#define SYMF_IMPORT		0x010	/* symbol is imported, it's value is
					 * unknown */
#define SYMF_LOCAL		0x020	/* symbol is a local symbol */
#define SYMF_DEFINED	0x040	/* symbol has been defined, not only
				 * referenced */
#define SYMF_MACRO		0x080	/* symbol is a macro */
#define SYMF_STRING		0x100	/* symbol is a stringsymbol */
#define SYMF_CONST		0x200	/* symbol has a constant value, will
					 * not be changed during linking */

ULONG calchash(char *s);
void sym_SetExportAll(BBOOL set);
void sym_PrepPass1(void);
void sym_PrepPass2(void);
void sym_AddLocalReloc(char *tzSym);
void sym_AddReloc(char *tzSym);
void sym_Export(char *tzSym);
void sym_PrintSymbolTable(void);
struct sSymbol *sym_FindMacro(char *s);
void sym_InitNewMacroArgs(void);
void sym_AddNewMacroArg(char *s);
void sym_SaveCurrentMacroArgs(char *save[]);
void sym_RestoreCurrentMacroArgs(char *save[]);
void sym_UseNewMacroArgs(void);
void sym_FreeCurrentMacroArgs(void);
void sym_AddEqu(char *tzSym, SLONG value);
void sym_AddSet(char *tzSym, SLONG value);
void sym_Init(void);
ULONG sym_GetConstantValue(char *s);
void sym_Import(char *tzSym);
ULONG sym_isConstant(char *s);
struct sSymbol *sym_FindSymbol(char *tzName);
void sym_Global(char *tzSym);
char *sym_FindMacroArg(SLONG i);
char *sym_GetStringValue(char *tzSym);
void sym_UseCurrentMacroArgs(void);
void sym_SetMacroArgID(ULONG nMacroCount);
ULONG sym_isString(char *tzSym);
void sym_AddMacro(char *tzSym);
void sym_ShiftCurrentMacroArgs(void);
void sym_AddString(char *tzSym, char *tzValue);
ULONG sym_GetValue(char *s);
ULONG sym_GetDefinedValue(char *s);
ULONG sym_isDefined(char *tzName);
void sym_Purge(char *tzName);
ULONG sym_isConstDefined(char *tzName);

#endif
