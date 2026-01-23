// SPDX-License-Identifier: MIT

#include "gfx/process.hpp"

#include <algorithm>
#include <array>
#include <errno.h>
#include <inttypes.h>
#include <ios>
#include <optional>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "diagnostics.hpp"
#include "file.hpp"
#include "helpers.hpp"
#include "itertools.hpp"
#include "style.hpp"
#include "verbosity.hpp"

#include "gfx/color_set.hpp"
#include "gfx/flip.hpp"
#include "gfx/main.hpp"
#include "gfx/pal_packing.hpp"
#include "gfx/pal_sorting.hpp"
#include "gfx/palette.hpp"
#include "gfx/png.hpp"
#include "gfx/rgba.hpp"
#include "gfx/warning.hpp"

static bool isBgColorTransparent() {
	return options.bgColor.has_value() && options.bgColor->isTransparent();
}

class ImagePalette {
	std::array<std::optional<Rgba>, NB_COLOR_SLOTS> _colors;

public:
	ImagePalette() = default;

	// Registers a color in the palette.
	// If the newly inserted color "conflicts" with another one (different color, but same CGB
	// color), then the other color is returned. Otherwise, `nullptr` is returned.
	[[nodiscard]]
	Rgba const *registerColor(Rgba const &rgba) {
		uint16_t color = rgba.cgbColor();
		std::optional<Rgba> &slot = _colors[color];

		if (color == Rgba::transparent && !isBgColorTransparent()) {
			options.hasTransparentPixels = true;
		}

		if (!slot.has_value()) {
			slot.emplace(rgba);
		} else if (*slot != rgba) {
			assume(slot->cgbColor() != UINT16_MAX);
			return &*slot;
		}
		return nullptr;
	}

	size_t size() const {
		return std::count_if(RANGE(_colors), [](std::optional<Rgba> const &slot) {
			return slot.has_value() && slot->isOpaque();
		});
	}
	decltype(_colors) const &raw() const { return _colors; }

	auto begin() const { return _colors.begin(); }
	auto end() const { return _colors.end(); }
};

struct Image {
	Png png{};
	ImagePalette colors{};

	Rgba &pixel(uint32_t x, uint32_t y) { return png.pixels[y * png.width + x]; }
	Rgba const &pixel(uint32_t x, uint32_t y) const { return png.pixels[y * png.width + x]; }

	enum GrayscaleResult {
		GRAY_OK,
		GRAY_TOO_MANY,
		GRAY_NONGRAY,
		GRAY_CONFLICT,
	};
	std::pair<GrayscaleResult, std::optional<Rgba>> isSuitableForGrayscale() const {
		// Check that all of the grays don't fall into the same "bin"
		if (colors.size() > options.maxOpaqueColors()) { // Apply the Pigeonhole Principle
			verbosePrint(
			    VERB_DEBUG,
			    "Too many colors for grayscale sorting (%zu > %" PRIu8 ")\n",
			    colors.size(),
			    options.maxOpaqueColors()
			);
			return {GrayscaleResult::GRAY_TOO_MANY, std::nullopt};
		}
		uint8_t bins = 0;
		for (std::optional<Rgba> const &color : colors) {
			if (!color.has_value() || color->isTransparent()) {
				continue;
			}
			if (!color->isGray()) {
				verbosePrint(
				    VERB_DEBUG,
				    "Found non-gray color #%08x, not using grayscale sorting\n",
				    color->toCSS()
				);
				return {GrayscaleResult::GRAY_NONGRAY, color};
			}
			uint8_t mask = 1 << color->grayIndex();
			if (bins & mask) { // Two in the same bin!
				verbosePrint(
				    VERB_DEBUG,
				    "Color #%08x conflicts with another one, not using grayscale sorting\n",
				    color->toCSS()
				);
				return {GrayscaleResult::GRAY_CONFLICT, color};
			}
			bins |= mask;
		}
		return {GrayscaleResult::GRAY_OK, std::nullopt};
	}

	explicit Image(std::string const &path) {
		File input;
		if (input.open(path, std::ios_base::in | std::ios_base::binary) == nullptr) {
			fatal("Failed to open input image (\"%s\"): %s", input.c_str(path), strerror(errno));
		}

		png = Png(input.c_str(path), *input);

		// Validate input slice
		if (options.inputSlice.width == 0 && png.width % 8 != 0) {
			fatal("Image width (%" PRIu32 " pixels) is not a multiple of 8", png.width);
		}
		if (options.inputSlice.height == 0 && png.height % 8 != 0) {
			fatal("Image height (%" PRIu32 " pixels) is not a multiple of 8", png.height);
		}
		if (options.inputSlice.right() > png.width || options.inputSlice.bottom() > png.height) {
			error(
			    "Image slice ((%" PRIu16 ", %" PRIu16 ") to (%" PRIu32 ", %" PRIu32
			    ")) is outside the image bounds (%" PRIu32 "x%" PRIu32 ")",
			    options.inputSlice.left,
			    options.inputSlice.top,
			    options.inputSlice.right(),
			    options.inputSlice.bottom(),
			    png.width,
			    png.height
			);
			if (options.inputSlice.width % 8 == 0 && options.inputSlice.height % 8 == 0) {
				fprintf(
				    stderr,
				    "       (Did you mean the slice \"%" PRIu32 ",%" PRIu32 ":%" PRId32 ",%" PRId32
				    "\"? The width and height are in tiles, not pixels!)\n",
				    options.inputSlice.left,
				    options.inputSlice.top,
				    options.inputSlice.width / 8,
				    options.inputSlice.height / 8
				);
			}
			giveUp();
		}

		// Holds colors whose alpha value is ambiguous to avoid erroring about them twice.
		std::unordered_set<uint32_t> ambiguous;
		// Holds fused color pairs to avoid warning about them twice.
		// We don't need to worry about transitivity, as ImagePalette slots are immutable once
		// assigned, and conflicts always occur between that and another color.
		// For the same reason, we don't need to worry about order, either.
		auto hashPair = [](std::pair<uint32_t, uint32_t> const &pair) {
			return pair.first * 31 + pair.second;
		};
		std::unordered_set<std::pair<uint32_t, uint32_t>, decltype(hashPair)> fusions;

		// Register colors from `png` into `colors`
		for (uint32_t y = 0; y < png.height; ++y) {
			for (uint32_t x = 0; x < png.width; ++x) {
				if (Rgba const &color = pixel(x, y); color.isTransparent() == color.isOpaque()) {
					// Report ambiguously transparent or opaque colors
					if (uint32_t css = color.toCSS(); ambiguous.find(css) == ambiguous.end()) {
						error(
						    "Color #%08x is neither transparent (alpha < %u) nor opaque (alpha >= "
						    "%u) (first seen at (%" PRIu32 ", %" PRIu32 "))",
						    css,
						    Rgba::transparency_threshold,
						    Rgba::opacity_threshold,
						    x,
						    y
						);
						ambiguous.insert(css); // Do not report this color again
					}
				} else if (Rgba const *other = colors.registerColor(color); other) {
					// Report fused colors that reduce to the same RGB555 value
					if (std::pair fused{color.toCSS(), other->toCSS()};
					    fusions.find(fused) == fusions.end()) {
						warnx(
						    "Colors #%08x and #%08x both reduce to the same Game Boy color $%04x "
						    "(first seen at (%" PRIu32 ", %" PRIu32 "))",
						    fused.first,
						    fused.second,
						    color.cgbColor(),
						    x,
						    y
						);
						fusions.insert(fused); // Do not report this fusion again
					}
				}
			}
		}
	}

	class TilesVisitor {
		Image const &_image;
		bool const _columnMajor;
		uint32_t const _width, _height;
		uint32_t const _limit = _columnMajor ? _height : _width;

	public:
		TilesVisitor(Image const &image, bool columnMajor, uint32_t width, uint32_t height)
		    : _image(image), _columnMajor(columnMajor), _width(width), _height(height) {}

		class Tile {
			Image const &_image;

		public:
			uint32_t const x, y;

			Tile(Image const &image, uint32_t x_, uint32_t y_) : _image(image), x(x_), y(y_) {}

			Rgba pixel(uint32_t xOfs, uint32_t yOfs) const {
				return _image.pixel(x + xOfs, y + yOfs);
			}
		};

	private:
		struct Iterator {
			TilesVisitor const &parent;
			uint32_t const limit;
			uint32_t x, y;

			std::pair<uint32_t, uint32_t> coords() const {
				return {x + options.inputSlice.left, y + options.inputSlice.top};
			}
			Tile operator*() const {
				return {parent._image, x + options.inputSlice.left, y + options.inputSlice.top};
			}

			Iterator &operator++() {
				auto [major, minor] = parent._columnMajor ? std::tie(y, x) : std::tie(x, y);
				major += 8;
				if (major == limit) {
					minor += 8;
					major = 0;
				}
				return *this;
			}

			bool operator==(Iterator const &rhs) const { return coords() == rhs.coords(); }
		};

	public:
		Iterator begin() const { return {*this, _limit, 0, 0}; }
		Iterator end() const {
			Iterator it{*this, _limit, _width - 8, _height - 8}; // Last valid one...
			return ++it;                                         // ...now one-past-last!
		}
	};

public:
	TilesVisitor visitAsTiles() const {
		return {
		    *this,
		    options.columnMajor,
		    options.inputSlice.width ? options.inputSlice.width * 8 : png.width,
		    options.inputSlice.height ? options.inputSlice.height * 8 : png.height,
		};
	}
};

class RawTiles {
	// A tile which only contains indices into the image's global palette
	class RawTile {
		std::array<std::array<size_t, 8>, 8> _pixelIndices{};

	public:
		// Not super clean, but it's closer to matrix notation
		size_t &operator()(size_t x, size_t y) { return _pixelIndices[y][x]; }
	};

private:
	std::vector<RawTile> _tiles;

public:
	// Creates a new raw tile, and returns a reference to it so it can be filled in
	RawTile &newTile() { return _tiles.emplace_back(); }
};

struct AttrmapEntry {
	// This field can either be a color set ID, or `transparent` to indicate that the
	// corresponding tile is fully transparent. If you are looking to get the palette ID for this
	// attrmap entry while correctly handling the above, use `getPalID`.
	size_t colorSetID; // Only this field is used when outputting "unoptimized" data
	uint8_t tileID;    // This is the ID as it will be output to the tilemap
	bool bank;
	bool yFlip;
	bool xFlip;

	static constexpr size_t transparent = static_cast<size_t>(-1);
	static constexpr size_t background = static_cast<size_t>(-2);

	bool isBackgroundTile() const { return colorSetID == background; }
	size_t getPalID(std::vector<size_t> const &mappings) const {
		return mappings[isBackgroundTile() || colorSetID == transparent ? 0 : colorSetID];
	}
};

static void generatePalSpec(Image const &image) {
	// Generate a palette spec from the first few colors in the embedded palette
	std::vector<Rgba> const &embPal = image.png.palette;
	if (embPal.empty()) {
		fatal("\"-c embedded\" was given, but the PNG does not have an embedded palette");
	}

	// Ignore extraneous colors if they are unused
	size_t nbColors = embPal.size();
	if (nbColors > options.maxOpaqueColors()) {
		nbColors = options.maxOpaqueColors();
	}

	// Fill in the palette spec
	options.palSpec.clear();
	auto &palette = options.palSpec.emplace_back();
	assume(nbColors <= palette.size());
	for (size_t i = 0; i < nbColors; ++i) {
		palette[i] = embPal[i];
	}
}

static std::pair<std::vector<size_t>, std::vector<Palette>>
    generatePalettes(std::vector<ColorSet> const &colorSets, Image const &image) {
	// Run a "pagination" problem solver
	auto [mappings, nbPalettes] = overloadAndRemove(colorSets);
	assume(mappings.size() == colorSets.size());

	// LCOV_EXCL_START
	if (checkVerbosity(VERB_INFO)) {
		style_Set(stderr, STYLE_MAGENTA, false);
		fprintf(
		    stderr, "Color set mappings: (%zu palette%s)\n", nbPalettes, nbPalettes != 1 ? "s" : ""
		);
		for (size_t i = 0; i < mappings.size(); ++i) {
			fprintf(stderr, "%zu -> %zu\n", i, mappings[i]);
		}
		style_Reset(stderr);
	}
	// LCOV_EXCL_STOP

	std::vector<Palette> palettes(nbPalettes);
	// If the image contains at least one transparent pixel, force transparency in the first slot of
	// all palettes
	if (options.hasTransparentPixels) {
		for (Palette &pal : palettes) {
			pal.colors[0] = Rgba::transparent;
		}
	}
	// Generate the actual palettes from the mappings
	for (size_t colorSetID = 0; colorSetID < mappings.size(); ++colorSetID) {
		Palette &pal = palettes[mappings[colorSetID]];
		for (uint16_t color : colorSets[colorSetID]) {
			pal.addColor(color);
		}
	}

	// "Sort" colors in the generated palettes, see the man page for the flowchart
	if (options.palSpecType == Options::DMG) {
		sortGrayscale(palettes, image.colors.raw());
	} else if (!image.png.palette.empty()) {
		warning(
		    WARNING_EMBEDDED,
		    "Sorting palette colors by PNG's embedded PLTE chunk without '-c/--colors embedded'"
		);
		sortIndexed(palettes, image.png.palette);
	} else if (image.isSuitableForGrayscale().first == Image::GRAY_OK) {
		sortGrayscale(palettes, image.colors.raw());
	} else {
		sortRgb(palettes);
	}
	return {mappings, palettes};
}

static std::pair<std::vector<size_t>, std::vector<Palette>>
    makePalsAsSpecified(std::vector<ColorSet> const &colorSets) {
	// Convert the palette spec to actual palettes
	std::vector<Palette> palettes(options.palSpec.size());
	for (auto [spec, pal] : zip(options.palSpec, palettes)) {
		for (size_t i = 0; i < options.nbColorsPerPal; ++i) {
			// If the spec has a gap, there's no need to copy anything.
			if (spec[i].has_value() && spec[i]->isOpaque()) {
				pal[i] = spec[i]->cgbColor();
			}
		}
	}

	auto listColors = [](auto const &list) {
		static char buf[sizeof(", $xxxx, $xxxx, $xxxx, $xxxx")];
		char *ptr = buf;
		for (uint16_t color : list) {
			ptr += snprintf(ptr, sizeof(", $xxxx"), ", $%04x", color);
		}
		return &buf[literal_strlen(", ")];
	};

	// Iterate through color sets, and try mapping them to the specified palettes
	std::vector<size_t> mappings(colorSets.size());
	bool bad = false;
	for (size_t i = 0; i < colorSets.size(); ++i) {
		ColorSet const &colorSet = colorSets[i];
		// Find the palette...
		auto iter = std::find_if(RANGE(palettes), [&colorSet](Palette const &pal) {
			// ...which contains all colors in this color set
			return std::all_of(RANGE(colorSet), [&pal](uint16_t color) {
				return std::find(RANGE(pal), color) != pal.end();
			});
		});

		if (iter == palettes.end()) {
			assume(!colorSet.empty());
			error("Failed to fit tile colors [%s] in specified palettes", listColors(colorSet));
			bad = true;
		}
		mappings[i] = iter - palettes.begin(); // Bogus value, but whatever
	}
	if (bad) {
		fprintf(
		    stderr,
		    "note: The following palette%s specified:\n",
		    palettes.size() == 1 ? " was" : "s were"
		);
		for (Palette const &pal : palettes) {
			fprintf(stderr, "        [%s]\n", listColors(pal));
		}
		giveUp();
	}

	return {mappings, palettes};
}

static void outputPalettes(std::vector<Palette> const &palettes) {
	// LCOV_EXCL_START
	if (checkVerbosity(VERB_INFO)) {
		style_Set(stderr, STYLE_MAGENTA, false);
		for (Palette const &palette : palettes) {
			fputs("{ ", stderr);
			for (uint16_t colorIndex : palette) {
				fprintf(stderr, "%04" PRIx16 ", ", colorIndex);
			}
			fputs("}\n", stderr);
		}
		style_Reset(stderr);
	}
	// LCOV_EXCL_STOP

	if (palettes.size() > options.nbPalettes) {
		// If the palette generation is wrong, other (dependee) operations are likely to be
		// nonsensical, so fatal-error outright
		fatal(
		    "Generated %zu palettes, over the maximum of %" PRIu16,
		    palettes.size(),
		    options.nbPalettes
		);
	}

	if (!options.palettes.empty()) {
		File output;
		if (!output.open(options.palettes, std::ios_base::out | std::ios_base::binary)) {
			// LCOV_EXCL_START
			fatal("Failed to create \"%s\": %s", output.c_str(options.palettes), strerror(errno));
			// LCOV_EXCL_STOP
		}

		for (Palette const &palette : palettes) {
			for (uint8_t i = 0; i < options.nbColorsPerPal; ++i) {
				// Will output `UINT16_MAX` for unused slots
				uint16_t color = palette.colors[i];
				output->sputc(color & 0xFF);
				output->sputc(color >> 8);
			}
		}
	}
}

static void hashBitplanes(uint16_t bitplanes, uint16_t &hash) {
	hash ^= bitplanes;
	if (options.allowMirroringX) {
		// Count the line itself as mirrored, which ensures the same hash as the tile's horizontal
		// flip; vertical mirroring is already taken care of because the symmetric line will be
		// XOR'd the same way. (This can trivially create some collisions, but real-world tile data
		// generally doesn't trigger them.)
		hash ^= flipTable[bitplanes >> 8] << 8 | flipTable[bitplanes & 0xFF];
	}
}

class TileData {
	// Importantly, `TileData` is **always** 2bpp.
	// If the active bit depth is 1bpp, all tiles are processed as 2bpp nonetheless, but emitted as
	// 1bpp. This massively simplifies internal processing, since bit depth is always identical
	// outside of I/O / serialization boundaries.
	std::array<uint8_t, 16> _data;
	// The hash is a bit lax: it's the XOR of all lines, and every other nibble is identical
	// if horizontal mirroring is in effect. It should still be a reasonable tie-breaker in
	// non-pathological cases.
	uint16_t _hash;
public:
	// This is an index within the "global" pool; no bank info is encoded here
	// It's marked as `mutable` so that it can be modified even on a `const` object;
	// this is necessary because the `set` in which it's inserted refuses any modification for fear
	// of altering the element's hash, but the tile ID is not part of it.
	mutable uint16_t tileID;

	static uint16_t
	    rowBitplanes(Image::TilesVisitor::Tile const &tile, Palette const &palette, uint32_t y) {
		uint16_t row = 0;
		for (uint32_t x = 0; x < 8; ++x) {
			row <<= 1;
			uint8_t index = palette.indexOf(tile.pixel(x, y).cgbColor());
			assume(index < palette.size()); // The color should be in the palette
			if (index & 1) {
				row |= 1;
			}
			if (index & 2) {
				row |= 0x100;
			}
		}
		return row;
	}

	TileData(std::array<uint8_t, 16> &&raw) : _data(raw), _hash(0) {
		for (uint8_t y = 0; y < 8; ++y) {
			uint16_t bitplanes = _data[y * 2] | _data[y * 2 + 1] << 8;
			hashBitplanes(bitplanes, _hash);
		}
	}

	TileData(Image::TilesVisitor::Tile const &tile, Palette const &palette) : _hash(0) {
		size_t writeIndex = 0;
		for (uint32_t y = 0; y < 8; ++y) {
			uint16_t bitplanes = rowBitplanes(tile, palette, y);
			hashBitplanes(bitplanes, _hash);

			_data[writeIndex++] = bitplanes & 0xFF;
			_data[writeIndex++] = bitplanes >> 8;
		}
	}

	std::array<uint8_t, 16> const &data() const { return _data; }
	uint16_t hash() const { return _hash; }

	enum MatchType {
		NOPE,
		EXACT,
		HFLIP,
		VFLIP,
		VHFLIP,
	};
	MatchType tryMatching(TileData const &other) const {
		// Check for strict equality first, as that can typically be optimized, and it allows
		// hoisting the mirroring check out of the loop
		if (_data == other._data) {
			return MatchType::EXACT;
		}

		// Check if we have horizontal mirroring, which scans the array forward again
		if (options.allowMirroringX
		    && std::equal(RANGE(_data), other._data.begin(), [](uint8_t lhs, uint8_t rhs) {
			       return lhs == flipTable[rhs];
		       })) {
			return MatchType::HFLIP;
		}

		// The remaining possibilities for matching all require vertical mirroring
		if (!options.allowMirroringY) {
			return MatchType::NOPE;
		}

		// Check if we have vertical or vertical+horizontal mirroring, for which we have to read
		// bitplane *pairs*  backwards
		bool hasVFlip = true, hasVHFlip = true;
		for (uint8_t i = 0; i < _data.size(); ++i) {
			// Flip the bottom bit to get the corresponding row's bitplane 0/1
			// (This works because the array size is even)
			uint8_t lhs = _data[i], rhs = other._data[(15 - i) ^ 1];
			if (lhs != rhs) {
				hasVFlip = false;
			}
			if (lhs != flipTable[rhs]) {
				hasVHFlip = false;
			}
			if (!hasVFlip && !hasVHFlip) {
				return MatchType::NOPE; // If both have been eliminated, all hope is lost!
			}
		}

		// If we have both (i.e. we have symmetry), default to vflip only
		if (hasVFlip) {
			return MatchType::VFLIP;
		}

		// If we allow both and have both, then use both
		if (options.allowMirroringX && hasVHFlip) {
			return MatchType::VHFLIP;
		}

		return MatchType::NOPE;
	}
	bool operator==(TileData const &rhs) const { return tryMatching(rhs) != MatchType::NOPE; }
};

template<>
struct std::hash<TileData> {
	size_t operator()(TileData const &tile) const { return tile.hash(); }
};

static void outputUnoptimizedTileData(
    Image const &image,
    std::vector<AttrmapEntry> const &attrmap,
    std::vector<Palette> const &palettes,
    std::vector<size_t> const &mappings
) {
	File output;
	if (!output.open(options.output, std::ios_base::out | std::ios_base::binary)) {
		// LCOV_EXCL_START
		fatal("Failed to create \"%s\": %s", output.c_str(options.output), strerror(errno));
		// LCOV_EXCL_STOP
	}

	uint16_t widthTiles = options.inputSlice.width ? options.inputSlice.width : image.png.width / 8;
	uint16_t heightTiles =
	    options.inputSlice.height ? options.inputSlice.height : image.png.height / 8;
	uint64_t nbTiles = widthTiles * heightTiles;
	uint64_t nbKeptTiles = nbTiles > options.trim ? nbTiles - options.trim : 0;
	uint64_t tileIdx = 0;

	for (auto const &[tile, attr] : zip(image.visitAsTiles(), attrmap)) {
		// Do not emit fully-background tiles.
		if (attr.isBackgroundTile()) {
			++tileIdx;
			continue;
		}

		// If the tile is fully transparent, this defaults to palette 0.
		Palette const &palette = palettes[attr.getPalID(mappings)];

		bool empty = true;
		for (uint32_t y = 0; y < 8; ++y) {
			uint16_t bitplanes = TileData::rowBitplanes(tile, palette, y);
			if (bitplanes != 0) {
				empty = false;
			}
			if (tileIdx < nbKeptTiles) {
				output->sputc(bitplanes & 0xFF);
				if (options.bitDepth == 2) {
					output->sputc(bitplanes >> 8);
				}
			}
		}

		if (!empty && tileIdx >= nbKeptTiles) {
			warning(
			    WARNING_TRIM_NONEMPTY, "Trimming a nonempty tile (configure with '-x/--trim-end')"
			);
			break; // Don't repeat the warning for subsequent tiles
		}
		++tileIdx;
	}
	assume(nbKeptTiles <= tileIdx && tileIdx <= nbTiles);
}

static void outputUnoptimizedMaps(
    std::vector<AttrmapEntry> const &attrmap, std::vector<size_t> const &mappings
) {
	std::optional<File> tilemapOutput, attrmapOutput, palmapOutput;
	auto autoOpenPath = [](std::string const &path, std::optional<File> &file) {
		if (!path.empty()) {
			file.emplace();
			if (!file->open(path, std::ios_base::out | std::ios_base::binary)) {
				// LCOV_EXCL_START
				fatal("Failed to create \"%s\": %s", file->c_str(options.tilemap), strerror(errno));
				// LCOV_EXCL_STOP
			}
		}
	};
	autoOpenPath(options.tilemap, tilemapOutput);
	autoOpenPath(options.attrmap, attrmapOutput);
	autoOpenPath(options.palmap, palmapOutput);

	uint8_t tileID = 0;
	uint8_t bank = 0;
	for (AttrmapEntry const &attr : attrmap) {
		if (tilemapOutput.has_value()) {
			(*tilemapOutput)
			    ->sputc((attr.isBackgroundTile() ? 0 : tileID) + options.baseTileIDs[bank]);
		}
		uint8_t palID = attr.getPalID(mappings) + options.basePalID;
		if (attrmapOutput.has_value()) {
			(*attrmapOutput)->sputc((palID & 0b111) | bank << 3); // The other flags are all 0
		}
		if (palmapOutput.has_value()) {
			(*palmapOutput)->sputc(palID);
		}

		// Background tiles are skipped in the tile data, so they should be skipped in the maps too.
		if (attr.isBackgroundTile()) {
			continue;
		}

		// Compare with `maxNbTiles` *before* incrementing, due to unsigned overflow!
		if (tileID + 1 < options.maxNbTiles[bank]) {
			++tileID;
		} else {
			assume(bank == 0);
			bank = 1;
			tileID = 0;
		}
	}
}

struct UniqueTiles {
	std::unordered_set<TileData> tileset;
	std::vector<TileData const *> tiles;

	UniqueTiles() = default;
	// Copies are likely to break pointers, so we really don't want those.
	// Copy elision should be relied on to be more sure that refs won't be invalidated, too!
	UniqueTiles(UniqueTiles const &) = delete;
	UniqueTiles(UniqueTiles &&) = default;

	// Adds a tile to the collection, and returns its ID
	std::pair<uint16_t, TileData::MatchType> addTile(TileData newTile) {
		if (auto [tileData, inserted] = tileset.insert(newTile); inserted) {
			// Give the new tile the next available unique ID
			tileData->tileID = static_cast<uint16_t>(tiles.size());
			tiles.emplace_back(&*tileData); // Pointers are never invalidated!
			return {tileData->tileID, TileData::NOPE};
		} else {
			return {tileData->tileID, tileData->tryMatching(newTile)};
		}
	}

	size_t size() const { return tiles.size(); }

	auto begin() const { return tiles.begin(); }
	auto end() const { return tiles.end(); }
};

// Generate tile data while deduplicating unique tiles (via mirroring if enabled)
// Additionally, while we have the info handy, convert from the 16-bit "global" tile IDs to
// 8-bit tile IDs + the bank bit; this will save the work when we output the data later (potentially
// twice)
static UniqueTiles dedupTiles(
    Image const &image,
    std::vector<AttrmapEntry> &attrmap,
    std::vector<Palette> const &palettes,
    std::vector<size_t> const &mappings
) {
	// Iterate throughout the image, generating tile data as we go
	// (We don't need the full tile data to be able to dedup tiles, but we don't lose anything
	// by caching the full tile data anyway, so we might as well.)
	UniqueTiles tiles;

	if (!options.inputTileset.empty()) {
		File inputTileset;
		if (!inputTileset.open(options.inputTileset, std::ios::in | std::ios::binary)) {
			fatal("Failed to open \"%s\": %s", options.inputTileset.c_str(), strerror(errno));
		}

		std::array<uint8_t, 16> tile;
		size_t const tileSize = options.bitDepth * 8;
		for (;;) {
			// It's okay to cast between character types.
			size_t len = inputTileset->sgetn(reinterpret_cast<char *>(tile.data()), tileSize);
			if (len == 0) { // EOF!
				break;
			} else if (len != tileSize) {
				fatal(
				    "\"%s\" does not contain a multiple of %zu bytes; is it actually tile data?",
				    options.inputTileset.c_str(),
				    tileSize
				);
			} else if (len == 8) {
				// Expand the tile data to 2bpp.
				for (size_t i = 8; i--;) {
					tile[i * 2 + 1] = 0;
					tile[i * 2] = tile[i];
				}
			}

			auto [tileID, matchType] = tiles.addTile(std::move(tile));

			if (matchType != TileData::NOPE) {
				error(
				    "The input tileset's tile #%hu was deduplicated; please check that your "
				    "deduplication flags ('-u', '-m') are consistent with what was used to "
				    "generate the input tileset",
				    tileID
				);
			}
		}
	}

	bool inputWithoutOutput = !options.inputTileset.empty() && options.output.empty();
	for (auto const &[tile, attr] : zip(image.visitAsTiles(), attrmap)) {
		if (attr.isBackgroundTile()) {
			attr.xFlip = false;
			attr.yFlip = false;
			attr.bank = 0;
			attr.tileID = 0;
		} else {
			auto [tileID, matchType] = tiles.addTile({tile, palettes[mappings[attr.colorSetID]]});

			if (inputWithoutOutput && matchType == TileData::NOPE) {
				error(
				    "Tile at (%" PRIu32 ", %" PRIu32
				    ") is not within the input tileset, and '-o' was not given",
				    tile.x,
				    tile.y
				);
			}

			attr.xFlip = matchType == TileData::HFLIP || matchType == TileData::VHFLIP;
			attr.yFlip = matchType == TileData::VFLIP || matchType == TileData::VHFLIP;
			attr.bank = tileID >= options.maxNbTiles[0];
			attr.tileID = (attr.bank ? tileID - options.maxNbTiles[0] : tileID)
			              + options.baseTileIDs[attr.bank];
		}
	}

	// Copy elision should prevent the contained `unordered_set` from being re-constructed
	return tiles;
}

static void outputTileData(UniqueTiles const &tiles) {
	File output;
	if (!output.open(options.output, std::ios_base::out | std::ios_base::binary)) {
		// LCOV_EXCL_START
		fatal("Failed to create \"%s\": %s", output.c_str(options.output), strerror(errno));
		// LCOV_EXCL_STOP
	}

	uint16_t tileID = 0;
	for (auto iter = tiles.begin(), end = tiles.end() - options.trim; iter != end; ++iter) {
		TileData const *tile = *iter;
		assume(tile->tileID == tileID);
		++tileID;
		if (options.bitDepth == 2) {
			output->sputn(reinterpret_cast<char const *>(tile->data().data()), 16);
		} else {
			assume(options.bitDepth == 1);
			for (size_t y = 0; y < 8; ++y) {
				output->sputc(tile->data()[y * 2]);
			}
		}
	}
}

static void outputTilemap(std::vector<AttrmapEntry> const &attrmap) {
	File output;
	if (!output.open(options.tilemap, std::ios_base::out | std::ios_base::binary)) {
		// LCOV_EXCL_START
		fatal("Failed to create \"%s\": %s", output.c_str(options.tilemap), strerror(errno));
		// LCOV_EXCL_STOP
	}

	for (AttrmapEntry const &entry : attrmap) {
		output->sputc(entry.tileID); // The tile ID has already been converted
	}
}

static void
    outputAttrmap(std::vector<AttrmapEntry> const &attrmap, std::vector<size_t> const &mappings) {
	File output;
	if (!output.open(options.attrmap, std::ios_base::out | std::ios_base::binary)) {
		// LCOV_EXCL_START
		fatal("Failed to create \"%s\": %s", output.c_str(options.attrmap), strerror(errno));
		// LCOV_EXCL_STOP
	}

	for (AttrmapEntry const &entry : attrmap) {
		uint8_t attr = entry.xFlip << 5 | entry.yFlip << 6;
		attr |= entry.bank << 3;
		attr |= (entry.getPalID(mappings) + options.basePalID) & 0b111;
		output->sputc(attr);
	}
}

static void
    outputPalmap(std::vector<AttrmapEntry> const &attrmap, std::vector<size_t> const &mappings) {
	File output;
	if (!output.open(options.palmap, std::ios_base::out | std::ios_base::binary)) {
		// LCOV_EXCL_START
		fatal("Failed to create \"%s\": %s", output.c_str(options.palmap), strerror(errno));
		// LCOV_EXCL_STOP
	}

	for (AttrmapEntry const &entry : attrmap) {
		output->sputc(entry.getPalID(mappings) + options.basePalID);
	}
}

void processPalettes() {
	verbosePrint(VERB_CONFIG, "Using libpng %s\n", png_get_libpng_ver(nullptr));

	std::vector<ColorSet> colorSets;
	std::vector<Palette> palettes;
	std::tie(std::ignore, palettes) = makePalsAsSpecified(colorSets);

	outputPalettes(palettes);
}

void process() {
	verbosePrint(VERB_CONFIG, "Using libpng %s\n", png_get_libpng_ver(nullptr));

	verbosePrint(VERB_NOTICE, "Reading tiles...\n");
	Image image(options.input); // This also sets `hasTransparentPixels` as a side effect

	// LCOV_EXCL_START
	if (checkVerbosity(VERB_INFO)) {
		style_Set(stderr, STYLE_MAGENTA, false);
		fputs("Image colors: [ ", stderr);
		for (std::optional<Rgba> const &slot : image.colors) {
			if (!slot.has_value()) {
				continue;
			}
			fprintf(stderr, "#%08x, ", slot->toCSS());
		}
		fputs("]\n", stderr);
		style_Reset(stderr);
	}
	// LCOV_EXCL_STOP

	if (options.palSpecType == Options::DMG) {
		char const *prefix =
		    "Image is not compatible with a DMG palette specification: it contains";
		if (options.hasTransparentPixels) {
			fatal("%s transparent pixels", prefix);
		}
		switch (auto const [result, color] = image.isSuitableForGrayscale(); result) {
		case Image::GRAY_OK:
			break;
		case Image::GRAY_TOO_MANY:
			fatal("%s too many colors (%zu)", prefix, image.colors.size());
		case Image::GRAY_NONGRAY:
			fatal("%s a non-gray color #%08x", prefix, color->toCSS());
		case Image::GRAY_CONFLICT:
			fatal(
			    "%s a color #%08x that reduces to the same gray shade as another one",
			    prefix,
			    color->toCSS()
			);
		}
	}

	// Now, iterate through the tiles, generating color sets as we go
	// We do this unconditionally because this performs the image validation (which we want to
	// perform even if no output is requested), and because it's necessary to generate any
	// output (with the exception of an un-duplicated tilemap, but that's an acceptable loss.)
	std::vector<ColorSet> colorSets;
	std::vector<AttrmapEntry> attrmap{};

	for (auto tile : image.visitAsTiles()) {
		AttrmapEntry &attrs = attrmap.emplace_back();

		// Count the unique non-transparent colors for packing
		std::unordered_set<uint16_t> tileColors;
		for (uint32_t y = 0; y < 8; ++y) {
			for (uint32_t x = 0; x < 8; ++x) {
				if (Rgba color = tile.pixel(x, y);
				    color.isOpaque() || !options.hasTransparentPixels) {
					tileColors.insert(color.cgbColor());
				}
			}
		}

		if (tileColors.size() > options.maxOpaqueColors()) {
			fatal(
			    "Tile at (%" PRIu32 ", %" PRIu32 ") has %zu colors, more than %" PRIu8,
			    tile.x,
			    tile.y,
			    tileColors.size(),
			    options.maxOpaqueColors()
			);
		}

		if (tileColors.empty()) {
			// "Empty" color sets screw with the packing process, so discard those
			assume(!isBgColorTransparent());
			attrs.colorSetID = AttrmapEntry::transparent;
			continue;
		}

		ColorSet colorSet;
		for (uint16_t color : tileColors) {
			colorSet.add(color);
		}

		if (options.bgColor.has_value()
		    && std::find(RANGE(tileColors), options.bgColor->cgbColor()) != tileColors.end()) {
			if (tileColors.size() == 1) {
				// The tile contains just the background color, skip it.
				attrs.colorSetID = AttrmapEntry::background;
				continue;
			}
			fatal(
			    "Tile (%" PRIu32 ", %" PRIu32 ") contains the background color (#%08x)",
			    tile.x,
			    tile.y,
			    options.bgColor->toCSS()
			);
		}

		// Insert the color set, making sure to avoid overlaps
		for (size_t n = 0; n < colorSets.size(); ++n) {
			switch (colorSet.compare(colorSets[n])) {
			case ColorSet::STRICT_SUPERSET:
				// Override the previous color set that this one is a strict superset of

				printf("- tile (%u, %u) overrides color set #%zu: [", tile.x, tile.y, n);
				for (uint16_t color : colorSets[n]) {
					printf("$%04x, ", color);
				}
				printf("] becomes [");
				for (uint16_t color : colorSet) {
					printf("$%04x, ", color);
				}
				puts("]");

				colorSets[n] = colorSet;
				// Remove any other color sets that we are also a strict superset of
				// (example: we have [(0, 1), (0, 2)] and are inserting (0, 1, 2))
				[[fallthrough]];

			case ColorSet::SUBSET_OR_EQUAL:
				// Use the previous color set that this one is a subset or duplicate of
				attrs.colorSetID = n;
				goto continue_visiting_tiles; // Can't `continue` from within a nested loop

			case ColorSet::INCOMPARABLE:
				// This color set is incomparable so far, so keep going
				break;
			}
		}

		// This color set is incomparable with all previous ones, so add it as a new one
		attrs.colorSetID = colorSets.size();
		if (colorSets.size() == AttrmapEntry::background) { // Check for overflow
			fatal(
			    "Reached %zu color sets... sorry, this image is too much for me to handle :(",
			    AttrmapEntry::transparent
			);
		}

		printf("- tile (%u, %u) adds color set #%zu: [", tile.x, tile.y, colorSets.size());
		for (uint16_t color : colorSet) {
			printf("$%04x, ", color);
		}
		puts("]");

		colorSets.push_back(colorSet);
continue_visiting_tiles:;
	}

	verbosePrint(
	    VERB_INFO,
	    "Image contains %zu color set%s\n",
	    colorSets.size(),
	    colorSets.size() != 1 ? "s" : ""
	);
	// LCOV_EXCL_START
	if (checkVerbosity(VERB_INFO)) {
		style_Set(stderr, STYLE_MAGENTA, false);
		for (ColorSet const &colorSet : colorSets) {
			fputs("[ ", stderr);
			for (uint16_t color : colorSet) {
				fprintf(stderr, "$%04x, ", color);
			}
			fputs("]\n", stderr);
		}
		style_Reset(stderr);
	}
	// LCOV_EXCL_STOP

	if (options.palSpecType == Options::EMBEDDED) {
		generatePalSpec(image);
	}
	auto [mappings, palettes] =
	    options.palSpecType == Options::NO_SPEC || options.palSpecType == Options::DMG
	        ? generatePalettes(colorSets, image)
	        : makePalsAsSpecified(colorSets);
	outputPalettes(palettes);

	// If deduplication is not happening, we just need to output the tile data and/or maps as-is
	if (!options.allowDedup) {
		uint32_t const nbTilesH = image.png.height / 8, nbTilesW = image.png.width / 8;

		// Check the tile count
		if (uint32_t nbTiles = nbTilesW * nbTilesH;
		    nbTiles > options.maxNbTiles[0] + options.maxNbTiles[1]) {
			fatal(
			    "Image contains %" PRIu32 " tiles, exceeding the limit of %" PRIu16 " + %" PRIu16,
			    nbTiles,
			    options.maxNbTiles[0],
			    options.maxNbTiles[1]
			);
		}

		// I currently cannot figure out useful semantics for this combination of flags.
		if (!options.inputTileset.empty()) {
			fatal("Input tilesets are not supported without '-u'");
		}

		if (!options.output.empty()) {
			verbosePrint(VERB_NOTICE, "Generating unoptimized tile data...\n");
			outputUnoptimizedTileData(image, attrmap, palettes, mappings);
		}

		if (!options.tilemap.empty() || !options.attrmap.empty() || !options.palmap.empty()) {
			verbosePrint(
			    VERB_NOTICE, "Generating unoptimized tilemap and/or attrmap and/or palmap...\n"
			);
			outputUnoptimizedMaps(attrmap, mappings);
		}
	} else {
		// All of these require the deduplication process to be performed to be output
		verbosePrint(VERB_NOTICE, "Deduplicating tiles...\n");
		UniqueTiles tiles = dedupTiles(image, attrmap, palettes, mappings);

		if (size_t nbTiles = tiles.size();
		    nbTiles > options.maxNbTiles[0] + options.maxNbTiles[1]) {
			fatal(
			    "Image contains %zu tiles, exceeding the limit of %" PRIu16 " + %" PRIu16,
			    nbTiles,
			    options.maxNbTiles[0],
			    options.maxNbTiles[1]
			);
		}

		if (!options.output.empty()) {
			verbosePrint(VERB_NOTICE, "Generating optimized tile data...\n");
			outputTileData(tiles);
		}

		if (!options.tilemap.empty()) {
			verbosePrint(VERB_NOTICE, "Generating optimized tilemap...\n");
			outputTilemap(attrmap);
		}

		if (!options.attrmap.empty()) {
			verbosePrint(VERB_NOTICE, "Generating optimized attrmap...\n");
			outputAttrmap(attrmap, mappings);
		}

		if (!options.palmap.empty()) {
			verbosePrint(VERB_NOTICE, "Generating optimized palmap...\n");
			outputPalmap(attrmap, mappings);
		}
	}
}
