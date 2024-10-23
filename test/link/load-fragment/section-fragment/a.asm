SECTION FRAGMENT "rom", ROM0
Part1::
LOAD FRAGMENT "ram", WRAM0
wPart1::
	jr wPart1
	jr wPart3
ENDL
Part1End::
