def string_start equs "println \"hello"
string_start world"

def triple_string_start equs "println \"\"\"multi"
triple_string_start;ple
line
strings"""

def cond_start equs "if 0\nprintln \"false\"\nelif"
cond_start 1
  println "true"
else
  println "nan"
endc

def loop_start equs "rept 3\nprintln"
loop_start "lol"
endr

def macro_start equs "macro foo\nprintln"
macro_start \1
endm
  foo 42
