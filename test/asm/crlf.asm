; This file is encoded with DOS CR+LF line endings!

DEF s EQUS "Hello, \
world!"
assert !strcmp("{s}", "Hello, world!")

DEF t EQUS """Hello,
world!"""
assert !strcmp("{t}", "Hello,\nworld!")
