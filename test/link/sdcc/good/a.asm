SECTION "entry", ROM0[$0100]
	jp start

SECTION "header", ROM0[$0104]
	ds $150 - $104, 0

SECTION "start", ROM0
start:
	ld de, 0
	call _function0    ; bc <- 1
	ld b, b            ; breakpoint

	ld d, b :: ld e, c ; de <- bc

	call _function1    ; bc <- 3
	ld b, b            ; breakpoint

	stop
