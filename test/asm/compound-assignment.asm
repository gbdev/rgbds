macro try
println \1, "\2:"
def prefix equs \1
{prefix}\2 = 10
println \2 ; 10
{prefix}\2 += 5
println \2 ; 15
{prefix}\2 -= 1
println \2 ; 14
{prefix}\2 *= 2
println \2 ; 28
{prefix}\2 /= 4
println \2 ; 7
{prefix}\2 %= 4
println \2 ; 3
{prefix}\2 **= 2
println \2 ; 9
{prefix}\2 |= 11
println \2 ; 11
{prefix}\2 ^= 12
println \2 ; 7
{prefix}\2 &= 21
println \2 ; 5
{prefix}\2 <<= 2
println \2 ; 20
{prefix}\2 >>= 1
println \2 ; 10
{prefix}\2 >>>= 1
println \2 ; 5
purge prefix
endm

  try "def ", q
  try "redef ", r

def _RS += 100
println _RS

def UnDeFiNeD ^= 300
println UnDeFiNeD
