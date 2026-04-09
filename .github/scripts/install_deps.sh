#!/bin/sh
# This script requires `sh` instead of `bash` because the latter is not always installed on FreeBSD.
set -eu

case "${1%-*}" in
	ubuntu)
		sudo apt-get -qq update
		sudo apt-get install -yq bison libpng-dev pkg-config
		;;
	macos)
		# macOS bundles GNU Make 3.81, which doesn't support synced output.
		# We leave it as the default in `PATH`, to test that our Makefile works with it.
		# However, CMake automatically uses Homebrew's `gmake`, so our CI has synced output.
		brew install bison md5sha1sum make
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

echo "PATH=($PATH)" | sed 's/:/\n      /g'
bison --version
make --version
cmake --version
