/*
 * Copyright © 2010 Anthony J. Bentley <anthonyjbentley@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern/err.h"
#include "extern/version.h"

static void
usage(void)
{
	printf(
"usage: rgbfix [-CcjsVv] [-i game_id] [-k licensee_str] [-l licensee_id]\n"
"              [-m mbc_type] [-n rom_version] [-p pad_value] [-r ram_size]\n"
"              [-t title_str] file\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	FILE *rom;
	int ch;
	char *ep;

	/*
	 * Parse command-line options
	 */

	/* all flags default to false unless options specify otherwise */
	bool validate = false;
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

	while ((ch = getopt(argc, argv, "Cci:jk:l:m:n:p:sr:t:Vv")) != -1) {
		switch (ch) {
		case 'C':
			coloronly = true;
			/* FALLTHROUGH */
		case 'c':
			colorcompatible = true;
			break;
		case 'i':
			setid = true;

			if (strlen(optarg) != 4) {
				errx(1, "Game ID %s must be exactly 4 "
				    "characters", optarg);
			}

			id = optarg;
			break;
		case 'j':
			nonjapan = true;
			break;
		case 'k':
			setnewlicensee = true;

			if (strlen(optarg) != 2) {
				errx(1, "New licensee code %s is not the "
				    "correct length of 2 characters", optarg);
			}

			newlicensee = optarg;
			break;
		case 'l':
			setlicensee = true;

			licensee = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'l'");
			}
			if (licensee < 0 || licensee > 0xFF) {
				errx(1, "Argument for option 'l' must be "
				    "between 0 and 255");
			}
			break;
		case 'm':
			setcartridge = true;

			cartridge = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'm'");
			}
			if (cartridge < 0 || cartridge > 0xFF) {
				errx(1, "Argument for option 'm' must be "
				    "between 0 and 255");
			}
			break;
		case 'n':
			setversion = true;

			version = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'n'");
			}
			if (version < 0 || version > 0xFF) {
				errx(1, "Argument for option 'n' must be "
				    "between 0 and 255");
			}
			break;
		case 'p':
			resize = true;

			padvalue = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'p'");
			}
			if (padvalue < 0 || padvalue > 0xFF) {
				errx(1, "Argument for option 'p' must be "
				    "between 0 and 255");
			}
			break;
		case 'r':
			setramsize = true;

			ramsize = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0') {
				errx(1, "Invalid argument for option 'r'");
			}
			if (ramsize < 0 || ramsize > 0xFF) {
				errx(1, "Argument for option 'r' must be "
				    "between 0 and 255");
			}
			break;
		case 's':
			super = true;
			break;
		case 't':
			settitle = true;

			if (strlen(optarg) > 16) {
				errx(1, "Title %s is greater than the "
				    "maximum of 16 characters", optarg);
			}

			if (strlen(optarg) == 16)
				warnx("Title %s is 16 chars, it is best to "
				    "keep it to 15 or fewer", optarg);

			title = optarg;
			break;
		case 'V':
			printf("rgbfix %s\n", get_package_version_string());
			exit(0);
		case 'v':
			validate = true;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	/*
	 * Open the ROM file
	 */

	if ((rom = fopen(argv[argc - 1], "rb+")) == NULL) {
		err(1, "Error opening file %s", argv[argc - 1]);
	}

	/*
	 * Write changes to ROM
	 */

	if (validate) {
		/*
		 * Offset 0x104–0x133: Nintendo Logo
		 * This is a bitmap image that displays when the Game Boy is
		 * turned on. It must be intact, or the game will not boot.
		 */

		/*
		 * See also: global checksums at 0x14D–0x14F, They must
		 * also be correct for the game to boot, so we fix them
		 * as well when the -v flag is set.
		 */

		uint8_t ninlogo[48] = {
			0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
			0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
			0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
			0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
			0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
			0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
		};

		fseek(rom, 0x104, SEEK_SET);
		fwrite(ninlogo, 1, 48, rom);
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

		fseek(rom, 0x134, SEEK_SET);
		fwrite(title, 1, strlen(title) + 1, rom);

		while (ftell(rom) < 0x143)
			fputc(0, rom);
	}

	if (setid) {
		/*
		 * Offset 0x13F–0x142: Game ID
		 * This is a four-character game ID in ASCII (no high-bit
		 * characters).
		 */

		fseek(rom,0x13F,SEEK_SET);
		fwrite(id, 1, 4, rom);
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

		uint8_t byte;

		fseek(rom, 0x143, SEEK_SET);
		byte = fgetc(rom);

		byte |= 1 << 7;
		if (coloronly)
			byte |= 1 << 6;

		if (byte & 0x3F)
			warnx("Color flag conflicts with game title");

		fseek(rom, 0x143, SEEK_SET);
		fputc(byte, rom);
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

		fseek(rom, 0x144, SEEK_SET);
		fwrite(newlicensee, 1, 2, rom);
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
			warnx("You should probably set both '-s' and "
			    "'-l 0x33'");

		fseek(rom, 0x146, SEEK_SET);
		fputc(3, rom);
	}

	if (setcartridge) {
		/*
		 * Offset 0x147: Cartridge Type
		 * Identifies whether the ROM uses a memory bank controller,
		 * external RAM, timer, rumble, or battery.
		 */

		fseek(rom, 0x147, SEEK_SET);
		fputc(cartridge, rom);
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
		fseek(rom, 0, SEEK_END);
		romsize = ftell(rom);
		newsize = 0x8000;

		headbyte = 0;
		while (romsize > newsize) {
			newsize <<= 1;
			headbyte++;
		}

		if (newsize > 0x800000) /* ROM is bigger than 8MiB */
			warnx("ROM size is bigger than 8MiB");

		buf = malloc(newsize - romsize);
		memset(buf, padvalue, newsize - romsize);
		fwrite(buf, 1, newsize - romsize, rom);

		fseek(rom, 0x148, SEEK_SET);
		fputc(headbyte, rom);

		free(buf);
	}

	if (setramsize) {
		/*
		 * Offset 0x149: RAM Size
		 */

		fseek(rom, 0x149, SEEK_SET);
		fputc(ramsize, rom);
	}

	if (nonjapan) {
		/*
		 * Offset 0x14A: Non-Japanese Region Flag
		 */

		fseek(rom, 0x14A, SEEK_SET);
		fputc(1, rom);
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

		fseek(rom, 0x14B, SEEK_SET);
		fputc(licensee, rom);
	}

	if (setversion) {
		/*
		 * Offset 0x14C: Mask ROM Version Number
		 * Which version of the ROM this is.
		 */

		fseek(rom, 0x14C, SEEK_SET);
		fputc(version, rom);
	}

	if (validate) {
		/*
		 * Offset 0x14D: Header Checksum
		 */

		uint8_t headcksum;

		headcksum = 0;
		fseek(rom, 0x134, SEEK_SET);
		for (int i = 0; i < (0x14D - 0x134); ++i)
			headcksum = headcksum - fgetc(rom) - 1;

		fseek(rom, 0x14D, SEEK_SET);
		fputc(headcksum, rom);

		/*
		 * Offset 0x14E–0x14F: Global Checksum
		 */

		uint16_t globalcksum;

		globalcksum = 0;

		rewind(rom);
		for (int i = 0; i < 0x14E; ++i)
			globalcksum += fgetc(rom);

		int byte;
		fseek(rom, 0x150, SEEK_SET);
		while ((byte = fgetc(rom)) != EOF)
			globalcksum += byte;

		fseek(rom, 0x14E, SEEK_SET);
		fputc(globalcksum >> 8, rom);
		fputc(globalcksum & 0xFF, rom);
	}

	fclose(rom);

	return 0;
}
