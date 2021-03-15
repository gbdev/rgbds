SECTION "test", ROM0

pusho
	opt p42, !L, Wno-div
	ds 1
	ld [$ff88], a
	println $8000_0000 / -1
popo

	ds 1
	ld [$ff88], a
	println $8000_0000 / -1
