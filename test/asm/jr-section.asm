SECTION "Test", ROM0

Label:
	jr Label
def DIFF equ Label - @
	PRINTLN "{DIFF}"
