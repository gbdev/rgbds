SECTION FRAGMENT "rom", ROM0
Part3::
LOAD FRAGMENT "ram", WRAM0
wPart3::
	jr wPart1
	jr wPart3
ENDL
Part3End::
