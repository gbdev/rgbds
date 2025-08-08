macro try
	def x = \1
	if _NARG > 1
		assert x == \2
	else
		assert x == 0
	endc
endm

; no digits
try $
try `
try 0b
try 0o
try 0x

; too large
try 999_876_543_210
try $ffff_0000_0000
try &7777_0000_0000_0000
try %1111_00000000_00000000_00000000_00000000
try `0123_3210_0123, `0123_3210
try 99999.0q16

; no precision suffix
try 3.14q, 3.14

; invalid precision suffix
try 3.14q40, 3.14
