; not inside a section
assert !DEF(@)
println @
println "{@}?"

; not inside a global scope
assert !DEF(.)
println .
println "{.}?"

; not inside a local scope
assert !DEF(..)
println ..
println "{..}?"

; not inside a macro
assert !DEF(_NARG)
println _NARG
println "{_NARG}?"

SECTION "s", ROM0[$42]
assert DEF(@)
println @
println "{@}!"

GlobalScope:
assert DEF(.)
println .
println "{.}!"

.localScope:
assert DEF(..)
println ..
println "{..}!"

MACRO m
	assert DEF(_NARG)
	println _NARG
	println "{_NARG}!"
ENDM
	m 1, 2, 3
