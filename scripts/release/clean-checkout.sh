#!/usr/bin/env bash
# clean-checkout.sh <binding-repo> <ref> <core-repo> <core-commit> <dest>
# A clean clone of <ref> (never the working tree) with vendor/anvil taken from <core-repo> and
# required to equal <core-commit>: the artifact is built from exactly the core commit it claims.
# Prints the binding commit on stdout. Submodule objects come from the local core repo, so a core
# commit that isn't pushed yet can still be built for a trial; the release itself uses a pushed tag.
set -eu
repo=$1 ref=$2 core=$3 want=$4 dest=$5
commit=$(git -C "$repo" rev-parse --verify -q "$ref^{commit}") || { echo "ref $ref not found in $repo" >&2; exit 1; }
git clone -q --no-hardlinks "$repo" "$dest"
git -C "$dest" checkout -q --detach "$commit"
git -C "$dest" -c protocol.file.allow=always -c submodule.vendor/anvil.url="$core" submodule update -q --init
got=$(git -C "$dest/vendor/anvil" rev-parse HEAD)
[ "$got" = "$(git -C "$core" rev-parse "$want^{commit}")" ] \
  || { echo "FAIL vendor/anvil pin $got != core commit $want" >&2; exit 1; }
echo "$commit"
