; This covers some edge cases of `skipToLeadingKeyword`

SECTION "test", ROM0

MACRO mac
	if 1
		DEF x = \1_WIDTH
		DEF y = \1_HEIGHT
	elif 0
		DEF x = \1_HEIGHT
		DEF y = \1_WIDTH
	else
		fail "nope"
	endc
	db \1_WIDTH, \1_HEIGHT, x, y
ENDM

	DEF NAME_WIDTH EQU 10
	DEF NAME_HEIGHT EQU 9
	mac NAME
