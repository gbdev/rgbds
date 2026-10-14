static_assert sizeof(a) == 1
static_assert sizeof(b) == 1
static_assert sizeof(c) == 1
static_assert sizeof(d) == 1
static_assert sizeof(e) == 1
static_assert sizeof(h) == 1
static_assert sizeof(l) == 1

static_assert sizeof([bc]) == 1
static_assert sizeof([de]) == 1
static_assert sizeof([hl]) == 1

static_assert sizeof([hli]) == 1
static_assert sizeof([hl+]) == 1
static_assert sizeof([hld]) == 1
static_assert sizeof([hl-]) == 1

static_assert sizeof(af) == 2
static_assert sizeof(bc) == 2
static_assert sizeof(de) == 2
static_assert sizeof(hl) == 2
static_assert sizeof(sp) == 2

static_assert sizeof(high(af)) == 1
static_assert sizeof(high(bc)) == 1
static_assert sizeof(low(bc)) == 1
static_assert sizeof(high(de)) == 1
static_assert sizeof(low(de)) == 1
static_assert sizeof(high(hl)) == 1
static_assert sizeof(low(hl)) == 1
