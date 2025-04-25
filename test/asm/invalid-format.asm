println STRFMT("%+d %++d", 42, 42)
println STRFMT("%#x %##x", 42, 42)
println STRFMT("%-4d %--4d", 42, 42)
println STRFMT("%.f %..f", 42.0, 42.0)
println STRFMT("%qf %q.16f", 42.0, 42.0)

DEF N = 42
println "{5d:N} {5d5:N}"
println "{x:N} {xx:N}"

println STRFMT("%+s", "hello")
println STRFMT("%0s", "hello")
println STRFMT("%.5s", "hello")
println STRFMT("%q16s", "hello")

println STRFMT("%#d", 42)
println STRFMT("%.5d", 42)
println STRFMT("%q5d", 42)
