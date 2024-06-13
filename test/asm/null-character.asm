MACRO echo
	print "\#"
ENDM
	; '\0' can be printed like any other character
	print "hello\0world\0"
	echo left\0right\0

SECTION "test", ROM0
	; '\0' can be included in ROM like any other character
	db "foo\0bar", 0
	charmap "a\0b", $42
	db "a\0b\0"
