
SECTION "ff00+c or not to ff00+c", ROMX

	ldh a, [$ff00 + c]
	ldh [65280 + c], a

	; Not ok
	ldh a, [$ff01 + c]
	ldh [xyz + c], a
