
SECTION "A", ROM0

Label:
	db 2, Label
.end

SECTION "root", ROM0[0]

	db Label.end - Label

SECTION "B", ROM0

Unrefd:
	db Unrefd
