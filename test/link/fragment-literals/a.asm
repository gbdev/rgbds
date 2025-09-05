section "PrintHL", rom0
PrintHL::
	; ...
	ret

section "PrintNth", rom0
PrintNth::
	ld hl, [[
		dw [[ db "one\0" ]]
		dw [[ db "two\0" ]]
		dw [[ db "three\0" ]]
		dw [[ db "four\0" ]]
	]]
	ld d, 0
	ld e, a
	add hl, de
	add hl, de
	call PrintHL
