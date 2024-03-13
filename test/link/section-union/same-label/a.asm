SECTION UNION "test", WRAM0
Same:
	ds 1
Foo:
	ds 2

SECTION "a", ROM0
	dw Same, Foo ; $c000, $c001
