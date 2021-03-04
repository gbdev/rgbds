
SECTION "root", ROM0[0]

	db Label

SECTION "A", ROM0

Label:
	db 2, Label

SECTION "B", ROM0

Unrefd:
	db Unrefd ; References itself, but unless referenced externally, that doesn't matter

	assert Label == 1 ; This assert will be ignored when smart linking, as nothing references section "B"
