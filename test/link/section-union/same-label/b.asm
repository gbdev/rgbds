SECTION UNION "test", WRAM0
Same:
	ds 2
Bar:
	ds 1

SECTION "b", ROM0
	dw Same, Bar ; $c000, $c002
