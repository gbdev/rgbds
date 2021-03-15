SECTION "test", ROM0

pusho
	opt p42, !L
	ds 1
	ld [$ff88], a
popo

	ds 1
	ld [$ff88], a
