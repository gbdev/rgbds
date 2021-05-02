SECTION "test", ROM0

	opt !L ; already the default, but tests parsing "!L"

pusho
	opt p42, L, Wno-div
	ds 1
	ld [$ff88], a
	println $8000_0000 / -1
popo

	ds 1
	ld [$ff88], a
	println $8000_0000 / -1
