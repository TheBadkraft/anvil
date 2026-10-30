# Sourced by the per-binding packagers. Parses the one shared flag set and defines helpers.
#
#   --version 0.12.0-rc   release version (core tag without the leading v)
#   --build 276           BUILD, `git rev-list --count <core tag>`, from build-count.sh (computed once)
#   --core-repo PATH      repo holding the core tag/commit (default: the shared anvil checkout, read-only use)
#   --core-commit SHA     the core commit this release is built from; the binding's vendor/anvil pin must equal it
#   --ref REF             binding ref to build (default: the release branch, release/v0.12.0-rc)
#   --out DIR             where the tarball goes
# RELEASE_TOOLS overrides where check-*.sh / clean-checkout.sh live (default: this directory).
set -eu
RELEASE_TOOLS=${RELEASE_TOOLS:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}
REF=release/v0.12.0-rc; CORE_REPO=$HOME/Projects/repos/anvil
while [ $# -gt 0 ]; do
  case $1 in
    --version) VERSION=$2 ;; --build) BUILD=$2 ;; --core-repo) CORE_REPO=$2 ;;
    --core-commit) CORE_COMMIT=$2 ;; --ref) REF=$2 ;; --out) OUT=$2 ;;
    *) echo "unknown flag: $1" >&2; exit 2 ;;
  esac; shift 2
done
: "${VERSION:?--version}" "${BUILD:?--build}" "${CORE_COMMIT:?--core-commit}" "${OUT:?--out}"
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)

# Deterministic tarball: sorted names, fixed mtime, no owner, no gzip timestamp.
make_tarball() { # <parent dir> <dir name> <mtime ISO> -> $OUT/<dir name>.tar.gz
  tar --sort=name --mtime="$3" --owner=0 --group=0 --numeric-owner -cf - -C "$1" "$2" | gzip -n -9 > "$OUT/$2.tar.gz"
}

# libanvil.so for bindings that ship the library: taken from the staged core tarball (built from the
# same tag with the same BUILD and already export-gated), so every artifact carries identical bytes.
core_lib() { # <dest dir>
  local tgz=$OUT/anvil-v$VERSION-linux-x86_64.tar.gz
  [ -f "$tgz" ] || { echo "FAIL core tarball not staged: $tgz (run package-core.sh first)" >&2; return 1; }
  tar -xzf "$tgz" -C "$1" --strip-components=1 "anvil-v$VERSION-linux-x86_64/lib/libanvil.so"
}
