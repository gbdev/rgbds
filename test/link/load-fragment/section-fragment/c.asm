SECTION FRAGMENT "rom", ROM0
; No labels in this fragment
LOAD FRAGMENT "ram", WRAM0
wPart3::
	jr wPart1
	jr wPart3
ENDL
