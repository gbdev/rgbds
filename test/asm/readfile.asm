def s equs readfile("readfile.inc")
println strupr(#s) ++ "!"

redef s equs readfile("readfile.inc", 5)
println strrpl(#s, "l", "w") ++ "?"
