assert !def(__SCOPE__)

section "test", rom0

assert !def(.)
assert !def(..)
assert #__SCOPE__ === ""

Alpha.local1:
assert !def(.)
assert #.. === "Alpha.local1"
assert #__SCOPE__ === ".."

Beta:
assert #. === "Beta"
assert !def(..)
assert #__SCOPE__ === "."

Alpha.local2:
assert #. === "Beta"
assert #.. === "Alpha.local2"
assert #__SCOPE__ === ".."

.newLocal:
assert #. === "Beta"
assert #.. === "Beta.newLocal"
assert #__SCOPE__ === ".."
