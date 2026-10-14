opt Wno-unmapped-char

def hello equs "world"
def name equs "hello"
println "{name}"
println #name
static_assert !strcmp(strslice(#name, 0, 4), "hell")
static_assert strlen(#hello) == charlen(#hello)
static_assert strlen("{hello}") == 5

def multi equs """the	quick
brown	fox"""
println #multi

def char equs "A"
def n = #char
println n
def n = (#char)
println n
def n = 1 + #char
println n
static_assert #char == $41

def fmt equs "%s %s %d"
println strfmt(#fmt, #name, #hello, (#char))

purge #name
static_assert !def(name) && !def(#name) && def(hello)

section "test", rom0

db #hello
dw #hello
dl #hello

#label:
dw BANK(#label), #label
