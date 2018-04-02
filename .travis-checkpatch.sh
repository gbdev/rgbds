#!/bin/bash

echo "Checking code style..."

# Return failure as soon as a command fails to execute

set -e

# Download checkpatch.pl and related files

echo "Getting checkpatch.pl..."

mkdir checkpatchdir

wget https://raw.githubusercontent.com/torvalds/linux/master/scripts/checkpatch.pl
mv checkpatch.pl checkpatchdir/checkpatch.pl
chmod +x checkpatchdir/checkpatch.pl

touch checkpatchdir/const_structs.checkpatch
touch checkpatchdir/spelling.txt

# Run checkpatch.pl on the new commits

echo "Running checkpatch.pl..."

fname=$(mktemp)
rc=0

git remote set-branches --add origin develop
git fetch

make CHECKPATCH=checkpatchdir/checkpatch.pl checkpatch > $fname

cat $fname

if grep "ERROR" $fname; then
    # At least one error found
    echo "Code style errors have been found!"
    rc=1
else
    echo "No code style errors found, your patches are ready!"
fi

# Cleanup

rm -rf checkpatchdir

exit $rc
