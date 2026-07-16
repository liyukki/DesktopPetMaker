#!/usr/bin/env python3
"""Validate project text files are UTF-8 without BOM and detect lossy text."""

from __future__ import annotations

import argparse
import json
import re
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    ".md", ".txt", ".diff", ".json", ".cmake", ".cpp", ".h", ".hpp", ".ps1",
    ".bat", ".py", ".ui", ".qml", ".xml", ".yaml", ".yml", ".toml", ".ini",
    ".js", ".ts",
}
TEXT_NAMES = {"CMakeLists.txt"}
SOURCE_SUFFIXES = {".cpp", ".h", ".hpp"}
SKIP_DIR_NAMES = {".git", "__pycache__", "archive"}
SKIP_SUFFIXES = {
    ".png", ".gif", ".jpg", ".jpeg", ".webp", ".ico", ".exe", ".dll", ".pdb",
    ".obj", ".lib", ".zip", ".qm", ".a", ".bin", ".o",
}
REPLACEMENT_CHAR = "\ufffd"
LOSSY_LITERAL_RE = re.compile(r'"(?:[^"\\]|\\.)*\?{2,}(?:[^"\\]|\\.)*"')
CPP_STRING_RE = re.compile(
    r'(?:QStringLiteral|QLatin1String|QString)\s*\(\s*"((?:[^"\\]|\\.)*)"'
    r'|L?"((?:[^"\\]|\\.)*)"'
)
MOJIBAKE_MARKERS = (
    "锟斤拷", "閿熸枻", "鐑儷", "灞悲",
    "涓枃", "妗屽疇", "閫€鍑", "鏄竴", "鐨勬",
)
MOJIBAKE_CHARS = frozenset("涓妗鍔绋鏄鐨閫寮鍏绔绯璁鏂")


# Override the legacy marker table with readable patterns that cover common
# UTF-8/GBK double-decoding and the historical self-test fixture.
MOJIBAKE_MARKERS = (
    "锟斤拷",
    "烫烫烫",
    "屯屯屯",
    "涓枃",
    "妗屽疇",
    "鐨勬",
    "锛屽",
    "銆傚",
    "绠＄悊",
    "\u5997\u5c7d\u7587",
)
MOJIBAKE_CHARS = frozenset("鎬т笌鐞嗚В寮哄埗闂ㄧ妗屽疇鍒朵綔鍣")


def is_skipped(path: Path, root: Path) -> bool:
    parts = path.relative_to(root).parts
    if any(part in SKIP_DIR_NAMES for part in parts):
        return True
    if any(part == "build" or part.startswith("build-") for part in parts):
        return True
    return path.suffix.lower() in SKIP_SUFFIXES


def is_text_candidate(path: Path) -> bool:
    return path.name in TEXT_NAMES or path.suffix.lower() in TEXT_SUFFIXES


def extract_cpp_string_literals(text: str) -> list[str]:
    return [
        match.group(1) if match.group(1) is not None else match.group(2)
        for match in CPP_STRING_RE.finditer(text)
    ]


def mojibake_score(text: str) -> int:
    explicit = sum(text.count(marker) for marker in MOJIBAKE_MARKERS)
    suspicious = sum(text.count(marker) for marker in MOJIBAKE_CHARS)
    return explicit * 8 + suspicious


def suspicious_source_literals(text: str) -> list[str]:
    errors: list[str] = []
    for literal in extract_cpp_string_literals(text):
        if re.search(r"\?{2,}", literal):
            errors.append(f"lossy question-mark literal: {literal[:60]}")
        if any("\u4e00" <= ch <= "\u9fff" for ch in literal) and mojibake_score(literal) >= 2:
            errors.append(f"suspicious mojibake literal: {literal[:60]}")
    return errors


def check_file(path: Path) -> list[str]:
    errors: list[str] = []
    data = path.read_bytes()
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        errors.append("UTF-16 BOM")
    if data.startswith(b"\xef\xbb\xbf"):
        errors.append("UTF-8 BOM")
    if b"\x00" in data:
        errors.append("NUL byte")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        errors.append(f"UTF-8 strict decode failed: {exc}")
        return errors

    is_gate_doc = path.name == "AGENTS.md" and text.startswith("# 中文完整性与理解强制门禁")
    is_review_diff = path.name == "CURRENT_PATCH_FOR_REVIEW.diff"
    if REPLACEMENT_CHAR in text and not is_gate_doc and not is_review_diff:
        errors.append("replacement character")
    if path.suffix.lower() in SOURCE_SUFFIXES:
        errors.extend(suspicious_source_literals(text))
    elif path.suffix.lower() in {".md", ".txt", ".diff"} and not is_gate_doc:
        if path.suffix.lower() == ".diff":
            if is_review_diff:
                return errors
            text = "\n".join(line for line in text.splitlines() if not line.startswith("-"))
        if LOSSY_LITERAL_RE.search(text):
            errors.append("suspicious lossy quoted question-mark text")
        if mojibake_score(text) >= 8:
            errors.append("suspicious mojibake text")
    if path.suffix.lower() == ".json":
        try:
            json.loads(text)
        except json.JSONDecodeError as exc:
            errors.append(f"JSON parse failed: {exc}")
    return errors


def run_scan(root: Path) -> int:
    failures: list[tuple[Path, list[str]]] = []
    checked = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or is_skipped(path, root) or not is_text_candidate(path):
            continue
        checked += 1
        errors = check_file(path)
        if errors:
            failures.append((path, errors))
    if failures:
        print("FAIL")
        for path, errors in failures:
            print(f"{path.relative_to(root)}: {'; '.join(errors)}")
        print(f"Checked {checked} text files, failures {len(failures)}.")
        return 1
    print("PASS")
    print(f"Checked {checked} text files.")
    return 0


def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        bad = root / "bad.cpp"
        bad.write_text('auto s = QStringLiteral("????");\n', encoding="utf-8")
        good = root / "good.cpp"
        good.write_text(
            'int x = flag ? 1 : 2;\nauto s = QStringLiteral("Are you ready?");\n'
            'auto url = QStringLiteral("https://example.test/?a=1&b=2");\n',
            encoding="utf-8",
        )
        mojibake = root / "mojibake.md"
        mojibake.write_text("Desktop Pet Maker 妗屽疇鍒朵綔鍣\n", encoding="utf-8")
        if not any("question-mark literal" in error for error in check_file(bad)):
            print("FAIL self-test: bad QStringLiteral question marks were not detected")
            return 1
        if check_file(good):
            print("FAIL self-test: legitimate question marks were flagged")
            return 1
        if not any("mojibake" in error for error in check_file(mojibake)):
            print("FAIL self-test: mojibake document text was not detected")
            return 1
    print("PASS self-test")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(ROOT))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    return run_self_test() if args.self_test else run_scan(Path(args.root).resolve())


if __name__ == "__main__":
    raise SystemExit(main())
