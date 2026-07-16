#!/usr/bin/env python3
"""Check that CMake references existing sources and local sources are classified."""

from __future__ import annotations

import re
import sys
from pathlib import Path


SOURCE_EXTENSIONS = {".cpp", ".cxx", ".cc", ".h", ".hpp"}
SKIP_DIRS = {".git", "build", "build-codex", "build-codex-debug", "__pycache__"}
ALLOW_UNREFERENCED = {
    # Historical/legacy UI still kept for compatibility review but intentionally
    # not linked into the current production/test target.
    "aisettingsdialog.cpp",
    "aisettingsdialog.h",
}


def strip_cmake_comments(text: str) -> str:
    return re.sub(r"#.*", "", text)


def referenced_sources(cmake_text: str) -> set[str]:
    text = strip_cmake_comments(cmake_text)
    pattern = re.compile(r'(?:"([^"]+\.(?:cpp|cxx|cc|h|hpp))"|([^\s()"]+\.(?:cpp|cxx|cc|h|hpp)))', re.IGNORECASE)
    refs: set[str] = set()
    for match in pattern.findall(text):
        rel = match[0] or match[1]
        if "\\\\." in rel:
            # Regex arguments such as main\\.cpp are not source paths.
            continue
        rel_path = Path(rel)
        if rel_path.is_absolute() or any(part.startswith("$") for part in rel_path.parts):
            continue
        refs.add(rel.replace("\\", "/"))
    return refs


def repository_sources(root: Path) -> set[str]:
    sources: set[str] = set()
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        rel_parts = path.relative_to(root).parts
        if any(part in SKIP_DIRS or part.startswith("build-") for part in rel_parts):
            continue
        sources.add(path.relative_to(root).as_posix())
    return sources


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd().resolve()
    cmake = root / "CMakeLists.txt"
    if not cmake.exists():
        print(f"FAIL CMakeLists.txt not found: {cmake}")
        return 1

    refs = referenced_sources(cmake.read_text(encoding="utf-8"))
    repo_sources = repository_sources(root)
    missing = sorted(rel for rel in refs if not (root / rel).exists())
    unreferenced = sorted(rel for rel in repo_sources - refs if Path(rel).name not in ALLOW_UNREFERENCED)
    allowed = sorted(rel for rel in repo_sources - refs if Path(rel).name in ALLOW_UNREFERENCED)

    print(f"Referenced source files: {len(refs)}")
    print(f"Repository source files: {len(repo_sources)}")
    if allowed:
        print("Allowed unreferenced:")
        for rel in allowed:
            print(f"- {rel}")

    if missing or unreferenced:
        print("FAIL")
        for rel in missing:
            print(f"Missing: {rel}")
        for rel in unreferenced:
            print(f"Unreferenced: {rel}")
        return 1

    print("Missing: 0")
    print("Unreferenced: 0")
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
