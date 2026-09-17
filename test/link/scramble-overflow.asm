DEF N equ 6

SECTION "fixed", ROMX, BANK[3]
ds $1000, $ff

FOR i, N
	SECTION "floating{d:i}", ROMX
	xLabel{d:i}:: ds $2000, i

	SECTION "wram{d:i}", WRAMX
	wLabel{d:i}:: dw

	SECTION "sram{d:i}", SRAM
	sLabel{d:i}:: dw
ENDR
