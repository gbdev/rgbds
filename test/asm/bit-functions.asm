assert BITWIDTH(0) == 0
assert BITWIDTH(42) == 6
assert BITWIDTH(-1) == 32
assert BITWIDTH($80000000) == 32

assert TZCOUNT(0) == 32
assert TZCOUNT(42) == 1
assert TZCOUNT(-1) == 0
assert TZCOUNT($80000000) == 31

assert TZCOUNT(1.0) == 16

assert POPCOUNT(0) == 0
assert POPCOUNT(1) == 1
assert POPCOUNT(-1) == 32
assert POPCOUNT(%10101100) == 4
assert POPCOUNT(-%10101100) == 27
