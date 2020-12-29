foreach n, 10
	printt "{d:n} "
endr
	printt "-> {d:n}\n"

foreach v, 0
	printt "unreached\n"
endr

foreach x, 1, 5+1
	printt "{d:x} "
endr
	printt "-> {d:x}\n"

foreach v, 10, -1, -1
	printt "{d:v} "
v = 42
endr
	printt "-> {d:v}\n"

foreach q, 5, 21, 5
	printt "{d:q} "
purge q
endr
	printt "-> {d:q}\n"

s EQUS "x"
foreach s, 3, 30, 3
	printt "{d:x} "
endr
	printt "-> {d:x}\n"
