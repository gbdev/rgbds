SECTION "test", ROM0
	assert @ == $80 ; Set by the linker script.
Label:: db 42 ; Make the section non-empty.
