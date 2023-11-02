SECTION "Fixed bank", ROMX,BANK[42]
	ldh a, [BANK(@) * 256] ; This should be complained about at assembly time

	DEF X = BANK(@)

SECTION "Something else", ROMX
	DEF Y = BANK("Fixed bank")

	PRINTLN "@: {X}\nStr: {Y}"

	DEF ERR = BANK(@)
