SECTION "test", ROM0[1]
	call Target
	LOAD "new", WRAM0[$C001]
Target:	dl $DEADBEEF
.end
	ENDL
After:
	jp Target
	ld hl, Word
	dw Byte, Target.end, After

SECTION "ram test", WRAM0 ; Should end up at $C005
Word:
	dw

SECTION "small ram test", WRAM0 ; Should end up at $C000
Byte:
	db

	PRINTT "{Target}\n{Target.end}\n{After}\n"
