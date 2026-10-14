def n equ 42
def s equs "hello"
macro m
endm

static_assert (#n) == 42
static_assert (#s) == 0
static_assert (#m) == 0
static_assert (#u) == 0

static_assert strlen(#n) == 0
static_assert strlen(#s) == 5
static_assert strlen(#m) == 0
static_assert strlen(#u) == 0

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
static_assert (#s) == 0
static_assert strlen(#s) == 0
