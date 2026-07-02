#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
NODE_DIR="$ROOT/bindings/node"

if [[ ! -f "$NODE_DIR/package.json" ]]; then
  echo "[bindings/node] package.json missing"
  exit 1
fi

echo "[bindings/node] scaffold present"
