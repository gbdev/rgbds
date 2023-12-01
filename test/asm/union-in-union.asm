SECTION "test", WRAM0
UNION
	UNION
		db
	NEXTU
		dw
	ENDU
NEXTU
	dl
ENDU
assert sizeof("test") == 4
