SECTION "period", ROM0

assert !def(.) && !def(..)

global1:
assert !strcmp("{.}", "global1") && !def(..)

.local1:
assert !strcmp("{.}", "global1") && !strcmp("{..}", "global1.local1")

global1.local2:
assert !strcmp("{.}", "global1") && !strcmp("{..}", "global1.local2")

global2:
assert !strcmp("{.}", "global2") && !def(..)

.local1:
assert !strcmp("{.}", "global2") && !strcmp("{..}", "global2.local1")

LOAD "load", WRAM0
assert !def(.) && !def(..)

wGlobal1:
assert !strcmp("{.}", "wGlobal1") && !def(..)

.wLocal1:
assert !strcmp("{.}", "wGlobal1") && !strcmp("{..}", "wGlobal1.wLocal1")

wGlobal2:
assert !strcmp("{.}", "wGlobal2") && !def(..)

ENDL
assert !strcmp("{.}", "global2") && !strcmp("{..}", "global2.local1")
