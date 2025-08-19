macro mac
	warn "from macro"
endm
mac ; normal

macro? quiet
	warn "from quiet macro"
endm
quiet
rept? 1
	warn "from quiet rept"
endr
for? x, 1
	warn "from quiet for (x={d:x})"
endr
include? "loud-backtrace.inc"
macro loud
	warn "from loud macro"
endm
mac?
