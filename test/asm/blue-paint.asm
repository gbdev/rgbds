OPT r5

DEF statement EQUS "println \"This is the song that never ends~\""

; macro arguments are painted blue
MACRO arg_to_arg
	\1statement\2
ENDM
arg_to_arg \\3, \\4, \{, \}

; ...but...
MACRO arg_to_interp
	\1statement\2
ENDM
arg_to_interp \{, \}

; interpolations are not
DEF open EQUS "\{"
DEF close EQUS "\}"
{open}statement{close}

; which is why this halts
MACRO endless
	\1
ENDM
endless \\1

; ...but this doesn't
DEF infinite EQUS "\{infinite\}"
{infinite}
