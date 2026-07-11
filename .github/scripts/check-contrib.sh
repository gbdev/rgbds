#!/usr/bin/env sh
set -e

bash --version
zsh --version

find contrib -type f \( -name '*.sh' -o -name '*.bash' \) -exec bash --norc -n {} \;
find contrib/zsh_compl -type f -name '_rgb*' -exec zsh -n {} \;
