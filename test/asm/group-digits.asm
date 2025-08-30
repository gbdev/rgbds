println STRFMT("%_d", 999)
println STRFMT("%+_d", 123456789)

println STRFMT("% _d", 0xfedc_ba98)
println STRFMT("% _u", 0xfedc_ba98)

println STRFMT("(%15_d)", 12345678)
println STRFMT("(%+15_d)", 12345678)
println STRFMT("(%-15_d)", 12345678)
println STRFMT("(%+-15_d)", 12345678)

println STRFMT("%10_x", 0xffff)
println STRFMT("%010_X", 0xffff)

println STRFMT("%#_x", 0xfedc_ba98)
println STRFMT("%#_X", 0xfedc_ba98)

println STRFMT("%014_o", $ffff_ffff)

println STRFMT("%16_.4f", 32_109.03125)
println STRFMT("%16_.4q4f", 1_234_567.25q4)

pusho Q4
println STRFMT("%16_.4f", 1_234_567.25)
popo

println STRFMT("%020d", 123456789)
println STRFMT("%020_d", 123456789)
println STRFMT("%020q2f", 268435456.75q2)
println STRFMT("%020_q2f", 268435456.75q2)
