SECTION "SECTION2", ROM0
LOAD FRAGMENT "test", SRAM
	jr Label
Label:
	dw Label
ENDL
