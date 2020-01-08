/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2010-2018, Anthony J. Bentley and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "extern/getopt.h"

#include "version.h"

/* Short options */
static char const *optstring = "Ccf:i:jk:l:m:n:p:r:st:Vv";

/*
 * Equivalent long options
 * Please keep in the same order as short opts
 *
 * Also, make sure long opts don't create ambiguity:
 * A long opt's name should start with the same letter as its short opt,
 * except if it doesn't create any ambiguity (`verbose` versus `version`).
 * This is because long opt matching, even to a single char, is prioritized
 * over short opt matching
 */
static struct option const longopts[] = {
	{ "color-only",       no_argument,       NULL, 'C' },
	{ "color-compatible", no_argument,       NULL, 'c' },
	{ "fix-spec",         required_argument, NULL, 'f' },
	{ "game-id",          required_argument, NULL, 'i' },
	{ "non-japanese",     no_argument,       NULL, 'j' },
	{ "new-licensee",     required_argument, NULL, 'k' },
	{ "old-licensee",     required_argument, NULL, 'l' },
	{ "mbc-type",         required_argument, NULL, 'm' },
	{ "rom-version",      required_argument, NULL, 'n' },
	{ "pad-value",        required_argument, NULL, 'p' },
	{ "ram-size",         required_argument, NULL, 'r' },
	{ "sgb-compatible",   no_argument,       NULL, 's' },
	{ "title",            required_argument, NULL, 't' },
	{ "version",          no_argument,       NULL, 'V' },
	{ "validate",         no_argument,       NULL, 'v' },
	{ NULL,               no_argument,       NULL, 0   }
};

static void print_usage(void)
{
	fputs(
"Usage: rgbfix [-jsVv] [-C | -c] [-f <fix_spec>] [-i <game_id>] [-k <licensee>]\n"
"              [-l <licensee_byte>] [-m <mbc_type>] [-n <rom_version>]\n"
"              [-p <pad_value>] [-r <ram_size>] [-t <title_str>] <file>\n"
"Useful options:\n"
"    -m, --mbc-type <value>      set the MBC type byte to this value; refer\n"
"                                  to the man page for a list of values\n"
"    -p, --pad-value <value>     pad to the next valid size using this value\n"
"    -r, --ram-size <code>       set the cart RAM size byte to this value\n"
"    -V, --version               print RGBFIX version and exit\n"
"    -v, --validate              fix the header logo and both checksums (-f lhg)\n"
"\n"
"For help, use `man rgbfix' or go to https://rednex.github.io/rgbds/\n",
	      stderr);
	exit(1);
}

int main(int argc, char *argv[])
{
	FILE *rom;
	int ch;
	char *ep;

	/*
	 * Parse command-line options
	 */

	/* all flags default to false unless options specify otherwise */
	bool fixlogo = false;
	bool fixheadsum = false;
	bool fixglobalsum = false;
	bool trashlogo = false;
	bool trashheadsum = false;
	bool trashglobalsum = false;
	bool settitle = false;
	bool setid = false;
	bool colorcompatible = false;
	bool coloronly = false;
	bool nonjapan = false;
	bool setlicensee = false;
	bool setnewlicensee = false;
	bool super = false;
	bool setcartridge = false;
	bool setramsize = false;
	bool resize = false;
	bool setversion = false;

	char *title; /* game title in ASCII */
	char *id; /* game ID in ASCII */
	char *newlicensee; /* new licensee ID, two ASCII characters */

	int licensee = 0;  /* old licensee ID */
	int cartridge = 0; /* cartridge hardware ID */
	int ramsize = 0;   /* RAM size ID */
	int version = 0;   /* mask ROM version number */
	int padvalue = 0;  /* to pad the rom with if it changes size */

	while ((ch = musl_getopt_long_only(argc, argv, optstring, longopts,
					   NULL)) != -1) {
		switch (ch) {
		case 'C':
			coloronly = true;
			/* FALLTHROUGH */
		case 'c':
			colorcompatible = true;
			break;
		case 'f':
			fixlogo = strchr(optarg, 'l');
			fixheadsum = strchr(optarg, 'h');
			fixglobalsum = strchr(optarg, 'g');
			trashlogo = strchr(optarg, 'L');
			trashheadsum = strchr(optarg, 'H');
			trashglobalsum = strchr(optarg, 'G');
			break;
		case 'i':
			setid = true;

			if (strlen(optarg) != 4)
				errx(1, "Game ID %s must be exactly 4 characters",
				     optarg);

			id = optarg;
			break;
		case 'j':
			nonjapan = true;
			break;
		case 'k':
			setnewlicensee = true;

			if (strlen(optarg) != 2)
				errx(1, "New licensee code %s is not the correct length of 2 characters",
				     optarg);

			newlicensee = optarg;
			break;
		case 'l':
			setlicensee = true;

			licensee = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'l'");

			if (licensee < 0 || licensee > 0xFF)
				errx(1, "Argument for option 'l' must be between 0 and 255");

			break;
		case 'm':
			setcartridge = true;

			cartridge = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'm'");

			if (cartridge < 0 || cartridge > 0xFF)
				errx(1, "Argument for option 'm' must be between 0 and 255");

			break;
		case 'n':
			setversion = true;

			version = strtoul(optarg, &ep, 0);

			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'n'");

			if (version < 0 || version > 0xFF)
				errx(1, "Argument for option 'n' must be between 0 and 255");

			break;
		case 'p':
			resize = true;

			padvalue = strtoul(optarg, &ep, 0);

			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'p'");

			if (padvalue < 0 || padvalue > 0xFF)
				errx(1, "Argument for option 'p' must be between 0 and 255");

			break;
		case 'r':
			setramsize = true;

			ramsize = strtoul(optarg, &ep, 0);

			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'r'");

			if (ramsize < 0 || ramsize > 0xFF)
				errx(1, "Argument for option 'r' must be between 0 and 255");

			break;
		case 's':
			super = true;
			break;
		case 't':
			settitle = true;

			if (strlen(optarg) > 16)
				errx(1, "Title \"%s\" is greater than the maximum of 16 characters",
				     optarg);

			if (strlen(optarg) == 16)
				warnx("Title \"%s\" is 16 chars, it is best to keep it to 15 or fewer",
				      optarg);

			title = optarg;
			break;
		case 'V':
			printf("rgbfix %s\n", get_package_version_string());
			exit(0);
		case 'v':
			fixlogo = true;
			fixheadsum = true;
			fixglobalsum = true;
			break;
		default:
			print_usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fputs("FATAL: no input files\n", stderr);
		print_usage();
	}

	/*
	 * Open the ROM file
	 */

	rom = fopen(argv[argc - 1], "rb+");

	if (rom == NULL)
		err(1, "Error opening file %s", argv[argc - 1]);

	/*
	 * Read ROM header
	 *
	 * Offsets in the buffer are 0x100 less than the equivalent in ROM.
	 */

	uint8_t header[0x50];

	if (fseek(rom, 0x100, SEEK_SET) != 0)
		err(1, "Could not locate ROM header");
	if (fread(header, sizeof(uint8_t), sizeof(header), rom)
	    != sizeof(header))
		err(1, "Could not read ROM header");

	if (fixlogo || trashlogo) {
		/*
		 * Offset 0x104–0x133: Nintendo Logo
		 * This is a bitmap image that displays when the Game Boy is
		 * turned on. It must be intact, or the game will not boot.
		 */

		/*
		 * See also: global checksums at 0x14D–0x14F, They must
		 * also be correct for the game to boot, so we fix them
		 * as well when requested with the -f flag.
		 */

		uint8_t ninlogo[48] = {
			0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
			0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
			0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
			0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
			0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
			0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
		};

		if (trashlogo) {
			for (int i = 0; i < sizeof(ninlogo); i++)
				ninlogo[i] = ~ninlogo[i];
		}

		memcpy(header + 0x04, ninlogo, sizeof(ninlogo));
	}

	if (settitle) {
		/*
		 * Offset 0x134–0x143: Game Title
		 * This is a sixteen-character game title in ASCII (no high-
		 * bit characters).
		 */

		/*
		 * See also: CGB flag at 0x143. The sixteenth character of
		 * the title is co-opted for use as the CGB flag, so they
		 * may conflict.
		 */

		/*
		 * See also: Game ID at 0x13F–0x142. These four ASCII
		 * characters may conflict with the title.
		 */

		strncpy((char *)header + 0x34, title, 16);
	}

	if (setid) {
		/*
		 * Offset 0x13F–0x142: Game ID
		 * This is a four-character game ID in ASCII (no high-bit
		 * characters).
		 */

		memcpy(header + 0x3F, id, 4);
	}

	if (colorcompatible) {
		/*
		 * Offset 0x143: Game Boy Color Flag
		 * If bit 7 is set, the ROM has Game Boy Color features.
		 * If bit 6 is also set, the ROM is for the Game Boy Color
		 * only. (However, this is not actually enforced by the
		 * Game Boy.)
		 */

		/*
		 * See also: Game Title at 0x134–0x143. The sixteenth
		 * character of the title overlaps with this flag, so they
		 * may conflict.
		 */

		header[0x43] |= 1 << 7;
		if (coloronly)
			header[0x43] |= 1 << 6;

		if (header[0x43] & 0x3F)
			warnx("Color flag conflicts with game title");
	}

	if (setnewlicensee) {
		/*
		 * Offset 0x144–0x145: New Licensee Code
		 * This is a two-character code identifying which company
		 * created the game.
		 */

		/*
		 * See also: the original Licensee ID at 0x14B.
		 * This is deprecated and in all newer games is used instead
		 * as a Super Game Boy flag.
		 */

		header[0x44] = newlicensee[0];
		header[0x45] = newlicensee[1];
	}

	if (super) {
		/*
		 * Offset 0x146: Super Game Boy Flag
		 * If not equal to 3, Super Game Boy functions will be
		 * disabled.
		 */

		/*
		 * See also: the original Licensee ID at 0x14B.
		 * If the Licensee code is not equal to 0x33, Super Game Boy
		 * functions will be disabled.
		 */

		if (!setlicensee)
			warnx("You should probably set both '-s' and '-l 0x33'");

		header[0x46] = 3;
	}

	if (setcartridge) {
		/*
		 * Offset 0x147: Cartridge Type
		 * Identifies whether the ROM uses a memory bank controller,
		 * external RAM, timer, rumble, or battery.
		 */

		header[0x47] = cartridge;
	}

	if (resize) {
		/*
		 * Offset 0x148: Cartridge Size
		 * Identifies the size of the cartridge ROM.
		 */

		/* We will pad the ROM to match the size given in the header. */
		long romsize, newsize;
		int headbyte;
		uint8_t *buf;

		if (fseek(rom, 0, SEEK_END) != 0)
			err(1, "Could not pad ROM file");

		romsize = ftell(rom);
		if (romsize == -1)
			err(1, "Could not pad ROM file");

		newsize = 0x8000;

		headbyte = 0;
		while (romsize > newsize) {
			newsize <<= 1;
			headbyte++;
		}

		if (newsize > 0x800000) /* ROM is bigger than 8MiB */
			warnx("ROM size is bigger than 8MiB");

		buf = malloc(newsize - romsize);
		if (buf == NULL)
			errx(1, "Couldn't allocate memory for padded ROM.");

		memset(buf, padvalue, newsize - romsize);
		if (fwrite(buf, 1, newsize - romsize, rom) != newsize - romsize)
			err(1, "Could not pad ROM file");

		header[0x48] = headbyte;

		free(buf);
	}

	if (setramsize) {
		/*
		 * Offset 0x149: RAM Size
		 */

		header[0x49] = ramsize;
	}

	if (nonjapan) {
		/*
		 * Offset 0x14A: Non-Japanese Region Flag
		 */

		header[0x4A] = 1;
	}

	if (setlicensee) {
		/*
		 * Offset 0x14B: Licensee Code
		 * This identifies which company created the game.
		 *
		 * This byte is deprecated and should be set to 0x33 in new
		 * releases.
		 */

		/*
		 * See also: the New Licensee ID at 0x144–0x145.
		 */

		header[0x4B] = licensee;
	}

	if (setversion) {
		/*
		 * Offset 0x14C: Mask ROM Version Number
		 * Which version of the ROM this is.
		 */

		header[0x4C] = version;
	}

	if (fixheadsum || trashheadsum) {
		/*
		 * Offset 0x14D: Header Checksum
		 */

		uint8_t headcksum = 0;

		for (int i = 0x34; i < 0x4D; ++i)
			headcksum = headcksum - header[i] - 1;

		if (trashheadsum)
			headcksum = ~headcksum;

		header[0x4D] = headcksum;
	}

	/*
	 * Before calculating the global checksum, we must write the modified
	 * header to the ROM.
	 */

	if (fseek(rom, 0x100, SEEK_SET) != 0)
		err(1, "Could not locate header for writing");

	if (fwrite(header, sizeof(uint8_t), sizeof(header), rom)
	    != sizeof(header))
		err(1, "Could not write modified ROM header");

	if (fixglobalsum || trashglobalsum) {
		/*
		 * Offset 0x14E–0x14F: Global Checksum
		 */

		uint16_t globalcksum = 0;

		if (fseek(rom, 0, SEEK_SET) != 0)
			err(1, "Could not start calculating global checksum");

		int i = 0;
		int byte;

		while ((byte = fgetc(rom)) != EOF) {
			if (i != 0x14E && i != 0x14F)
				globalcksum += byte;
			i++;
		}

		if (ferror(rom))
			err(1, "Could not calculate global checksum");

		if (trashglobalsum)
			globalcksum = ~globalcksum;

		fseek(rom, 0x14E, SEEK_SET);
		fputc(globalcksum >> 8, rom);
		fputc(globalcksum & 0xFF, rom);
		if (ferror(rom))
			err(1, "Could not write global checksum");
	}

	if (fclose(rom) != 0)
		err(1, "Could not complete ROM write");

	return 0;
}
