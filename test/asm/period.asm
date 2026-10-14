SECTION "period", ROM0

static_assert !def(.) && !def(..)

global1:
static_assert !strcmp("{.}", "global1") && !def(..)

.local1:
static_assert !strcmp("{.}", "global1") && !strcmp("{..}", "global1.local1")

global1.local2:
static_assert !strcmp("{.}", "global1") && !strcmp("{..}", "global1.local2")

global2:
static_assert !strcmp("{.}", "global2") && !def(..)

.local1:
static_assert !strcmp("{.}", "global2") && !strcmp("{..}", "global2.local1")

LOAD "load", WRAM0
static_assert !def(.) && !def(..)

wGlobal1:
static_assert !strcmp("{.}", "wGlobal1") && !def(..)

.wLocal1:
static_assert !strcmp("{.}", "wGlobal1") && !strcmp("{..}", "wGlobal1.wLocal1")

wGlobal2:
static_assert !strcmp("{.}", "wGlobal2") && !def(..)

ENDL
static_assert !strcmp("{.}", "global2") && !strcmp("{..}", "global2.local1")
