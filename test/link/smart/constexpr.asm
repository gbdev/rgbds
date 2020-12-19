
SECTION "A", ROM0[1]

Label:
	db 2, Label

SECTION "root", ROM0[0]

	db Label

SECTION "B", ROM0[3]

Unrefd:
	db Unrefd
