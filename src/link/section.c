/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "link/main.h"
#include "link/section.h"

#include "extern/err.h"

#include "hashmap.h"

HashMap sections;

struct ForEachArg {
	void (*callback)(struct Section *section, void *arg);
	void *arg;
};

static void forEach(void *section, void *arg)
{
	struct ForEachArg *callbackArg = (struct ForEachArg *)arg;

	callbackArg->callback((struct Section *)section, callbackArg->arg);
}

void sect_ForEach(void (*callback)(struct Section *, void *), void *arg)
{
	struct ForEachArg callbackArg = { .callback = callback, .arg = arg};

	hash_ForEach(sections, forEach, &callbackArg);
}

void sect_AddSection(struct Section *section)
{
	/* Check if the section already exists */
	if (hash_GetElement(sections, section->name))
		errx(1, "Section name \"%s\" is already in use", section->name);

	/* If not, add it */
	bool collided = hash_AddElement(sections, section->name, section);

	if (beVerbose && collided)
		warnx("Section hashmap collision occurred!");
}

struct Section *sect_GetSection(char const *name)
{
	return (struct Section *)hash_GetElement(sections, name);
}

void sect_CleanupSections(void)
{
	hash_EmptyMap(sections);
}

static bool sanityChecksFailed;

static void doSanityChecks(struct Section *section, void *ptr)
{
	(void)ptr;
#define fail(...) do { \
	warnx(__VA_ARGS__); \
	sanityChecksFailed = true; \
} while (0)

	/* Sanity check the section's type */

	if (section->type < 0 || section->type >= SECTTYPE_INVALID)
		fail("Section \"%s\" has an invalid type.", section->name);
	if (is32kMode && section->type == SECTTYPE_ROMX) {
		if (section->isBankFixed && section->bank != 1)
			fail("%s: ROMX sections must be in bank 1 with option -t.",
			     section->name);
		else
			section->type = SECTTYPE_ROM0;
	}
	if (isWRA0Mode && section->type == SECTTYPE_WRAMX) {
		if (section->isBankFixed && section->bank != 1)
			fail("%s: WRAMX sections must be in bank 1 with options -w or -d.",
			     section->name);
		else
			section->type = SECTTYPE_WRAMX;
	}
	if (isDmgMode && section->type == SECTTYPE_VRAM && section->bank == 1)
		fail("%s: VRAM bank 1 can't be used with option -d.",
		     section->name);

	/*
	 * Check if alignment is reasonable, this is important to avoid UB
	 * An alignment of zero is equivalent to no alignment, basically
	 */
	if (section->isAlignFixed && section->alignMask == 1)
		section->isAlignFixed = false;

	uint32_t minbank = bankranges[section->type][0],
		 maxbank = bankranges[section->type][1];

	if (section->isBankFixed && section->bank < minbank
				 && section->bank > maxbank)
		fail(minbank == maxbank
			? "Cannot place section \"%s\" in bank %d, it must be %d"
			: "Cannot place section \"%s\" in bank %d, it must be between %d and %d",
		     section->name, section->bank, minbank, maxbank);

	/* Check if section has a chance to be placed */
	if (section->size > maxsize[section->type])
		fail("Section \"%s\" is bigger than the max size for that type: %#x > %#x",
		     section->size, maxsize[section->type]);

	/* Translate loose constraints to strong ones when they're equivalent */

	if (minbank == maxbank) {
		section->bank = minbank;
		section->isBankFixed = true;
	}

	if (section->isAlignFixed) {
		enum SectionType type = section->type;

		/* It doesn't make sense to have both org and alignment set */
		if (section->isAddressFixed) {
			if (section->org & section->alignMask)
				fail("Section \"%s\"'s fixed address doesn't match its alignment",
				     section->name);
			section->isAlignFixed = false;
		} else if ((endaddr(type) & section->alignMask)
			   == startaddr[type]) {
			section->org = startaddr[type];
			section->isAlignFixed = false;
			section->isAddressFixed = true;
		}
	}

	if (section->isAddressFixed) {
		/* Ensure the target address is valid */
		if (section->org < startaddr[section->type]
		 || section->org > endaddr(section->type))
			fail("Section \"%s\"'s fixed address %#x is outside of range [%#x; %#x]",
			     section->name, section->org,
			     startaddr[section->type], endaddr(section->type));

		if (section->org + section->size > endaddr(section->type) + 1)
			fail("Section \"%s\"'s end address %#x is greater than last address %#x",
			     section->name, section->org + section->size,
			     endaddr(section->type) + 1);
	}

#undef fail
}

void sect_DoSanityChecks(void)
{
	sect_ForEach(doSanityChecks, NULL);
	if (sanityChecksFailed)
		errx(1, "Sanity checks failed");
}
