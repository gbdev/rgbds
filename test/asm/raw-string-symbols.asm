opt Wno-unmapped-char

def hello equs "world"
def name equs "hello"
println "{name}"
println #name
assert !strcmp(strsub(#name, 1, 4), "hell")
assert strlen(#hello) == charlen(#hello)
assert strlen("{hello}") == 5

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
assert #char == $41

def fmt equs "%s %s %d"
println strfmt(#fmt, #name, #hello, (#char))

purge #name
assert !def(name) && !def(#name) && def(hello)

section "test", rom0
#label:
db #hello
dw #hello
dw BANK(#label), #label
