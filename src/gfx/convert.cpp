/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx/convert.hpp"

#include <algorithm>
#include <assert.h>
#include <cinttypes>
#include <errno.h>
#include <fstream>
#include <memory>
#include <optional>
#include <png.h>
#include <setjmp.h>
#include <string.h>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "defaultinitalloc.hpp"
#include "helpers.h"

#include "gfx/main.hpp"
#include "gfx/pal_packing.hpp"
#include "gfx/pal_sorting.hpp"
#include "gfx/proto_palette.hpp"

class ImagePalette {
	// Use as many slots as there are CGB colors (plus transparency)
	std::array<std::optional<Rgba>, 0x8001> _colors;

public:
	ImagePalette() = default;

	void registerColor(Rgba const &rgba) {
		decltype(_colors)::value_type &slot = _colors[rgba.cgbColor()];

		if (!slot.has_value()) {
			slot.emplace(rgba);
		} else if (*slot != rgba) {
			warning("Different colors melded together (#%08x into #%08x as %04x)", rgba.toCSS(),
			        slot->toCSS(), rgba.cgbColor()); // TODO: indicate position
		}
	}

	size_t size() const {
		return std::count_if(_colors.begin(), _colors.end(),
		                     [](decltype(_colors)::value_type const &slot) {
			                     return slot.has_value() && !slot->isTransparent();
		                     });
	}
	decltype(_colors) const &raw() const { return _colors; }

	auto begin() const { return _colors.begin(); }
	auto end() const { return _colors.end(); }
};

class Png {
	std::string const &path;
	std::filebuf file{};
	png_structp png = nullptr;
	png_infop info = nullptr;

	// These are cached for speed
	uint32_t width, height;
	DefaultInitVec<Rgba> pixels;
	ImagePalette colors;
	int colorType;
	int nbColors;
	png_colorp embeddedPal = nullptr;
	png_bytep transparencyPal = nullptr;

	[[noreturn]] static void handleError(png_structp png, char const *msg) {
		struct Png *self = reinterpret_cast<Png *>(png_get_error_ptr(png));

		fatal("Error reading input image (\"%s\"): %s", self->path.c_str(), msg);
	}

	static void handleWarning(png_structp png, char const *msg) {
		struct Png *self = reinterpret_cast<Png *>(png_get_error_ptr(png));

		warning("In input image (\"%s\"): %s", self->path.c_str(), msg);
	}

	static void readData(png_structp png, png_bytep data, size_t length) {
		struct Png *self = reinterpret_cast<Png *>(png_get_io_ptr(png));
		std::streamsize expectedLen = length;
		std::streamsize nbBytesRead = self->file.sgetn(reinterpret_cast<char *>(data), expectedLen);

		if (nbBytesRead != expectedLen) {
			fatal("Error reading input image (\"%s\"): file too short (expected at least %zd more "
			      "bytes after reading %lld)",
			      self->path.c_str(), length - nbBytesRead,
			      self->file.pubseekoff(0, std::ios_base::cur));
		}
	}

public:
	ImagePalette const &getColors() const { return colors; }

	int getColorType() const { return colorType; }

	std::tuple<int, png_const_colorp, png_bytep> getEmbeddedPal() const {
		return {nbColors, embeddedPal, transparencyPal};
	}

	uint32_t getWidth() const { return width; }

	uint32_t getHeight() const { return height; }

	Rgba &pixel(uint32_t x, uint32_t y) { return pixels[y * width + x]; }

	Rgba const &pixel(uint32_t x, uint32_t y) const { return pixels[y * width + x]; }

	bool isSuitableForGrayscale() const {
		// Check that all of the grays don't fall into the same "bin"
		if (colors.size() > options.maxPalSize()) { // Apply the Pigeonhole Principle
			options.verbosePrint("Too many colors for grayscale sorting (%zu > %" PRIu8 ")\n",
			                     colors.size(), options.maxPalSize());
			return false;
		}
		uint8_t bins = 0;
		for (auto const &color : colors) {
			if (color->isTransparent()) {
				continue;
			}
			if (!color->isGray()) {
				options.verbosePrint("Found non-gray color #%08x, not using grayscale sorting\n",
				                     color->toCSS());
				return false;
			}
			uint8_t mask = 1 << color->grayIndex();
			if (bins & mask) { // Two in the same bin!
				options.verbosePrint(
				    "Color #%08x conflicts with another one, not using grayscale sorting\n",
				    color->toCSS());
				return false;
			}
			bins |= mask;
		}
		return true;
	}

	/**
	 * Reads a PNG and notes all of its colors
	 *
	 * This code is more complicated than strictly necessary, but that's because of the API
	 * being used: the "high-level" interface doesn't provide all the transformations we need,
	 * so we use the "lower-level" one instead.
	 * We also use that occasion to only read the PNG one line at a time, since we store all of
	 * the pixel data in `pixels`, which saves on memory allocations.
	 */
	explicit Png(std::string const &filePath) : path(filePath), colors() {
		if (file.open(path, std::ios_base::in | std::ios_base::binary) == nullptr) {
			fatal("Failed to open input image (\"%s\"): %s", path.c_str(), strerror(errno));
		}

		options.verbosePrint("Opened input file\n");

		std::array<unsigned char, 8> pngHeader;

		if (file.sgetn(reinterpret_cast<char *>(pngHeader.data()), pngHeader.size())
		        != static_cast<std::streamsize>(pngHeader.size()) // Not enough bytes?
		    || png_sig_cmp(pngHeader.data(), 0, pngHeader.size()) != 0) {
			fatal("Input file (\"%s\") is not a PNG image!", path.c_str());
		}

		options.verbosePrint("PNG header signature is OK\n");

		png = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)this, handleError,
		                             handleWarning);
		if (!png) {
			fatal("Failed to allocate PNG structure: %s", strerror(errno));
		}

		info = png_create_info_struct(png);
		if (!info) {
			png_destroy_read_struct(&png, nullptr, nullptr);
			fatal("Failed to allocate PNG info structure: %s", strerror(errno));
		}

		png_set_read_fn(png, this, readData);
		png_set_sig_bytes(png, pngHeader.size());

		// TODO: png_set_crc_action(png, PNG_CRC_ERROR_QUIT, PNG_CRC_WARN_DISCARD);

		// Skipping chunks we don't use should improve performance
		// TODO: png_set_keep_unknown_chunks(png, ...);

		// Process all chunks up to but not including the image data
		png_read_info(png, info);

		int bitDepth, interlaceType; //, compressionType, filterMethod;

		png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, &interlaceType, nullptr,
		             nullptr);

		if (width % 8 != 0)
			fatal("Image width (%" PRIu32 " pixels) is not a multiple of 8!", width);
		if (height % 8 != 0)
			fatal("Image height (%" PRIu32 " pixels) is not a multiple of 8!", height);

		pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

		auto colorTypeName = [this]() {
			switch (colorType) {
			case PNG_COLOR_TYPE_GRAY:
				return "grayscale";
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				return "grayscale + alpha";
			case PNG_COLOR_TYPE_PALETTE:
				return "palette";
			case PNG_COLOR_TYPE_RGB:
				return "RGB";
			case PNG_COLOR_TYPE_RGB_ALPHA:
				return "RGB + alpha";
			default:
				fatal("Unknown color type %d", colorType);
			}
		};
		auto interlaceTypeName = [&interlaceType]() {
			switch (interlaceType) {
			case PNG_INTERLACE_NONE:
				return "not interlaced";
			case PNG_INTERLACE_ADAM7:
				return "interlaced (Adam7)";
			default:
				fatal("Unknown interlace type %d", interlaceType);
			}
		};
		options.verbosePrint("Input image: %" PRIu32 "x%" PRIu32 " pixels, %dbpp %s, %s\n", height,
		                     width, bitDepth, colorTypeName(), interlaceTypeName());

		if (png_get_PLTE(png, info, &embeddedPal, &nbColors) != 0) {
			int nbTransparentEntries;
			if (png_get_tRNS(png, info, &transparencyPal, &nbTransparentEntries, nullptr)) {
				assert(nbTransparentEntries == nbColors);
			}

			options.verbosePrint("Embedded palette has %d colors: [", nbColors);
			for (int i = 0; i < nbColors; ++i) {
				auto const &color = embeddedPal[i];
				options.verbosePrint("#%02x%02x%02x%02x%s", color.red, color.green, color.blue,
				                     transparencyPal ? transparencyPal[i] : 0xFF,
				                     i != nbColors - 1 ? ", " : "]\n");
			}
		} else {
			options.verbosePrint("No embedded palette\n");
		}

		// Set up transformations; to turn everything into RGBA888
		// TODO: it's not necessary to uniformize the pixel data (in theory), and not doing
		// so *might* improve performance, and should reduce memory usage.

		// Convert grayscale to RGB
		switch (colorType & ~PNG_COLOR_MASK_ALPHA) {
		case PNG_COLOR_TYPE_GRAY:
			png_set_gray_to_rgb(png); // This also converts tRNS to alpha
			break;
		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(png);
			break;
		}

		// If we read a tRNS chunk, convert it to alpha
		if (png_get_valid(png, info, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png);
		// Otherwise, if we lack an alpha channel, default to full opacity
		else if (!(colorType & PNG_COLOR_MASK_ALPHA))
			png_set_add_alpha(png, 0xFFFF, PNG_FILLER_AFTER);

		// Scale 16bpp back to 8 (we don't need all of that precision anyway)
		if (bitDepth == 16)
			png_set_scale_16(png);
		else if (bitDepth < 8)
			png_set_packing(png);

		// Set interlace handling (MUST be done before `png_read_update_info`)
		int nbPasses = png_set_interlace_handling(png);

		// Update `info` with the transformations
		png_read_update_info(png, info);
		// These shouldn't have changed
		assert(png_get_image_width(png, info) == width);
		assert(png_get_image_height(png, info) == height);
		// These should have changed, however
		assert(png_get_color_type(png, info) == PNG_COLOR_TYPE_RGBA);
		assert(png_get_bit_depth(png, info) == 8);

		// Now that metadata has been read, we can process the image data

		std::vector<png_byte> row(png_get_rowbytes(png, info));

		if (interlaceType == PNG_INTERLACE_NONE) {
			for (png_uint_32 y = 0; y < height; ++y) {
				png_read_row(png, row.data(), nullptr);

				for (png_uint_32 x = 0; x < width; ++x) {
					Rgba rgba(row[x * 4], row[x * 4 + 1], row[x * 4 + 2], row[x * 4 + 3]);

					colors.registerColor(rgba);
					pixel(x, y) = rgba;
				}
			}
		} else {
			// For interlace to work properly, we must read the image `nbPasses` times
			for (int pass = 0; pass < nbPasses; ++pass) {
				// The interlacing pass must be skipped if its width or height is reported as zero
				if (PNG_PASS_COLS(width, pass) == 0 || PNG_PASS_ROWS(height, pass) == 0) {
					continue;
				}

				png_uint_32 xStep = 1u << PNG_PASS_COL_SHIFT(pass);
				png_uint_32 yStep = 1u << PNG_PASS_ROW_SHIFT(pass);

				for (png_uint_32 y = PNG_PASS_START_ROW(pass); y < height; y += yStep) {
					png_bytep ptr = row.data();

					png_read_row(png, ptr, nullptr);
					for (png_uint_32 x = PNG_PASS_START_COL(pass); x < width; x += xStep) {
						Rgba rgba(ptr[0], ptr[1], ptr[2], ptr[3]);

						colors.registerColor(rgba);
						pixel(x, y) = rgba;
						ptr += 4;
					}
				}
			}
		}

		// We don't care about chunks after the image data (comments, etc.)
		png_read_end(png, nullptr);
	}

	~Png() { png_destroy_read_struct(&png, &info, nullptr); }

	class TilesVisitor {
		Png const &_png;
		bool const _columnMajor;
		uint32_t const _width, _height;
		uint32_t const _limit = _columnMajor ? _height : _width;

	public:
		TilesVisitor(Png const &png, bool columnMajor, uint32_t width, uint32_t height)
		    : _png(png), _columnMajor(columnMajor), _width(width), _height(height) {}

		class Tile {
			Png const &_png;
			uint32_t const _x, _y;

		public:
			Tile(Png const &png, uint32_t x, uint32_t y) : _png(png), _x(x), _y(y) {}

			Rgba pixel(uint32_t xOfs, uint32_t yOfs) const {
				return _png.pixel(_x + xOfs, _y + yOfs);
			}
		};

	private:
		struct iterator {
			TilesVisitor const &parent;
			uint32_t const limit;
			uint32_t x, y;

		public:
			std::pair<uint32_t, uint32_t> coords() const { return {x, y}; }
			Tile operator*() const { return {parent._png, x, y}; }

			iterator &operator++() {
				auto [major, minor] = parent._columnMajor ? std::tie(y, x) : std::tie(x, y);
				major += 8;
				if (major == limit) {
					minor += 8;
					major = 0;
				}

				return *this;
			}

			bool operator!=(iterator const &rhs) const {
				return coords() != rhs.coords(); // Compare the returned coord pairs
			}
		};

	public:
		iterator begin() const { return {*this, _limit, 0, 0}; }
		iterator end() const {
			iterator it{*this, _limit, _width - 8, _height - 8}; // Last valid one...
			return ++it; // ...now one-past-last!
		}
	};
public:
	TilesVisitor visitAsTiles(bool columnMajor) const {
		return {*this, columnMajor, width, height};
	}
};

class RawTiles {
public:
	/**
	 * A tile which only contains indices into the image's global palette
	 */
	class RawTile {
		std::array<std::array<size_t, 8>, 8> _pixelIndices{};

	public:
		// Not super clean, but it's closer to matrix notation
		size_t &operator()(size_t x, size_t y) { return _pixelIndices[y][x]; }
	};

private:
	std::vector<RawTile> _tiles;

public:
	/**
	 * Creates a new raw tile, and returns a reference to it so it can be filled in
	 */
	RawTile &newTile() {
		_tiles.emplace_back();
		return _tiles.back();
	}
};

struct AttrmapEntry {
	size_t protoPaletteID;
	uint16_t tileID;
	bool yFlip;
	bool xFlip;
};

static std::tuple<DefaultInitVec<size_t>, std::vector<Palette>>
    generatePalettes(std::vector<ProtoPalette> const &protoPalettes, Png const &png) {
	// Run a "pagination" problem solver
	// TODO: allow picking one of several solvers?
	auto [mappings, nbPalettes] = packing::overloadAndRemove(protoPalettes);
	assert(mappings.size() == protoPalettes.size());

	if (options.beVerbose) {
		options.verbosePrint("Proto-palette mappings: (%zu palette%s)\n", nbPalettes,
		                     nbPalettes != 1 ? "s" : "");
		for (size_t i = 0; i < mappings.size(); ++i) {
			options.verbosePrint("%zu -> %zu\n", i, mappings[i]);
		}
	}

	std::vector<Palette> palettes(nbPalettes);
	// Generate the actual palettes from the mappings
	for (size_t protoPalID = 0; protoPalID < mappings.size(); ++protoPalID) {
		auto &pal = palettes[mappings[protoPalID]];
		for (uint16_t color : protoPalettes[protoPalID]) {
			pal.addColor(color);
		}
	}

	// "Sort" colors in the generated palettes, see the man page for the flowchart
	auto [embPalSize, embPalRGB, embPalAlpha] = png.getEmbeddedPal();
	if (embPalRGB != nullptr) {
		sorting::indexed(palettes, embPalSize, embPalRGB, embPalAlpha);
	} else if (png.isSuitableForGrayscale()) {
		sorting::grayscale(palettes, png.getColors().raw());
	} else {
		sorting::rgb(palettes);
	}
	return {mappings, palettes};
}

static std::tuple<DefaultInitVec<size_t>, std::vector<Palette>>
    makePalsAsSpecified(std::vector<ProtoPalette> const &protoPalettes, Png const &png) {
	if (options.palSpecType == Options::EMBEDDED) {
		// Generate a palette spec from the first few colors in the embedded palette
		auto [embPalSize, embPalRGB, embPalAlpha] = png.getEmbeddedPal();
		if (embPalRGB == nullptr) {
			fatal("`-c embedded` was given, but the PNG does not have an embedded palette!");
		}

		// Fill in the palette spec
		options.palSpec.emplace_back(); // A single palette, with `#00000000`s (transparent)
		assert(options.palSpec.size() == 1);
		// TODO: abort if ignored colors are being used; do it now for a friendlier error
		// message
		if (embPalSize > options.maxPalSize()) { // Ignore extraneous colors if they are unused
			embPalSize = options.maxPalSize();
		}
		for (int i = 0; i < embPalSize; ++i) {
			options.palSpec[0][i] = Rgba(embPalRGB[i].red, embPalRGB[i].green, embPalRGB[i].blue,
			                             embPalAlpha ? embPalAlpha[i] : 0xFF);
		}
	}

	// Convert the palette spec to actual palettes
	std::vector<Palette> palettes(options.palSpec.size());
	auto palIter = palettes.begin(); // TODO: `zip`
	for (auto const &spec : options.palSpec) {
		for (size_t i = 0; i < options.maxPalSize(); ++i) {
			(*palIter)[i] = spec[i].cgbColor();
		}
		++palIter;
	}

	// Iterate through proto-palettes, and try mapping them to the specified palettes
	DefaultInitVec<size_t> mappings(protoPalettes.size());
	for (size_t i = 0; i < protoPalettes.size(); ++i) {
		ProtoPalette const &protoPal = protoPalettes[i];
		// Find the palette...
		auto iter = std::find_if(palettes.begin(), palettes.end(), [&protoPal](Palette const &pal) {
			// ...which contains all colors in this proto-pal
			return std::all_of(protoPal.begin(), protoPal.end(), [&pal](uint16_t color) {
				return std::find(pal.begin(), pal.end(), color) != pal.end();
			});
		});
		assert(iter != palettes.end()); // TODO: produce a proper error message
		mappings[i] = iter - palettes.begin();
	}

	return {mappings, palettes};
}

static void outputPalettes(std::vector<Palette> const &palettes) {
	std::filebuf output;
	output.open(options.palettes, std::ios_base::out | std::ios_base::binary);

	for (Palette const &palette : palettes) {
		for (uint16_t color : palette) {
			output.sputc(color & 0xFF);
			output.sputc(color >> 8);
		}
	}
}

namespace unoptimized {

// TODO: this is very redundant with `TileData::TileData`; try merging both?
static void outputTileData(Png const &png, DefaultInitVec<AttrmapEntry> const &attrmap,
                           std::vector<Palette> const &palettes,
                           DefaultInitVec<size_t> const &mappings) {
	std::filebuf output;
	output.open(options.output, std::ios_base::out | std::ios_base::binary);

	uint64_t remainingTiles = (png.getWidth() / 8) * (png.getHeight() / 8);
	if (remainingTiles <= options.trim) {
		return;
	}
	remainingTiles -= options.trim;

	auto iter = attrmap.begin();
	for (auto tile : png.visitAsTiles(options.columnMajor)) {
		Palette const &palette = palettes[mappings[iter->protoPaletteID]];
		for (uint32_t y = 0; y < 8; ++y) {
			uint16_t row = 0;
			for (uint32_t x = 0; x < 8; ++x) {
				row <<= 1;
				uint8_t index = palette.indexOf(tile.pixel(x, y).cgbColor());
				if (index & 1) {
					row |= 0x001;
				}
				if (index & 2) {
					row |= 0x100;
				}
			}
			output.sputc(row & 0xFF);
			if (options.bitDepth == 2) {
				output.sputc(row >> 8);
			}
		}
		++iter;

		--remainingTiles;
		if (remainingTiles == 0) {
			break;
		}
	}
	assert(remainingTiles == 0);
	assert(iter + options.trim == attrmap.end());
}

static void outputMaps(Png const &png, DefaultInitVec<AttrmapEntry> const &attrmap,
                       DefaultInitVec<size_t> const &mappings) {
	std::optional<std::filebuf> tilemapOutput, attrmapOutput;
	if (!options.tilemap.empty()) {
		tilemapOutput.emplace();
		tilemapOutput->open(options.tilemap, std::ios_base::out | std::ios_base::binary);
	}
	if (!options.attrmap.empty()) {
		attrmapOutput.emplace();
		attrmapOutput->open(options.attrmap, std::ios_base::out | std::ios_base::binary);
	}

	uint8_t tileID = 0;
	uint8_t bank = 0;
	auto iter = attrmap.begin();
	for ([[maybe_unused]] auto tile : png.visitAsTiles(options.columnMajor)) {
		if (tileID == options.maxNbTiles[bank]) {
			assert(bank == 0);
			bank = 1;
			tileID = 0;
		}

		if (tilemapOutput.has_value()) {
			tilemapOutput->sputc(tileID + options.baseTileIDs[bank]);
		}
		if (attrmapOutput.has_value()) {
			uint8_t palID = mappings[iter->protoPaletteID] & 7;
			attrmapOutput->sputc(palID | bank << 3); // The other flags are all 0
			++iter;
		}
		++tileID;
	}
	assert(iter == attrmap.end());
}

} // namespace unoptimized

static uint8_t flip(uint8_t byte) {
	// To flip all the bits, we'll flip both nibbles, then each nibble half, etc.
	byte = (byte & 0x0F) << 4 | (byte & 0xF0) >> 4;
	byte = (byte & 0x33) << 2 | (byte & 0xCC) >> 2;
	byte = (byte & 0x55) << 1 | (byte & 0xAA) >> 1;
	return byte;
}

class TileData {
	std::array<uint8_t, 16> _data;
	// The hash is a bit lax: it's the XOR of all lines, and every other nibble is identical
	// if horizontal mirroring is in effect. It should still be a reasonable tie-breaker in
	// non-pathological cases.
	uint16_t _hash;
public:
	mutable uint16_t tileID;

	TileData(Png::TilesVisitor::Tile const &tile, Palette const &palette) : _hash(0) {
		size_t writeIndex = 0;
		for (uint32_t y = 0; y < 8; ++y) {
			uint16_t bitplanes = 0;
			for (uint32_t x = 0; x < 8; ++x) {
				bitplanes <<= 1;
				uint8_t index = palette.indexOf(tile.pixel(x, y).cgbColor());
				if (index & 1) {
					bitplanes |= 1;
				}
				if (index & 2) {
					bitplanes |= 0x100;
				}
			}
			_data[writeIndex++] = bitplanes & 0xFF;
			if (options.bitDepth == 2) {
				_data[writeIndex++] = bitplanes >> 8;
			}

			// Update the hash
			_hash ^= bitplanes;
			if (options.allowMirroring) {
				// Count the line itself as mirrorred; vertical mirroring is
				// already taken care of because the symmetric line will be XOR'd
				// the same way. (...which is a problem, but probably benign.)
				_hash ^= flip(bitplanes >> 8) << 8 | flip(bitplanes & 0xFF);
			}
		}
	}

	auto const &data() const { return _data; }
	uint16_t hash() const { return _hash; }

	enum MatchType {
		NOPE,
		EXACT,
		HFLIP,
		VFLIP,
		VHFLIP,
	};
	MatchType tryMatching(TileData const &other) const {
		if (std::equal(_data.begin(), _data.end(), other._data.begin()))
			return MatchType::EXACT;

		if (options.allowMirroring) {
			// TODO
		}

		return MatchType::NOPE;
	}
	friend bool operator==(TileData const &lhs, TileData const &rhs) {
		return lhs.tryMatching(rhs) != MatchType::NOPE;
	}
};

template<>
struct std::hash<TileData> {
	std::size_t operator()(TileData const &tile) const { return tile.hash(); }
};

namespace optimized {

struct UniqueTiles {
	std::unordered_set<TileData> tileset;
	std::vector<TileData const *> tiles;

	UniqueTiles() = default;
	// Copies are likely to break pointers, so we really don't want those.
	// Copy elision should be relied on to be more sure that refs won't be invalidated, too!
	UniqueTiles(UniqueTiles const &) = delete;
	UniqueTiles(UniqueTiles &&) = default;

	/**
	 * Adds a tile to the collection, and returns its ID
	 */
	std::tuple<uint16_t, TileData::MatchType> addTile(Png::TilesVisitor::Tile const &tile,
	                                                  Palette const &palette) {
		TileData newTile(tile, palette);
		auto [tileData, inserted] = tileset.insert(newTile);

		TileData::MatchType matchType = TileData::EXACT;
		if (inserted) {
			// Give the new tile the next available unique ID
			tileData->tileID = static_cast<uint16_t>(tiles.size());
			// Pointers are never invalidated!
			tiles.emplace_back(&*tileData);
		} else {
			matchType = tileData->tryMatching(newTile);
		}
		return {tileData->tileID, matchType};
	}

	auto size() const { return tiles.size(); }

	auto begin() const { return tiles.begin(); }
	auto end() const { return tiles.end(); }
};

static UniqueTiles dedupTiles(Png const &png, DefaultInitVec<AttrmapEntry> &attrmap,
                              std::vector<Palette> const &palettes,
                              DefaultInitVec<size_t> const &mappings) {
	// Iterate throughout the image, generating tile data as we go
	// (We don't need the full tile data to be able to dedup tiles, but we don't lose anything
	// by caching the full tile data anyway, so we might as well.)
	UniqueTiles tiles;

	auto iter = attrmap.begin();
	for (auto tile : png.visitAsTiles(options.columnMajor)) {
		auto [tileID, matchType] = tiles.addTile(tile, palettes[mappings[iter->protoPaletteID]]);

		iter->xFlip = matchType == TileData::HFLIP || matchType == TileData::VHFLIP;
		iter->yFlip = matchType == TileData::VFLIP || matchType == TileData::VHFLIP;
		assert(tileID < 1 << 10);
		iter->tileID = tileID;

		++iter;
	}
	assert(iter == attrmap.end());

	// Copy elision should prevent the contained `unordered_set` from being re-constructed
	return tiles;
}

static void outputTileData(UniqueTiles const &tiles) {
	std::filebuf output;
	output.open(options.output, std::ios_base::out | std::ios_base::binary);

	uint16_t tileID = 0;
	for (auto iter = tiles.begin(), end = tiles.end() - options.trim; iter != end; ++iter) {
		TileData const *tile = *iter;
		assert(tile->tileID == tileID);
		++tileID;
		output.sputn(reinterpret_cast<char const *>(tile->data().data()), options.bitDepth * 8);
	}
}

static void outputTilemap(DefaultInitVec<AttrmapEntry> const &attrmap) {
	std::filebuf output;
	output.open(options.tilemap, std::ios_base::out | std::ios_base::binary);

	assert(options.baseTileIDs[0] == 0 && options.baseTileIDs[1] == 0); // TODO: deal with offset
	for (AttrmapEntry const &entry : attrmap) {
		output.sputc(entry.tileID & 0xFF);
	}
}

static void outputAttrmap(DefaultInitVec<AttrmapEntry> const &attrmap,
                          DefaultInitVec<size_t> const &mappings) {
	std::filebuf output;
	output.open(options.attrmap, std::ios_base::out | std::ios_base::binary);

	assert(options.baseTileIDs[0] == 0 && options.baseTileIDs[1] == 0); // TODO: deal with offset
	for (AttrmapEntry const &entry : attrmap) {
		uint8_t attr = entry.xFlip << 5 | entry.yFlip << 6;
		attr |= (entry.tileID >= options.maxNbTiles[0]) << 3;
		attr |= mappings[entry.protoPaletteID] & 7;
		output.sputc(attr);
	}
}

} // namespace optimized

void process() {
	options.verbosePrint("Using libpng %s\n", png_get_libpng_ver(nullptr));

	options.verbosePrint("Reading tiles...\n");
	Png png(options.input);
	ImagePalette const &colors = png.getColors();

	// Now, we have all the image's colors in `colors`
	// The next step is to order the palette

	if (options.beVerbose) {
		options.verbosePrint("Image colors: [ ");
		size_t i = 0;
		for (auto const &slot : colors) {
			if (!slot.has_value()) {
				continue;
			}
			options.verbosePrint("#%02x%02x%02x%02x%s", slot->red, slot->green, slot->blue,
			                     slot->alpha, i != colors.size() - 1 ? ", " : "");
			++i;
		}
		options.verbosePrint("]\n");
	}

	// Now, iterate through the tiles, generating proto-palettes as we go
	// We do this unconditionally because this performs the image validation (which we want to
	// perform even if no output is requested), and because it's necessary to generate any
	// output (with the exception of an un-duplicated tilemap, but that's an acceptable loss.)
	std::vector<ProtoPalette> protoPalettes;
	DefaultInitVec<AttrmapEntry> attrmap{};

	for (auto tile : png.visitAsTiles(options.columnMajor)) {
		ProtoPalette tileColors;
		AttrmapEntry &attrs = attrmap.emplace_back();

		for (uint32_t y = 0; y < 8; ++y) {
			for (uint32_t x = 0; x < 8; ++x) {
				tileColors.add(tile.pixel(x, y).cgbColor());
			}
		}

		// Insert the proto-palette, making sure to avoid overlaps
		for (size_t n = 0; n < protoPalettes.size(); ++n) {
			switch (tileColors.compare(protoPalettes[n])) {
			case ProtoPalette::WE_BIGGER:
				protoPalettes[n] = tileColors; // Override them
				// Remove any other proto-palettes that we encompass
				// (Example [(0, 1), (0, 2)], inserting (0, 1, 2))
				/* The following code does its job, except that references to the removed
				   proto-palettes are not updated, causing issues.
				   TODO: overlap might not be detrimental to the packing algorithm.
				   Investigation is necessary, especially if pathological cases are found.

				for (size_t i = protoPalettes.size(); --i != n;) {
				    if (tileColors.compare(protoPalettes[i]) == ProtoPalette::WE_BIGGER) {
				        protoPalettes.erase(protoPalettes.begin() + i);
				    }
				}
				*/
				[[fallthrough]];

			case ProtoPalette::THEY_BIGGER:
				// Do nothing, they already contain us
				attrs.protoPaletteID = n;
				goto contained;

			case ProtoPalette::NEITHER:
				break; // Keep going
			}
		}
		attrs.protoPaletteID = protoPalettes.size();
		protoPalettes.push_back(tileColors);
contained:;
	}

	options.verbosePrint("Image contains %zu proto-palette%s\n", protoPalettes.size(),
	                     protoPalettes.size() != 1 ? "s" : "");

	// Sort the proto-palettes by size, which improves the packing algorithm's efficiency
	// We sort after all insertions to avoid moving items: https://stackoverflow.com/a/2710332
	std::sort(
	    protoPalettes.begin(), protoPalettes.end(),
	    [](ProtoPalette const &lhs, ProtoPalette const &rhs) { return lhs.size() < rhs.size(); });

	auto [mappings, palettes] = options.palSpecType == Options::NO_SPEC
	                                ? generatePalettes(protoPalettes, png)
	                                : makePalsAsSpecified(protoPalettes, png);

	if (options.beVerbose) {
		for (auto &&palette : palettes) {
			options.verbosePrint("{ ");
			for (uint16_t colorIndex : palette) {
				options.verbosePrint("%04" PRIx16 ", ", colorIndex);
			}
			options.verbosePrint("}\n");
		}
	}

	if (!options.palettes.empty()) {
		outputPalettes(palettes);
	}

	// If deduplication is not happening, we just need to output the tile data and/or maps as-is
	if (!options.allowDedup) {
		uint32_t const nbTilesH = png.getHeight() / 8, nbTilesW = png.getWidth() / 8;

		// Check the tile count
		if (nbTilesW * nbTilesH > options.maxNbTiles[0] + options.maxNbTiles[1]) {
			fatal("Image contains %" PRIu32 " tiles, exceeding the limit of %" PRIu16 " + %" PRIu16,
			      nbTilesW * nbTilesH, options.maxNbTiles[0], options.maxNbTiles[1]);
		}

		if (!options.output.empty()) {
			options.verbosePrint("Generating unoptimized tile data...\n");
			unoptimized::outputTileData(png, attrmap, palettes, mappings);
		}

		if (!options.tilemap.empty() || !options.attrmap.empty()) {
			options.verbosePrint("Generating unoptimized tilemap and/or attrmap...\n");
			unoptimized::outputMaps(png, attrmap, mappings);
		}
	} else {
		// All of these require the deduplication process to be performed to be output
		options.verbosePrint("Deduplicating tiles...\n");
		optimized::UniqueTiles tiles = optimized::dedupTiles(png, attrmap, palettes, mappings);

		if (tiles.size() > options.maxNbTiles[0] + options.maxNbTiles[1]) {
			fatal("Image contains %zu tiles, exceeding the limit of %" PRIu16 " + %" PRIu16,
			      tiles.size(), options.maxNbTiles[0], options.maxNbTiles[1]);
		}

		if (!options.output.empty()) {
			options.verbosePrint("Generating optimized tile data...\n");
			optimized::outputTileData(tiles);
		}

		if (!options.tilemap.empty()) {
			options.verbosePrint("Generating optimized tilemap...\n");
			optimized::outputTilemap(attrmap);
		}

		if (!options.attrmap.empty()) {
			options.verbosePrint("Generating optimized attrmap...\n");
			optimized::outputAttrmap(attrmap, mappings);
		}
	}
}
