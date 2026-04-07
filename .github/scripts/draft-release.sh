#!/bin/bash
set -euo pipefail

tag=$1
[ $# -eq 1 ]

flags=(--draft --fail-on-no-commits --verify-tag)
if [[ "$tag" = v*-rc* ]]; then
	flags+=(--prerelease)
fi

files=(
	win64/rgbds-win64.zip
	win32/rgbds-win32.zip
	macos/rgbds-macos.zip
	linux/rgbds-linux-x86_64.tar.xz
	rgbds-source.tar.gz
)

set -x
gh release create "${flags[@]}" --notes-file - "$tag" "${files[@]}" <<"EOF"
Please ensure that the packages below work properly.
Once that's done, replace this text with the changelog, un-draft the release, and update the `release` branch.
By the way, if you forgot to update `include/version.hpp`, RGBASM's version test is going to fail in the tag's regression testing! (Use `git push --delete origin <tag>` to delete it)
EOF
