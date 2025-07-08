#!/usr/bin/env bash
set -e

# Build RGBDS with gcov support

make coverage -j

# Run the tests
pushd test
./fetch-test-deps.sh
./run-tests.sh
popd

# Generate coverage logs
gcov src/**/*.cpp
mkdir -p coverage

# Generate coverage report
lcov -c --no-external -d . -o coverage/coverage.info --ignore-errors format,inconsistent,unsupported
genhtml -f -s -o coverage/ coverage/coverage.info --ignore-errors category,corrupt,inconsistent

# Open report in web browser
if [ "$(uname)" == "Darwin" ]; then
  open coverage/index.html
else
  xdg-open coverage/index.html
fi
