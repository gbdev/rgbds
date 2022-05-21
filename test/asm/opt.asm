SECTION "test", ROM0

	opt !h, !L ; already the default, but tests parsing "!"

pusho
	opt p42, h, L, Wno-div
	ds 1
	ld [$ff88], a
	halt
	println $8000_0000 / -1
popo

	opt H, l

	ds 1
	ld [$ff88], a
	halt
	println $8000_0000 / -1
