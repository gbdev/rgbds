for n, 10
	print "{d:n} "
endr
	println "-> {d:n}"

for v, 0
	print "unreached"
endr

for v, 2, 1
	print "backwards"
endr

for v, 1, 2, 0
	print "unreached"
endr

for v, 1, 2, -1
	print "backwards"
endr

for x, 1, 5+1
	print "{d:x} "
endr
	println "-> {d:x}"

for v, 10, -1, -1
	print "{d:v} "
	def v = 42
endr
	println "-> {d:v}"

for q, 5, 21, 5
	print "{d:q} "
purge q
endr
	println "-> {d:q}"

DEF s EQUS "x"
for {s}, 3, 30, 3
	print "{d:x} "
endr
	println "-> {d:x}"
for s, 10
	println "{d:s}"
endr

for v, 10
	println "{d:v}"
if v == 3
purge v
def v equ 42 ; causes a fatal error
endc
endr
	println "-> {d:v}"
