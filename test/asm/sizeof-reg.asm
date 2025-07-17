assert sizeof(a) == 1
assert sizeof(b) == 1
assert sizeof(c) == 1
assert sizeof(d) == 1
assert sizeof(e) == 1
assert sizeof(h) == 1
assert sizeof(l) == 1

assert sizeof([bc]) == 1
assert sizeof([de]) == 1
assert sizeof([hl]) == 1

assert sizeof([hli]) == 1
assert sizeof([hl+]) == 1
assert sizeof([hld]) == 1
assert sizeof([hl-]) == 1

assert sizeof(af) == 2
assert sizeof(bc) == 2
assert sizeof(de) == 2
assert sizeof(hl) == 2
assert sizeof(sp) == 2

assert sizeof(high(af)) == 1
assert sizeof(high(bc)) == 1
assert sizeof(low(bc)) == 1
assert sizeof(high(de)) == 1
assert sizeof(low(de)) == 1
assert sizeof(high(hl)) == 1
assert sizeof(low(hl)) == 1
