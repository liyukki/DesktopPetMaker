#!/usr/bin/env python3
"""Verify a SHA-256 manifest and reject missing, changed, or unexpected files."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from create_source_manifest import included


def parse_manifest(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            digest, relative = line.split("  ", 1)
        except ValueError as exc:
            raise ValueError(f"invalid manifest line {line_number}") from exc
        normalized = Path(relative).as_posix()
        if len(digest) != 64 or any(ch not in "0123456789abcdef" for ch in digest.lower()):
            raise ValueError(f"invalid SHA-256 on line {line_number}")
        if normalized in entries:
            raise ValueError(f"duplicate manifest entry: {normalized}")
        entries[normalized] = digest.lower()
    return entries


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--all-files", action="store_true")
    parser.add_argument("--exclude-name", action="append", default=[])
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = (args.manifest or root / "SOURCE_MANIFEST_SHA256.txt").resolve()

    try:
        expected = parse_manifest(manifest)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"FAIL manifest parse: {exc}")
        return 1

    actual_paths: dict[str, Path] = {}
    for path in (item for item in root.rglob("*") if item.is_file()):
        if path.resolve() == manifest or path.name in args.exclude_name:
            continue
        if not args.all_files and not included(path, root):
            continue
        actual_paths[path.relative_to(root).as_posix()] = path

    missing = sorted(set(expected) - set(actual_paths))
    unexpected = sorted(set(actual_paths) - set(expected))
    mismatched = []
    for relative in sorted(set(expected) & set(actual_paths)):
        digest = hashlib.sha256(actual_paths[relative].read_bytes()).hexdigest()
        if digest != expected[relative]:
            mismatched.append(relative)

    print(f"Manifest entries: {len(expected)}")
    print(f"Missing: {len(missing)}")
    print(f"Hash Mismatch: {len(mismatched)}")
    print(f"Unexpected Source File: {len(unexpected)}")
    for label, values in (("MISSING", missing), ("HASH_MISMATCH", mismatched), ("UNEXPECTED", unexpected)):
        for value in values:
            print(f"{label} {value}")
    if missing or mismatched or unexpected:
        print("FAIL")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
