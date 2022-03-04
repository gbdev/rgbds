
#include "gfx/rgba.hpp"

#include <assert.h>
#include <stdint.h>

#include "gfx/main.hpp" // options

uint16_t Rgba::cgbColor() const {
	if (isTransparent()) {
		return transparent;
	}
	if (options.useColorCurve) {
		assert(!"TODO");
	} else {
		return (red >> 3) | (green >> 3) << 5 | (blue >> 3) << 10;
	}
}

uint8_t Rgba::grayIndex() const {
	assert(isGray());
	// Convert from [0; 256[ to [0; maxPalSize[
	return static_cast<uint16_t>(255 - red) * options.maxPalSize() / 256;
}
