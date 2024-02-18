
SECTION "Byte", ROM0

	db 2

SECTION "ROM0", ROM0, ALIGN[16]

	db 1
	println @ ; Ensure that PC is constant.

SECTION "Mid-section align makes PC constant", ROM0

	align 16, 42
	println @
