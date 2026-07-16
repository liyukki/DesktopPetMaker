#!/usr/bin/env python3
"""Create a separately distributed original robot sample petpack."""

from __future__ import annotations

import hashlib
import json
import tempfile
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT.parent / "DesktopPet-Original-Robot-Sample.petpack"


def frame(path: Path, phase: int) -> None:
    image = Image.new("RGBA", (160, 180), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    y = 24 + phase
    draw.rounded_rectangle((35, y + 20, 125, y + 120), radius=24,
                           fill=(69, 190, 184, 255), outline=(20, 61, 72, 255), width=6)
    draw.rectangle((61, y, 99, y + 24), fill=(247, 145, 104, 255))
    draw.ellipse((55, y + 50, 73, y + 68), fill=(20, 61, 72, 255))
    draw.ellipse((87, y + 50, 105, y + 68), fill=(20, 61, 72, 255))
    draw.arc((62, y + 61, 98, y + 91), 10, 170, fill=(20, 61, 72, 255), width=5)
    draw.line((54, y + 120, 42, 165), fill=(20, 61, 72, 255), width=8)
    draw.line((106, y + 120, 118, 165), fill=(20, 61, 72, 255), width=8)
    image.save(path, format="PNG", optimize=True)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="desktop_pet_original_sample_") as temp:
        root = Path(temp)
        idle = root / "assets" / "idle"
        idle.mkdir(parents=True)
        frame(idle / "0001.png", 0)
        frame(idle / "0002.png", 3)
        (root / "cover.png").write_bytes((idle / "0001.png").read_bytes())
        project = {
            "version": 1,
            "name": "Original Robot Sample",
            "template": "simple",
            "cover": "cover.png",
            "canvas": {"width": 160, "height": 180},
            "anchor": {"x": 80, "y": 170},
            "runtime": {
                "scale": 1.0,
                "locked": False,
                "mousePassthrough": False,
                "topMost": True,
                "patrolEnabled": True,
                "invertWalkDirection": False,
            },
            "ai": {
                "characterName": "小方",
                "systemPrompt": "你是一个简洁、友善的原创机器人桌宠。",
                "providerProfileId": "default",
            },
            "actions": {
                "idle": {
                    "displayName": "待机",
                    "frames": [
                        {"path": "assets/idle/0001.png", "offset": {"x": 0, "y": 0}},
                        {"path": "assets/idle/0002.png", "offset": {"x": 0, "y": 0}},
                    ],
                    "fps": 3,
                    "loop": True,
                    "next": "idle",
                    "events": {},
                    "frameDurationsMs": [],
                    "allowAiTrigger": False,
                    "allowRandomTrigger": False,
                }
            },
        }
        (root / "pet.json").write_text(
            json.dumps(project, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        license_text = (
            "Original Robot Sample\n"
            "Created for Desktop Pet Maker on 2026-07-16.\n"
            "The geometric artwork and metadata are original project assets.\n"
            "Redistribution and modification with Desktop Pet Maker are permitted.\n"
        )
        (root / "ASSET_LICENSE.txt").write_text(license_text, encoding="utf-8", newline="\n")
        temporary = OUTPUT.with_suffix(OUTPUT.suffix + ".tmp")
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(root).as_posix())
        temporary.replace(OUTPUT)
    print(f"PASS original sample -> {OUTPUT}")
    print(f"SHA256 {hashlib.sha256(OUTPUT.read_bytes()).hexdigest().upper()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
