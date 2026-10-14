def m = $8000_0000
static_assert m == 1 << 31
static_assert m == -(1 << 31)
static_assert m == (-2)**31

	println "{m}"
	println "({12d:m})"
	println "({-12d:m})"
