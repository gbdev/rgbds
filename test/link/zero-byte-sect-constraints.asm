; Sections going into space already reserved.
SECTION "address", ROM0[$0135]
	assert @ == $0135

SECTION "align16", ROM0, ALIGN[16,$0123] ; Should be equivalent to the above.
	assert @ == $0123

SECTION "align", ROM0, ALIGN[8,42]
	assert @ == 42 ; Assuming that it goes into the first suitable location.


SECTION "om nom nom", ROM0[0]
	ds $200 ; Filling the first part of ROM0, so that the above don't land in a “free space” block but the next do.


SECTION "free address", ROM0[$2468]
	assert @ == $2468

SECTION "free align16", ROM0, ALIGN[16,$2222] ; Should be equivalent to the above.
	assert @ == $2222

SECTION "free align", ROM0, ALIGN[13,$1234] ; Has more than one suitable location, so cannot be trivially solved.
	assert @ == $1234 ; Assuming that it goes into the first suitable location.


SECTION "hram align", HRAM, ALIGN[8, $84]
	assert @ == $FF84
