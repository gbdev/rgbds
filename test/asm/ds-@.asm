SECTION "test fixed", ROM0[0]

FixedStart:
	ds 4, @
	ds 4, (@ - FixedStart) * 2 + zero
	ds 8, (@ - FixedStart) + zero, (@ - FixedStart) * 3 + zero, (@ - FixedStart) * 4 + zero

SECTION "test floating", ROM0

FloatingStart:
	ds 4, @
	ds 4, (@ - FloatingStart) * 2 + zero
	ds 8, (@ - FloatingStart) + zero, (@ - FloatingStart) * 3 + zero, (@ - FloatingStart) * 4 + zero

SECTION "zero", ROM0[0]
zero:
