// SPDX-License-Identifier: MIT

#ifndef RGBDS_EXTERN_UTF8DECODER_HPP
#define RGBDS_EXTERN_UTF8DECODER_HPP

#include <stdint.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

uint32_t decode(uint32_t *state, uint32_t *codep, uint8_t byte);

#endif // RGBDS_EXTERN_UTF8DECODER_HPP
