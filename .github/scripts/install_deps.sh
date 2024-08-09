#!/usr/bin/env bash
set -euo pipefail

case "${1%-*}" in
	ubuntu)
		sudo apt-get -qq update
		sudo apt-get install -yq bison libpng-dev pkg-config
		;;
	macos)
		brew install bison sha2 md5sha1sum
		# Export `bison` to allow using the version we install from Homebrew,
		# instead of the outdated one preinstalled on macOS (which doesn't even support `-Wall`...)
		export PATH="/opt/homebrew/opt/bison/bin:$PATH"
		printf 'PATH=%s\n' "$PATH" >>"$GITHUB_ENV" # Make it available to later CI steps too
		;;
	*)
		echo "WARNING: Cannot install deps for OS '$1'"
		;;
esac

bison --version
make --version
cmake --version
