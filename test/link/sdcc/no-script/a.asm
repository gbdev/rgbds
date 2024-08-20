SECTION "entry", ROM0[$0100]
	jp start

SECTION "header", ROM0[$0104]
	ds $150 - $104, 0

SECTION "start", ROM0
start:
	ld de, 1234
	call _function ; de <- 1234 * 2
	ld b, b        ; breakpoint
	stop
