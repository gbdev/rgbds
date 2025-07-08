; no digits
def x = $
def x = `
def x = 0b
def x = 0o
def x = 0x

; too large
def x = 9_876_543_210
def x = $f_0000_0000
def x = &400_0000_0000
def x = %1_00000000_00000000_00000000_00000000
def x = 65537.0q16

; no precision suffix
def x = 3.14q

; invalid precision suffix
def x = 3.14q40

