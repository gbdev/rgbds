// SPDX-License-Identifier: MIT

#include "fix/fix.hpp"
#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "platform.hpp"

#include "fix/main.hpp"
#include "fix/mbc.hpp"
#include "fix/warning.hpp"

static constexpr off_t BANK_SIZE = 0x4000;

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
	ssize_t headerSize = (options.cartridgeType & 0xFF00) == TPP1 ? 0x154 : 0x150;

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

	if (options.fixSpec & (FIX_LOGO | TRASH_LOGO)) {
		overwriteBytes(
		    rom0,
		    0x0104,
		    options.logo,
		    sizeof(options.logo),
		    options.logoFilename ? "logo" : "Nintendo logo"
		);
	}

	if (options.title) {
		overwriteBytes(
		    rom0, 0x134, reinterpret_cast<uint8_t const *>(options.title), options.titleLen, "title"
		);
	}

	if (options.gameID) {
		overwriteBytes(
		    rom0,
		    0x13F,
		    reinterpret_cast<uint8_t const *>(options.gameID),
		    options.gameIDLen,
		    "manufacturer code"
		);
	}

	if (options.model != DMG) {
		overwriteByte(rom0, 0x143, options.model == BOTH ? 0x80 : 0xC0, "CGB flag");
	}

	if (options.newLicensee) {
		overwriteBytes(
		    rom0,
		    0x144,
		    reinterpret_cast<uint8_t const *>(options.newLicensee),
		    options.newLicenseeLen,
		    "new licensee code"
		);
	}

	if (options.sgb) {
		overwriteByte(rom0, 0x146, 0x03, "SGB flag");
	}

	// If a valid MBC was specified...
	if (options.cartridgeType < MBC_NONE) {
		uint8_t byte = options.cartridgeType;

		if ((options.cartridgeType & 0xFF00) == TPP1) {
			// Cartridge type isn't directly actionable, translate it
			byte = 0xBC;
			// The other TPP1 identification bytes will be written below
		}
		overwriteByte(rom0, 0x147, byte, "cartridge type");
	}

	// ROM size will be written last, after evaluating the file's size

	if ((options.cartridgeType & 0xFF00) == TPP1) {
		uint8_t const tpp1Code[2] = {0xC1, 0x65};

		overwriteBytes(rom0, 0x149, tpp1Code, sizeof(tpp1Code), "TPP1 identification code");

		overwriteBytes(
		    rom0, 0x150, options.tpp1Rev, sizeof(options.tpp1Rev), "TPP1 revision number"
		);

		if (options.ramSize != UNSPECIFIED) {
			overwriteByte(rom0, 0x152, options.ramSize, "RAM size");
		}

		overwriteByte(rom0, 0x153, options.cartridgeType & 0xFF, "TPP1 feature flags");
	} else {
		// Regular mappers

		if (options.ramSize != UNSPECIFIED) {
			overwriteByte(rom0, 0x149, options.ramSize, "RAM size");
		}

		if (!options.japanese) {
			overwriteByte(rom0, 0x14A, 0x01, "destination code");
		}
	}

	if (options.oldLicensee != UNSPECIFIED) {
		overwriteByte(rom0, 0x14B, options.oldLicensee, "old licensee code");
	} else if (options.sgb && rom0[0x14B] != 0x33) {
		warning(
		    WARNING_SGB,
		    "SGB compatibility enabled, but old licensee was 0x%02x, not 0x33",
		    rom0[0x14B]
		);
	}

	if (options.romVersion != UNSPECIFIED) {
		overwriteByte(rom0, 0x14C, options.romVersion, "mask ROM version number");
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
				// We're going to read another bank, check that it won't be too much
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
	if (options.padValue != UNSPECIFIED) {
		// We want at least 2 banks
		if (nbBanks == 1) {
			if (rom0Len != sizeof(rom0)) {
				memset(&rom0[rom0Len], options.padValue, sizeof(rom0) - rom0Len);
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
		globalSum += options.padValue * ((nbBanks - 1) * BANK_SIZE - totalRomxLen);
	}

	// Handle the header checksum after the ROM size has been written
	if (options.fixSpec & (FIX_HEADER_SUM | TRASH_HEADER_SUM)) {
		uint8_t sum = 0;

		for (uint16_t i = 0x134; i < 0x14D; ++i) {
			sum -= rom0[i] + 1;
		}

		overwriteByte(
		    rom0, 0x14D, options.fixSpec & TRASH_HEADER_SUM ? ~sum : sum, "header checksum"
		);
	}

	if (options.fixSpec & (FIX_GLOBAL_SUM | TRASH_GLOBAL_SUM)) {
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

		if (options.fixSpec & TRASH_GLOBAL_SUM) {
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
		if (options.padValue == UNSPECIFIED) {
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
	if (options.padValue != UNSPECIFIED) {
		if (input == output) {
			if (lseek(output, 0, SEEK_END) == static_cast<off_t>(-1)) {
				// LCOV_EXCL_START
				error("Failed to seek to end of \"%s\": %s", name, strerror(errno));
				return;
				// LCOV_EXCL_STOP
			}
		}
		memset(bank, options.padValue, sizeof(bank));
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

bool fix_ProcessFile(char const *name, char const *outputName) {
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
