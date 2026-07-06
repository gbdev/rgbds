#!/usr/bin/env bash
set -e

usage() {
	cat <<"EOF"
Generates LCOV code coverage report for RGBDS.
Options:
    -h, --help     show this help message
    -o, --open     open the HTML report in the preferred application
    --jobs <n>     build RGBDS and external codebases with `make -j<n>`
    --os <os>      skip tests known to fail on <os> (e.g. `macos-14`)
EOF
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
make_args=()
runtests_args=()
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
			make_args+=(-j "$1")
			runtests_args+=(--jobs "$1")
			;;
		--os)
			shift
			runtests_args+=(--os "$1")
			;;
		*)
			echo "$(basename "$0"): unknown option '$1'"
			exit 1
			;;
	esac
	shift
done

# Build RGBDS with gcov support
make coverage "${make_args[@]}"

# Run the tests
pushd test
external/fetch-repos.sh
./run-tests.sh "${runtests_args[@]}"
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
	# Open the report in the preferred web browser
	if [ "$(uname)" == "Darwin" ]; then
		open coverage/index.html
	else
		xdg-open coverage/index.html
	fi
else
	# Output the path to the report
	echo "Generated LCOV report at $PWD/coverage/index.html"
fi
