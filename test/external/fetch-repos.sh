#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
	cat <<"EOF"
Downloads source code of Game Boy project repos used as RGBDS test cases.
Options:
    -h, --help      show this help message
    --only-free     download only freely licensed codebases
    --get-hash      print repos' commit hashes instead of downloading them
    --get-paths     print repos' clone paths instead of downloading them
EOF
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
free=true
actionname=
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-free)
			nonfree=false
			;;
		--only-nonfree)
			free=false
			;;
		--get-hash|--get-paths)
			actionname="$1"
			;;
		--)
			break
			;;
		*)
			echo "$(basename "$0"): unknown option '$1'"
			exit 1
			;;
	esac
	shift
done

if ! "$nonfree" && ! "$free"; then
	echo "Specifying --only-nonfree with --only-free is a contradiction"
	exit 1
fi

case "$actionname" in
	--get-hash)
		action() {
			printf "%s@%s-" "$EXT_TEST_REPO" "$EXT_TEST_COMMIT"
		}
		;;

	--get-paths)
		action() {
			printf "test/external/%s," "$EXT_TEST_REPO"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() {
			if [ ! -d "$EXT_TEST_REPO" ]; then
				git clone "https://$EXT_TEST_DOMAIN/$EXT_TEST_OWNER/$EXT_TEST_REPO.git" \
					--revision="$EXT_TEST_COMMIT" --depth=1 --recursive --shallow-submodules \
					--config advice.detachedHead=false
			fi
			pushd "$EXT_TEST_REPO"
			git checkout --force --detach "$EXT_TEST_COMMIT" --
			if [ -f "../patches/$EXT_TEST_REPO.patch" ]; then
				git apply --ignore-whitespace "../patches/$EXT_TEST_REPO.patch"
			fi
			popd
		}
esac

# Each iteration is isolated in a (subshell) so the sourced cfg variables don't "leak" out.
for cfg in *.cfg; do (
	# Sourcing "$cfg" defines `EXT_TEST_*` variables that get used by `action`.
	. "$cfg"

	if $EXT_TEST_IS_NONFREE; then
		if $nonfree; then
			action
		fi
	else
		if $free; then
			action
		fi
	fi
); done
