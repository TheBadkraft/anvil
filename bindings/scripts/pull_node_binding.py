#!/usr/bin/env python3
"""
Copy built Anvil Node addon from this repo into sibling anvil.js repo.

Default source:
  <anvil>/bindings/node/build/Release/anvil_binding.node

Default destination:
  <anvil.js>/build/Release/anvil.node
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path


def default_paths(script_path: Path) -> tuple[Path, Path]:
    # script path: <anvil>/bindings/scripts/pull_node_binding.py
    anvil_root = script_path.resolve().parents[2]
    src = anvil_root / "bindings" / "node" / "build" / "Release" / "anvil_binding.node"
    dst = anvil_root.parent / "anvil.js" / "build" / "Release" / "anvil.node"
    return src, dst


def parse_args() -> argparse.Namespace:
    here = Path(__file__)
    src_default, dst_default = default_paths(here)

    p = argparse.ArgumentParser(description="Pull built node binding into anvil.js")
    p.add_argument("--source", default=str(src_default), help="Path to built .node source")
    p.add_argument("--dest", default=str(dst_default), help="Destination .node path")
    p.add_argument("--dry-run", action="store_true", help="Show actions without copying")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    src = Path(args.source).expanduser().resolve()
    dst = Path(args.dest).expanduser().resolve()

    if not src.exists():
        print(f"[pull-node] source not found: {src}", file=sys.stderr)
        print("[pull-node] build first: npm run build in bindings/node", file=sys.stderr)
        return 1

    dst.parent.mkdir(parents=True, exist_ok=True)

    if args.dry_run:
        print(f"[pull-node] dry-run: {src} -> {dst}")
        return 0

    shutil.copy2(src, dst)
    print(f"[pull-node] copied: {src} -> {dst}")
    print(f"[pull-node] size: {dst.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
