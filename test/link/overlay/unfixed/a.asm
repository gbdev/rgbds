FOR n, 1, 5
	SECTION "ROMX[$40{02x:n}] BANK[5]", ROMX[$4000 + n], BANK[5]
	db n
	SECTION "ROMX BANK[{d:n}]", ROMX, BANK[n]
	db n
	SECTION "ROMX[$40{02x:n}]", ROMX[$4000 + n]
	db n
	SECTION "ROMX #{d:n}", ROMX
	db n
ENDR
