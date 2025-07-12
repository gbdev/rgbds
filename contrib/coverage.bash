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

COVERAGE_INFO=coverage/coverage.info

# Check whether running from coverage.yml workflow
if [ "$1" != "false" ]; then
  # Generate coverage report
  lcov -c --no-external -d . -o $COVERAGE_INFO --ignore-errors format,inconsistent,unsupported
  lcov -r $COVERAGE_INFO -o $COVERAGE_INFO src/asm/parser.{hpp,cpp} src/link/script.{hpp,cpp} \
       --ignore-errors format,inconsistent,unsupported
  genhtml -f -s -o coverage/ $COVERAGE_INFO --ignore-errors category,corrupt,inconsistent

  # Open report in web browser
  if [ "$(uname)" == "Darwin" ]; then
    open coverage/index.html
  else
    xdg-open coverage/index.html
  fi
else
  # Generate coverage report
  lcov -c --no-external -d . -o $COVERAGE_INFO
  lcov -r $COVERAGE_INFO -o $COVERAGE_INFO src/asm/parser.{hpp,cpp} src/link/script.{hpp,cpp}
  genhtml -f -s -o coverage/ $COVERAGE_INFO
fi
