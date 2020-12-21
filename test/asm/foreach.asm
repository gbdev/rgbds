foreach n, 10
	printt "{d:n} "
endr
	printt "-> {d:n}\n"

foreach x, 1, 5+1
	printt "{d:x} "
endr
	printt "-> {d:x}\n"

foreach v, 10, -1, -1
	printt "{d:v} "
v = 42
endr
	printt "-> {d:v}\n"

s EQUS "x"
foreach s, 3, 30, 3
	printt "{d:x} "
endr
	printt "-> {d:x}\n"
