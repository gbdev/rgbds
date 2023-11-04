	MACRO new ; comment
		println "in with the ", \1
	ENDM ; comment

	new 2

	old: MACRO ; comment
		println "out with the ", \1
	ENDM ; comment

	old 1

