DEF warn_unique EQUS "WARN \"\\@!\""

macro m
	warn_unique
	REPT 2
		warn_unique
	ENDR
	warn_unique
endm

	warn_unique
	m
	warn_unique
	m
	warn_unique
