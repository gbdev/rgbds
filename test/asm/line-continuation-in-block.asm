if 0 ; skipped
	mac startc, \
		endc
endc

rept 0 ; captured and skipped
	mac startr, \
		endr
endr

macro m ; captured
	mac startm, \
		endm
endm

macro mac
	println "from \1 to \2"
endm

if 1 ; evaluated
	mac startc, \
		endc
endc

rept 1 ; captured and evaluated
	mac startr, \
		endr
endr

m ; evaluated
