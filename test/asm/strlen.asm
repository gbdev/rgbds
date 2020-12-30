SECTION "sec", ROM0

xstrlen: MACRO
	PRINTLN STRLEN(\1)
ENDM

	xstrlen "ABC"
	xstrlen "カタカナ"
