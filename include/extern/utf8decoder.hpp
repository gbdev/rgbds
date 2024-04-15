/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_EXTERN_UTF8DECODER_HPP
#define RGBDS_EXTERN_UTF8DECODER_HPP

#include <stdint.h>

uint32_t decode(uint32_t *state, uint32_t *codep, uint8_t byte);

#endif // RGBDS_EXTERN_UTF8DECODER_HPP
