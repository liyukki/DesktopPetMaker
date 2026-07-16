#!/usr/bin/env python3
"""Create an atomic, deterministic ZIP with POSIX entry separators."""

from __future__ import annotations

import argparse
import os
import tempfile
import zipfile
from pathlib import Path


def create_zip(root: Path, output: Path) -> int:
    root = root.resolve()
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    fd, temporary_name = tempfile.mkstemp(
        prefix=output.name + ".", suffix=".tmp", dir=output.parent
    )
    os.close(fd)
    temporary = Path(temporary_name)
    try:
        with zipfile.ZipFile(
            temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
        ) as archive:
            for path in sorted(
                (item for item in root.rglob("*") if item.is_file()),
                key=lambda item: item.relative_to(root).as_posix().lower(),
            ):
                archive.write(path, path.relative_to(root).as_posix())
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)

    with zipfile.ZipFile(output) as archive:
        bad = [name for name in archive.namelist() if "\\" in name]
        if bad:
            print(f"FAIL backslash ZIP entry: {bad[0]}")
            return 1
        print(f"PASS standard ZIP entries: {len(archive.infolist())}")
    return 0


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory) / "source"
        nested = root / "folder" / "child"
        nested.mkdir(parents=True)
        (nested / "中文.txt").write_text("ok", encoding="utf-8")
        output = Path(directory) / "fixture.zip"
        if create_zip(root, output) != 0:
            return 1
        extract = Path(directory) / "extract"
        with zipfile.ZipFile(output) as archive:
            archive.extractall(extract)
            if archive.namelist() != ["folder/child/中文.txt"]:
                print("FAIL unexpected ZIP entry names")
                return 1
        if (extract / "folder" / "child" / "中文.txt").read_text(encoding="utf-8") != "ok":
            print("FAIL extracted content")
            return 1
    print("PASS standard ZIP self-test")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.root is None or args.output is None:
        parser.error("--root and --output are required unless --self-test is used")
    return create_zip(args.root, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
