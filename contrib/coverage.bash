#!/usr/bin/env bash
set -e

# Build RGBDS with gcov support
make coverage -j $(getconf _NPROCESSORS_ONLN)

# Run the tests, forwarding all script arguments
pushd test
./fetch-test-deps.sh
./run-tests.sh "$@"
popd

# Generate coverage logs
gcov src/**/*.cpp
mkdir -p coverage

# Generate coverage report, excluding Bison-generated files
COVERAGE_INFO=coverage/coverage.info
lcov -c --no-external -d . -o "$COVERAGE_INFO"
lcov -r "$COVERAGE_INFO" src/asm/parser.{hpp,cpp} src/link/script.{hpp,cpp} -o "$COVERAGE_INFO"
genhtml --dark-mode --num-spaces 4 -f -s -o coverage/ "$COVERAGE_INFO"

# Output the path to the report
echo "Open $PWD/coverage/index.html"
