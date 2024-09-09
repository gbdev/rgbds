ASSERT !DEF(@) && !DEF(.)

PURGE @, .

SECTION "test", ROM0[42]
Foobar:

PURGE @, .

ASSERT DEF(@) && DEF(.) && DEF(Foobar)

PRINTLN "PC: {#05X:@}; label scope: \"{.}\"; {.}: {#05X:{.}}"
