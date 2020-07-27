SECTION "Test", ROM0

Label:
	jr Label
DIFF equ Label - @
	PRINTT "{DIFF}\n"
