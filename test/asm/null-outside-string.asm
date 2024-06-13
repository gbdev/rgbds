SECTION "test", ROM0
	; '\0' is not special here; it's lexed as a line continuation...
	DEF foo\0bar EQU 42
	db foo\0bar
	; ...just like any other non-whitespace character
	DEF spam\Xeggs EQU 69
	db spam\Xeggs
