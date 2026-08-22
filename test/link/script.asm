; script*.link are tests for the linker script.
; So there isn't much to see here.

SECTION "ROM0", ROM0
SECTION "ROM1", ROMX,BANK[1]
SECTION "ROM2 1K", ROMX,BANK[2]
	ds $1000
SECTION "ROM2 1", ROMX,BANK[2]
	ds 1
SECTION "\\\"\'\n\r\t", ROM0
