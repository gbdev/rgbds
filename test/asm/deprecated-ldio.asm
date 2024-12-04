SECTION "LDIO", ROM0

	ldh [c], a
	ldh a, [c]
	ldh [$11], a
	ldh a, [$11]

	ld [$ff00+c], a
	ld a, [$ff00+c]
	ld [$ff11], a
	ld a, [$ff11]

	ldio [c], a
	ldio a, [c]
	ldio [$ff11], a
	ldio a, [$ff11]

	LDH [C], A
	LDH A, [C]
	LDH [$11], A
	LDH A, [$11]

	LD [$FF00+C], A
	LD A, [$FF00+C]
	LD [$FF11], A
	LD A, [$FF11]

	LDIO [C], A
	LDIO A, [C]
	LDIO [$FF11], A
	LDIO A, [$FF11]
