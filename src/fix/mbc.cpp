#include "fix/mbc.hpp"

#include "helpers.hpp" // unreachable_

void mbc_PrintAcceptedNames(FILE *file) {
	fputs("Accepted MBC names:\n", file);
	fputs("\tROM ($00) [aka ROM_ONLY]\n", file);
	fputs("\tMBC1 ($01), MBC1+RAM ($02), MBC1+RAM+BATTERY ($03)\n", file);
	fputs("\tMBC2 ($05), MBC2+BATTERY ($06)\n", file);
	fputs("\tROM+RAM ($08) [deprecated], ROM+RAM+BATTERY ($09) [deprecated]\n", file);
	fputs("\tMMM01 ($0B), MMM01+RAM ($0C), MMM01+RAM+BATTERY ($0D)\n", file);
	fputs("\tMBC3+TIMER+BATTERY ($0F), MBC3+TIMER+RAM+BATTERY ($10)\n", file);
	fputs("\tMBC3 ($11), MBC3+RAM ($12), MBC3+RAM+BATTERY ($13)\n", file);
	fputs("\tMBC5 ($19), MBC5+RAM ($1A), MBC5+RAM+BATTERY ($1B)\n", file);
	fputs("\tMBC5+RUMBLE ($1C), MBC5+RUMBLE+RAM ($1D), MBC5+RUMBLE+RAM+BATTERY ($1E)\n", file);
	fputs("\tMBC6 ($20)\n", file);
	fputs("\tMBC7+SENSOR+RUMBLE+RAM+BATTERY ($22)\n", file);
	fputs("\tPOCKET_CAMERA ($FC)\n", file);
	fputs("\tBANDAI_TAMA5 ($FD) [aka TAMA5]\n", file);
	fputs("\tHUC3 ($FE)\n", file);
	fputs("\tHUC1+RAM+BATTERY ($FF)\n", file);

	fputs("\n\tTPP1_1.0, TPP1_1.0+RUMBLE, TPP1_1.0+MULTIRUMBLE, TPP1_1.0+TIMER,\n", file);
	fputs("\tTPP1_1.0+TIMER+RUMBLE, TPP1_1.0+TIMER+MULTIRUMBLE, TPP1_1.0+BATTERY,\n", file);
	fputs("\tTPP1_1.0+BATTERY+RUMBLE, TPP1_1.0+BATTERY+MULTIRUMBLE,\n", file);
	fputs("\tTPP1_1.0+BATTERY+TIMER, TPP1_1.0+BATTERY+TIMER+RUMBLE,\n", file);
	fputs("\tTPP1_1.0+BATTERY+TIMER+MULTIRUMBLE\n", file);
}

char const *mbc_Name(MbcType type) {
	switch (type) {
	case ROM:
		return "ROM";
	case ROM_RAM:
		return "ROM+RAM";
	case ROM_RAM_BATTERY:
		return "ROM+RAM+BATTERY";
	case MBC1:
		return "MBC1";
	case MBC1_RAM:
		return "MBC1+RAM";
	case MBC1_RAM_BATTERY:
		return "MBC1+RAM+BATTERY";
	case MBC2:
		return "MBC2";
	case MBC2_BATTERY:
		return "MBC2+BATTERY";
	case MMM01:
		return "MMM01";
	case MMM01_RAM:
		return "MMM01+RAM";
	case MMM01_RAM_BATTERY:
		return "MMM01+RAM+BATTERY";
	case MBC3:
		return "MBC3";
	case MBC3_TIMER_BATTERY:
		return "MBC3+TIMER+BATTERY";
	case MBC3_TIMER_RAM_BATTERY:
		return "MBC3+TIMER+RAM+BATTERY";
	case MBC3_RAM:
		return "MBC3+RAM";
	case MBC3_RAM_BATTERY:
		return "MBC3+RAM+BATTERY";
	case MBC5:
		return "MBC5";
	case MBC5_RAM:
		return "MBC5+RAM";
	case MBC5_RAM_BATTERY:
		return "MBC5+RAM+BATTERY";
	case MBC5_RUMBLE:
		return "MBC5+RUMBLE";
	case MBC5_RUMBLE_RAM:
		return "MBC5+RUMBLE+RAM";
	case MBC5_RUMBLE_RAM_BATTERY:
		return "MBC5+RUMBLE+RAM+BATTERY";
	case MBC6:
		return "MBC6";
	case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
		return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
	case POCKET_CAMERA:
		return "POCKET CAMERA";
	case BANDAI_TAMA5:
		return "BANDAI TAMA5";
	case HUC3:
		return "HUC3";
	case HUC1_RAM_BATTERY:
		return "HUC1+RAM+BATTERY";
	case TPP1:
		return "TPP1";
	case TPP1_RUMBLE:
		return "TPP1+RUMBLE";
	case TPP1_MULTIRUMBLE:
	case TPP1_MULTIRUMBLE_RUMBLE:
		return "TPP1+MULTIRUMBLE";
	case TPP1_TIMER:
		return "TPP1+TIMER";
	case TPP1_TIMER_RUMBLE:
		return "TPP1+TIMER+RUMBLE";
	case TPP1_TIMER_MULTIRUMBLE:
	case TPP1_TIMER_MULTIRUMBLE_RUMBLE:
		return "TPP1+TIMER+MULTIRUMBLE";
	case TPP1_BATTERY:
		return "TPP1+BATTERY";
	case TPP1_BATTERY_RUMBLE:
		return "TPP1+BATTERY+RUMBLE";
	case TPP1_BATTERY_MULTIRUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE_RUMBLE:
		return "TPP1+BATTERY+MULTIRUMBLE";
	case TPP1_BATTERY_TIMER:
		return "TPP1+BATTERY+TIMER";
	case TPP1_BATTERY_TIMER_RUMBLE:
		return "TPP1+BATTERY+TIMER+RUMBLE";
	case TPP1_BATTERY_TIMER_MULTIRUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE:
		return "TPP1+BATTERY+TIMER+MULTIRUMBLE";

	// Error values
	case MBC_NONE:
	case MBC_BAD:
	case MBC_WRONG_FEATURES:
	case MBC_BAD_RANGE:
	case MBC_BAD_TPP1:
		// LCOV_EXCL_START
		unreachable_();
	}

	unreachable_();
	// LCOV_EXCL_STOP
}

bool mbc_HasRAM(MbcType type) {
	switch (type) {
	case ROM:
	case MBC1:
	case MBC2: // Technically has RAM, but not marked as such
	case MBC2_BATTERY:
	case MMM01:
	case MBC3:
	case MBC3_TIMER_BATTERY:
	case MBC5:
	case MBC5_RUMBLE:
	case BANDAI_TAMA5: // "Game de Hakken!! Tamagotchi - Osutchi to Mesutchi" has RAM size 0
	case MBC_NONE:
	case MBC_BAD:
	case MBC_WRONG_FEATURES:
	case MBC_BAD_RANGE:
	case MBC_BAD_TPP1:
		return false;

	case ROM_RAM:
	case ROM_RAM_BATTERY:
	case MBC1_RAM:
	case MBC1_RAM_BATTERY:
	case MMM01_RAM:
	case MMM01_RAM_BATTERY:
	case MBC3_TIMER_RAM_BATTERY:
	case MBC3_RAM:
	case MBC3_RAM_BATTERY:
	case MBC5_RAM:
	case MBC5_RAM_BATTERY:
	case MBC5_RUMBLE_RAM:
	case MBC5_RUMBLE_RAM_BATTERY:
	case MBC6: // "Net de Get - Minigame @ 100" has RAM size 3 (32 KiB)
	case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
	case POCKET_CAMERA:
	case HUC3:
	case HUC1_RAM_BATTERY:
		return true;

	// TPP1 may or may not have RAM, don't call this function for it
	case TPP1:
	case TPP1_RUMBLE:
	case TPP1_MULTIRUMBLE:
	case TPP1_MULTIRUMBLE_RUMBLE:
	case TPP1_TIMER:
	case TPP1_TIMER_RUMBLE:
	case TPP1_TIMER_MULTIRUMBLE:
	case TPP1_TIMER_MULTIRUMBLE_RUMBLE:
	case TPP1_BATTERY:
	case TPP1_BATTERY_RUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE_RUMBLE:
	case TPP1_BATTERY_TIMER:
	case TPP1_BATTERY_TIMER_RUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE:
		break;
	}

	unreachable_(); // LCOV_EXCL_LINE
}
