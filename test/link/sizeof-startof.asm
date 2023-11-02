SECTION "meta", ROM0[0]
	db BANK("sect")
	dw STARTOF("sect")
	dw SIZEOF("sect")

MACRO typemeta
	rept _NARG
		dw STARTOF(\1), SIZEOF(\1)
		shift
	endr
ENDM

	typemeta ROM0, ROMX, VRAM, SRAM, WRAM0, WRAMX, OAM, HRAM

SECTION "sect", ROMX[$4567], BANK[$23]
	ds 42
