#!/usr/bin/env sh

local_bin_dir="$PWD/.local/bin"

mkdir -p "$local_bin_dir"

# Alias `make` to `gmake` and `python` to `python3` for use by external tests.
ln -s "$(which gmake)" "$local_bin_dir/make"
ln -s "$(which python3)" "$local_bin_dir/python"

export PATH="$local_bin_dir:$PATH"
