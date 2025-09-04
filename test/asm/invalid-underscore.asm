; good
println 123_456
println %_1010_1010
println 0b_1010_1010
println &_555_555
println 0o_777_777
println $_dead_beef
println 0x_cafe_babe
println `_0101_2323
println 12_34.56_78
println 12_34.56_q8

; bad (multiple '_')
println 123__456
println %1010__1010
println &123__456
println $abc__def
println `0101__2323
println 3.14__15
println 2.718__Q16

; bad (trailing '_')
println 12345_
println 0b101010_
println 0o123456_
println 0xabcdef_
println `01230123_
println 123.456_

; bad ('_' next to '.')
println 1_.618
println 2._718
