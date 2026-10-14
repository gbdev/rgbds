; not inside a section
static_assert !DEF(@)
println @
println "{@}?"

; not inside a section
static_assert !DEF(__SCOPE__)
println __SCOPE__
println "{__SCOPE__}?"

; not inside a global scope
static_assert !DEF(.)
println .
println "{.}?"

; not inside a local scope
static_assert !DEF(..)
println ..
println "{..}?"

; not inside a macro
static_assert !DEF(_NARG)
println _NARG
println "{_NARG}?"

SECTION "s", ROM0[$42]
static_assert DEF(@)
println @
println "{@}!"

static_assert DEF(__SCOPE__)
println __SCOPE__
println "{__SCOPE__}!"

GlobalScope:
static_assert DEF(.)
println .
println "{.}!"

.localScope:
static_assert DEF(..)
println ..
println "{..}!"

MACRO m
	static_assert DEF(_NARG)
	println _NARG
	println "{_NARG}!"
ENDM
	m 1, 2, 3
