SECTION "fixed", ROM0[420]
	PRINTT "{@}\n"
	ds 69
	PRINTT "{@}\n"

; FIXME: expected to land at $0000
SECTION "floating", ROM0
	db @
	ds 42
	db @
