MACRO data
	db SECTION(@), \#
ENDM

MACRO now_in
	if strcmp("\1", "nothing")
		assert !strcmp(SECTION(@), \1)
	else
		assert !def(@)
	endc
ENDM

now_in nothing

SECTION "A", ROM0
	now_in "A"
	data 1
	LOAD "P", WRAM0
		now_in "P"
		data 2

	; LOAD after LOAD
	LOAD "Q", WRAM0
		now_in "Q"
		data 3

; SECTION after LOAD
SECTION "B", ROM0
	now_in "B"
	data 4
	LOAD "R", WRAM0
		now_in "R"
		data 5

		; PUSHS after LOAD
		PUSHS
			SECTION "C", ROM0
				now_in "C"
				data 6
				LOAD "S", WRAM0
					now_in "S"
					data 7

		; POPS after LOAD
		POPS
		now_in "R"
		data 8

; ENDSECTION after LOAD
ENDSECTION
now_in nothing
