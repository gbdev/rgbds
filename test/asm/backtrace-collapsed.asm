macro careful
	if _NARG == 20
		warn "You're in too deep!"
	else
		careful \#, deeper
	endc
endm
careful surface

macro recurse
	recurse
endm
rept 3
	recurse
endr
