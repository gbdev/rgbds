/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "link/assign.h"
#include "link/main.h"
#include "link/object.h"
#include "link/patch.h"
#include "link/section.h"
#include "link/symbol.h"

#include "extern/err.h"
#include "helpers.h"
#include "linkdefs.h"

static struct SymbolList {
	size_t nbSymbols;
	struct Symbol **symbolList;
	struct SymbolList *next;
} *symbolLists;

unsigned int nbObjFiles;
static struct {
	struct FileStackNode *nodes;
	uint32_t nbNodes;
} *nodes;
static struct Assertion *assertions;

/***** Helper functions for reading object files *****/

/*
 * Internal, DO NOT USE.
 * For helper wrapper macros defined below, such as `tryReadlong`
 */
#define tryRead(func, type, errval, var, file, ...) \
	do { \
		FILE *tmpFile = file; \
		type tmpVal = func(tmpFile); \
		if (tmpVal == (errval)) { \
			errx(1, __VA_ARGS__, feof(tmpFile) \
						? "Unexpected end of file" \
						: strerror(errno)); \
		} \
		var = tmpVal; \
	} while (0)

/**
 * Reads an unsigned long (32-bit) value from a file.
 * @param file The file to read from. This will read 4 bytes from the file.
 * @return The value read, cast to a int64_t, or -1 on failure.
 */
static int64_t readlong(FILE *file)
{
	uint32_t value = 0;

	/* Read the little-endian value byte by byte */
	for (uint8_t shift = 0; shift < sizeof(value) * CHAR_BIT; shift += 8) {
		int byte = getc(file);

		if (byte == EOF)
			return INT64_MAX;
		/* This must be casted to `unsigned`, not `uint8_t`. Rationale:
		 * the type of the shift is the type of `byte` after undergoing
		 * integer promotion, which would be `int` if this was casted to
		 * `uint8_t`, because int is large enough to hold a byte. This
		 * however causes values larger than 127 to be too large when
		 * shifted, potentially triggering undefined behavior.
		 */
		value |= (unsigned int)byte << shift;
	}
	return value;
}

/**
 * Helper macro for reading longs from a file, and errors out if it fails to.
 * Not as a function to avoid overhead in the general case.
 * TODO: maybe mark the condition as `unlikely`; how to do that portably?
 * @param var The variable to stash the number into
 * @param file The file to read from. Its position will be advanced
 * @param ... A format string and related arguments; note that an extra string
 *            argument is provided, the reason for failure
 */
#define tryReadlong(var, file, ...) \
	tryRead(readlong, int64_t, INT64_MAX, var, file, __VA_ARGS__)

/* There is no `readbyte`, just use `fgetc` or `getc`. */

/**
 * Helper macro for reading bytes from a file, and errors out if it fails to.
 * Differs from `tryGetc` in that the backing function is fgetc(1).
 * Not as a function to avoid overhead in the general case.
 * TODO: maybe mark the condition as `unlikely`; how to do that portably?
 * @param var The variable to stash the number into
 * @param file The file to read from. Its position will be advanced
 * @param ... A format string and related arguments; note that an extra string
 *            argument is provided, the reason for failure
 */
#define tryFgetc(var, file, ...) \
	tryRead(fgetc, int, EOF, var, file, __VA_ARGS__)

/**
 * Helper macro for reading bytes from a file, and errors out if it fails to.
 * Differs from `tryGetc` in that the backing function is fgetc(1).
 * Not as a function to avoid overhead in the general case.
 * TODO: maybe mark the condition as `unlikely`; how to do that portably?
 * @param var The variable to stash the number into
 * @param file The file to read from. Its position will be advanced
 * @param ... A format string and related arguments; note that an extra string
 *            argument is provided, the reason for failure
 */
#define tryGetc(var, file, ...) \
	tryRead(getc, int, EOF, var, file, __VA_ARGS__)

/**
 * Reads a '\0'-terminated string from a file.
 * @param file The file to read from. The file position will be advanced.
 * @return The string read, or NULL on failure.
 *         If a non-NULL pointer is returned, make sure to `free` it when done!
 */
static char *readstr(FILE *file)
{
	/* Default buffer size, have it close to the average string length */
	size_t capacity = 32 / 2;
	size_t index = -1;
	/* Force the first iteration to allocate */
	char *str = NULL;

	do {
		/* Prepare going to next char */
		index++;

		/* If the buffer isn't suitable to write the next char... */
		if (index >= capacity || !str) {
			capacity *= 2;
			str = realloc(str, capacity);
			/* End now in case of error */
			if (!str)
				return NULL;
		}

		/* Read char */
		int byte = getc(file);

		if (byte == EOF)
			return NULL;
		str[index] = byte;
	} while (str[index]);
	return str;
}

/**
 * Helper macro for reading bytes from a file, and errors out if it fails to.
 * Not as a function to avoid overhead in the general case.
 * TODO: maybe mark the condition as `unlikely`; how to do that portably?
 * @param var The variable to stash the string into
 * @param file The file to read from. Its position will be advanced
 * @param ... A format string and related arguments; note that an extra string
 *            argument is provided, the reason for failure
 */
#define tryReadstr(var, file, ...) \
	tryRead(readstr, char*, NULL, var, file, __VA_ARGS__)

/***** Functions to parse object files *****/

/**
 * Reads a file stack node form a file.
 * @param file The file to read from
 * @param nodes The file's array of nodes
 * @param i The ID of the node in the array
 * @param fileName The filename to report in errors
 */
static void readFileStackNode(FILE *file, struct FileStackNode fileNodes[], uint32_t i,
			      char const *fileName)
{
	uint32_t parentID;

	tryReadlong(parentID, file,
		    "%s: Cannot read node #%" PRIu32 "'s parent ID: %s", fileName, i);
	fileNodes[i].parent = parentID == -1 ? NULL : &fileNodes[parentID];
	tryReadlong(fileNodes[i].lineNo, file,
		    "%s: Cannot read node #%" PRIu32 "'s line number: %s", fileName, i);
	tryGetc(fileNodes[i].type, file, "%s: Cannot read node #%" PRIu32 "'s type: %s",
		fileName, i);
	switch (fileNodes[i].type) {
	case NODE_FILE:
	case NODE_MACRO:
		tryReadstr(fileNodes[i].name, file,
			   "%s: Cannot read node #%" PRIu32 "'s file name: %s", fileName, i);
		break;

	case NODE_REPT:
		tryReadlong(fileNodes[i].reptDepth, file,
			    "%s: Cannot read node #%" PRIu32 "'s rept depth: %s", fileName, i);
		fileNodes[i].iters = malloc(sizeof(*fileNodes[i].iters) * fileNodes[i].reptDepth);
		if (!fileNodes[i].iters)
			fatal(NULL, 0, "%s: Failed to alloc node #%" PRIu32 "'s iters: %s",
			      fileName, i, strerror(errno));
		for (uint32_t k = 0; k < fileNodes[i].reptDepth; k++)
			tryReadlong(fileNodes[i].iters[k], file,
				    "%s: Cannot read node #%" PRIu32 "'s iter #%" PRIu32 ": %s",
				    fileName, i, k);
		if (!fileNodes[i].parent)
			fatal(NULL, 0, "%s is not a valid object file: root node (#%"
			      PRIu32 ") may not be REPT", fileName, i);
	}
}

/**
 * Reads a symbol from a file.
 * @param file The file to read from
 * @param symbol The struct to fill
 * @param fileName The filename to report in errors
 */
static void readSymbol(FILE *file, struct Symbol *symbol,
		       char const *fileName, struct FileStackNode fileNodes[])
{
	tryReadstr(symbol->name, file, "%s: Cannot read symbol name: %s",
		   fileName);
	tryGetc(symbol->type, file, "%s: Cannot read \"%s\"'s type: %s",
		fileName, symbol->name);
	/* If the symbol is defined in this file, read its definition */
	if (symbol->type != SYMTYPE_IMPORT) {
		symbol->objFileName = fileName;
		uint32_t nodeID;

		tryReadlong(nodeID, file,
			   "%s: Cannot read \"%s\"'s node ID: %s",
			   fileName, symbol->name);
		symbol->src = &fileNodes[nodeID];
		tryReadlong(symbol->lineNo, file,
			    "%s: Cannot read \"%s\"'s line number: %s",
			    fileName, symbol->name);
		tryReadlong(symbol->sectionID, file,
			    "%s: Cannot read \"%s\"'s section ID: %s",
			    fileName, symbol->name);
		tryReadlong(symbol->offset, file,
			    "%s: Cannot read \"%s\"'s value: %s",
			    fileName, symbol->name);
	} else {
		symbol->sectionID = -1;
	}
}

/**
 * Reads a patch from a file.
 * @param file The file to read from
 * @param patch The struct to fill
 * @param fileName The filename to report in errors
 * @param i The number of the patch to report in errors
 */
static void readPatch(FILE *file, struct Patch *patch, char const *fileName,
		      char const *sectName, uint32_t i,
		      struct Section *fileSections[], struct FileStackNode fileNodes[])
{
	uint32_t nodeID;

	tryReadlong(nodeID, file,
		   "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s node ID: %s",
		   fileName, sectName, i);
	patch->src = &fileNodes[nodeID];
	tryReadlong(patch->lineNo, file,
		    "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s line number: %s",
		    fileName, sectName, i);
	tryReadlong(patch->offset, file,
		    "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s offset: %s",
		    fileName, sectName, i);
	tryReadlong(patch->pcSectionID, file,
		    "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s PC offset: %s",
		    fileName, sectName, i);
	patch->pcSection = patch->pcSectionID == -1 ? NULL
						    : fileSections[patch->pcSectionID];
	tryReadlong(patch->pcOffset, file,
		    "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s PC offset: %s",
		    fileName, sectName, i);
	tryGetc(patch->type, file,
		"%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s type: %s",
		fileName, sectName, i);
	tryReadlong(patch->rpnSize, file,
		    "%s: Unable to read \"%s\"'s patch #%" PRIu32 "'s RPN size: %s",
		    fileName, sectName, i);

	patch->rpnExpression = malloc(sizeof(*patch->rpnExpression) * patch->rpnSize);
	if (!patch->rpnExpression)
		err(1, "%s: Failed to alloc \"%s\"'s patch #%" PRIu32 "'s RPN expression",
		    fileName, sectName, i);
	size_t nbElementsRead = fread(patch->rpnExpression, sizeof(*patch->rpnExpression),
				      patch->rpnSize, file);

	if (nbElementsRead != patch->rpnSize)
		errx(1, "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s RPN expression: %s",
		     fileName, sectName, i,
		     feof(file) ? "Unexpected end of file" : strerror(errno));
}

/**
 * Reads a section from a file.
 * @param file The file to read from
 * @param section The struct to fill
 * @param fileName The filename to report in errors
 */
static void readSection(FILE *file, struct Section *section, char const *fileName,
			struct Section *fileSections[], struct FileStackNode fileNodes[])
{
	int32_t tmp;
	uint8_t byte;

	tryReadstr(section->name, file, "%s: Cannot read section name: %s", fileName);
	tryReadlong(tmp, file, "%s: Cannot read \"%s\"'s' size: %s", fileName, section->name);
	if (tmp < 0 || tmp > UINT16_MAX)
		errx(1, "\"%s\"'s section size (%" PRId32 ") is invalid", section->name, tmp);
	section->size = tmp;
	section->offset = 0;
	tryGetc(byte, file, "%s: Cannot read \"%s\"'s type: %s", fileName, section->name);
	section->type = byte & 0x3F;
	if (byte >> 7)
		section->modifier = SECTION_UNION;
	else if (byte >> 6)
		section->modifier = SECTION_FRAGMENT;
	else
		section->modifier = SECTION_NORMAL;
	tryReadlong(tmp, file, "%s: Cannot read \"%s\"'s org: %s", fileName, section->name);
	section->isAddressFixed = tmp >= 0;
	if (tmp > UINT16_MAX) {
		error(NULL, 0, "\"%s\"'s org is too large (%" PRId32 ")", section->name, tmp);
		tmp = UINT16_MAX;
	}
	section->org = tmp;
	tryReadlong(tmp, file, "%s: Cannot read \"%s\"'s bank: %s", fileName, section->name);
	section->isBankFixed = tmp >= 0;
	section->bank = tmp;
	tryGetc(byte, file, "%s: Cannot read \"%s\"'s alignment: %s", fileName, section->name);
	section->isAlignFixed = byte != 0;
	section->alignMask = (1 << byte) - 1;
	tryReadlong(tmp, file, "%s: Cannot read \"%s\"'s alignment offset: %s",
		    fileName, section->name);
	if (tmp > UINT16_MAX) {
		error(NULL, 0, "\"%s\"'s alignment offset is too large (%" PRId32 ")",
		      section->name, tmp);
		tmp = UINT16_MAX;
	}
	section->alignOfs = tmp;

	if (sect_HasData(section->type)) {
		/* Ensure we never allocate 0 bytes */
		uint8_t *data = malloc(sizeof(*data) * section->size + 1);

		if (!data)
			err(1, "%s: Unable to read \"%s\"'s data", fileName, section->name);
		if (section->size) {
			size_t nbElementsRead = fread(data, sizeof(*data), section->size, file);
			if (nbElementsRead != section->size)
				errx(1, "%s: Cannot read \"%s\"'s data: %s",
				     fileName, section->name,
				     feof(file) ? "Unexpected end of file" : strerror(errno));
		}
		section->data = data;

		tryReadlong(section->nbPatches, file,
			    "%s: Cannot read \"%s\"'s number of patches: %s",
			    fileName, section->name);

		struct Patch *patches = malloc(sizeof(*patches) * section->nbPatches + 1);

		if (!patches)
			err(1, "%s: Unable to read \"%s\"'s patches", fileName, section->name);
		for (uint32_t i = 0; i < section->nbPatches; i++) {
			readPatch(file, &patches[i], fileName, section->name,
				  i, fileSections, fileNodes);
		}
		section->patches = patches;
	}

	section->smartLinked = false;
}

/**
 * Links a symbol to a section, keeping the section's symbol list sorted.
 * @param symbol The symbol to link
 * @param section The section to link
 */
static void linkSymToSect(struct Symbol const *symbol, struct Section *section)
{
	uint32_t a = 0, b = section->nbSymbols;

	while (a != b) {
		uint32_t c = (a + b) / 2;

		if (section->symbols[c]->offset > symbol->offset)
			b = c;
		else
			a = c + 1;
	}

	struct Symbol const *tmp = symbol;

	for (uint32_t i = a; i <= section->nbSymbols; i++) {
		symbol = tmp;
		tmp = section->symbols[i];
		section->symbols[i] = symbol;
	}

	section->nbSymbols++;
}

/**
 * Reads an assertion from a file
 * @param file The file to read from
 * @param assert The struct to fill
 * @param fileName The filename to report in errors
 */
static void readAssertion(FILE *file, struct Assertion *assert,
			  char const *fileName, uint32_t i,
			  struct Section *fileSections[], struct FileStackNode fileNodes[])
{
	char assertName[sizeof("Assertion #" EXPAND_AND_STR(UINT32_MAX))];

	snprintf(assertName, sizeof(assertName), "Assertion #%" PRIu32, i);

	readPatch(file, &assert->patch, fileName, assertName, 0, fileSections, fileNodes);
	tryReadstr(assert->message, file, "%s: Cannot read assertion's message: %s",
		   fileName);
}

static inline struct Section *getMainSection(struct Section *section)
{
	if (section->modifier != SECTION_NORMAL)
		section = sect_GetSection(section->name);

	return section;
}

void obj_ReadFile(char const *fileName, unsigned int fileID)
{
	FILE *file = strcmp("-", fileName) ? fopen(fileName, "rb") : stdin;

	if (!file)
		err(1, "Could not open file %s", fileName);

	/* Begin by reading the magic bytes and version number */
	unsigned versionNumber;
	int matchedElems = fscanf(file, RGBDS_OBJECT_VERSION_STRING,
				  &versionNumber);

	if (matchedElems != 1)
		errx(1, "\"%s\" is not a RGBDS object file", fileName);

	verbosePrint("Reading object file %s, version %u\n",
		     fileName, versionNumber);

	if (versionNumber != RGBDS_OBJECT_VERSION_NUMBER)
		errx(1, "\"%s\" is an incompatible version %u object file",
		     fileName, versionNumber);

	uint32_t revNum;

	tryReadlong(revNum, file, "%s: Cannot read revision number: %s",
		    fileName);
	if (revNum != RGBDS_OBJECT_REV)
		errx(1, "%s is a revision 0x%04" PRIx32 " object file; only 0x%04x is supported",
		     fileName, revNum, RGBDS_OBJECT_REV);

	uint32_t nbSymbols;
	uint32_t nbSections;

	tryReadlong(nbSymbols, file, "%s: Cannot read number of symbols: %s",
		    fileName);
	tryReadlong(nbSections, file, "%s: Cannot read number of sections: %s",
		    fileName);

	nbSectionsToAssign += nbSections;

	tryReadlong(nodes[fileID].nbNodes, file, "%s: Cannot read number of nodes: %s", fileName);
	nodes[fileID].nodes = calloc(nodes[fileID].nbNodes, sizeof(nodes[fileID].nodes[0]));
	if (!nodes[fileID].nodes)
		err(1, "Failed to get memory for %s's nodes", fileName);
	verbosePrint("Reading %u nodes...\n", nodes[fileID].nbNodes);
	for (uint32_t i = nodes[fileID].nbNodes; i--; )
		readFileStackNode(file, nodes[fileID].nodes, i, fileName);

	/* This file's symbols, kept to link sections to them */
	struct Symbol **fileSymbols =
		malloc(sizeof(*fileSymbols) * nbSymbols + 1);

	if (!fileSymbols)
		err(1, "Failed to get memory for %s's symbols", fileName);

	struct SymbolList *symbolList = malloc(sizeof(*symbolList));

	if (!symbolList)
		err(1, "Failed to register %s's symbol list", fileName);
	symbolList->symbolList = fileSymbols;
	symbolList->nbSymbols = nbSymbols;
	symbolList->next = symbolLists;
	symbolLists = symbolList;

	uint32_t *nbSymPerSect = calloc(nbSections ? nbSections : 1,
					sizeof(*nbSymPerSect));

	verbosePrint("Reading %" PRIu32 " symbols...\n", nbSymbols);
	for (uint32_t i = 0; i < nbSymbols; i++) {
		/* Read symbol */
		struct Symbol *symbol = malloc(sizeof(*symbol));

		if (!symbol)
			err(1, "%s: Couldn't create new symbol", fileName);
		readSymbol(file, symbol, fileName, nodes[fileID].nodes);

		fileSymbols[i] = symbol;
		if (symbol->type == SYMTYPE_EXPORT)
			sym_AddSymbol(symbol);
		if (symbol->sectionID != -1)
			nbSymPerSect[symbol->sectionID]++;
	}

	/* This file's sections, stored in a table to link symbols to them */
	struct Section **fileSections = malloc(sizeof(*fileSections)
					    * (nbSections ? nbSections : 1));

	verbosePrint("Reading %" PRIu32 " sections...\n", nbSections);
	for (uint32_t i = 0; i < nbSections; i++) {
		/* Read section */
		fileSections[i] = malloc(sizeof(*fileSections[i]));
		if (!fileSections[i])
			err(1, "%s: Couldn't create new section", fileName);

		fileSections[i]->nextu = NULL;
		readSection(file, fileSections[i], fileName, fileSections, nodes[fileID].nodes);
		fileSections[i]->fileSymbols = fileSymbols;
		if (nbSymPerSect[i]) {
			fileSections[i]->symbols = malloc(nbSymPerSect[i]
					* sizeof(*fileSections[i]->symbols));
			if (!fileSections[i]->symbols)
				err(1, "%s: Couldn't link to symbols",
				    fileName);
		} else {
			fileSections[i]->symbols = NULL;
		}
		fileSections[i]->nbSymbols = 0;

		sect_AddSection(fileSections[i]);
	}

	free(nbSymPerSect);

	/* Give symbols pointers to their sections */
	for (uint32_t i = 0; i < nbSymbols; i++) {
		int32_t sectionID = fileSymbols[i]->sectionID;

		if (sectionID == -1) {
			fileSymbols[i]->section = NULL;
		} else {
			struct Section *section = fileSections[sectionID];

			/* Give the section a pointer to the symbol as well */
			linkSymToSect(fileSymbols[i], section);

			if (section->modifier != SECTION_NORMAL) {
				if (section->modifier == SECTION_FRAGMENT)
					/* Add the fragment's offset to the symbol's */
					fileSymbols[i]->offset += section->offset;
				section = getMainSection(section);
			}
			fileSymbols[i]->section = section;
		}
	}

	uint32_t nbAsserts;

	tryReadlong(nbAsserts, file, "%s: Cannot read number of assertions: %s",
		    fileName);
	verbosePrint("Reading %" PRIu32 " assertions...\n", nbAsserts);
	for (uint32_t i = 0; i < nbAsserts; i++) {
		struct Assertion *assertion = malloc(sizeof(*assertion));

		if (!assertion)
			err(1, "%s: Couldn't create new assertion", fileName);
		readAssertion(file, assertion, fileName, i, fileSections, nodes[fileID].nodes);
		assertion->fileSymbols = fileSymbols;
		assertion->next = assertions;
		assertions = assertion;
	}

	free(fileSections);
	fclose(file);
}

void obj_DoSanityChecks(void)
{
	sect_DoSanityChecks();
}

struct Assertion const *obj_GetFirstAssertion(void)
{
	return assertions;
}

void obj_CheckAssertions(void)
{
	patch_CheckAssertions(assertions);
}

void obj_Setup(unsigned int nbFiles)
{
	nbObjFiles = nbFiles;

	if (nbFiles > SIZE_MAX / sizeof(*nodes))
		fatal(NULL, 0, "Impossible to link more than %zu files!", SIZE_MAX / sizeof(*nodes));
	nodes = malloc(sizeof(*nodes) * nbFiles);
}

void obj_Cleanup(void)
{
	for (unsigned int i = 0; i < nbObjFiles; i++) {
		for (uint32_t j = 0; j < nodes[i].nbNodes; j++) {
			if (nodes[i].nodes[j].type == NODE_REPT)
				free(nodes[i].nodes[j].iters);
		}
		free(nodes[i].nodes);
	}
	free(nodes);

	struct SymbolList *list = symbolLists;

	while (list) {
		for (size_t i = 0; i < list->nbSymbols; i++)
			sym_FreeSymbol(list->symbolList[i]);
		free(list->symbolList);

		struct SymbolList *next = list->next;

		free(list);
		list = next;
	}
}
