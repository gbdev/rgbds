DEF x = 0
DEF s EQUS "\nprintln \"lol\", x\ndef x += 1"

if 0
	println "no"
endc{#s}

if 1
	println "yes"
else{#s}
	println "nope"
endc{#s}

rept 0
	println "no way"
endr{#s}

println "x = ", x

MACRO m
	if 0
		println "no how"
	endc\1
ENDM
m \nprintln "haha"
