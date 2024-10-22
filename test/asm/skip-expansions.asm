; skipping ahead to `elif`/`else`/`endc`/`endr`/`endm` disables expansions!

MACRO mac1
	if _NARG != 2
		println "args: \#"
	elif (\2) == 42  
		println "forty-two!"
	else
		println "it's ", (\2)
	endc
ENDM

	mac1 elif, 6 * 7  ; this prints "forty-two!" because it takes the `elif`
	mac1 elif, 6 * 9
	mac1 elif
	mac1

MACRO mac2
	if _NARG != 2
		println "args: \#"
	\1 (\2) == 42  ; if the `if` is not taken, this is not expanded!
		println "forty-two!"
	else
		println "it's ", (\2)
	endc
ENDM

	mac2 elif, 6 * 7  ; this prints "it's $2A" because it skips the `\1` line and takes the `else`
	mac2 elif, 6 * 9
	mac2 elif
	mac2 ; this prints "args:" *and* "forty-two!" since it doesn't create an `elif`
