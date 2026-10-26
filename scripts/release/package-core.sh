#!/usr/bin/env bash
# package-core.sh <core-repo> <tag> <out-dir>
#
# Builds the core tarball anvil-v<version>-linux-x86_64.tar.gz from a clean export of <tag> (git
# archive: no working tree, no .git), with BUILD passed explicitly (BR-2610-anvl-version-001), then
# gates the *extracted tarball*: grammar + version (check-grammar.sh), exports (check-exports.sh),
# and that every shipped header compiles standalone. Same inputs produce the same tarball bytes.
set -eu
repo=$(cd "${1:?core repo}" && pwd); tag=${2:?tag e.g. v0.10.0-rc}; out=${3:?out dir}
here=$(cd "$(dirname "$0")" && pwd); ver=${tag#v}
mkdir -p "$out"; out=$(cd "$out" && pwd)
build=$("$here/build-count.sh" "$repo" "$tag")
commit=$(git -C "$repo" rev-parse "$tag^{commit}")
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
name=anvil-v$ver-linux-x86_64

git -C "$repo" archive --format=tar "$tag" | tar -x -C "$work"
# Sigma sources are vendored in-tree (src/sigma), the allocator comes from the installed
# sigma.system.alloc package the Makefile already links.
( cd "$work" && make -j"$(nproc)" lib-release so-release BUILD_NUM="$build" ANVL_BUILD="$build" >"$work/make.log" 2>&1 ) \
  || { tail -20 "$work/make.log"; echo "FAIL core build" >&2; exit 1; }

pkg=$work/$name; mkdir -p "$pkg/lib" "$pkg/include"
cp "$work/lib/release/libanvil.a" "$work/lib/release/libanvil.so" "$pkg/lib/"
cp "$work"/include/anvil_*.h "$pkg/include/"
sed "s/@VERSION@/$ver/; s/@COMMIT@/${commit:0:12}/; s/@BUILD@/$build/" "$here/templates/README-core.md" > "$pkg/README.md"
cat > "$pkg/BUILD_INFO.json" <<JSON
{
  "artifact": "anvil",
  "version": "$ver",
  "reported_version": "$ver+$build",
  "build": $build,
  "core_tag": "$tag",
  "core_commit": "$commit"
}
JSON

for h in "$pkg"/include/anvil_*.h; do
  echo "#include \"$(basename "$h")\"" | cc -fsyntax-only -x c -I"$pkg/include" - \
    || { echo "FAIL header does not compile standalone: $h" >&2; exit 1; }
done
"$here/check-grammar.sh" core "$pkg" "$ver" "$build"
sh "$here/../check-exports.sh" "$pkg/lib/libanvil.so" "$pkg/include"

# Deterministic tarball: sorted names, commit time as mtime, no owner, no gzip timestamp.
mtime=$(git -C "$repo" log -1 --format=%cI "$tag")
tar --sort=name --mtime="$mtime" --owner=0 --group=0 --numeric-owner -cf - -C "$work" "$name" | gzip -n -9 > "$out/$name.tar.gz"
echo "OK $out/$name.tar.gz (core $tag $commit, BUILD $build)"
