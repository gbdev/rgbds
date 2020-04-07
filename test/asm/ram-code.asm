SECTION "test", ROM0[1]
	call Target
	PRINTT "PC in ROM: {@}\n"
	LOAD "new", WRAMX[$D001],BANK[1]
	PRINTT "PC in WRAM: {@}\n"
	assert @ == $D001
Target:	dl DEAD << 16 | BEEF
	db BANK(@)
	jr .end
.end
	jr .end
	ds 2, $2A
	ENDL
After:
	jp Target
	ld hl, Word
	dw Byte, Target.end, After

SECTION "dead", WRAMX[$DEAD],BANK[2]
DEAD:
SECTION "beef", SRAM[$BEEF]
BEEF:

SECTION "ram test", WRAMX,BANK[1] ; Should end up at $D005
Word:
	dw

SECTION "small ram test", WRAMX,BANK[1] ; Should end up at $D000
Byte:
	db

	PRINTT "{Target}\n{Target.end}\n{After}\n"
