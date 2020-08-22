SECTION "test", ROM0[0]

	dw WORD($12)
	dw WORD($1234)
	dw WORD($123456)
	dw WORD(-Label)


SECTION "ram", WRAM0[$c123]

Label:


SECTION "test2", ROM0[8]

	dw -Label
	dw WORD(-Label)
