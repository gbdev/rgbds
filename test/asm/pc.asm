SECTION "fixed", ROM0[420]
	PRINTLN "{@}"
	ds 69
	PRINTLN "{@}"

SECTION "floating", ROM0
	db @
	ds 42
	db @

; We rely on this landing at address $0000, which isn't *guaranteed*...
assert STARTOF("floating") == 0
