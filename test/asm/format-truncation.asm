; Strings used to be truncated to 255 characters with a warning.

def num equ 42
def fix equ 123.0
def str equs "hello"

println "{#0260x:num}"
println "{#-260x:num}"
println "{0280.260f:fix}"
println "{260s:str}"
println "{-260s:str}"

println "<{#0260x:num}>"
println "<{#-260x:num}>"
println "<{0280.260f:fix}>"
println "<{260s:str}>"
println "<{-260s:str}>"
