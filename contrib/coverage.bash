#!/usr/bin/env bash
set -e

# Build RGBDS with gcov support
make coverage -j

# Run the tests
pushd test
./fetch-test-deps.sh
if [[ $# -eq 0 ]]; then
  ./run-tests.sh
else
  ./run-tests.sh --os "$1"
fi
popd

# Generate coverage logs
gcov src/**/*.cpp
mkdir -p coverage

# Generate coverage report, excluding Bison-generated files
COVERAGE_INFO=coverage/coverage.info
lcov -c --no-external -d . -o "$COVERAGE_INFO"
lcov -r "$COVERAGE_INFO" src/asm/parser.{hpp,cpp} src/link/script.{hpp,cpp} -o "$COVERAGE_INFO"
genhtml --dark-mode -f -s -o coverage/ "$COVERAGE_INFO"

# Check whether running from coverage.yml workflow
if [ "$1" != "ubuntu-ci" ]; then
  # Open report in web browser
  if [ "$(uname)" == "Darwin" ]; then
    open coverage/index.html
  else
    xdg-open coverage/index.html
  fi
fi
