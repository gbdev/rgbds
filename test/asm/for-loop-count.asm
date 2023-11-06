; `FOR value, start, stop, step` calculates an initial count of loop repetitions as follows:
; - if (step > 0 && start < stop) then count = (stop - start - 1) / step + 1
; - if (step < 0 && stop < start) then count = (start - stop - 1) / -step + 1
; - else count = 0
; The absolute vaue |stop - start| needs to be 64-bit for this formula to work.
; Results may be compared with Python range(start, stop, step).

MACRO test
  PRINTLN STRFMT("FOR x, %d, %d, %d", \1, \2, \3)
  FOR x, \1, \2, \3
    PRINTLN "  {08x:x} = {d:x}"
  ENDR
  PRINTLN "  done {08x:x} = {d:x}"
ENDM

  test $8000_0000, $6000_0000, $4000_0000 ; [-2147483648, -1073741824, 0, 1073741824]
  test $8000_0000, $7000_0000, $4400_0000 ; [-2147483648, -1006632960, 134217728, 1275068416]
  test $7fff_ffff, $8000_0000, $8000_0000 ; [2147483647, -1]
  test $8000_0000, $ffff_ffff, $8000_0000 ; []
