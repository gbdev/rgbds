SECTION "sec", ROM0

xstrlen: MACRO
	PRINTV STRLEN(\1)
	PRINTT "\n"
ENDM

	xstrlen "ABC"
	xstrlen "カタカナ"
