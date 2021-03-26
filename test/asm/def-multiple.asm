def w = 1, x = w+1, y = x+1
	println "{d:w} {d:x} {d:y}"
def w set y+2, x set w+2, y set x+2,
	println "{d:w} {d:x} {d:y}"
redef w = w*2, x = w*2, y = x*2,
	println "{d:w} {d:x} {d:y}"
redef w set w-5, x set w-5, y set x-5
	println "{d:w} {d:x} {d:y}"

DEF spam EQU 6, eggs EQU 7, liff EQU spam*eggs
	println "{d:spam} x {d:eggs} = {d:liff}"

DEF true EQUS "no", false EQUS "yes", filenotfound EQUS "maybe"
	println "{true} or {false} (or {filenotfound})"
REDEF true EQUS "{false}", false EQUS "{true}" ; oops, this isn't Python!
	println "{true} xor {false}"

def toast EQU 1, spam EQU 2, lobster EQU 3 ; error, 'spam' already defined
	println "{d:toast}, {d:spam}, {d:lobster}"

def cannot = 10, mix EQU 20, types EQUS "30" ; syntax error
