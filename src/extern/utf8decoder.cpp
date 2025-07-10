// SPDX-License-Identifier: MIT

// This implementation was taken from
// http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
// and modified for RGBDS.

#include "extern/utf8decoder.hpp"

// clang-format off: vertically align values
static uint8_t const utf8d[] = {
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00..0f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10..1f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20..2f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 30..3f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40..4f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 50..5f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60..6f
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 70..7f
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80..8f
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 90..9f
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // a0..af
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // b0..bf
     8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // c0..cf
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // d0..df
    10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, // e0..ef
    11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // f0..ff
    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
     0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72, // s0
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, // s1
    12,  0, 12, 12, 12, 12, 12,  0, 12,  0, 12, 12, // s2
    12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12, // s3
    12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, // s4
    12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12, // s5
    12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, // s6
    12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, // s7
    12, 36, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, // s8
};
// clang-format on

uint32_t decode(uint32_t *state, uint32_t *codep, uint8_t byte) {
	uint8_t type = utf8d[byte];
	*codep = *state != UTF8_ACCEPT ? (byte & 0b111111) | (*codep << 6) : (0xff >> type) & byte;
	*state = utf8d[0x100 + *state + type];
	return *state;
}
