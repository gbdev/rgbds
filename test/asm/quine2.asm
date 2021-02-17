q: macro
	println \1,"\1"
endm
	q "q: macro\n\tprintln \\1,\"\\1\"\nendm\n\tq "
