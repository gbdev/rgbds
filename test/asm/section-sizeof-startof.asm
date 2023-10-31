SECTION "sect", ROMX[$4567], BANK[$23]
	ds 42

DEF W = BANK("sect")
DEF X = SIZEOF("sect") ; unknown
DEF Y = STARTOF("sect")

	println "sect1: {W} {X} {Y}"

SECTION "sect2", ROMX

DEF W = BANK("sect")
DEF X = SIZEOF("sect")
DEF Y = STARTOF("sect")

	println "sect1: {W} {X} {Y}"

PUSHS
SECTION FRAGMENT "sect3", ROMX[$4567], BANK[$12]

DEF W = BANK("sect2") ; unknown
DEF X = SIZEOF("sect2") ; unknown
DEF Y = STARTOF("sect2") ; unknown

	println "sect2: {W} {X} {Y}"

POPS

DEF W = BANK("sect3")
DEF X = SIZEOF("sect3") ; unknown
DEF Y = STARTOF("sect3")

	println "sect3: {W} {X} {Y}"
