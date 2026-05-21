#!/usr/bin/env bash
set -e

usage() {
  cat <<"EOF"
Generates LCOV code coverage report for RGBDS.
Options:
    -h, --help     show this help message
    -o, --open     open the HTML report in the preferred application
    --os <os>      specify <os> for test/run-tests.sh (e.g. `macos-14`)
    --jobs <n>     build with `make -j<n>`
EOF
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
runtests_args=
make_jobs=
open_report=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -o|--open)
      open_report=true
      ;;
    --jobs)
      shift
      runtests_args="$runtests_args --jobs $1"
      make_jobs="-j$1"
      ;;
    --os)
      shift
      runtests_os="$runtests_args --os $1"
      ;;
    *)
      echo "$(basename "$0"): unknown option '$1'"
      exit 1
      ;;
  esac
  shift
done

# Build RGBDS with gcov support
# shellcheck disable=SC2086 # (This word splitting is intentional.)
make coverage $make_jobs

# Run the tests, forwarding all script arguments
pushd test
./fetch-test-deps.sh
# shellcheck disable=SC2086 # (This word splitting is intentional.)
./run-tests.sh $runtests_args
popd

# Generate coverage logs
gcov src/**/*.cpp
mkdir -p coverage

# Generate coverage report, excluding Bison-generated files
COVERAGE_INFO=coverage/coverage.info
lcov -c --no-external -d . -o "$COVERAGE_INFO"
lcov -r "$COVERAGE_INFO" src/asm/parser.{hpp,cpp} src/link/script.{hpp,cpp} -o "$COVERAGE_INFO"
genhtml --dark-mode --num-spaces 4 -f -s -o coverage/ "$COVERAGE_INFO"

if "$open_report"; then
  # Open the report in preferred web browser
  if [ "$(uname)" == "Darwin" ]; then
    open coverage/index.html
  else
    xdg-open coverage/index.html
  fi
else
  # Output the path to the report
  echo "Open $PWD/coverage/index.html"
fi
