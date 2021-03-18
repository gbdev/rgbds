SECTION "main", ROM0
LOAD FRAGMENT "test", SRAM
ENDL

; The RPN patch for 'jr Label' in section "alt" refers to section "test",
; but the object file puts section "test" after section "alt".
; This case needs to be handled when identifying patches' PC sections.
SECTION "alt", ROM0
LOAD FRAGMENT "test", SRAM
	jr Label
Label:
ENDL
