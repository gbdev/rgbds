#!/bin/sh
# This script requires `sh` instead of `bash` because the latter is not always installed on FreeBSD.
set -eu

USAGE="Usage: $0 <os> [additional toolset]"

OS="${1:?$USAGE}"
shift
TOOLSET="${1-}"
shift 2>/dev/null || : # That argument is optional.
if [ $# -ne 0 ]; then
	echo >&2 "$USAGE"
	exit 1
fi

case "${OS%-*}" in
	ubuntu)
		pkgs=bison
		case "$TOOLSET" in
			mingw32) TOOLSET=
				pkgs="$pkgs dpkg-dev mingw-w64-tools libz-mingw-w64-dev g++-mingw-w64-i686-win32"
				;;
			mingw64) TOOLSET=
				pkgs="$pkgs dpkg-dev mingw-w64-tools libz-mingw-w64-dev g++-mingw-w64-x86-64-win32"
				;;
			'' | lcov)
				pkgs="$pkgs libpng-dev $TOOLSET"
				;;
		esac
		sudo apt-get -qq update
		#shellcheck disable=SC2086 # (This word splitting is intentional.)
		sudo apt-get install -yq $pkgs
		;;
	macos)
		# macOS bundles GNU Make 3.81, which doesn't support synced output.
		# We leave it as the default in `PATH`, to test that our Makefile works with it.
		# However, CMake automatically uses Homebrew's `gmake`, so our CI has synced output.
		brew install bison sha2 md5sha1sum make
		# Export `bison` to allow using the version we install from Homebrew,
		# instead of the outdated one preinstalled on macOS (which doesn't even support `-Wall`...)
		export PATH="/opt/homebrew/opt/bison/bin:$PATH"
		printf 'PATH=%s\n' "$PATH" >>"$GITHUB_ENV" # Make it available to later CI steps too
		;;
	freebsd)
		pkg install -y bash bison cmake git png
		;;
	windows)
		choco install -y winflexbison3
		# The below expects the base name, not the Windows-specific name.
		bison() { win_bison "$@"; } # An alias doesn't work, so we use a function instead.
		;;
	*)
		echo "WARNING: Cannot install deps for OS '$1'"
		;;
esac

if [ -n "$TOOLSET" ]; then
	printf >&2 'Unknown toolset `%s` for OS `%s`\n' "$TOOLSET" "$OS"
	exit 1
fi

echo "PATH=($PATH)" | sed 's/:/\n      /g'
bison --version
make --version
cmake --version
