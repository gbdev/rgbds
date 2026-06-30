#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
	cat <<"EOF"
Downloads source code of Game Boy programs used as RGBDS test cases.
Options:
    -h, --help      show this help message
    --only-free     download only freely licensed codebases
    --get-hash      print programs' commit hashes instead of downloading them
    --get-paths     print programs' GitHub paths instead of downloading them
EOF
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
actionname=
osname=
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-free)
			nonfree=false
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

case "$actionname" in
	--get-hash)
		action() {
			printf "%s@%s-" "$EXTERNAL_TEST_REPO" "$EXTERNAL_TEST_COMMIT"
		}
		;;

	--get-paths)
		action() {
			printf "test/%s," "$EXTERNAL_TEST_REPO"
		}
		;;

	*)
		echo "Fetching test dependency repositories"

		action() {
			if [ ! -d "$EXTERNAL_TEST_REPO" ]; then
				git clone "https://$EXTERNAL_TEST_DOMAIN/$EXTERNAL_TEST_OWNER/$EXTERNAL_TEST_REPO.git" \
					--revision="$EXTERNAL_TEST_COMMIT" --depth=1 --recursive --shallow-submodules \
					--config advice.detachedHead=false
			fi
			pushd "$EXTERNAL_TEST_REPO"
			git checkout --force --detach "$EXTERNAL_TEST_COMMIT" --
			if [ -f "../patches/$EXTERNAL_TEST_REPO.patch" ]; then
				git apply --ignore-whitespace "../patches/$EXTERNAL_TEST_REPO.patch"
			fi
			popd
		}
esac

# Sourcing each "external/*.sh" file defines `EXTERNAL_TEST_*` values used by the `action` functions.
if "$nonfree"; then
	. external/pokecrystal.sh && action
	. external/pokered.sh     && action
	. external/ladx.sh        && action
fi
. external/ucity.sh          && action
. external/libbet.sh         && action
. external/sameboy.sh        && action
. external/gb-starter-kit.sh && action
