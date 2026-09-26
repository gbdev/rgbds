def n = 0
for i_type, 5
	redef type equs strslice("     warn fail warn,fail,", i_type * 5, (i_type + 1) * 5)
	for i_message, 2
		redef message equs ", \"hello {d:n}\""
		redef message equs strslice(#message, 0, i_message * strlen(#message))
		for cond, 2
			redef line equs "assert {type} {cond} {message}"
			println "[static_]{line}"
			{line}
			static_{line}
			def n += 1
		endr
	endr
endr

println "[static_]assert fatal..."
assert fatal 1
assert fatal 1, "goodbye 0"
static_assert fatal 1
static_assert fatal 1, "goodbye 0"

assert fatal 0
assert fatal 0, "goodbye 1" ; not reached
