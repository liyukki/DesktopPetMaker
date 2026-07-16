#!/usr/bin/env python3
"""Create a detached, auditable hash chain for final release artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from xml.etree import ElementTree


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def junit_summary(path: Path) -> dict[str, object]:
    root = ElementTree.parse(path).getroot()
    return {
        "path": str(path),
        "sha256": digest(path),
        "tests": int(root.attrib.get("tests", 0)),
        "failures": int(root.attrib.get("failures", 0)),
        "skipped": int(root.attrib.get("skipped", 0)),
    }


def artifact(path: Path) -> dict[str, object]:
    return {"path": str(path), "size": path.stat().st_size, "sha256": digest(path)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-zip", type=Path, required=True)
    parser.add_argument("--review-zip", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--runtime-manifest", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--debug-junit", type=Path, required=True)
    parser.add_argument("--release-junit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    paths = [
        args.runtime_zip, args.review_zip, args.source_manifest,
        args.runtime_manifest, args.executable, args.debug_junit, args.release_junit,
    ]
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        print("FAIL missing artifact: " + ", ".join(missing))
        return 1

    chain = {
        "schemaVersion": 2,
        "generatedAtUtc": datetime.now(timezone.utc).isoformat(),
        "runtimeZip": artifact(args.runtime_zip),
        "sourceReviewZip": artifact(args.review_zip),
        "sourceManifest": artifact(args.source_manifest),
        "runtimeManifest": artifact(args.runtime_manifest),
        "executable": {
            **artifact(args.executable),
            "authenticode": "NOT_SIGNED",
        },
        "junit": {
            "debug": junit_summary(args.debug_junit),
            "release": junit_summary(args.release_junit),
        },
        "externalStatus": {
            "cleanWindows10Vm": "BLOCKED_EXTERNAL",
            "cleanWindows11Vm": "BLOCKED_EXTERNAL",
            "authenticodeCertificate": "BLOCKED_EXTERNAL",
            "realCubismRenderer": "BLOCKED_EXTERNAL",
            "licenseOwnerApproval": "MANUAL_TEST",
        },
    }
    data = json.dumps(chain, ensure_ascii=False, indent=2).encode("utf-8") + b"\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary_name = tempfile.mkstemp(
        prefix=args.output.name + ".", suffix=".tmp", dir=args.output.parent
    )
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, args.output)
    finally:
        Path(temporary_name).unlink(missing_ok=True)
    print(f"PASS final artifact chain -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
