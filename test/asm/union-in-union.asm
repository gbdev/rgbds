SECTION "test", WRAM0
: UNION
	UNION
		db
	NEXTU
		dw
	ENDU
NEXTU
	dl
ENDU
static_assert @ - :- == 4
