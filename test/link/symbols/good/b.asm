SECTION "beta", ROM0
Beta::
	db 4, 5, 6
End:

SECTION "b", WRAM0
wBeta::
	ds 3
.End::

SECTION UNION "U", WRAM0
wStart:
	.long1: dl
wEnd:
