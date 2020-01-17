test: macro
	; Test the rpn system, as well as the linker...
	dl \1 + zero

	; ...as well as the constexpr system
result\@ equ \1
	printt "\1 = {result\@}\n"
endm

section "test", ROM0[0]

	test 1 << 1
	test 1 << 32
	test 1 << 9001
	test -1 << 1
	test -1 << 32
	test -1 << -9001

	test -1 >> 1
	test -1 >> 32
	test -1 >> 9001
	test -4 >> 1
	test -4 >> 2
	test -1 >> -9001

SECTION "Zero", ROM0[0]
zero:
