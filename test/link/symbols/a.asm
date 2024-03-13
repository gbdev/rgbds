SECTION "alpha", ROM0
Alpha::
	db 1, 2, 3
End:

SECTION "a", WRAM0
wAlpha::
	ds 3
.End::

SECTION UNION "U", WRAM0
wStart:
	.word1: dw
	.word2: dw
wEnd:
