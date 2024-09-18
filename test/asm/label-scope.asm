ASSERT !DEF(@) && !DEF(.) && !DEF(..)

PURGE @, ., ..

SECTION "test", ROM0[42]
db 1
Foo:
db 2
.bar
db 3

PURGE @, ., ..

ASSERT DEF(@) && DEF(.) && DEF(..) && DEF(Foo) && DEF(.bar)

PRINTLN "PC: {#05X:@}"
PRINTLN "global scope: \"{.}\" ({#05X:{.}})"
PRINTLN "local scope: \"{..}\" ({#05X:{..}})"
