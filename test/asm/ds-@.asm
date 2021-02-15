SECTION "test fixed", ROM0[0]

FixedStart:
	ds 8, (@ - FixedStart) * 2 + zero
	ds 8, (@ - FixedStart) * 2 + zero

SECTION "test floating", ROM0

FloatingStart:
	ds 8, (@ - FloatingStart) * 2 + zero
	ds 8, (@ - FloatingStart) * 2 + zero

SECTION "zero", ROM0[0]
zero:
