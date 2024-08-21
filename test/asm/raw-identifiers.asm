def #DEF equ 1
def #def equ 2
def #ghi equ 3
export #def, #ghi

def #align = 0
def #rb rb #def

MACRO #macro
	println "\<#def> is not \<#DEF>"
ENDM
	#macro first, second
	purge #macro
	assert !def(#macro)

section "section", rom0
#section::
	dw #section
#.rom0:
	db BANK(#section.rom0)
#section.romx:
	println "section.romx is in ", SECTION(.romx)

def #add equs "#"
def #sub equs "def"

for #for, {{#add}{#sub}}
	println "for == ", #for
endr
	assert #for == 2
	assert !def(#FOR)

	newcharmap #charmap, #main
	charmap "#", $42
	setcharmap #charmap
	db "#"
