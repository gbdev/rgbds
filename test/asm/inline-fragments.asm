SECTION "1", ROM0[0]

DEF VERSION EQU $11
GetVersion::
	ld a, [ [[db VERSION]] ]
	ret

SECTION "2", ROM0, ALIGN[4]

MACRO text
	db \1, 0
ENDM

MACRO text_pointer
	dw [[
		text \1
	]]
ENDM

GetText::
	ld hl, [[
		dw [[ db "Alpha", 0 ]]
		dw [[
			text "Beta"
		]]
		text_pointer "Gamma"
		dw 0
	]]
	ld c, a
	ld b, 0
	add hl, bc
	add hl, bc
	ld a, [hli]
	ld h, [hl]
	ld l, a
	ret

SECTION "C", ROM0

Foo::
	call [[ jp [[ jp [[ ret ]] ]] ]]
	call [[
Label::
		call GetVersion
		DEF MYTEXT EQU 3
		ld a, MYTEXT
		call GetText
		ld b, h
		ld c, l
		ret
	]]
	jp [[
Bar:
		inc hl
.loop
		nop
:		dec l
		jr nz, :-
		dec h
		jr nz, .loop
		ret
	]]
