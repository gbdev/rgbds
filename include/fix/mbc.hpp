// SPDX-License-Identifier: MIT

#ifndef RGBDS_FIX_MBC_HPP
#define RGBDS_FIX_MBC_HPP

#include <stdint.h>
#include <stdio.h>

constexpr uint16_t UNSPECIFIED = 0x200;
static_assert(UNSPECIFIED > 0xFF, "UNSPECIFIED should not be in byte range!");

enum MbcType {
	ROM = 0x00,
	ROM_RAM = 0x08,
	ROM_RAM_BATTERY = 0x09,

	MBC1 = 0x01,
	MBC1_RAM = 0x02,
	MBC1_RAM_BATTERY = 0x03,

	MBC2 = 0x05,
	MBC2_BATTERY = 0x06,

	MMM01 = 0x0B,
	MMM01_RAM = 0x0C,
	MMM01_RAM_BATTERY = 0x0D,

	MBC3 = 0x11,
	MBC3_TIMER_BATTERY = 0x0F,
	MBC3_TIMER_RAM_BATTERY = 0x10,
	MBC3_RAM = 0x12,
	MBC3_RAM_BATTERY = 0x13,

	MBC5 = 0x19,
	MBC5_RAM = 0x1A,
	MBC5_RAM_BATTERY = 0x1B,
	MBC5_RUMBLE = 0x1C,
	MBC5_RUMBLE_RAM = 0x1D,
	MBC5_RUMBLE_RAM_BATTERY = 0x1E,

	MBC6 = 0x20,

	MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,

	POCKET_CAMERA = 0xFC,

	BANDAI_TAMA5 = 0xFD,

	HUC3 = 0xFE,

	HUC1_RAM_BATTERY = 0xFF,

	// "Extended" values (still valid, but not directly actionable)

	// A high byte of 0x01 means TPP1, the low byte is the requested features
	// This does not include SRAM, which is instead implied by a non-zero SRAM size
	// Note: Multiple rumble speeds imply rumble
	TPP1 = 0x100,
	TPP1_RUMBLE = 0x101,
	TPP1_MULTIRUMBLE = 0x102, // Should not be possible
	TPP1_MULTIRUMBLE_RUMBLE = 0x103,
	TPP1_TIMER = 0x104,
	TPP1_TIMER_RUMBLE = 0x105,
	TPP1_TIMER_MULTIRUMBLE = 0x106, // Should not be possible
	TPP1_TIMER_MULTIRUMBLE_RUMBLE = 0x107,
	TPP1_BATTERY = 0x108,
	TPP1_BATTERY_RUMBLE = 0x109,
	TPP1_BATTERY_MULTIRUMBLE = 0x10A, // Should not be possible
	TPP1_BATTERY_MULTIRUMBLE_RUMBLE = 0x10B,
	TPP1_BATTERY_TIMER = 0x10C,
	TPP1_BATTERY_TIMER_RUMBLE = 0x10D,
	TPP1_BATTERY_TIMER_MULTIRUMBLE = 0x10E, // Should not be possible
	TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE = 0x10F,

	// Error values
	MBC_NONE = UNSPECIFIED, // No MBC specified, do not act on it
};

bool mbc_HasRAM(MbcType type);
char const *mbc_Name(MbcType type);
MbcType mbc_ParseName(char const *name, uint8_t &tpp1Major, uint8_t &tpp1Minor);

#endif // RGBDS_FIX_MBC_HPP
