#!/usr/bin/env bash
# check-license.sh <staging-dir> <release-version>
# LICENSE.txt is present in every tarball of the release (core, py, net, anvil.js) and byte-identical to
# templates/LICENSE.txt, hence identical across all four.
set -eu
dir=$(cd "${1:?staging dir}" && pwd); ver=${2:?version}; here=$(cd "$(dirname "$0")" && pwd)
want=$(sha256sum < "$here/templates/LICENSE.txt" | cut -d' ' -f1); rc=0
for f in anvil-v$ver-linux-x86_64 anvil-py-v$ver-linux-x64 anvil-net-v$ver-linux-x64 anvil.js-v$ver; do
  got=$(tar -xzOf "$dir/$f.tar.gz" "$(tar -tzf "$dir/$f.tar.gz" | grep -E '^[^/]+/LICENSE\.txt$' | head -1)" 2>/dev/null | sha256sum | cut -d' ' -f1)
  if [ "$got" = "$want" ]; then echo "PASS license: $f.tar.gz has LICENSE.txt $want"; else echo "FAIL license: $f.tar.gz LICENSE.txt missing or different"; rc=1; fi
done
exit $rc
