#!/usr/bin/env python3
"""Create a deterministic SHA-256 manifest for review-relevant project files."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


IGNORED_DIRS = {".git", ".idea", ".vs", ".vscode", "__pycache__", "archive"}
IGNORED_PREFIXES = ("build",)
IGNORED_SUFFIXES = {
    ".7z", ".a", ".dll", ".exe", ".gif", ".ico", ".lib", ".obj", ".pdb",
    ".png", ".pyc", ".zip",
}
GENERATED_FILES = {"CURRENT_PATCH_FOR_REVIEW.diff", "SOURCE_MANIFEST_SHA256.txt"}
GENERATED_SUFFIXES = ("_LOG.txt", "_RESULTS.xml", "-result.txt", "-test.txt")


def included(path: Path, root: Path) -> bool:
    relative = path.relative_to(root)
    if any(part in IGNORED_DIRS or part.lower().startswith(IGNORED_PREFIXES) for part in relative.parts[:-1]):
        return False
    if path.suffix.lower() in IGNORED_SUFFIXES:
        return False
    return path.name not in GENERATED_FILES and not path.name.endswith(GENERATED_SUFFIXES)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--all-files", action="store_true",
                        help="Hash every file except the manifest itself (for portable bundles).")
    parser.add_argument("--exclude-name", action="append", default=[],
                        help="Exclude a file by basename. May be repeated.")
    args = parser.parse_args()
    root = args.root.resolve()
    output = (args.output or root / "SOURCE_MANIFEST_SHA256.txt").resolve()

    rows: list[str] = []
    for path in sorted((item for item in root.rglob("*") if item.is_file()), key=lambda item: item.as_posix().lower()):
        if (path.resolve() == output or path.name in args.exclude_name
                or (not args.all_files and not included(path, root))):
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        rows.append(f"{digest}  {path.relative_to(root).as_posix()}")

    output.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS source manifest: {len(rows)} files -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
