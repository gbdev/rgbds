#!/bin/sh
# This script requires `sh` instead of `bash` because the latter is not always installed on FreeBSD.
set -eu

case "${1%%-*}" in
	ubuntu|debian)
		sudo apt-get -qq update
		sudo apt-get install -yq bison libpng-dev pkgconf
		;;
	macos)
		# macOS bundles GNU Make 3.81, which doesn't support synced output.
		# We leave it as the default in `PATH`, to test that our Makefile works with it.
		# However, CMake automatically uses Homebrew's `gmake`, so our CI has synced output.
		brew install bison md5sha1sum make
		# Export `bison` to allow using the version we install from Homebrew,
		# instead of the outdated one preinstalled on macOS (which doesn't even support `-Wall`...).
		export PATH="$(brew --prefix)/opt/bison/bin:$PATH"
		printf 'PATH=%s\n' "$PATH" >>"$GITHUB_ENV" # Make it available to later CI steps too.
		;;
	freebsd)
		pkg install -y bash bison cmake git png
		;;
	windows)
		# GitHub Actions' hosted runners ship CMake 3.x, but versions prior to 4.0.0 ignore `CPACK_PACKAGE_FILE_NAME`.
		choco install -y winflexbison3 cmake
		# The below expects the base name, not the Windows-specific name.
		bison() { win_bison "$@"; } # An alias doesn't work, so we use a function instead.
		;;
	*)
		echo "Cannot install deps for OS '$1'"
		exit 1
		;;
esac

# Print some system info, for easier debugging.
# https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-commands#grouping-log-lines

echo ::group::PATH
echo "PATH=($PATH)" | sed 's/:/\n      /g'
echo ::endgroup::

for prog in bison make cmake; do
	printf '::group::' # No line terminator, the next command's first line becomes the group's title.
	$prog --version
	type $prog
	echo ::endgroup::
done
