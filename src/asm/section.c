
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
	struct Section *section;
	char const *scope; /* Section's symbol scope */
	uint32_t offset;
	struct SectionStackEntry *next;
};

struct SectionStackEntry *sectionStack;
uint32_t curOffset; /* Offset into the current section (see sect_GetSymbolOffset) */
static struct Section *currentLoadSection = NULL;
uint32_t loadOffset; /* The offset of the LOAD section within its parent */

struct UnionStackEntry {
	uint32_t start;
	uint32_t size;
	struct UnionStackEntry *next;
} *unionStack = NULL;

/*
 * A quick check to see if we have an initialized section
 */
static inline void checksection(void)
{
	if (pCurrentSection == NULL)
		fatalerror("Code generation before SECTION directive\n");
}

/*
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 */
static inline void checkcodesection(void)
{
	checksection();

	if (!sect_HasData(pCurrentSection->type))
		fatalerror("Section '%s' cannot contain code or data (not ROM0 or ROMX)\n",
			   pCurrentSection->name);
}

static inline void checkSectionSize(struct Section const *sect, uint32_t size)
{
	uint32_t maxSize = maxsize[sect->type];

	if (size > maxSize)
		fatalerror("Section '%s' grew too big (max size = 0x%" PRIX32
			   " bytes, reached 0x%" PRIX32 ").\n", sect->name, maxSize, size);
}

/*
 * Check if the section has grown too much.
 */
static inline void reserveSpace(uint32_t delta_size)
{
	/*
	 * This check is here to trap broken code that generates sections that
	 * are too big and to prevent the assembler from generating huge object
	 * files or trying to allocate too much memory.
	 * A check at the linking stage is still necessary.
	 */
	checkSectionSize(pCurrentSection, curOffset + loadOffset + delta_size);
	if (currentLoadSection)
		checkSectionSize(currentLoadSection, curOffset + delta_size);
}

struct Section *out_FindSectionByName(const char *name)
{
	struct Section *sect = pSectionList;

	while (sect) {
		if (strcmp(name, sect->name) == 0)
			return sect;

		sect = sect->next;
	}

	return NULL;
}

/*
 * Find a section by name and type. If it doesn't exist, create it
 */
static struct Section *getSection(char const *name, enum SectionType type,
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
			error("BANK only allowed for ROMX, WRAMX, SRAM, or VRAM sections\n");
		else if (bank < bankranges[type][0]
		      || bank > bankranges[type][1])
			error("%s bank value $%" PRIx32 " out of range ($%" PRIx32 " to $%"
				PRIx32 ")\n", typeNames[type], bank,
				bankranges[type][0], bankranges[type][1]);
	}

	if (alignOffset >= 1 << alignment) {
		error("Alignment offset must not be greater than alignment (%" PRIu16 " < %u)\n",
			alignOffset, 1U << alignment);
		alignOffset = 0;
	}

	if (alignment != 0) {
		/* It doesn't make sense to have both alignment and org set */
		uint32_t mask = mask(alignment);

		if (org != -1) {
			if ((org - alignOffset) & mask)
				error("Section \"%s\"'s fixed address doesn't match its alignment\n",
					name);
			alignment = 0; /* Ignore it if it's satisfied */
		} else if (startaddr[type] & mask) {
			error("Section \"%s\"'s alignment cannot be attained in %s\n",
				name, typeNames[type]);
		}
	}

	if (org != -1) {
		if (org < startaddr[type] || org > endaddr(type))
			error("Section \"%s\"'s fixed address %#" PRIx32
				" is outside of range [%#" PRIx16 "; %#" PRIx16 "]\n",
				name, org, startaddr[type], endaddr(type));
	}

	if (nbbanks(type) == 1)
		bank = bankranges[type][0];

	struct Section *sect = out_FindSectionByName(name);

	if (sect) {
		unsigned int nbSectErrors = 0;
#define fail(...) \
	do { \
		error(__VA_ARGS__); \
		nbSectErrors++; \
	} while (0)

		if (type != sect->type)
			fail("Section \"%s\" already exists but with type %s\n",
			     sect->name, typeNames[sect->type]);

		if (sect->modifier != mod)
			fail("Section \"%s\" already declared as %s section\n",
			     sect->name, sectionModNames[sect->modifier]);
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
				fail("Cannot declare ROM sections as UNION\n");
			if (org != -1) {
				/* If both are fixed, they must be the same */
				if (sect->org != -1 && sect->org != org)
					fail("Section \"%s\" already declared as fixed at different address $%"
					     PRIx32 "\n",
					     sect->name, sect->org);
				else if (sect->align != 0
				      && (mask(sect->align)
						& (org - sect->alignOfs)))
					fail("Section \"%s\" already declared as aligned to %u bytes (offset %"
					     PRIu16 ")\n", sect->name, 1U << sect->align, sect->alignOfs);
				else
					/* Otherwise, just override */
					sect->org = org;
			} else if (alignment != 0) {
				/* Make sure any fixed address is compatible */
				if (sect->org != -1) {
					if ((sect->org - alignOffset)
							& mask(alignment))
						fail("Section \"%s\" already declared as fixed at incompatible address $%"
						     PRIx32 "\n", sect->name, sect->org);
				/* Check if alignment offsets are compatible */
				} else if ((alignOffset & mask(sect->align))
					!= (sect->alignOfs
							& mask(alignment))) {
					fail("Section \"%s\" already declared with incompatible %"
					     PRIu8 "-byte alignment (offset %" PRIu16 ")\n",
					     sect->name, sect->align, sect->alignOfs);
				} else if (alignment > sect->align) {
					/*
					 * If the section is not fixed,
					 * its alignment is the largest of both
					 */
					sect->align = alignment;
					sect->alignOfs = alignOffset;
				}
			}
			/* If the section's bank is unspecified, override it */
			if (sect->bank == -1)
				sect->bank = bank;
			/* If both specify a bank, it must be the same one */
			else if (bank != -1 && sect->bank != bank)
				fail("Section \"%s\" already declared with different bank %"
				     PRIu32 "\n", sect->name, sect->bank);
		} else { /* Section fragments are handled identically in RGBASM */
			/* However, concaternating non-fragments will be made an error */
			if (sect->modifier != SECTION_FRAGMENT || mod != SECTION_FRAGMENT)
				warning(WARNING_OBSOLETE,
					"Concatenation of non-fragment sections is deprecated\n");

			if (org != sect->org) {
				if (sect->org == -1)
					fail("Section \"%s\" already declared as floating\n",
					     sect->name);
				else
					fail("Section \"%s\" already declared as fixed at $%"
					     PRIx32 "\n", sect->name, sect->org);
			}
			if (bank != sect->bank) {
				if (sect->bank == -1)
					fail("Section \"%s\" already declared as floating bank\n",
					     sect->name);
				else
					fail("Section \"%s\" already declared as fixed at bank %"
					     PRIu32 "\n", sect->name, sect->bank);
			}
			if (alignment != sect->align) {
				if (sect->align == 0)
					fail("Section \"%s\" already declared as unaligned\n",
					     sect->name);
				else
					fail("Section \"%s\" already declared as aligned to %u bytes\n",
					     sect->name, 1U << sect->align);
			}
		}

		if (nbSectErrors)
			fatalerror("Cannot create section \"%s\" (%u errors)\n",
				   sect->name, nbSectErrors);
#undef fail
		return sect;
	}

	sect = malloc(sizeof(*sect));
	if (sect == NULL)
		fatalerror("Not enough memory for section: %s\n", strerror(errno));

	sect->name = strdup(name);
	if (sect->name == NULL)
		fatalerror("Not enough memory for section name: %s\n", strerror(errno));

	sect->type = type;
	sect->modifier = mod;
	sect->size = 0;
	sect->org = org;
	sect->bank = bank;
	sect->align = alignment;
	sect->alignOfs = alignOffset;
	sect->next = pSectionList;
	sect->patches = NULL;

	/* It is only needed to allocate memory for ROM sections. */
	if (sect_HasData(type)) {
		uint32_t sectsize;

		sectsize = maxsize[type];
		sect->data = malloc(sectsize);
		if (sect->data == NULL)
			fatalerror("Not enough memory for section: %s\n", strerror(errno));
	} else {
		sect->data = NULL;
	}

	/*
	 * Add the new section to the list
	 * at the beginning because order doesn't matter
	 */
	pSectionList = sect;

	return sect;
#undef mask
}

/*
 * Set the current section
 */
static void changeSection(void)
{
	if (unionStack)
		fatalerror("Cannot change the section within a UNION\n");

	sym_SetCurrentSymbolScope(NULL);
}

/*
 * Set the current section by name and type
 */
void out_NewSection(char const *name, uint32_t type, uint32_t org,
		    struct SectionSpec const *attribs, enum SectionModifier mod)
{
	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block\n");

	struct Section *sect = getSection(name, type, org, attribs, mod);

	changeSection();
	curOffset = mod == SECTION_UNION ? 0 : sect->size;
	pCurrentSection = sect;
}

/*
 * Set the current section by name and type
 */
void out_SetLoadSection(char const *name, uint32_t type, uint32_t org,
			struct SectionSpec const *attribs)
{
	checkcodesection();

	if (currentLoadSection)
		fatalerror("`LOAD` blocks cannot be nested\n");

	struct Section *sect = getSection(name, type, org, attribs, false);

	loadOffset = curOffset;
	curOffset = 0; /* curOffset -= loadOffset; */
	changeSection();
	currentLoadSection = sect;
}

void out_EndLoadSection(void)
{
	if (!currentLoadSection)
		error("Found `ENDL` outside of a `LOAD` block\n");
	currentLoadSection = NULL;

	changeSection();
	curOffset += loadOffset;
	loadOffset = 0;
}

struct Section *sect_GetSymbolSection(void)
{
	return currentLoadSection ? currentLoadSection : pCurrentSection;
}

/*
 * The offset into the section above
 */
uint32_t sect_GetSymbolOffset(void)
{
	return curOffset;
}

uint32_t sect_GetOutputOffset(void)
{
	return curOffset + loadOffset;
}

void sect_AlignPC(uint8_t alignment, uint16_t offset)
{
	checksection();
	struct Section *sect = sect_GetSymbolSection();

	if (sect->org != -1) {
		if ((sym_GetPCValue() - offset) % (1 << alignment))
			error("Section's fixed address fails required alignment (PC = $%04"
				PRIx32 ")\n", sym_GetPCValue());
	} else if (sect->align != 0) {
		if ((((sect->alignOfs + curOffset) % (1 << sect->align))
						- offset) % (1 << alignment)) {
			error("Section's alignment fails required alignment (offset from section start = $%04"
				PRIx32 ")\n", curOffset);
		} else if (alignment > sect->align) {
			sect->align = alignment;
			sect->alignOfs =
					(offset - curOffset) % (1 << alignment);
		}
	} else {
		sect->align = alignment;
		sect->alignOfs = offset;
	}
}

static inline void growSection(uint32_t growth)
{
	curOffset += growth;
	if (curOffset + loadOffset > pCurrentSection->size)
		pCurrentSection->size = curOffset + loadOffset;
	if (currentLoadSection && curOffset > currentLoadSection->size)
		currentLoadSection->size = curOffset;
}

static inline void writebyte(uint8_t byte)
{
	pCurrentSection->data[sect_GetOutputOffset()] = byte;
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

void sect_StartUnion(void)
{
	if (!pCurrentSection)
		fatalerror("UNIONs must be inside a SECTION\n");
	if (sect_HasData(pCurrentSection->type))
		fatalerror("Cannot use UNION inside of ROM0 or ROMX sections\n");
	struct UnionStackEntry *entry = malloc(sizeof(*entry));

	if (!entry)
		fatalerror("Failed to allocate new union stack entry: %s\n", strerror(errno));
	entry->start = curOffset;
	entry->size = 0;
	entry->next = unionStack;
	unionStack = entry;
}

static void endUnionMember(void)
{
	uint32_t memberSize = curOffset - unionStack->start;

	if (memberSize > unionStack->size)
		unionStack->size = memberSize;
	curOffset = unionStack->start;
}

void sect_NextUnionMember(void)
{
	if (!unionStack)
		fatalerror("Found NEXTU outside of a UNION construct\n");
	endUnionMember();
}

void sect_EndUnion(void)
{
	if (!unionStack)
		fatalerror("Found ENDU outside of a UNION construct\n");
	endUnionMember();
	curOffset += unionStack->size;
	struct UnionStackEntry *next = unionStack->next;

	free(unionStack);
	unionStack = next;
}

void sect_CheckUnionClosed(void)
{
	if (unionStack)
		fatalerror("Unterminated UNION construct!\n");
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

	if (!ds && sect_HasData(pCurrentSection->type))
		warning(WARNING_EMPTY_DATA_DIRECTIVE, "db/dw/dl directive without data in ROM\n");

	if (!sect_HasData(pCurrentSection->type)) {
		growSection(skip);
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
	struct Symbol const *pc = sym_GetPC();

	if (!rpn_IsDiffConstant(expr, pc)) {
		createPatch(PATCHTYPE_JR, expr);
		writebyte(0);
	} else {
		struct Symbol const *sym = rpn_SymbolOf(expr);
		/* The offset wraps (jump from ROM to HRAM, for example) */
		int16_t offset;

		/* Offset is relative to the byte *after* the operand */
		if (sym == pc)
			offset = -2; /* PC as operand to `jr` is lower than reference PC by 2 */
		else
			offset = sym_GetValue(sym) - (sym_GetValue(pc) + 1);

		if (offset < -128 || offset > 127) {
			error("jr target out of reach (expected -129 < %" PRId16 " < 128)\n",
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
void out_BinaryFile(char const *s, int32_t startPos)
{
	if (startPos < 0) {
		error("Start position cannot be negative (%" PRId32 ")\n", startPos);
		startPos = 0;
	}

	char *fullPath = NULL;
	size_t size = 0;
	FILE *f = NULL;

	if (fstk_FindFile(s, &fullPath, &size))
		f = fopen(fullPath, "rb");
	free(fullPath);

	if (!f) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		fatalerror("Error opening INCBIN file '%s': %s\n", s, strerror(errno));
	}

	int32_t fsize = -1;
	int byte;

	checkcodesection();
	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);

		if (startPos >= fsize) {
			error("Specified start position is greater than length of file\n");
			fclose(f);
			return;
		}

		fseek(f, startPos, SEEK_SET);
		reserveSpace(fsize - startPos);
	} else {
		if (errno != ESPIPE)
			error("Error determining size of INCBIN file '%s': %s\n",
				s, strerror(errno));
		/* The file isn't seekable, so we'll just skip bytes */
		while (startPos--)
			(void)fgetc(f);
	}

	while ((byte = fgetc(f)) != EOF) {
		if (fsize == -1)
			growSection(1);
		writebyte(byte);
	}

	if (ferror(f))
		error("Error reading INCBIN file '%s': %s\n", s, strerror(errno));

	fclose(f);
}

void out_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length)
{
	if (start_pos < 0) {
		error("Start position cannot be negative (%" PRId32 ")\n", start_pos);
		start_pos = 0;
	}

	if (length < 0) {
		error("Number of bytes to read cannot be negative (%" PRId32 ")\n", length);
		length = 0;
	}
	if (length == 0) /* Don't even bother with 0-byte slices */
		return;

	char *fullPath = NULL;
	size_t size = 0;
	FILE *f = NULL;

	if (fstk_FindFile(s, &fullPath, &size))
		f = fopen(fullPath, "rb");

	if (!f) {
		free(fullPath);
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		fatalerror("Error opening INCBIN file '%s': %s\n", s, strerror(errno));
	}

	checkcodesection();
	reserveSpace(length);

	int32_t fsize;

	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);

		if (start_pos >= fsize) {
			error("Specified start position is greater than length of file\n");
			return;
		}

		if ((start_pos + length) > fsize)
			fatalerror("Specified range in INCBIN is out of bounds\n");

		fseek(f, start_pos, SEEK_SET);
	} else {
		if (errno != ESPIPE)
			error("Error determining size of INCBIN file '%s': %s\n",
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
			error("Error reading INCBIN file '%s': %s\n", s, strerror(errno));
		} else {
			error("Premature end of file (%" PRId32 " bytes left to read)\n",
				todo + 1);
		}
	}

	fclose(f);
	free(fullPath);
}

/*
 * Section stack routines
 */
void out_PushSection(void)
{
	struct SectionStackEntry *sect = malloc(sizeof(*sect));

	if (sect == NULL)
		fatalerror("No memory for section stack: %s\n",  strerror(errno));
	sect->section = pCurrentSection;
	sect->scope = sym_GetCurrentSymbolScope();
	sect->offset = curOffset;
	sect->next = sectionStack;
	sectionStack = sect;
	/* TODO: maybe set current section to NULL? */
}

void out_PopSection(void)
{
	if (!sectionStack)
		fatalerror("No entries in the section stack\n");

	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block!\n");

	struct SectionStackEntry *sect;

	sect = sectionStack;
	changeSection();
	pCurrentSection = sect->section;
	sym_SetCurrentSymbolScope(sect->scope);
	curOffset = sect->offset;

	sectionStack = sect->next;
	free(sect);
}
