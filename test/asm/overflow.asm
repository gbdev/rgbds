SECTION "sec", ROM0

x = 2147483647
x = x + 1
	dl 2147483647+1
	printv x

x = -2147483648
x = x - 1
	dl -2147483648-1
	printv x

x = -2147483648
x = x * -1
	dl -2147483648 * -1
	printv x

x = -2147483648
x = x / -1
	dl -2147483648 / -1
	printv x

x = -2147483648
x = x % -1
	dl -2147483648 % -1
	printv x

x = -1
x = x << 1
	dl -1 << 1
	printv x

x = 4294967295
x = 4294967296

x = `33333333
x = `333333333
