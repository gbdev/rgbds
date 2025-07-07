SECTION "sec", ROM0

MACRO print_x
	println x
ENDM

def x = 2147483647
def x = x + 1
	dl 2147483647+1
	print_x

def x = -2147483648
def x = x - 1
	dl -2147483648-1
	print_x

def x = -2147483648
def x = x * -1
	dl -2147483648 * -1
	print_x

def x = -2147483648
def x = x / -1
	dl -2147483648 / -1
	print_x

def x = -2147483648
def x = x % -1
	dl -2147483648 % -1
	print_x

def x = -1
def x = x << 1
	dl -1 << 1
	print_x

def x = 2 ** 31
	print_x

def x = 5 ** 29
	print_x

def x = 4294967295
def x = 4294967296

def x = `33333333
def x = `333333333
