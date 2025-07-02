SECTION "OAMDMACode", ROM0
OAMDMACode:
LOAD "hOAMDMA", HRAM
hOAMDMA::
	ldh [$ff46], a
	ld a, 40
	jp [[
:		dec a
		jr nz, :-
		ret
	]]
.end
ENDL
OAMDMACodeEnd:
