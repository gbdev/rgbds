new_: MACRO
	IF _NARG > 1
	printt "newcharmap \1, \2\n"
	newcharmap \1, \2
	ELSE
	printt "newcharmap \1\n"
	newcharmap \1
	ENDC
ENDM

set_: MACRO
	printt "setcharmap \1\n"
	setcharmap \1
ENDM

push_: MACRO
	printt "pushc\n"
	pushc
ENDM

pop_: MACRO
	printt "popc\n"
	popc
ENDM

print: MACRO
x = \1
printt "{x}\n"
ENDM

printt "main charmap\n"

charmap "ab", $0

	print "ab"

	new_ map1

	print "ab"

	new_ map2, main

	print "ab"

	set_ map1

	print "ab"

	new_ map3

charmap "ab", $1

	print "ab"

	new_ map4, map3

charmap "ab", $1
charmap "cd", $2

	print "ab"
	print "cd"

	set_ map3

	print "ab"
	print "cd"

	set_ main

SECTION "sec0", ROM0

	print "ab"

printt "modify main charmap\n"
charmap "ef", $3

	print "ab"
	print "ef"

	set_ map1

	push_
	set_ map2
	push_

	set_ map3

	print "ab"
	print "cd"
	print "ef"

	pop_

	print "ab"

	pop_

	print "ab"

	new_ map1

	set_ map5

	pop_
