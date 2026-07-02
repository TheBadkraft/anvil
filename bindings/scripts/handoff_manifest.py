#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import subprocess
from datetime import datetime, timezone


def file_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def read_constants(path: str) -> dict:
    version = {
        "major": None,
        "minor": None,
        "patch": None,
        "tag": None,
        "str": None,
    }
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if s.startswith("#define ANVL_VERSION_MAJOR"):
                version["major"] = s.split()[-1]
            elif s.startswith("#define ANVL_VERSION_MINOR"):
                version["minor"] = s.split()[-1]
            elif s.startswith("#define ANVL_VERSION_PATCH"):
                version["patch"] = s.split()[-1]
            elif s.startswith("#define ANVL_VERSION_TAG"):
                version["tag"] = s.split()[-1].strip('"')
            elif s.startswith("#define ANVL_VERSION_STR"):
                version["str"] = s.split()[-1].strip('"')
    return version


def git_rev(root: str) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", root, "rev-parse", "HEAD"], text=True
        ).strip()
    except Exception:
        return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser(description="Create bindings handoff manifest")
    parser.add_argument("--root", required=True, help="Repository root")
    parser.add_argument("--out", required=True, help="Manifest output path")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    out = os.path.abspath(args.out)

    tracked = [
        "include/anvil.h",
        "include/context.h",
        "include/resolver.h",
        "include/schema.h",
        "include/vars.h",
        "include/constants.h",
        "docs/bindings-api-reference.md",
        "docs/Anvil C Users Guide.md",
        "docs/changelog.md",
        "README.md",
    ]

    files = {}
    for rel in tracked:
        path = os.path.join(root, rel)
        files[rel] = {
            "exists": os.path.exists(path),
            "sha256": file_sha256(path) if os.path.exists(path) else None,
        }

    manifest = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "repo_root": root,
        "git_rev": git_rev(root),
        "version": read_constants(os.path.join(root, "include/constants.h")),
        "files": files,
        "notes": [
            "Use docs/maintainers/bindings-signoff-checklist.md before release.",
            "If build/version changes, update docs/Anvil C Users Guide.md and docs/changelog.md.",
        ],
    }

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
