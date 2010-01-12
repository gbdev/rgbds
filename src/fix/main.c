/*
 * RGBFix : Perform various tasks on a Gameboy image-file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "asmotor.h"

/*
 * Option defines
 *
 */

#define OPTF_DEBUG	0x001L
#define OPTF_PAD	0x002L
#define OPTF_VALIDATE	0x004L
#define OPTF_TITLE	0x008L
#define OPTF_TRUNCATE	0x010L
#define OPTF_QUIET	0x020L
#define OPTF_RAMSIZE	0x040L
#define OPTF_MBCTYPE	0x080L
#define OPTF_GBCMODE	0x100L
#define OPTF_JAPAN	0x200L
#define OPTF_SGBMODE	0x400L
#define OPTF_NLICENSEE	0x800L

unsigned long ulOptions;

/*
 * Misc. variables
 *
 */

unsigned char NintendoChar[48] = {
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
	0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
	0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
	0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};
/*
 * Misc. routines
 *
 */

void 
PrintUsage(void)
{
	printf("RGBFix v" RGBFIX_VERSION
	    " (part of ASMotor " ASMOTOR_VERSION ")\n\n");

	printf("Usage: rgbfix [options] image[.gb]\n");
	printf("Options:\n");
	printf("  -h\t\tThis text\n");
	printf("  -d\t\tDebug: Don't change image\n");
	printf("  -m<hx>\tChange RAM size of cartridge\n");
	printf("  -j\t\tSet the non-Japanese region flag\n");
	printf("  -p[<hx>]\tPad image to valid size\n"
		"\t\tPads to 32/64/128/256/512kB as appropriate\n"
		"\t\tAn optional hexadecimal pad value can be supplied (default is 0)");
	printf("  -r\t\ttRuncate image to valid size\n\t\t\tTruncates to 32/64/128/256/512kB as appropriate\n");
	printf("  -t<name>\tChange cartridge title field (16 characters)\n");
	printf("  -k<code>\tChange licensee code (2 characters)\n");
	printf("  -v\t\tValidate header\n"
		"\t\tCorrects - Nintendo Character Area (0x0104)\n"
		"\t\t\t - ROM type (0x0147)\n"
		"\t\t\t - ROM size (0x0148)\n"
		"\t\t\t - Checksums (0x014D-0x014F)\n");
	printf("  -b<hx>\tSet MBC type\n");
	printf("  -c\t\tSet Game Boy Color compatible flag ([0x143] = 0x80)\n");
	printf("  -o\t\tSet Game Boy Color only flag       ([0x143] = 0xC0)\n");
	printf("  -s\t\tSet Super Game Boy flags\n");
	printf("  -q\t\tExecute quietly (suppress all text except errors)\n");

	exit(0);
}

void 
vFatalError(char *s, va_list ap)
{
	fprintf(stderr, "*ERROR* : ");
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
}

void 
FatalError(char *s,...)
{
	va_list ap;
	va_start(ap, s);
	vFatalError(s, ap);
	va_end(ap);
	exit(5);
}

void 
vWarning(char *s, va_list ap)
{
	fprintf(stderr, "*WARNING* : ");
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
}

void 
Warning(char *s,...)
{
	va_list ap;
	va_start(ap, s);
	vWarning(s, ap);
	va_end(ap);
}

long int 
FileSize(FILE * f)
{
	long prevpos;
	long r;

	fflush(f);
	prevpos = ftell(f);
	fseek(f, 0, SEEK_END);
	r = ftell(f);
	fseek(f, prevpos, SEEK_SET);
	return (r);
}

int 
FileExists(char *s)
{
	FILE *f;

	if ((f = fopen(s, "rb")) != NULL) {
		fclose(f);
		return (1);
	} else
		return (0);
}
/*
0147       Cartridge type:
           0 - ROM ONLY                12 - ROM+MBC3+RAM
           1 - ROM+MBC1                13 - ROM+MBC3+RAM+BATT
           2 - ROM+MBC1+RAM            19 - ROM+MBC5
           3 - ROM+MBC1+RAM+BATT       1A - ROM+MBC5+RAM
           5 - ROM+MBC2                1B - ROM+MBC5+RAM+BATT
           6 - ROM+MBC2+BATTERY        1C - ROM+MBC5+RUMBLE
           8 - ROM+RAM                 1D - ROM+MBC5+RUMBLE+SRAM
           9 - ROM+RAM+BATTERY         1E - ROM+MBC5+RUMBLE+SRAM+BATT
           B - ROM+MMM01               1F - Pocket Camera
           C - ROM+MMM01+SRAM          FD - Bandai TAMA5
           D - ROM+MMM01+SRAM+BATT     FE - Hudson HuC-3
           F - ROM+MBC3+TIMER+BATT     FF - Hudson HuC-1
          10 - ROM+MBC3+TIMER+RAM+BATT
          11 - ROM+MBC3
*/
char *
MBC_String(unsigned char mbc_type)
{
	switch (mbc_type) {
		case 0x00:
			return "ROM ONLY";
		case 0x01:
			return "ROM+MBC1";
		case 0x02:
			return "ROM+MBC1+RAM";
		case 0x03:
			return "ROM+MBC1+RAM+BATTERY";
		case 0x05:
			return "ROM+MBC2";
		case 0x06:
			return "ROM+MBC2+BATTERY";
		case 0x08:
			return "ROM+RAM";
		case 0x09:
			return "ROM+RAM+BATTERY";
		case 0x0B:
			return "ROM+MMM01";
		case 0x0C:
			return "ROM+MMM01+SRAM";
		case 0x0D:
			return "ROM+MMM01+SRAM+BATTERY";
		case 0x0F:
			return "ROM+MBC3+TIMER+BATTERY";
		case 0x10:
			return "ROM+MBC3+TIMER+RAM+BATTERY";
		case 0x11:
			return "ROM+MBC3";
		case 0x12:
			return "ROM+MBC3+RAM";
		case 0x13:
			return "ROM+MBC3+RAM+BATTERY";
		case 0x19:
			return "ROM+MBC5";
		case 0x1A:
			return "ROM+MBC5+RAM";
		case 0x1B:
			return "ROM+MBC5+RAM+BATTERY";
		case 0x1C:
			return "ROM+MBC5+RUMBLE";
		case 0x1D:
			return "ROM+MBC5+RUMBLE+SRAM";
		case 0x1E:
			return "ROM+MBC5+RUMBLE+SRAM+BATTERY";
		case 0x1F:
			return "Pocket Camera";
		case 0xFD:
			return "Bandai TAMA5";
		case 0xFE:
			return "Hudson HuC-3";
		case 0xFF:
			return "Hudson HuC-1";
		default:
			return "Unknown MBC type";
	}
}
/*
 * Das main
 *
 */

int 
main(int argc, char *argv[])
{
	int ch;
	char *ep;

	int argn = 1;
	char filename[512];
	char cartname[32];
	char nlicensee[3];
	FILE *f;
	int pad_value = 0;
	int ram_size = 0;
	int mbc_type = 0;
	int gbc_mode = 0;

	ulOptions = 0;

	if (argc == 1)
		PrintUsage();

	while ((ch = getopt(argc, argv, "b:cdjk:m:op:qrst:v")) != -1) {
		switch (ch) {
		case 'd':
			ulOptions |= OPTF_DEBUG;
			break;
		case 'm':
			ulOptions |= OPTF_RAMSIZE;
			ram_size = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0')
				FatalError("Invalid argument for option 'm'");
			if (ram_size < 0 || ram_size > 0xFF)
				FatalError("Argument for option 'm' must be between 0 and 0xFF");
			break;
		case 'j':
			ulOptions |= OPTF_JAPAN;
			break;
		case 'b':
			ulOptions |= OPTF_MBCTYPE;
			mbc_type = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0')
				FatalError("Invalid argument for option 'b'");
			if (mbc_type < 0 || mbc_type > 0xFF)
				FatalError("Argument for option 'b' must be between 0 and 0xFF");
			break;
		case 'p':
			ulOptions |= OPTF_PAD;
			pad_value = strtoul(optarg, &ep, 0);
			if (optarg[0] == '\0' || *ep != '\0')
				FatalError("Invalid argument for option 'p'");
			if (pad_value < 0 || pad_value > 0xFF)
				FatalError("Argument for option 'p' must be between 0 and 0xFF");
			break;
		case 'r':
			ulOptions |= OPTF_TRUNCATE;
			break;
		case 'v':
			ulOptions |= OPTF_VALIDATE;
			break;
		case 't':
			strncpy(cartname, optarg, 16);
			ulOptions |= OPTF_TITLE;
			break;
		case 'k':
			strncpy(nlicensee, optarg, 2);
			ulOptions |= OPTF_NLICENSEE;
			break;
		case 'q':
			ulOptions |= OPTF_QUIET;
			break;
		case 'o':
			if (ulOptions & OPTF_GBCMODE) {
				FatalError("-c and -o can't be used together");
			}
			ulOptions |= OPTF_GBCMODE;
			gbc_mode = 0xC0;
			break;
		case 'c':
			if (ulOptions & OPTF_GBCMODE) {
				FatalError("-c and -o can't be used together");
			}
			ulOptions |= OPTF_GBCMODE;
			gbc_mode = 0x80;
			break;
		case 's':
			ulOptions |= OPTF_SGBMODE;
			break;
		default:
			PrintUsage();
			/* NOTREACHED */
		}
		}
		argc -= optind;
		argv += optind;

	if (argc == 0)
		PrintUsage();

	strcpy(filename, argv[argc - 1]);

	if (!FileExists(filename))
		strncat(filename, ".gb", 3);

	f = fopen(filename, "rb+");
	if ((f = fopen(filename, "rb+")) != NULL) {

		/*
		 * -d (Debug) option code
		 *
		 */

		if ((ulOptions & OPTF_DEBUG) && !(ulOptions & OPTF_QUIET)) {
			printf("-d (Debug) option enabled...\n");
		}
		/*
		 * -p (Pad) option code
		 *
		 */

		if (ulOptions & OPTF_PAD) {
			long size, padto;
			long bytesadded = 0;

			size = FileSize(f);
			padto = 0x8000L;
			while (size > padto)
				padto *= 2;

			if (!(ulOptions & OPTF_QUIET)) {
				printf("Padding to %ldkB with pad value %#02X\n", padto / 1024, pad_value & 0xFF);
			}
			/*
			   if( padto<=0x80000L )
			   {
			   */
			if (size != padto) {
				fflush(stdout);

				fseek(f, 0, SEEK_END);
				while (size < padto) {
					size += 1;
					if ((ulOptions & OPTF_DEBUG) == 0)
						fputc(pad_value & 0xFF, f);
					bytesadded += 1;
				}
				fflush(f);

				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tAdded %ld bytes\n", bytesadded);
				}
			} else {
				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tNo padding needed\n");
				}
			}
			/*
			   }
			   else
			   FatalError( "Image size exceeds 512kB" );
			   */
		}
		/*
		 * -r (Truncate) option code
		 *
		 */

		if (ulOptions & OPTF_TRUNCATE) {
			long size, padto;
			char tempfile[] = "/tmp/rgbfix-XXXXXX";
			FILE *tf;

			size = FileSize(f);
			padto = 256 * 32768;
			while (size < padto)
				padto /= 2;

			if (!(ulOptions & OPTF_QUIET)) {
				printf("Truncating to %ldkB:\n", padto / 1024);
			}
			mkstemp(tempfile);

			if ((ulOptions & OPTF_DEBUG) == 0) {
				if ((tf = fopen(tempfile, "wb")) != NULL) {
					fseek(f, 0, SEEK_SET);
					while (padto--) {
						fputc(fgetc(f), tf);
					}
					fclose(f);
					fclose(tf);
					remove(filename);
					rename(tempfile, filename);
					f = fopen(filename, "rb+");
				}
			}
		}
		/*
		 * -t (Set carttitle) option code
		 *
		 */

		if (ulOptions & OPTF_TITLE) {
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Setting cartridge title:\n");
			}
			if ((ulOptions & OPTF_DEBUG) == 0) {
				fflush(f);
				fseek(f, 0x0134L, SEEK_SET);
				fwrite(cartname, 16, 1, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("\tTitle set to %s\n", cartname);
			}
		}
		/*
		 * -k (Set new licensee) option code
		 */
		if (ulOptions & OPTF_NLICENSEE) {
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Setting new licensee:\n");
			}
			if ((ulOptions & OPTF_DEBUG) == 0) {
				fflush(f);
				fseek(f, 0x144, SEEK_SET);
				fwrite(nlicensee, 2, 1, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("\tLicensee set to %s\n", nlicensee);
			}
		}
		/*
		 * -m (Set ram size) option code
		 *
		 */
		if (ulOptions & OPTF_RAMSIZE) {
			/* carttype byte can be anything? */
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Setting RAM size\n");
			}
			if (!(ulOptions & OPTF_DEBUG)) {
				fflush(f);
				fseek(f, 0x149L, SEEK_SET);
				fputc(ram_size, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("\tRAM size set to %#02X\n", ram_size);
			}
		}
		/*
		 * -j (Set region flag) option code
		 */
		if (ulOptions & OPTF_JAPAN) {
			if (!(ulOptions & OPTF_DEBUG)) {
				fflush(f);
				fseek(f, 0x14A, SEEK_SET);
				fputc(1, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Region code set to non-Japan\n");
			}
		}
		/*
		 * -b (Set MBC type) option code
		 */
		if (ulOptions & OPTF_MBCTYPE) {
			/* carttype byte can be anything? */
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Setting MBC type\n");
			}
			if (!(ulOptions & OPTF_DEBUG)) {
				fflush(f);
				fseek(f, 0x147L, SEEK_SET);
				fputc(mbc_type, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("\tMBC type set to %#02X (%s)\n", mbc_type, MBC_String(mbc_type));
			}
		}
		/*
		 * -c/-o (Set GBC only/compatible mode)
		 */
		if (ulOptions & OPTF_GBCMODE) {
			if (!(ulOptions & OPTF_QUIET) && gbc_mode == 0x80) {
				printf("Setting Game Boy Color compatible mode\n");
			}
			if (!(ulOptions & OPTF_QUIET) && gbc_mode == 0xC0) {
				printf("Setting Game Boy Color only mode\n");
			}
			if (!(ulOptions & OPTF_DEBUG)) {
				fflush(f);
				fseek(f, 0x143L, SEEK_SET);
				fputc(gbc_mode, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET) && gbc_mode == 0x80) {
				printf("\tGame Boy Color compatible mode set\n");
			}
			if (!(ulOptions & OPTF_QUIET) && gbc_mode == 0xC0) {
				printf("\tGame Boy Color only mode set\n");
			}

			if (ulOptions & OPTF_TITLE) { 
				if (cartname[0xF]) {
					Warning("Last character of cartridge title was overwritten by '-%c' option", gbc_mode == 0x80 ? 'c' : 'o');
				}
			}
		}
		/*
		 * -s (Set SGB mode) option code
		 */
		if (ulOptions & OPTF_SGBMODE) {
			if (!(ulOptions & OPTF_DEBUG)) {
				fflush(f);
				// set old licensee code to 0x33
				fseek(f, 0x14B, SEEK_SET);
				fputc(0x33, f);
				// set SGB flag to 0x03
				fseek(f, 0x146, SEEK_SET);
				fputc(3, f);
				fflush(f);
			}
			if (!(ulOptions & OPTF_QUIET)) {
				printf("Setting SGB mode flag\n");
			}
		}
		/*
		 * -v (Validate header) option code
		 *
		 */

		if (ulOptions & OPTF_VALIDATE) {
			long i, byteschanged = 0;
			long cartromsize, calcromsize = 0, filesize;
			long carttype;
			unsigned short cartchecksum = 0, calcchecksum = 0;
			unsigned char cartcompchecksum = 0, calccompchecksum =
			0;
			int ch;

			if (!(ulOptions & OPTF_VALIDATE)) {
				printf("Validating header:\n");
			}
			fflush(stdout);

			/* Nintendo Character Area */

			fflush(f);
			fseek(f, 0x0104L, SEEK_SET);

			for (i = 0; i < 48; i += 1) {
				int ch;

				ch = fgetc(f);
				if (ch == EOF)
					ch = 0x00;
				if (ch != NintendoChar[i]) {
					byteschanged += 1;

					if ((ulOptions & OPTF_DEBUG) == 0) {
						fseek(f, -1, SEEK_CUR);
						fputc(NintendoChar[i], f);
						fflush(f);
					}
				}
			}

			fflush(f);

			if (!(ulOptions & OPTF_QUIET)) {
				if (byteschanged)
					printf
					    ("\tChanged %ld bytes in the Nintendo Character Area\n",
					    byteschanged);
				else
					printf("\tNintendo Character Area is OK\n");
			}
			/* ROM size */

			fflush(f);
			fseek(f, 0x0148L, SEEK_SET);
			cartromsize = fgetc(f);
			if (cartromsize == EOF)
				cartromsize = 0x00;
			filesize = FileSize(f);
			while (filesize > (0x8000L << calcromsize))
				calcromsize += 1;

			if (calcromsize != cartromsize) {
				if ((ulOptions & OPTF_DEBUG) == 0) {
					fseek(f, -1, SEEK_CUR);
					fputc(calcromsize, f);
					fflush(f);
				}
				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tChanged ROM size byte from 0x%02lX (%ldkB) to 0x%02lX (%ldkB)\n",
					    cartromsize,
					    (0x8000L << cartromsize) / 1024,
					    calcromsize,
					    (0x8000L << calcromsize) / 1024);
				}
			} else if (!(ulOptions & OPTF_QUIET)) {
				printf("\tROM size byte is OK\n");
			}
			/* Cartridge type */

			fflush(f);
			fseek(f, 0x0147L, SEEK_SET);
			carttype = fgetc(f);
			if (carttype == EOF)
				carttype = 0x00;

			if (FileSize(f) > 0x8000L) {
				/* carttype byte must != 0x00 */
				if (carttype == 0x00) {
					if ((ulOptions & OPTF_DEBUG) == 0) {
						fseek(f, -1, SEEK_CUR);
						fputc(0x01, f);
						fflush(f);
					}
					if (!(ulOptions & OPTF_QUIET)) {
						printf
						    ("\tCartridge type byte changed to 0x01\n");
					}
				} else if (!(ulOptions & OPTF_QUIET)) {
					printf("\tCartridge type byte is OK\n");
				}
			} else {
				/* carttype byte can be anything? */
				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tCartridge type byte is OK\n");
				}
			}

			/* Checksum */

			fflush(f);
			fseek(f, 0, SEEK_SET);

			for (i = 0; i < (0x8000L << calcromsize); i += 1) {
				ch = fgetc(f);
				if (ch == EOF)
					ch = 0;

				if (i < 0x0134L)
					calcchecksum += ch;
				else if (i < 0x014DL) {
					calccompchecksum += ch;
					calcchecksum += ch;
				} else if (i == 0x014DL)
					cartcompchecksum = ch;
				else if (i == 0x014EL)
					cartchecksum = ch << 8;
				else if (i == 0x014FL)
					cartchecksum |= ch;
				else
					calcchecksum += ch;
			}

			calccompchecksum = 0xE7 - calccompchecksum;
			calcchecksum += calccompchecksum;

			if (cartchecksum != calcchecksum) {
				fflush(f);
				fseek(f, 0x014EL, SEEK_SET);
				if ((ulOptions & OPTF_DEBUG) == 0) {
					fputc(calcchecksum >> 8, f);
					fputc(calcchecksum & 0xFF, f);
				}
				fflush(f);
				if (!(ulOptions & OPTF_QUIET)) {
					printf
					    ("\tChecksum changed from 0x%04lX to 0x%04lX\n",
					    (long) cartchecksum, (long) calcchecksum);
				}
			} else {
				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tChecksum is OK\n");
				}
			}

			if (cartcompchecksum != calccompchecksum) {
				fflush(f);
				fseek(f, 0x014DL, SEEK_SET);
				if ((ulOptions & OPTF_DEBUG) == 0)
					fputc(calccompchecksum, f);
				fflush(f);
				if (!(ulOptions & OPTF_QUIET)) {
					printf
					    ("\tCompChecksum changed from 0x%02lX to 0x%02lX\n",
					    (long) cartcompchecksum,
					    (long) calccompchecksum);
				}
			} else {
				if (!(ulOptions & OPTF_QUIET)) {
					printf("\tCompChecksum is OK\n");
				}
			}

		}
		fclose(f);
	} else {
		FatalError("File '%s' not found", filename);
	}

	return (0);
}
