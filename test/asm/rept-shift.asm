m: macro
	PRINTT "\1 "
	REPT 4
		SHIFT
	ENDR
	PRINTT "\1s!\n"

	; Shifting a little more to check that over-shifting doesn't crash
	SHIFT
	SHIFT
	REPT 256
		SHIFT
	ENDR
	PRINTT "\1\n"
endm

 m This, used, not, to, work
