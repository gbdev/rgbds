SECTION "DMA ROM", ROM0[0]
SetUpDMA::
	ld c, LOW(DMARoutine)
	ld b, DMARoutineCode.end - DMARoutineCode
	ld hl, DMARoutineCode
.loop
	ld a, [hl+]
	ldh [c], a
	inc c
	dec b
	jr nz, .loop
	ret

DMARoutineCode::
LOAD "DMA RAM", HRAM[$FF80]
DMARoutine::
	ld a, $C0
	ldh [rDMA], a
	ld a, $28
.loop
	dec a
	jr nz, .loop
	ret
ENDL
.end ; This label should be in the DMARoutineCode scope after ENDL
