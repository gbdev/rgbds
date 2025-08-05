def n equ 42
def s equs "hello"
macro m
endm

assert (#n) == 42
assert (#s) == 0
assert (#m) == 0
assert (#u) == 0

assert strlen(#n) == 0
assert strlen(#s) == 5
assert strlen(#m) == 0
assert strlen(#u) == 0

def d_n = (#n)
def d_s = (#s)
def d_m = (#m)
def d_u = (#u)

def s_n equs #n
def s_s equs #s
def s_m equs #m
def s_u equs #u

purge #s
purge #s
assert (#s) == 0
assert strlen(#s) == 0
