SECTION "test", ROM0

	db "A" ; OK, default empty charmap

	pushc
	newcharmap custom
	db "A" ; unmapped in non-default charmap
	popc

	db "A" ; OK, default empty charmap again

	charmap "B", $42
	db "A" ; unmapped in non-empty charmap

	println "A" ; does not use charmap

	opt Wno-unmapped-char
	db "A" ; warning silenced
