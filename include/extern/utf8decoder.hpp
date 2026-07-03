// SPDX-License-Identifier: MIT

#ifndef RGBDS_EXTERN_UTF8DECODER_HPP
#define RGBDS_EXTERN_UTF8DECODER_HPP

#include <stdint.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

struct Utf8Decoder {
	uint32_t state = UTF8_ACCEPT;
	uint32_t codepoint = 0;

	uint32_t update(uint8_t byte);
};

#endif // RGBDS_EXTERN_UTF8DECODER_HPP
