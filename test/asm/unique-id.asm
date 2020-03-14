print EQUS "WARN \"\\@\""

m: macro
    print
    REPT 2
    	print
    ENDR
    print
endm
	; TODO: Ideally we'd test now as well, but it'd cause a fatal error
	;print
	m
	;print
	m
	print
