static_assert BITWIDTH(0) == 0
static_assert BITWIDTH(42) == 6
static_assert BITWIDTH(-1) == 32
static_assert BITWIDTH($80000000) == 32

static_assert TZCOUNT(0) == 32
static_assert TZCOUNT(42) == 1
static_assert TZCOUNT(-1) == 0
static_assert TZCOUNT($80000000) == 31

static_assert TZCOUNT(1.0) == 16

static_assert POPCOUNT(0) == 0
static_assert POPCOUNT(1) == 1
static_assert POPCOUNT(-1) == 32
static_assert POPCOUNT(%10101100) == 4
static_assert POPCOUNT(-%10101100) == 27
