MACRO test
	def num = \1
	def den = \2
	def quo = num / den
	def rem = num % den
	def rev = quo * den + rem
	assert num == rev
	println "{ 11d:num} (${08x:num}) / { 11d:den} (${08x:den}) = { 11d:quo} (${08x:quo}) R { 11d:rem} (${08x:rem})"
ENDM

test $8000_0000, $8000_0000
test $8000_0000, $c000_0000
test $8000_0000, $f000_0000
test $8000_0000, $f800_0000
test $8000_0000, $ff00_0000

test $c000_0000, $8000_0000
test $c000_0000, $c000_0000
test $c000_0000, $f000_0000
test $c000_0000, $f800_0000
test $c000_0000, $ff00_0000

test $f800_0000, $8000_0000
test $f800_0000, $c000_0000
test $f800_0000, $f000_0000
test $f800_0000, $f800_0000
test $f800_0000, $ff00_0000

test $0000_0000, $8000_0000
test $0000_0000, $c000_0000
test $0000_0000, $f000_0000
test $0000_0000, $f800_0000
test $0000_0000, $ff00_0000

test $0080_0000, $ff80_0000

test $0300_0000, $fd00_0000
test $0300_0000, $ff00_0000
test $0300_0000, $ff80_0000

test $6000_0000, $fd00_0000
test $6000_0000, $ff00_0000
test $6000_0000, $ff80_0000

test $7f00_0000, $fd00_0000
test $7f00_0000, $ff00_0000
test $7f00_0000, $ff80_0000
