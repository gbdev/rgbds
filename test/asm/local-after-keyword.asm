section "test", rom0

if 0
	section.local "oops"
else
	println "*sips coffee*"
endc

rept 0
	assert.local "lol"
endr
rept 1
	println "this is fine"
endr

macro m
	db.local 42
endm

db.local 123
