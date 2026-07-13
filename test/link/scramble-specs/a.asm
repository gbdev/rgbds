DEF N = 6

SECTION "fixed", ROMX, BANK[3]
; XXX: We rely on these landing at certain banks, which isn't *guaranteed*...
FOR i, 1, N + 1
	db BANK(xLabel{d:i})
ENDR
FOR i, 1, N + 1
	db BANK(wLabel{d:i})
ENDR
FOR i, 1, N + 1
	db BANK(sLabel{d:i})
ENDR
ds $1000 - N * 3, $ff

FOR i, 1, N + 1
	SECTION "floating{d:i}", ROMX
	xLabel{d:i}:: ds $2000, i

	SECTION "wram{d:i}", WRAMX
	wLabel{d:i}:: dw

	SECTION "sram{d:i}", SRAM
	sLabel{d:i}:: dw
ENDR
