# RGBFIX tests

These tests check that RGBFIX behaves properly.

## Structure of a test

- `test.bin`: The file passed as input to RGBFIX.
- `test.flags`: The command-line flags passed to RGBFIX's invocation.
  Actually, only the first line is considered; the rest of the file may contain comments about the test.
- `test.gb`: The expected output.
  May not exist, generally when the test expects an error, in which case the comparison is skipped.
- `test.err`: The expected error output.

## Special tests

- `noexist.err` is the expected error output when RGBFIX is given a non-existent input file.
- Symlink tests in `test.sh` check that in-place edits and explicit outputs reject symbolic
  links without changing the target, input, or an unrelated output file, including with padding,
  chained links, and dangling links. Reading a link with a separate output is also checked.
  These tests are skipped when native symbolic links cannot be created (for example, when
  Windows denies the required privilege).
