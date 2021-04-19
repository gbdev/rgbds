
#include <assert.h>
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
#include "asm/symbol.h"
#include "asm/warning.h"

#include "extern/err.h"
#include "platform.h" // strdup

uint8_t fillByte;

struct SectionStackEntry {
	struct Section *section;
	char const *scope; /* Section's symbol scope */
	uint32_t offset;
	struct SectionStackEntry *next;
};

struct SectionStackEntry *sectionStack;
uint32_t curOffset; /* Offset into the current section (see sect_GetSymbolOffset) */
static struct Section *currentLoadSection = NULL;
int32_t loadOffset; /* Offset into the LOAD section's parent (see sect_GetOutputOffset) */

struct UnionStackEntry {
	uint32_t start;
	uint32_t size;
	struct UnionStackEntry *next;
} *unionStack = NULL;

/*
 * A quick check to see if we have an initialized section
 */
static void checksection(void)
{
	if (currentSection == NULL)
		fatalerror("Code generation before SECTION directive\n");
}

/*
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 */
static void checkcodesection(void)
{
	checksection();

	if (!sect_HasData(currentSection->type))
		fatalerror("Section '%s' cannot contain code or data (not ROM0 or ROMX)\n",
			   currentSection->name);
}

static void checkSectionSize(struct Section const *sect, uint32_t size)
{
	uint32_t maxSize = maxsize[sect->type];

	if (size > maxSize)
		fatalerror("Section '%s' grew too big (max size = 0x%" PRIX32
			   " bytes, reached 0x%" PRIX32 ").\n", sect->name, maxSize, size);
}

/*
 * Check if the section has grown too much.
 */
static void reserveSpace(uint32_t delta_size)
{
	/*
	 * This check is here to trap broken code that generates sections that
	 * are too big and to prevent the assembler from generating huge object
	 * files or trying to allocate too much memory.
	 * A check at the linking stage is still necessary.
	 */
	checkSectionSize(currentSection, curOffset + loadOffset + delta_size);
	if (currentLoadSection)
		checkSectionSize(currentLoadSection, curOffset + delta_size);
}

struct Section *out_FindSectionByName(const char *name)
{
	for (struct Section *sect = sectionList; sect; sect = sect->next) {
		if (strcmp(name, sect->name) == 0)
			return sect;
	}
	return NULL;
}

#define mask(align) ((1U << (align)) - 1)
#define fail(...) \
do { \
	error(__VA_ARGS__); \
	nbSectErrors++; \
} while (0)

static unsigned int mergeSectUnion(struct Section *sect, enum SectionType type, uint32_t org,
				   uint8_t alignment, uint16_t alignOffset)
{
	assert(alignment < 16); // Should be ensured by the caller
	unsigned int nbSectErrors = 0;

	/*
	 * Unionized sections only need "compatible" constraints, and they end up with the strictest
	 * combination of both.
	 */
	if (sect_HasData(type))
		fail("Cannot declare ROM sections as UNION\n");

	if (org != (uint32_t)-1) {
		/* If both are fixed, they must be the same */
		if (sect->org != (uint32_t)-1 && sect->org != org)
			fail("Section already declared as fixed at different address $%04"
			     PRIx32 "\n", sect->org);
		else if (sect->align != 0 && (mask(sect->align) & (org - sect->alignOfs)))
			fail("Section already declared as aligned to %u bytes (offset %"
				   PRIu16 ")\n", 1U << sect->align, sect->alignOfs);
		else
			/* Otherwise, just override */
			sect->org = org;

	} else if (alignment != 0) {
		/* Make sure any fixed address given is compatible */
		if (sect->org != (uint32_t)-1) {
			if ((sect->org - alignOffset) & mask(alignment))
				fail("Section already declared as fixed at incompatible address $%04"
				     PRIx32 "\n", sect->org);
		/* Check if alignment offsets are compatible */
		} else if ((alignOffset & mask(sect->align))
			   != (sect->alignOfs & mask(alignment))) {
			fail("Section already declared with incompatible %" PRIu8
			     "-byte alignment (offset %" PRIu16 ")\n",
			     sect->align, sect->alignOfs);
		} else if (alignment > sect->align) {
			// If the section is not fixed, its alignment is the largest of both
			sect->align = alignment;
			sect->alignOfs = alignOffset;
		}
	}

	return nbSectErrors;
}

static unsigned int mergeFragments(struct Section *sect, enum SectionType type, uint32_t org,
				   uint8_t alignment, uint16_t alignOffset)
{
	(void)type;
	assert(alignment < 16); // Should be ensured by the caller
	unsigned int nbSectErrors = 0;

	/*
	 * Fragments only need "compatible" constraints, and they end up with the strictest
	 * combination of both.
	 * The merging is however performed at the *end* of the original section!
	 */
	if (org != (uint32_t)-1) {
		uint16_t curOrg = org - sect->size;

		/* If both are fixed, they must be the same */
		if (sect->org != (uint32_t)-1 && sect->org != curOrg)
			fail("Section already declared as fixed at incompatible address $%04"
			     PRIx32 " (cur addr = %04" PRIx32 ")\n",
			     sect->org, sect->org + sect->size);
		else if (sect->align != 0 && (mask(sect->align) & (curOrg - sect->alignOfs)))
			fail("Section already declared as aligned to %u bytes (offset %"
			     PRIu16 ")\n", 1U << sect->align, sect->alignOfs);
		else
			/* Otherwise, just override */
			sect->org = curOrg;

	} else if (alignment != 0) {
		int32_t curOfs = (alignOffset - sect->size) % (1U << alignment);

		if (curOfs < 0)
			curOfs += 1U << alignment;

		/* Make sure any fixed address given is compatible */
		if (sect->org != (uint32_t)-1) {
			if ((sect->org - curOfs) & mask(alignment))
				fail("Section already declared as fixed at incompatible address $%04"
				     PRIx32 "\n", sect->org);
		/* Check if alignment offsets are compatible */
		} else if ((curOfs & mask(sect->align)) != (sect->alignOfs & mask(alignment))) {
			fail("Section already declared with incompatible %" PRIu8
			     "-byte alignment (offset %" PRIu16 ")\n",
			     sect->align, sect->alignOfs);
		} else if (alignment > sect->align) {
			// If the section is not fixed, its alignment is the largest of both
			sect->align = alignment;
			sect->alignOfs = curOfs;
		}
	}

	return nbSectErrors;
}

static void mergeSections(struct Section *sect, enum SectionType type, uint32_t org, uint32_t bank,
			  uint8_t alignment, uint16_t alignOffset, enum SectionModifier mod)
{
	unsigned int nbSectErrors = 0;

	if (type != sect->type)
		fail("Section already exists but with type %s\n", typeNames[sect->type]);

	if (sect->modifier != mod) {
		fail("Section already declared as %s section\n", sectionModNames[sect->modifier]);
	} else {
		switch (mod) {
		case SECTION_UNION:
		case SECTION_FRAGMENT:
			nbSectErrors += (mod == SECTION_UNION ? mergeSectUnion : mergeFragments)
						(sect, type, org, alignment, alignOffset);

			// Common checks

			/* If the section's bank is unspecified, override it */
			if (sect->bank == (uint32_t)-1)
				sect->bank = bank;
			/* If both specify a bank, it must be the same one */
			else if (bank != (uint32_t)-1 && sect->bank != bank)
				fail("Section already declared with different bank %" PRIu32 "\n",
				     sect->bank);
			break;

		case SECTION_NORMAL:
			fail("Section already defined previously at ");
			fstk_Dump(sect->src, sect->fileLine);
			putc('\n', stderr);
			break;
		}
	}

	if (nbSectErrors)
		fatalerror("Cannot create section \"%s\" (%u error%s)\n",
			   sect->name, nbSectErrors, nbSectErrors == 1 ? "" : "s");
}

#undef fail

/*
 * Create a new section, not yet in the list.
 */
static struct Section *createSection(char const *name, enum SectionType type,
				     uint32_t org, uint32_t bank, uint8_t alignment,
				     uint16_t alignOffset, enum SectionModifier mod)
{
	struct Section *sect = malloc(sizeof(*sect));

	if (sect == NULL)
		fatalerror("Not enough memory for section: %s\n", strerror(errno));

	sect->name = strdup(name);
	if (sect->name == NULL)
		fatalerror("Not enough memory for section name: %s\n", strerror(errno));

	sect->type = type;
	sect->modifier = mod;
	sect->src = fstk_GetFileStack();
	sect->fileLine = lexer_GetLineNo();
	sect->size = 0;
	sect->org = org;
	sect->bank = bank;
	sect->align = alignment;
	sect->alignOfs = alignOffset;
	sect->next = NULL;
	sect->patches = NULL;

	/* It is only needed to allocate memory for ROM sections. */
	if (sect_HasData(type)) {
		sect->data = malloc(maxsize[type]);
		if (sect->data == NULL)
			fatalerror("Not enough memory for section: %s\n", strerror(errno));
	} else {
		sect->data = NULL;
	}

	return sect;
}

/*
 * Find a section by name and type. If it doesn't exist, create it.
 */
static struct Section *getSection(char const *name, enum SectionType type, uint32_t org,
				  struct SectionSpec const *attrs, enum SectionModifier mod)
{
	uint32_t bank = attrs->bank;
	uint8_t alignment = attrs->alignment;
	uint16_t alignOffset = attrs->alignOfs;

	// First, validate parameters, and normalize them if applicable

	if (bank != (uint32_t)-1) {
		if (type != SECTTYPE_ROMX && type != SECTTYPE_VRAM
		 && type != SECTTYPE_SRAM && type != SECTTYPE_WRAMX)
			error("BANK only allowed for ROMX, WRAMX, SRAM, or VRAM sections\n");
		else if (bank < bankranges[type][0]
		      || bank > bankranges[type][1])
			error("%s bank value $%04" PRIx32 " out of range ($%04" PRIx32 " to $%04"
				PRIx32 ")\n", typeNames[type], bank,
				bankranges[type][0], bankranges[type][1]);
	} else if (nbbanks(type) == 1) {
		// If the section type only has a single bank, implicitly force it
		bank = bankranges[type][0];
	}

	if (alignOffset >= 1 << alignment) {
		error("Alignment offset (%" PRIu16 ") must be smaller than alignment size (%u)\n",
		      alignOffset, 1U << alignment);
		alignOffset = 0;
	}

	if (org != (uint32_t)-1) {
		if (org < startaddr[type] || org > endaddr(type))
			error("Section \"%s\"'s fixed address %#" PRIx32
				" is outside of range [%#" PRIx16 "; %#" PRIx16 "]\n",
				name, org, startaddr[type], endaddr(type));
	}

	if (alignment != 0) {
		if (alignment > 16) {
			error("Alignment must be between 0 and 16, not %u\n", alignment);
			alignment = 16;
		}
		/* It doesn't make sense to have both alignment and org set */
		uint32_t mask = mask(alignment);

		if (org != (uint32_t)-1) {
			if ((org - alignOffset) & mask)
				error("Section \"%s\"'s fixed address doesn't match its alignment\n",
					name);
			alignment = 0; /* Ignore it if it's satisfied */
		} else if (startaddr[type] & mask) {
			error("Section \"%s\"'s alignment cannot be attained in %s\n",
				name, typeNames[type]);
			alignment = 0; /* Ignore it if it's unattainable */
			org = 0;
		} else if (alignment == 16) {
			// Treat an alignment of 16 as being fixed at address 0
			alignment = 0;
			org = 0;
			// The address is known to be valid, since the alignment is
		}
	}

	// Check if another section exists with the same name; merge if yes, otherwise create one

	struct Section *sect = out_FindSectionByName(name);

	if (sect) {
		mergeSections(sect, type, org, bank, alignment, alignOffset, mod);
	} else {
		sect = createSection(name, type, org, bank, alignment, alignOffset, mod);
		// Add the new section to the list (order doesn't matter)
		sect->next = sectionList;
		sectionList = sect;
	}

	return sect;
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

	for (struct SectionStackEntry *stack = sectionStack; stack; stack = stack->next) {
		if (stack->section && !strcmp(name, stack->section->name))
			fatalerror("Section '%s' is already on the stack\n", name);
	}

	struct Section *sect = getSection(name, type, org, attribs, mod);

	changeSection();
	curOffset = mod == SECTION_UNION ? 0 : sect->size;
	currentSection = sect;
}

/*
 * Set the current section by name and type
 */
void out_SetLoadSection(char const *name, uint32_t type, uint32_t org,
			struct SectionSpec const *attribs,
			enum SectionModifier mod)
{
	checkcodesection();

	if (currentLoadSection)
		fatalerror("`LOAD` blocks cannot be nested\n");

	if (sect_HasData(type))
		error("`LOAD` blocks cannot create a ROM section\n");

	struct Section *sect = getSection(name, type, org, attribs, mod);

	changeSection();
	loadOffset = curOffset - (mod == SECTION_UNION ? 0 : sect->size);
	curOffset -= loadOffset;
	currentLoadSection = sect;
}

void out_EndLoadSection(void)
{
	if (!currentLoadSection)
		error("Found `ENDL` outside of a `LOAD` block\n");

	changeSection();
	curOffset += loadOffset;
	loadOffset = 0;
	currentLoadSection = NULL;
}

struct Section *sect_GetSymbolSection(void)
{
	return currentLoadSection ? currentLoadSection : currentSection;
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
	uint16_t alignSize = 1 << alignment; // Size of an aligned "block"

	if (sect->org != (uint32_t)-1) {
		if ((sym_GetPCValue() - offset) % alignSize)
			error("Section's fixed address fails required alignment (PC = $%04" PRIx32
			      ")\n", sym_GetPCValue());
	} else if (sect->align != 0) {
		if ((((sect->alignOfs + curOffset) % (1 << sect->align)) - offset) % alignSize) {
			error("Section's alignment fails required alignment (offset from section start = $%04"
				PRIx32 ")\n", curOffset);
		} else if (alignment > sect->align) {
			sect->align = alignment;
			sect->alignOfs = (offset - curOffset) % alignSize;
		}
	} else {
		sect->align = alignment;
		// We need `(sect->alignOfs + curOffset) % alignSize == offset
		sect->alignOfs = (offset - curOffset) % alignSize;
	}
}

static void growSection(uint32_t growth)
{
	curOffset += growth;
	if (curOffset + loadOffset > currentSection->size)
		currentSection->size = curOffset + loadOffset;
	if (currentLoadSection && curOffset > currentLoadSection->size)
		currentLoadSection->size = curOffset;
}

static void writebyte(uint8_t byte)
{
	currentSection->data[sect_GetOutputOffset()] = byte;
	growSection(1);
}

static void writeword(uint16_t b)
{
	writebyte(b & 0xFF);
	writebyte(b >> 8);
}

static void writelong(uint32_t b)
{
	writebyte(b & 0xFF);
	writebyte(b >> 8);
	writebyte(b >> 16);
	writebyte(b >> 24);
}

static void createPatch(enum PatchType type, struct RPNBuffer *rpn, uint32_t pcShift)
{
	out_CreatePatch(type, rpn, sect_GetOutputOffset(), pcShift);
}

void sect_StartUnion(void)
{
	if (!currentSection)
		fatalerror("UNIONs must be inside a SECTION\n");
	if (sect_HasData(currentSection->type))
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
		error("Unterminated UNION construct!\n");
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

void out_AbsWordGroup(uint8_t const *s, int32_t length)
{
	checkcodesection();
	reserveSpace(length * 2);

	while (length--)
		writeword(*s++);
}

void out_AbsLongGroup(uint8_t const *s, int32_t length)
{
	checkcodesection();
	reserveSpace(length * 4);

	while (length--)
		writelong(*s++);
}

/*
 * Skip this many bytes
 */
void out_Skip(int32_t skip, bool ds)
{
	checksection();
	reserveSpace(skip);

	if (!ds && sect_HasData(currentSection->type))
		warning(WARNING_EMPTY_DATA_DIRECTIVE, "%s directive without data in ROM\n",
			(skip == 4) ? "DL" : (skip == 2) ? "DW" : "DB");

	if (!sect_HasData(currentSection->type)) {
		growSection(skip);
	} else {
		checkcodesection();
		while (skip--)
			writebyte(fillByte);
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
void out_RelByte(struct Expression const *expr, uint32_t pcShift)
{
	checkcodesection();
	reserveSpace(1);
	struct RPNBuffer *rpnBufPtr = NULL;

	writebyte(rpn_Eval(expr, &rpnBufPtr));
	if (rpnBufPtr)
		createPatch(PATCHTYPE_BYTE, rpnBufPtr, pcShift);
}

/*
 * Output several copies of a relocatable byte. Checking will be done to see if
 * it is an absolute value in disguise.
 */
void out_RelBytes(uint32_t n, struct Expression * const *exprs, size_t size)
{
	checkcodesection();
	reserveSpace(n);
	struct RPNBuffer *rpnBufPtr;

	for (uint32_t i = 0; i < n; i++) {
		rpnBufPtr = NULL;
		writebyte(rpn_Eval(exprs[i % size], &rpnBufPtr));
		if (rpnBufPtr)
			createPatch(PATCHTYPE_BYTE, rpnBufPtr, i);
	}
}

/*
 * Output a relocatable word. Checking will be done to see if
 * it's an absolute value in disguise.
 */
void out_RelWord(struct Expression const *expr, uint32_t pcShift)
{
	checkcodesection();
	reserveSpace(2);
	struct RPNBuffer *rpnBufPtr = NULL;

	writeword(rpn_Eval(expr, &rpnBufPtr));
	if (rpnBufPtr)
		createPatch(PATCHTYPE_WORD, rpnBufPtr, pcShift);
}

/*
 * Output a relocatable longword. Checking will be done to see if
 * is an absolute value in disguise.
 */
void out_RelLong(struct Expression const *expr, uint32_t pcShift)
{
	checkcodesection();
	reserveSpace(2);
	struct RPNBuffer *rpnBufPtr = NULL;

	writelong(rpn_Eval(expr, &rpnBufPtr));
	if (rpnBufPtr)
		createPatch(PATCHTYPE_LONG, rpnBufPtr, pcShift);
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
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCBIN file '%s' (%s)\n", s, strerror(errno));
			failedOnMissingInclude = true;
			return;
		}
		error("Error opening INCBIN file '%s': %s\n", s, strerror(errno));
		return;
	}

	int32_t fsize = -1;
	int byte;

	checkcodesection();
	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);

		if (startPos > fsize) {
			error("Specified start position is greater than length of file\n");
			goto cleanup;
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

cleanup:
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
	free(fullPath);

	if (!f) {
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCBIN file '%s' (%s)\n", s, strerror(errno));
			failedOnMissingInclude = true;
		} else {
			error("Error opening INCBIN file '%s': %s\n", s, strerror(errno));
		}
		return;
	}

	checkcodesection();
	reserveSpace(length);

	int32_t fsize;

	if (fseek(f, 0, SEEK_END) != -1) {
		fsize = ftell(f);

		if (start_pos > fsize) {
			error("Specified start position is greater than length of file\n");
			goto cleanup;
		}

		if ((start_pos + length) > fsize) {
			error("Specified range in INCBIN is out of bounds (%" PRIu32 " + %" PRIu32
			      " > %" PRIu32 ")\n", start_pos, length, fsize);
			goto cleanup;
		}

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

cleanup:
	fclose(f);
}

/*
 * Section stack routines
 */
void out_PushSection(void)
{
	struct SectionStackEntry *sect = malloc(sizeof(*sect));

	if (sect == NULL)
		fatalerror("No memory for section stack: %s\n",  strerror(errno));
	sect->section = currentSection;
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
	currentSection = sect->section;
	sym_SetCurrentSymbolScope(sect->scope);
	curOffset = sect->offset;

	sectionStack = sect->next;
	free(sect);
}
