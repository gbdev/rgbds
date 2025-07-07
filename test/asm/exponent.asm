def n = 30
for x, -n, n
  for p, n
    def v1 = x ** p
    def v2 = 1
    for i, p
      def v2 *= x
    endr
    assert v1 == v2, "{d:x}**{d:p} = {d:v1} or {d:v2}?"
  endr
endr
