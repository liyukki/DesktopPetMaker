#!/usr/bin/env python3
"""Exercise manifest verification failure modes without changing the source manifest."""

from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path


VERIFY = Path(__file__).with_name("verify_source_manifest.py")


def write_manifest(root: Path, entries: dict[str, bytes]) -> Path:
    manifest = root / "SOURCE_MANIFEST_SHA256.txt"
    rows = [
        f"{hashlib.sha256(content).hexdigest()}  {relative}"
        for relative, content in sorted(entries.items())
    ]
    manifest.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    return manifest


def verify(root: Path, manifest: Path) -> int:
    result = subprocess.run(
        [
            sys.executable,
            str(VERIFY),
            "--root",
            str(root),
            "--manifest",
            str(manifest),
        ],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode


def require_failure(label: str, mutate) -> None:
    with tempfile.TemporaryDirectory(prefix="desktop_pet_manifest_gate_") as temp:
        root = Path(temp)
        source = root / "sample.cpp"
        source.write_text("int value = 1;\n", encoding="utf-8", newline="\n")
        manifest = write_manifest(root, {"sample.cpp": source.read_bytes()})
        mutate(root, source, manifest)
        if verify(root, manifest) == 0:
            raise AssertionError(f"{label} was not rejected")


def main() -> int:
    require_failure(
        "modified source",
        lambda _root, source, _manifest: source.write_text(
            "int value = 2;\n", encoding="utf-8", newline="\n"
        ),
    )
    require_failure(
        "unregistered source",
        lambda root, _source, _manifest: (root / "new_file.h").write_text(
            "#pragma once\n", encoding="utf-8", newline="\n"
        ),
    )
    require_failure(
        "deleted source",
        lambda _root, source, _manifest: source.unlink(),
    )

    def corrupt_hash(_root: Path, _source: Path, manifest: Path) -> None:
        text = manifest.read_text(encoding="utf-8")
        manifest.write_text("0" * 64 + text[64:], encoding="utf-8", newline="\n")

    require_failure("incorrect hash", corrupt_hash)
    print("PASS manifest gate rejects modified, new, deleted, and wrong-hash inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
