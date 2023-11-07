#!/usr/bin/env bash
set -e

# Build RGBDS with gcov support
make coverage -j

# Run the tests
for dir in asm link fix gfx; do
	pushd test/$dir
	./test.sh
	popd
done

# Generate coverage logs
gcov src/**/*.{c,cpp}
mkdir -p coverage

# Generate coverage report
lcov -c --no-external -d . -o coverage/coverage.info
genhtml -f -s -o coverage/ coverage/coverage.info

# Open report in web browser
if [ "$(uname)" == "Darwin" ]; then
  open coverage/index.html
else
  xdg-open coverage/index.html
fi
