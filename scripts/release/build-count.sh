#!/usr/bin/env bash
# build-count.sh <core-repo> <tag>  -- the one BUILD number for a release (BR-2610-anvl-version-001).
# Computed once from the core release tag and passed to every artifact's build; never from a
# submodule's or a tarball's own git state.
set -eu
git -C "${1:?core repo}" rev-parse -q --verify "refs/tags/${2:?tag}^{commit}" >/dev/null \
  || { echo "tag $2 not found in $1" >&2; exit 1; }
git -C "$1" rev-list --count "$2"
