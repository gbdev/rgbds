warn_unique EQUS "WARN \"\\@\""

m: macro
    warn_unique
    REPT 2
    	warn_unique
    ENDR
    warn_unique
endm
	; TODO: Ideally we'd test now as well, but it'd cause a fatal error
	;warn_unique
	m
	;warn_unique
	m
	warn_unique
