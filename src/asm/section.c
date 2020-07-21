
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fstack.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/warning.h"

#include "extern/err.h"
#include "platform.h" // strdup

struct SectionStackEntry {
	struct Section *pSection;
	struct Symbol *pScope; /* Section's symbol scope */
	uint32_t offset;
	struct SectionStackEntry *pNext;
};

struct SectionStackEntry *pSectionStack;
static struct Section *currentLoadSection = NULL;
uint32_t loadOffset = 0; /* The offset of the LOAD section within its parent */

/*
 * A quick check to see if we have an initialized section
 */
static inline void checksection(void)
{
	if (pCurrentSection == NULL)
		fatalerror("Code generation before SECTION directive");
}

/*
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 */
static inline void checkcodesection(void)
{
	checksection();

	if (!sect_HasData(pCurrentSection->nType))
		fatalerror("Section '%s' cannot contain code or data (not ROM0 or ROMX)",
			   pCurrentSection->pzName);
	else if (nUnionDepth > 0)
		fatalerror("UNIONs cannot contain code or data");
}

/*
 * Check if the section has grown too much.
 */
static void reserveSpace(uint32_t delta_size)
{
	uint32_t maxSize = maxsize[pCurrentSection->nType];
	uint32_t newSize = curOffset + delta_size;

	/*
	 * This check is here to trap broken code that generates sections that
	 * are too big and to prevent the assembler from generating huge object
	 * files or trying to allocate too much memory.
	 * A check at the linking stage is still necessary.
	 */
	if (newSize > maxSize)
		fatalerror("Section '%s' is too big (max size = 0x%" PRIX32 " bytes, reached 0x%" PRIX32 ").",
			   pCurrentSection->pzName, maxSize, newSize);
}

struct Section *out_FindSectionByName(const char *pzName)
{
	struct Section *pSect = pSectionList;

	while (pSect) {
		if (strcmp(pzName, pSect->pzName) == 0)
			return pSect;

		pSect = pSect->pNext;
	}

	return NULL;
}

/*
 * Find a section by name and type. If it doesn't exist, create it
 */
static struct Section *getSection(char const *pzName, enum SectionType type,
				  uint32_t org, struct SectionSpec const *attrs,
				  enum SectionModifier mod)
{
#define mask(align) ((1 << (align)) - 1)
	uint32_t bank = attrs->bank;
	uint8_t alignment = attrs->alignment;
	uint16_t alignOffset = attrs->alignOfs;

	if (bank != -1) {
		if (type != SECTTYPE_ROMX && type != SECTTYPE_VRAM
		 && type != SECTTYPE_SRAM && type != SECTTYPE_WRAMX)
			yyerror("BANK only allowed for ROMX, WRAMX, SRAM, or VRAM sections");
		else if (bank < bankranges[type][0]
		      || bank > bankranges[type][1])
			yyerror("%s bank value $%" PRIx32 " out of range ($%" PRIx32 " to $%" PRIx32 ")",
				typeNames[type], bank,
				bankranges[type][0], bankranges[type][1]);
	}

	if (alignOffset >= 1 << alignment) {
		yyerror("Alignment offset must not be greater than alignment (%" PRIu16 " < %u)",
			alignOffset, 1U << alignment);
		alignOffset = 0;
	}

	if (alignment != 0) {
		/* It doesn't make sense to have both alignment and org set */
		uint32_t mask = mask(alignment);

		if (org != -1) {
			if ((org - alignOffset) & mask)
				yyerror("Section \"%s\"'s fixed address doesn't match its alignment",
					pzName);
			alignment = 0; /* Ignore it if it's satisfied */
		} else if (startaddr[type] & mask) {
			yyerror("Section \"%s\"'s alignment cannot be attained in %s",
				pzName, typeNames[type]);
		}
	}

	if (org != -1) {
		if (org < startaddr[type] || org > endaddr(type))
			yyerror("Section \"%s\"'s fixed address %#" PRIx32 " is outside of range [%#" PRIx16 "; %#" PRIx16 "]",
				pzName, org, startaddr[type], endaddr(type));
	}

	if (nbbanks(type) == 1)
		bank = bankranges[type][0];

	struct Section *pSect = out_FindSectionByName(pzName);

	if (pSect) {
		unsigned int nbSectErrors = 0;
#define fail(...) \
	do { \
		yyerror(__VA_ARGS__); \
		nbSectErrors++; \
	} while (0)

		if (type != pSect->nType)
			fail("Section \"%s\" already exists but with type %s",
			     pSect->pzName, typeNames[pSect->nType]);

		if (pSect->modifier != mod)
			fail("Section \"%s\" already declared as %s section",
			     pSect->pzName, sectionModNames[pSect->modifier]);
		/*
		 * Normal sections need to have exactly identical constraints;
		 * but unionized sections only need "compatible" constraints,
		 * and they end up with the strictest combination of both
		 */
		if (mod == SECTION_UNION) {
			/*
			 * WARNING: see comment about assumption in
			 * `EndLoadSection` if modifying the following check!
			 */
			if (sect_HasData(type))
				fail("Cannot declare ROM sections as UNION");
			if (org != -1) {
				/* If both are fixed, they must be the same */
				if (pSect->nOrg != -1 && pSect->nOrg != org)
					fail("Section \"%s\" already declared as fixed at different address $%" PRIx32,
					     pSect->pzName, pSect->nOrg);
				else if (pSect->nAlign != 0
				      && (mask(pSect->nAlign)
						& (org - pSect->alignOfs)))
					fail("Section \"%s\" already declared as aligned to %u bytes (offset %" PRIu16 ")",
					     pSect->pzName, 1U << pSect->nAlign,
					     pSect->alignOfs);
				else
					/* Otherwise, just override */
					pSect->nOrg = org;
			} else if (alignment != 0) {
				/* Make sure any fixed address is compatible */
				if (pSect->nOrg != -1) {
					if ((pSect->nOrg - alignOffset)
							& mask(alignment))
						fail("Section \"%s\" already declared as fixed at incompatible address $%" PRIx32,
						     pSect->pzName,
						     pSect->nOrg);
				/* Check if alignment offsets are compatible */
				} else if ((alignOffset & mask(pSect->nAlign))
					!= (pSect->alignOfs
							& mask(alignment))) {
					fail("Section \"%s\" already declared with incompatible %" PRIu8 "-byte alignment (offset %" PRIu16 ")",
					     pSect->pzName, pSect->nAlign,
					     pSect->alignOfs);
				} else if (alignment > pSect->nAlign) {
					/*
					 * If the section is not fixed,
					 * its alignment is the largest of both
					 */
					pSect->nAlign = alignment;
					pSect->alignOfs = alignOffset;
				}
			}
			/* If the section's bank is unspecified, override it */
			if (pSect->nBank == -1)
				pSect->nBank = bank;
			/* If both specify a bank, it must be the same one */
			else if (bank != -1 && pSect->nBank != bank)
				fail("Section \"%s\" already declared with different bank %" PRIu32,
				     pSect->pzName, pSect->nBank);
		} else { /* Section fragments are handled identically in RGBASM */
			/* However, concaternating non-fragments will be made an error */
			if (pSect->modifier != SECTION_FRAGMENT || mod != SECTION_FRAGMENT)
				warning(WARNING_OBSOLETE, "Concatenation of non-fragment sections is deprecated");

			if (org != pSect->nOrg) {
				if (pSect->nOrg == -1)
					fail("Section \"%s\" already declared as floating",
					     pSect->pzName);
				else
					fail("Section \"%s\" already declared as fixed at $%" PRIx32,
					     pSect->pzName, pSect->nOrg);
			}
			if (bank != pSect->nBank) {
				if (pSect->nBank == -1)
					fail("Section \"%s\" already declared as floating bank",
					     pSect->pzName);
				else
					fail("Section \"%s\" already declared as fixed at bank %" PRIu32,
					     pSect->pzName, pSect->nBank);
			}
			if (alignment != pSect->nAlign) {
				if (pSect->nAlign == 0)
					fail("Section \"%s\" already declared as unaligned",
					     pSect->pzName);
				else
					fail("Section \"%s\" already declared as aligned to %u bytes",
					     pSect->pzName,
					     1U << pSect->nAlign);
			}
		}

		if (nbSectErrors)
			fatalerror("Cannot create section \"%s\" (%u errors)",
				   pSect->pzName, nbSectErrors);
#undef fail
		return pSect;
	}

	pSect = malloc(sizeof(*pSect));
	if (pSect == NULL)
		fatalerror("Not enough memory for section");

	pSect->pzName = strdup(pzName);
	if (pSect->pzName == NULL)
		fatalerror("Not enough memory for sectionname");

	pSect->nType = type;
	pSect->modifier = mod;
	pSect->size = 0;
	pSect->nOrg = org;
	pSect->nBank = bank;
	pSect->nAlign = alignment;
	pSect->alignOfs = alignOffset;
	pSect->pNext = pSectionList;
	pSect->pPatches = NULL;

	/* It is only needed to allocate memory for ROM sections. */
	if (sect_HasData(type)) {
		uint32_t sectsize;

		sectsize = maxsize[type];
		pSect->tData = malloc(sectsize);
		if (pSect->tData == NULL)
			fatalerror("Not enough memory for section");
	} else {
		pSect->tData = NULL;
	}

	/*
	 * Add the new section to the list
	 * at the beginning because order doesn't matter
	 */
	pSectionList = pSect;

	return pSect;
#undef mask
}

/*
 * Set the current section
 */
static void changeSection(void)
{
	if (nUnionDepth > 0)
		fatalerror("Cannot change the section within a UNION");

	sym_SetCurrentSymbolScope(NULL);
}

/*
 * Set the current section by name and type
 */
void out_NewSection(char const *pzName, uint32_t type, uint32_t org,
		    struct SectionSpec const *attribs, enum SectionModifier mod)
{
	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block");

	struct Section *pSect = getSection(pzName, type, org, attribs, mod);

	changeSection();
	curOffset = mod == SECTION_UNION ? 0 : pSect->size;
	pCurrentSection = pSect;
}

/*
 * Set the current section by name and type
 */
void out_SetLoadSection(char const *name, uint32_t type, uint32_t org,
			struct SectionSpec const *attribs)
{
	checkcodesection();

	if (currentLoadSection)
		fatalerror("`LOAD` blocks cannot be nested");

	struct Section *pSect = getSection(name, type, org, attribs, false);

	loadOffset = curOffset;
	curOffset = 0; /* curOffset -= loadOffset; */
	changeSection();
	currentLoadSection = pSect;
}

void out_EndLoadSection(void)
{
	if (!currentLoadSection)
		yyerror("Found `ENDL` outside of a `LOAD` block");
	currentLoadSection = NULL;

	changeSection();
	curOffset += loadOffset;
	loadOffset = 0;
}

struct Section *sect_GetSymbolSection(void)
{
	return currentLoadSection ? currentLoadSection : pCurrentSection;
}

uint32_t sect_GetOutputOffset(void)
{
	return curOffset + loadOffset;
}

void sect_AlignPC(uint8_t alignment, uint16_t offset)
{
	struct Section *sect = sect_GetSymbolSection();

	if (sect->nOrg != -1) {
		if ((sym_GetPCValue() - offset) % (1 << alignment))
			yyerror("Section's fixed address fails required alignment (PC = $%04" PRIx32 ")",
				sym_GetPCValue());
	} else if (sect->nAlign != 0) {
		if ((((sect->alignOfs + curOffset) % (1 << sect->nAlign))
						- offset) % (1 << alignment)) {
			yyerror("Section's alignment fails required alignment (offset from section start = $%04" PRIx32 ")",
				curOffset);
		} else if (alignment > sect->nAlign) {
			sect->nAlign = alignment;
			sect->alignOfs =
					(offset - curOffset) % (1 << alignment);
		}
	} else {
		sect->nAlign = alignment;
		sect->alignOfs = offset;
	}
}

static inline void growSection(uint32_t growth)
{
	curOffset += growth;
	if (curOffset > pCurrentSection->size)
		pCurrentSection->size = curOffset;
	if (currentLoadSection && curOffset > currentLoadSection->size)
		currentLoadSection->size = curOffset;
}

static inline void writebyte(uint8_t byte)
{
	pCurrentSection->tData[sect_GetOutputOffset()] = byte;
	growSection(1);
}

static inline void writeword(uint16_t b)
{
	writebyte(b & 0xFF);
	writebyte(b >> 8);
}

static inline void writelong(uint32_t b)
{
	writebyte(b & 0xFF);
	writebyte(b >> 8);
	writebyte(b >> 16);
	writebyte(b >> 24);
}

static inline void createPatch(enum PatchType type,
			       struct Expression const *expr)
{
	out_CreatePatch(type, expr, sect_GetOutputOffset());
}

/*
 * Output an absolute byte
 */
void out_AbsByte(uint8_t b)
{
	checkcodesection();
	reserveSpace(1);

	writebyte(b);
}

void out_AbsByteGroup(uint8_t const *s, int32_t length)
{
	checkcodesection();
	reserveSpace(length);

	while (length--)
		writebyte(*s++);
}

/*
 * Skip this many bytes
 */
void out_Skip(int32_t skip, bool ds)
{
	checksection();
	reserveSpace(skip);

	if (!ds && sect_HasData(pCurrentSection->nType))
		warning(WARNING_EMPTY_DATA_DIRECTIVE, "db/dw/dl directive without data in ROM");

	if (!sect_HasData(pCurrentSection->nType)) {
		growSection(skip);
	} else if (nUnionDepth > 0) {
		while (skip--)
			writebyte(CurrentOptions.fillchar);
	} else {
		checkcodesection();
		while (skip--)
			writebyte(CurrentOptions.fillchar);
	}
}

/*
 * Output a NULL terminated string (excluding the NULL-character)
 */
void out_String(char const *s)
{
	checkcodesection();
	reserveSpace(strlen(s));

	while (*s)
		writebyte(*s++);
}

/*
 * Output a relocatable byte. Checking will be done to see if it
 * is an absolute value in disguise.
 */
void out_RelByte(struct Expression *expr)
{
	checkcodesection();
	reserveSpace(1);

	if (!rpn_isKnown(expr)) {
		createPatch(PATCHTYPE_BYTE, expr);
		writebyte(0);
	} else {
		writebyte(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output several copies of a relocatable byte. Checking will be done to see if
 * it is an absolute value in disguise.
 */
void out_RelBytes(struct Expression *expr, uint32_t n)
{
	checkcodesection();
	reserveSpace(n);

	while (n--) {
		if (!rpn_isKnown(expr)) {
			createPatch(PATCHTYPE_BYTE, expr);
			writebyte(0);
		} else {
			writebyte(expr->nVal);
		}
	}
	rpn_Free(expr);
}

/*
 * Output a relocatable word. Checking will be done to see if
 * it's an absolute value in disguise.
 */
void out_RelWord(struct Expression *expr)
{
	checkcodesection();
	reserveSpace(2);

	if (!rpn_isKnown(expr)) {
		createPatch(PATCHTYPE_WORD, expr);
		writeword(0);
	} else {
		writeword(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output a relocatable longword. Checking will be done to see if
 * is an absolute value in disguise.
 */
void out_RelLong(struct Expression *expr)
{
	checkcodesection();
	reserveSpace(2);

	if (!rpn_isKnown(expr)) {
		createPatch(PATCHTYPE_LONG, expr);
		writelong(0);
	} else {
		writelong(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output a PC-relative relocatable byte. Checking will be done to see if it
 * is an absolute value in disguise.
 */
void out_PCRelByte(struct Expression *expr)
{
	checkcodesection();
	reserveSpace(1);

	if (!rpn_isKnown(expr) || pCurrentSection->nOrg == -1) {
		createPatch(PATCHTYPE_JR, expr);
		writebyte(0);
	} else {
		/* Target is relative to the byte *after* the operand */
		uint16_t address = sym_GetPCValue() + 1;
		/* The offset wraps (jump from ROM to HRAM, for loopexample) */
		int16_t offset = expr->nVal - address;

		if (offset < -128 || offset > 127) {
			yyerror("jr target out of reach (expected -129 < %" PRId16 " < 128)",
				offset);
			writebyte(0);
		} else {
			writebyte(offset);
		}
	}
	rpn_Free(expr);
}

/*
 * Output a binary file
 */
void out_BinaryFile(char const *s)
{
	FILE *f = fstk_FindFile(s, NULL);

	if (!f) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		fatalerror("Error opening INCBIN file '%s': %s", s,
			   strerror(errno));
	}

	int32_t fsize = -1;
	int byte;

	checkcodesection();
	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);
		rewind(f);

		reserveSpace(fsize);
	} else if (errno != ESPIPE) {
		yyerror("Error determining size of INCBIN file '%s': %s", s,
			strerror(errno));
	}

	while ((byte = fgetc(f)) != EOF) {
		if (fsize == -1)
			growSection(1);
		writebyte(byte);
	}

	if (ferror(f))
		yyerror("Error reading INCBIN file '%s': %s", s,
			strerror(errno));

	fclose(f);
}

void out_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length)
{
	if (start_pos < 0) {
		yyerror("Start position cannot be negative (%" PRId32 ")",
			start_pos);
		start_pos = 0;
	}

	if (length < 0) {
		yyerror("Number of bytes to read cannot be negative (%" PRId32 ")",
			length);
		length = 0;
	}
	if (length == 0) /* Don't even bother with 0-byte slices */
		return;

	FILE *f = fstk_FindFile(s, NULL);

	if (!f) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		fatalerror("Error opening INCBIN file '%s': %s", s,
			   strerror(errno));
	}

	checkcodesection();
	reserveSpace(length);

	int32_t fsize;

	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);

		if (start_pos >= fsize) {
			yyerror("Specified start position is greater than length of file");
			return;
		}

		if ((start_pos + length) > fsize)
			fatalerror("Specified range in INCBIN is out of bounds");

		fseek(f, start_pos, SEEK_SET);
	} else {
		if (errno != ESPIPE)
			yyerror("Error determining size of INCBIN file '%s': %s",
				s, strerror(errno));
		/* The file isn't seekable, so we'll just skip bytes */
		while (start_pos--)
			(void)fgetc(f);
	}

	int32_t todo = length;

	while (todo--) {
		int byte = fgetc(f);

		if (byte != EOF) {
			writebyte(byte);
		} else if (ferror(f)) {
			yyerror("Error reading INCBIN file '%s': %s", s,
				strerror(errno));
		} else {
			yyerror("Premature end of file (%" PRId32 " bytes left to read)",
				todo + 1);
		}
	}

	fclose(f);
}

/*
 * Section stack routines
 */
void out_PushSection(void)
{
	struct SectionStackEntry *pSect;

	pSect = malloc(sizeof(struct SectionStackEntry));
	if (pSect == NULL)
		fatalerror("No memory for section stack");

	pSect->pSection = pCurrentSection;
	pSect->pScope = sym_GetCurrentSymbolScope();
	pSect->offset = curOffset;
	pSect->pNext = pSectionStack;
	pSectionStack = pSect;
	/* TODO: maybe set current section to NULL? */
}

void out_PopSection(void)
{
	if (pSectionStack == NULL)
		fatalerror("No entries in the section stack");

	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block!");

	struct SectionStackEntry *pSect;

	pSect = pSectionStack;
	changeSection();
	pCurrentSection = pSect->pSection;
	sym_SetCurrentSymbolScope(pSect->pScope);
	curOffset = pSect->offset;

	pSectionStack = pSect->pNext;
	free(pSect);
}
