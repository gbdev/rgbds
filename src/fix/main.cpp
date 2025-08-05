// SPDX-License-Identifier: MIT

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "diagnostics.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "style.hpp"
#include "usage.hpp"
#include "version.hpp"

#include "fix/mbc.hpp"
#include "fix/warning.hpp"

static constexpr off_t BANK_SIZE = 0x4000;

// Short options
static char const *optstring = "Ccf:hi:jk:L:l:m:n:Oo:p:r:st:VvW:w";

// Variables for the long-only options
static int longOpt; // `--color`

// Equivalent long options
// Please keep in the same order as short opts.
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching.
static option const longopts[] = {
    {"color-only",       no_argument,       nullptr,  'C'},
    {"color-compatible", no_argument,       nullptr,  'c'},
    {"fix-spec",         required_argument, nullptr,  'f'},
    {"help",             no_argument,       nullptr,  'h'},
    {"game-id",          required_argument, nullptr,  'i'},
    {"non-japanese",     no_argument,       nullptr,  'j'},
    {"new-licensee",     required_argument, nullptr,  'k'},
    {"logo",             required_argument, nullptr,  'L'},
    {"old-licensee",     required_argument, nullptr,  'l'},
    {"mbc-type",         required_argument, nullptr,  'm'},
    {"rom-version",      required_argument, nullptr,  'n'},
    {"overwrite",        no_argument,       nullptr,  'O'},
    {"output",           required_argument, nullptr,  'o'},
    {"pad-value",        required_argument, nullptr,  'p'},
    {"ram-size",         required_argument, nullptr,  'r'},
    {"sgb-compatible",   no_argument,       nullptr,  's'},
    {"title",            required_argument, nullptr,  't'},
    {"version",          no_argument,       nullptr,  'V'},
    {"validate",         no_argument,       nullptr,  'v'},
    {"warning",          required_argument, nullptr,  'W'},
    {"color",            required_argument, &longOpt, 'c'},
    {nullptr,            no_argument,       nullptr,  0  },
};

// clang-format off: nested initializers
static Usage usage = {
    .name = "rgbfix",
    .flags = {
        "[-hjOsVvw]", "[-C | -c]", "[-f <fix_spec>]", "[-i <game_id>]", "[-k <licensee>]",
        "[-L <logo_file>]", "[-l <licensee_byte>]", "[-m <mbc_type>]", "[-n <rom_version>]",
        "[-p <pad_value>]", "[-r <ram_size>]", "[-t <title_str>]", "[-W warning]", "<file> ...",
    },
    .options = {
        {
            {"-m", "--mbc-type <value>"},
            {
                "set the MBC type byte to this value; `-m help'",
                "or `-m list' prints the accepted values",
            },
        },
        {{"-p", "--pad-value <value>"}, {"pad to the next valid size using this value"}},
        {{"-r", "--ram-size <code>"}, {"set the cart RAM size byte to this value"}},
        {{"-o", "--output <path>"}, {"set the output file"}},
        {{"-V", "--version"}, {"print RGBFIX version and exit"}},
        {{"-v", "--validate"}, {"fix the header logo and both checksums (`-f lhg')"}},
        {{"-W", "--warning <warning>"}, {"enable or disable warnings"}},
    },
};
// clang-format on

static uint8_t tpp1Rev[2];

static uint8_t const nintendoLogo[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

static uint8_t fixSpec = 0;
// clang-format off: vertically align values
static constexpr uint8_t FIX_LOGO         = 1 << 7;
static constexpr uint8_t TRASH_LOGO       = 1 << 6;
static constexpr uint8_t FIX_HEADER_SUM   = 1 << 5;
static constexpr uint8_t TRASH_HEADER_SUM = 1 << 4;
static constexpr uint8_t FIX_GLOBAL_SUM   = 1 << 3;
static constexpr uint8_t TRASH_GLOBAL_SUM = 1 << 2;
// clang-format on

static enum { DMG, BOTH, CGB } model = DMG; // If DMG, byte is left alone
static char const *gameID = nullptr;
static uint8_t gameIDLen;
static bool japanese = true;
static char const *logoFilename = nullptr;
static uint8_t logo[sizeof(nintendoLogo)] = {};
static char const *newLicensee = nullptr;
static uint8_t newLicenseeLen;
static uint16_t oldLicensee = UNSPECIFIED;
static MbcType cartridgeType = MBC_NONE;
static uint16_t romVersion = UNSPECIFIED;
static uint16_t padValue = UNSPECIFIED;
static uint16_t ramSize = UNSPECIFIED;
static bool sgb = false; // If false, SGB flags are left alone
static char const *title = nullptr;
static uint8_t titleLen;

static uint8_t maxTitleLen() {
	return gameID ? 11 : model != DMG ? 15 : 16;
}

static ssize_t readBytes(int fd, uint8_t *buf, size_t len) {
	// POSIX specifies that lengths greater than SSIZE_MAX yield implementation-defined results
	assume(len <= SSIZE_MAX);

	ssize_t total = 0;

	while (len) {
		ssize_t ret = read(fd, buf, len);

		// Return errors, unless we only were interrupted
		if (ret == -1 && errno != EINTR) {
			return -1; // LCOV_EXCL_LINE
		}
		// EOF reached
		if (ret == 0) {
			return total;
		}
		// If anything was read, accumulate it, and continue
		if (ret != -1) {
			total += ret;
			len -= ret;
			buf += ret;
		}
	}

	return total;
}

static ssize_t writeBytes(int fd, uint8_t *buf, size_t len) {
	// POSIX specifies that lengths greater than SSIZE_MAX yield implementation-defined results
	assume(len <= SSIZE_MAX);

	ssize_t total = 0;

	while (len) {
		ssize_t ret = write(fd, buf, len);

		// Return errors, unless we only were interrupted
		if (ret == -1 && errno != EINTR) {
			return -1; // LCOV_EXCL_LINE
		}
		// If anything was written, accumulate it, and continue
		if (ret != -1) {
			total += ret;
			len -= ret;
			buf += ret;
		}
	}

	return total;
}

static void overwriteByte(uint8_t *rom0, uint16_t addr, uint8_t fixedByte, char const *areaName) {
	uint8_t origByte = rom0[addr];

	if (origByte != 0 && origByte != fixedByte) {
		warning(WARNING_OVERWRITE, "Overwrote a non-zero byte in the %s", areaName);
	}

	rom0[addr] = fixedByte;
}

static void overwriteBytes(
    uint8_t *rom0, uint16_t startAddr, uint8_t const *fixed, uint8_t size, char const *areaName
) {
	for (uint8_t i = 0; i < size; ++i) {
		uint8_t origByte = rom0[i + startAddr];

		if (origByte != 0 && origByte != fixed[i]) {
			warning(WARNING_OVERWRITE, "Overwrote a non-zero byte in the %s", areaName);
			break;
		}
	}

	memcpy(&rom0[startAddr], fixed, size);
}

static void
    processFile(int input, int output, char const *name, off_t fileSize, bool expectFileSize) {
	if (expectFileSize) {
		assume(fileSize != 0);
	} else {
		assume(fileSize == 0);
	}

	uint8_t rom0[BANK_SIZE];
	ssize_t rom0Len = readBytes(input, rom0, sizeof(rom0));
	// Also used as how many bytes to write back when fixing in-place
	ssize_t headerSize = (cartridgeType & 0xFF00) == TPP1 ? 0x154 : 0x150;

	if (rom0Len == -1) {
		// LCOV_EXCL_START
		error("Failed to read \"%s\"'s header: %s", name, strerror(errno));
		return;
		// LCOV_EXCL_STOP
	} else if (rom0Len < headerSize) {
		error(
		    "\"%s\" too short, expected at least %jd ($%jx) bytes, got only %jd",
		    name,
		    static_cast<intmax_t>(headerSize),
		    static_cast<intmax_t>(headerSize),
		    static_cast<intmax_t>(rom0Len)
		);
		return;
	}
	// Accept partial reads if the file contains at least the header

	if (fixSpec & (FIX_LOGO | TRASH_LOGO)) {
		overwriteBytes(rom0, 0x0104, logo, sizeof(logo), logoFilename ? "logo" : "Nintendo logo");
	}

	if (title) {
		overwriteBytes(rom0, 0x134, reinterpret_cast<uint8_t const *>(title), titleLen, "title");
	}

	if (gameID) {
		overwriteBytes(
		    rom0, 0x13F, reinterpret_cast<uint8_t const *>(gameID), gameIDLen, "manufacturer code"
		);
	}

	if (model != DMG) {
		overwriteByte(rom0, 0x143, model == BOTH ? 0x80 : 0xC0, "CGB flag");
	}

	if (newLicensee) {
		overwriteBytes(
		    rom0,
		    0x144,
		    reinterpret_cast<uint8_t const *>(newLicensee),
		    newLicenseeLen,
		    "new licensee code"
		);
	}

	if (sgb) {
		overwriteByte(rom0, 0x146, 0x03, "SGB flag");
	}

	// If a valid MBC was specified...
	if (cartridgeType < MBC_NONE) {
		uint8_t byte = cartridgeType;

		if ((cartridgeType & 0xFF00) == TPP1) {
			// Cartridge type isn't directly actionable, translate it
			byte = 0xBC;
			// The other TPP1 identification bytes will be written below
		}
		overwriteByte(rom0, 0x147, byte, "cartridge type");
	}

	// ROM size will be written last, after evaluating the file's size

	if ((cartridgeType & 0xFF00) == TPP1) {
		uint8_t const tpp1Code[2] = {0xC1, 0x65};

		overwriteBytes(rom0, 0x149, tpp1Code, sizeof(tpp1Code), "TPP1 identification code");

		overwriteBytes(rom0, 0x150, tpp1Rev, sizeof(tpp1Rev), "TPP1 revision number");

		if (ramSize != UNSPECIFIED) {
			overwriteByte(rom0, 0x152, ramSize, "RAM size");
		}

		overwriteByte(rom0, 0x153, cartridgeType & 0xFF, "TPP1 feature flags");
	} else {
		// Regular mappers

		if (ramSize != UNSPECIFIED) {
			overwriteByte(rom0, 0x149, ramSize, "RAM size");
		}

		if (!japanese) {
			overwriteByte(rom0, 0x14A, 0x01, "destination code");
		}
	}

	if (oldLicensee != UNSPECIFIED) {
		overwriteByte(rom0, 0x14B, oldLicensee, "old licensee code");
	} else if (sgb && rom0[0x14B] != 0x33) {
		warning(
		    WARNING_SGB,
		    "SGB compatibility enabled, but old licensee was 0x%02x, not 0x33",
		    rom0[0x14B]
		);
	}

	if (romVersion != UNSPECIFIED) {
		overwriteByte(rom0, 0x14C, romVersion, "mask ROM version number");
	}

	// Remain to be handled the ROM size, and header checksum.
	// The latter depends on the former, and so will be handled after it.
	// The former requires knowledge of the file's total size, so read that first.

	uint16_t globalSum = 0;

	// To keep file sizes fairly reasonable, we'll cap the amount of banks at 65536.
	// Official mappers only go up to 512 banks, but at least the TPP1 spec allows up to
	// 65536 banks = 1 GiB.
	// This should be reasonable for the time being, and may be extended later.
	std::vector<uint8_t> romx; // Buffer of ROMX bank data
	uint32_t nbBanks = 1;      // Number of banks *targeted*, including ROM0
	size_t totalRomxLen = 0;   // *Actual* size of ROMX data
	uint8_t bank[BANK_SIZE];   // Temp buffer used to store a whole bank's worth of data

	// Handle ROMX
	if (input == output) {
		if (fileSize >= 0x10000 * BANK_SIZE) {
			error("\"%s\" has more than 65536 banks", name);
			return;
		}
		// This should be guaranteed from the size cap...
		static_assert(0x10000 * BANK_SIZE <= SSIZE_MAX, "Max input file size too large for OS");
		// Compute number of banks and ROMX len from file size
		nbBanks = (fileSize + (BANK_SIZE - 1)) / BANK_SIZE; // ceil(fileSize / BANK_SIZE)
		totalRomxLen = fileSize >= BANK_SIZE ? fileSize - BANK_SIZE : 0;
	} else if (rom0Len == BANK_SIZE) {
		// Copy ROMX when reading a pipe, and we're not at EOF yet
		for (;;) {
			romx.resize(nbBanks * BANK_SIZE);
			ssize_t bankLen = readBytes(input, &romx[(nbBanks - 1) * BANK_SIZE], BANK_SIZE);

			// Update bank count, ONLY IF at least one byte was read
			if (bankLen) {
				// We're gonna read another bank, check that it won't be too much
				static_assert(
				    0x10000 * BANK_SIZE <= SSIZE_MAX, "Max input file size too large for OS"
				);
				if (nbBanks == 0x10000) {
					error("\"%s\" has more than 65536 banks", name);
					return;
				}
				++nbBanks;

				// Update global checksum, too
				for (uint16_t i = 0; i < bankLen; ++i) {
					globalSum += romx[totalRomxLen + i];
				}
				totalRomxLen += bankLen;
			}
			// Stop when an incomplete bank has been read
			if (bankLen != BANK_SIZE) {
				break;
			}
		}
	}

	// Handle setting the ROM size if padding was requested
	// Pad to the next valid power of 2. This is because padding is required by flashers, which
	// flash to ROM chips, whose size is always a power of 2... so there'd be no point in
	// padding to something else.
	// Additionally, a ROM must be at least 32k, so we guarantee a whole amount of banks...
	if (padValue != UNSPECIFIED) {
		// We want at least 2 banks
		if (nbBanks == 1) {
			if (rom0Len != sizeof(rom0)) {
				memset(&rom0[rom0Len], padValue, sizeof(rom0) - rom0Len);
				// The global checksum hasn't taken ROM0 into consideration yet!
				// ROM0 was padded, so treat it as entirely written: update its size
				// Update how many bytes were read in total, too
				rom0Len = sizeof(rom0);
			}
			nbBanks = 2;
		} else {
			assume(rom0Len == sizeof(rom0));
		}
		assume(nbBanks >= 2);
		// Alter number of banks to reflect required value
		// x&(x-1) is zero iff x is a power of 2, or 0; we know for sure it's non-zero,
		// so this is true (non-zero) when we don't have a power of 2
		if (nbBanks & (nbBanks - 1)) {
			nbBanks = 1 << (CHAR_BIT * sizeof(nbBanks) - clz(nbBanks));
		}
		// Write final ROM size
		rom0[0x148] = ctz(nbBanks / 2);
		// Alter global checksum based on how many bytes will be added (not counting ROM0)
		globalSum += padValue * ((nbBanks - 1) * BANK_SIZE - totalRomxLen);
	}

	// Handle the header checksum after the ROM size has been written
	if (fixSpec & (FIX_HEADER_SUM | TRASH_HEADER_SUM)) {
		uint8_t sum = 0;

		for (uint16_t i = 0x134; i < 0x14D; ++i) {
			sum -= rom0[i] + 1;
		}

		overwriteByte(rom0, 0x14D, fixSpec & TRASH_HEADER_SUM ? ~sum : sum, "header checksum");
	}

	if (fixSpec & (FIX_GLOBAL_SUM | TRASH_GLOBAL_SUM)) {
		// Computation of the global checksum does not include the checksum bytes
		assume(rom0Len >= 0x14E);
		for (uint16_t i = 0; i < 0x14E; ++i) {
			globalSum += rom0[i];
		}
		for (uint16_t i = 0x150; i < rom0Len; ++i) {
			globalSum += rom0[i];
		}
		// Pipes have already read ROMX and updated globalSum, but not regular files
		if (input == output) {
			for (;;) {
				ssize_t bankLen = readBytes(input, bank, sizeof(bank));

				for (uint16_t i = 0; i < bankLen; ++i) {
					globalSum += bank[i];
				}
				if (bankLen != sizeof(bank)) {
					break;
				}
			}
		}

		if (fixSpec & TRASH_GLOBAL_SUM) {
			globalSum = ~globalSum;
		}

		uint8_t bytes[2] = {
		    static_cast<uint8_t>(globalSum >> 8), static_cast<uint8_t>(globalSum & 0xFF)
		};

		overwriteBytes(rom0, 0x14E, bytes, sizeof(bytes), "global checksum");
	}

	ssize_t writeLen;

	// In case the output depends on the input, reset to the beginning of the file, and only
	// write the header
	if (input == output) {
		if (lseek(output, 0, SEEK_SET) == static_cast<off_t>(-1)) {
			// LCOV_EXCL_START
			error("Failed to rewind \"%s\": %s", name, strerror(errno));
			return;
			// LCOV_EXCL_STOP
		}
		// If modifying the file in-place, we only need to edit the header
		// However, padding may have modified ROM0 (added padding), so don't in that case
		if (padValue == UNSPECIFIED) {
			rom0Len = headerSize;
		}
	}
	writeLen = writeBytes(output, rom0, rom0Len);

	if (writeLen == -1) {
		// LCOV_EXCL_START
		error("Failed to write \"%s\"'s ROM0: %s", name, strerror(errno));
		return;
		// LCOV_EXCL_STOP
	} else if (writeLen < rom0Len) {
		// LCOV_EXCL_START
		error(
		    "Could only write %jd of \"%s\"'s %jd ROM0 bytes",
		    static_cast<intmax_t>(writeLen),
		    name,
		    static_cast<intmax_t>(rom0Len)
		);
		return;
		// LCOV_EXCL_STOP
	}

	// Output ROMX if it was buffered
	if (!romx.empty()) {
		// The value returned is either -1, or smaller than `totalRomxLen`,
		// so it's fine to cast to `size_t`
		writeLen = writeBytes(output, romx.data(), totalRomxLen);
		if (writeLen == -1) {
			// LCOV_EXCL_START
			error("Failed to write \"%s\"'s ROMX: %s", name, strerror(errno));
			return;
			// LCOV_EXCL_STOP
		} else if (static_cast<size_t>(writeLen) < totalRomxLen) {
			// LCOV_EXCL_START
			error(
			    "Could only write %jd of \"%s\"'s %zu ROMX bytes",
			    static_cast<intmax_t>(writeLen),
			    name,
			    totalRomxLen
			);
			return;
			// LCOV_EXCL_STOP
		}
	}

	// Output padding
	if (padValue != UNSPECIFIED) {
		if (input == output) {
			if (lseek(output, 0, SEEK_END) == static_cast<off_t>(-1)) {
				// LCOV_EXCL_START
				error("Failed to seek to end of \"%s\": %s", name, strerror(errno));
				return;
				// LCOV_EXCL_STOP
			}
		}
		memset(bank, padValue, sizeof(bank));
		size_t len = (nbBanks - 1) * BANK_SIZE - totalRomxLen; // Don't count ROM0!

		while (len) {
			static_assert(sizeof(bank) <= SSIZE_MAX, "Bank too large for reading");
			size_t thisLen = len > sizeof(bank) ? sizeof(bank) : len;
			ssize_t ret = writeBytes(output, bank, thisLen);

			// The return value is either -1, or at most `thisLen`,
			// so it's fine to cast to `size_t`
			if (static_cast<size_t>(ret) != thisLen) {
				// LCOV_EXCL_START
				error("Failed to write \"%s\"'s padding: %s", name, strerror(errno));
				break;
				// LCOV_EXCL_STOP
			}
			len -= thisLen;
		}
	}
}

static bool processFilename(char const *name, char const *outputName) {
	warnings.nbErrors = 0;

	bool inputStdin = !strcmp(name, "-");
	if (inputStdin && !outputName) {
		outputName = "-";
	}

	int output = -1;
	bool openedOutput = false;
	if (outputName) {
		if (!strcmp(outputName, "-")) {
			output = STDOUT_FILENO;
			(void)setmode(STDOUT_FILENO, O_BINARY);
		} else {
			output = open(outputName, O_WRONLY | O_BINARY | O_CREAT, 0600);
			if (output == -1) {
				error("Failed to open \"%s\" for writing: %s", outputName, strerror(errno));
				return true;
			}
			openedOutput = true;
		}
	}
	Defer closeOutput{[&] {
		if (openedOutput) {
			close(output);
		}
	}};

	if (inputStdin) {
		name = "<stdin>";
		(void)setmode(STDIN_FILENO, O_BINARY);
		processFile(STDIN_FILENO, output, name, 0, false);
	} else if (int input = open(name, (outputName ? O_RDONLY : O_RDWR) | O_BINARY); input == -1) {
		// POSIX specifies that the results of O_RDWR on a FIFO are undefined.
		// However, this is necessary to avoid a TOCTTOU, if the file was changed between
		// `stat()` and `open(O_RDWR)`, which could trigger the UB anyway.
		// Thus, we're going to hope that either the `open` fails, or it succeeds but I/O
		// operations may fail, all of which we handle.
		error("Failed to open \"%s\" for reading+writing: %s", name, strerror(errno));
	} else {
		Defer closeInput{[&] { close(input); }};
		struct stat stat;
		if (fstat(input, &stat) == -1) {
			error("Failed to stat \"%s\": %s", name, strerror(errno)); // LCOV_EXCL_LINE
		} else if (!S_ISREG(stat.st_mode)) { // We do not support FIFOs or symlinks
			// LCOV_EXCL_START
			error("\"%s\" is not a regular file, and thus cannot be modified in-place", name);
			// LCOV_EXCL_STOP
		} else if (stat.st_size < 0x150) {
			// This check is in theory redundant with the one in `processFile`, but it
			// prevents passing a file size of 0, which usually indicates pipes
			error(
			    "\"%s\" too short, expected at least 336 ($150) bytes, got only %jd",
			    name,
			    static_cast<intmax_t>(stat.st_size)
			);
		} else {
			if (!outputName) {
				output = input;
			}
			processFile(input, output, name, stat.st_size, true);
		}
	}

	return checkErrors(name);
}

static void parseByte(uint16_t &output, char name) {
	if (musl_optarg[0] == 0) {
		fatal("Argument to option '%c' may not be empty", name);
	}

	char *endptr;
	unsigned long value;
	if (musl_optarg[0] == '$') {
		value = strtoul(&musl_optarg[1], &endptr, 16);
	} else {
		value = strtoul(musl_optarg, &endptr, 0);
	}

	if (*endptr) {
		fatal("Expected number as argument to option '%c', got %s", name, musl_optarg);
	} else if (value > 0xFF) {
		fatal("Argument to option '%c' is larger than 255: %lu", name, value);
	}

	output = value;
}

static void initLogo() {
	if (logoFilename) {
		FILE *logoFile;
		if (strcmp(logoFilename, "-")) {
			logoFile = fopen(logoFilename, "rb");
		} else {
			logoFilename = "<stdin>";
			(void)setmode(STDIN_FILENO, O_BINARY);
			logoFile = stdin;
		}
		if (!logoFile) {
			fatal("Failed to open \"%s\" for reading: %s", logoFilename, strerror(errno));
		}
		Defer closeLogo{[&] { fclose(logoFile); }};

		uint8_t logoBpp[sizeof(logo)];
		if (size_t nbRead = fread(logoBpp, 1, sizeof(logoBpp), logoFile);
		    nbRead != sizeof(logo) || fgetc(logoFile) != EOF || ferror(logoFile)) {
			fatal("\"%s\" is not %zu bytes", logoFilename, sizeof(logo));
		}
		auto highs = [&logoBpp](size_t i) {
			return (logoBpp[i * 2] & 0xF0) | ((logoBpp[i * 2 + 1] & 0xF0) >> 4);
		};
		auto lows = [&logoBpp](size_t i) {
			return ((logoBpp[i * 2] & 0x0F) << 4) | (logoBpp[i * 2 + 1] & 0x0F);
		};
		constexpr size_t mid = sizeof(logo) / 2;
		for (size_t i = 0; i < mid; i += 4) {
			logo[i + 0] = highs(i + 0);
			logo[i + 1] = highs(i + 1);
			logo[i + 2] = lows(i + 0);
			logo[i + 3] = lows(i + 1);
			logo[mid + i + 0] = highs(i + 2);
			logo[mid + i + 1] = highs(i + 3);
			logo[mid + i + 2] = lows(i + 2);
			logo[mid + i + 3] = lows(i + 3);
		}
	} else {
		memcpy(logo, nintendoLogo, sizeof(nintendoLogo));
	}

	if (fixSpec & TRASH_LOGO) {
		for (uint16_t i = 0; i < sizeof(logo); ++i) {
			logo[i] = 0xFF ^ logo[i];
		}
	}
}

int main(int argc, char *argv[]) {
	char const *outputFilename = nullptr;
	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
			size_t len;

		case 'C':
		case 'c':
			model = ch == 'c' ? BOTH : CGB;
			if (titleLen > 15) {
				titleLen = 15;
				warning(WARNING_TRUNCATION, "Truncating title \"%s\" to 15 chars", title);
			}
			break;

		case 'f':
			fixSpec = 0;
			while (*musl_optarg) {
				switch (*musl_optarg) {
#define OVERRIDE_SPEC(cur, bad, curFlag, badFlag) \
	case STR(cur)[0]: \
		if (fixSpec & badFlag) { \
			warnx("'" STR(cur) "' overriding '" STR(bad) "' in fix spec"); \
		} \
		fixSpec = (fixSpec & ~badFlag) | curFlag; \
		break
#define overrideSpecs(fix, fixFlag, trash, trashFlag) \
	OVERRIDE_SPEC(fix, trash, fixFlag, trashFlag); \
	OVERRIDE_SPEC(trash, fix, trashFlag, fixFlag)
					overrideSpecs(l, FIX_LOGO, L, TRASH_LOGO);
					overrideSpecs(h, FIX_HEADER_SUM, H, TRASH_HEADER_SUM);
					overrideSpecs(g, FIX_GLOBAL_SUM, G, TRASH_GLOBAL_SUM);
#undef OVERRIDE_SPEC
#undef overrideSpecs

				default:
					fatal("Invalid character '%c' in fix spec", *musl_optarg);
				}
				++musl_optarg;
			}
			break;

		case 'h':
			usage.printAndExit(0); // LCOV_EXCL_LINE

		case 'i':
			gameID = musl_optarg;
			len = strlen(gameID);
			if (len > 4) {
				len = 4;
				warning(WARNING_TRUNCATION, "Truncating game ID \"%s\" to 4 chars", gameID);
			}
			gameIDLen = len;
			if (titleLen > 11) {
				titleLen = 11;
				warning(WARNING_TRUNCATION, "Truncating title \"%s\" to 11 chars", title);
			}
			break;

		case 'j':
			japanese = false;
			break;

		case 'k':
			newLicensee = musl_optarg;
			len = strlen(newLicensee);
			if (len > 2) {
				len = 2;
				warning(
				    WARNING_TRUNCATION, "Truncating new licensee \"%s\" to 2 chars", newLicensee
				);
			}
			newLicenseeLen = len;
			break;

		case 'L':
			logoFilename = musl_optarg;
			break;

		case 'l':
			parseByte(oldLicensee, 'l');
			break;

		case 'm':
			cartridgeType = mbc_ParseName(musl_optarg, tpp1Rev[0], tpp1Rev[1]);
			if (cartridgeType == ROM_RAM || cartridgeType == ROM_RAM_BATTERY) {
				warning(
				    WARNING_MBC, "MBC \"%s\" is under-specified and poorly supported", musl_optarg
				);
			}
			break;

		case 'n':
			parseByte(romVersion, 'n');
			break;

		case 'O':
			warnings.processWarningFlag("no-overwrite");
			break;

		case 'o':
			outputFilename = musl_optarg;
			break;

		case 'p':
			parseByte(padValue, 'p');
			break;

		case 'r':
			parseByte(ramSize, 'r');
			break;

		case 's':
			sgb = true;
			break;

		case 't': {
			title = musl_optarg;
			len = strlen(title);
			uint8_t maxLen = maxTitleLen();

			if (len > maxLen) {
				len = maxLen;
				warning(WARNING_TRUNCATION, "Truncating title \"%s\" to %u chars", title, maxLen);
			}
			titleLen = len;
			break;
		}

		case 'V':
			// LCOV_EXCL_START
			printf("rgbfix %s\n", get_package_version_string());
			exit(0);
			// LCOV_EXCL_STOP

		case 'v':
			fixSpec = FIX_LOGO | FIX_HEADER_SUM | FIX_GLOBAL_SUM;
			break;

		case 'W':
			warnings.processWarningFlag(musl_optarg);
			break;

		case 'w':
			warnings.state.warningsEnabled = false;
			break;

		// Long-only options
		case 0:
			if (longOpt == 'c') {
				if (!strcasecmp(musl_optarg, "always")) {
					style_Enable(true);
				} else if (!strcasecmp(musl_optarg, "never")) {
					style_Enable(false);
				} else if (strcasecmp(musl_optarg, "auto")) {
					fatal("Invalid argument for option '--color'");
				}
			}
			break;

		default:
			usage.printAndExit(1); // LCOV_EXCL_LINE
		}
	}

	if ((cartridgeType & 0xFF00) == TPP1 && !japanese) {
		warning(
		    WARNING_MBC, "TPP1 overwrites region flag for its identification code, ignoring `-j`"
		);
	}

	// Check that RAM size is correct for "standard" mappers
	if (ramSize != UNSPECIFIED && (cartridgeType & 0xFF00) == 0) {
		if (cartridgeType == ROM_RAM || cartridgeType == ROM_RAM_BATTERY) {
			if (ramSize != 1) {
				warning(
				    WARNING_MBC,
				    "MBC \"%s\" should have 2 KiB of RAM (-r 1)",
				    mbc_Name(cartridgeType)
				);
			}
		} else if (mbc_HasRAM(cartridgeType)) {
			if (!ramSize) {
				warning(
				    WARNING_MBC,
				    "MBC \"%s\" has RAM, but RAM size was set to 0",
				    mbc_Name(cartridgeType)
				);
			} else if (ramSize == 1) {
				warning(
				    WARNING_MBC,
				    "RAM size 1 (2 KiB) was specified for MBC \"%s\"",
				    mbc_Name(cartridgeType)
				);
			}
		} else if (ramSize) {
			warning(
			    WARNING_MBC,
			    "MBC \"%s\" has no RAM, but RAM size was set to %u",
			    mbc_Name(cartridgeType),
			    ramSize
			);
		}
	}

	if (sgb && oldLicensee != UNSPECIFIED && oldLicensee != 0x33) {
		warning(
		    WARNING_SGB,
		    "SGB compatibility enabled, but old licensee is 0x%02x, not 0x33",
		    oldLicensee
		);
	}

	initLogo();

	argv += musl_optind;
	if (!*argv) {
		usage.printAndExit("No input file specified (pass `-` to read from standard input)");
	}

	if (outputFilename && argc != musl_optind + 1) {
		usage.printAndExit("If `-o` is set then only a single input file may be specified");
	}

	bool failed = warnings.nbErrors > 0;
	do {
		failed |= processFilename(*argv, outputFilename);
	} while (*++argv);

	return failed;
}
