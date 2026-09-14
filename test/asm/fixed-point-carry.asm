opt Q8
println strfmt("%f", 1.999)
println strfmt("%f", 2.999)
println strfmt("%f", 3.999)
println strfmt("%f", 16777215.999)
println strfmt("%f", 4294967295.999)

opt Q16
println strfmt("%f", 1.999999)
println strfmt("%f", 2.999999)
println strfmt("%f", 3.999999)
println strfmt("%f", 65535.999999)
println strfmt("%f", 4294967295.999999)

opt Q24
println strfmt("%f", 1.999999999)
println strfmt("%f", 2.999999999)
println strfmt("%f", 3.999999999)
println strfmt("%f", 255.999999999)
println strfmt("%f", 4294967295.999999999)
