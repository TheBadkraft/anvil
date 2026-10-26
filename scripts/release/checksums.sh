#!/usr/bin/env bash
# checksums.sh write|verify <dir>  -- SHA256SUMS next to the tarballs.
set -eu
mode=${1:?write|verify}; dir=$(cd "${2:?dir}" && pwd); cd "$dir"
case $mode in
  write)  sha256sum *.tar.gz > SHA256SUMS; cat SHA256SUMS ;;
  verify) sha256sum -c SHA256SUMS ;;
  *) echo "usage: checksums.sh write|verify <dir>" >&2; exit 2 ;;
esac
