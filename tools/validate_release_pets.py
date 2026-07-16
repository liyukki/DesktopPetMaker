#!/usr/bin/env python3
import json
import sys
import hashlib
from pathlib import Path

try:
    from PIL import Image
except Exception:
    Image = None


def is_transparent(path: Path) -> bool:
    if Image is None:
        return False
    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        return rgba.getbbox() is None


def validate_pet(pet_json: Path):
    failures = []
    root = json.loads(pet_json.read_text(encoding="utf-8"))
    project_dir = pet_json.parent
    actions = root.get("actions", {})
    if not actions:
        failures.append("no actions")
    role_owners = {}
    frame_path_sequences = {}
    frame_hash_sequences = {}
    for action_id, action in actions.items():
        frames = action.get("frames", [])
        if not frames:
            failures.append(f"{action_id}: zero-frame release action")
        role = action.get("systemRole", "")
        if role and role != "None":
            role_owners.setdefault(role, []).append(action_id)
        next_id = action.get("nextActionId") or action.get("next") or ""
        if next_id and next_id not in actions:
            failures.append(f"{action_id}: next references missing action {next_id}")
        rel_sequence = []
        hash_sequence = []
        for frame in frames:
            rel = frame.get("path", "")
            rel_sequence.append(rel.replace("\\", "/"))
            frame_path = (project_dir / rel).resolve()
            if not frame_path.exists():
                failures.append(f"{action_id}: missing frame {rel}")
                continue
            if frame_path.suffix.lower() != ".png":
                failures.append(f"{action_id}: non-png frame {rel}")
            try:
                hash_sequence.append(hashlib.sha256(frame_path.read_bytes()).hexdigest())
            except Exception as exc:
                failures.append(f"{action_id}: unreadable frame bytes {rel}: {exc}")
            if Image is not None:
                try:
                    if is_transparent(frame_path):
                        failures.append(f"{action_id}: fully transparent frame {rel}")
                except Exception as exc:
                    failures.append(f"{action_id}: unreadable frame {rel}: {exc}")
        if frames:
            frame_path_sequences.setdefault(tuple(rel_sequence), []).append(action_id)
            if len(hash_sequence) == len(frames):
                frame_hash_sequences.setdefault(tuple(hash_sequence), []).append(action_id)
    for role, owners in role_owners.items():
        if len(owners) > 1:
            failures.append(f"duplicate systemRole {role}: {', '.join(owners)}")
    for sequence, owners in frame_path_sequences.items():
        if len(owners) > 1:
            failures.append(f"duplicate frame path sequence: {', '.join(owners)}")
    for sequence, owners in frame_hash_sequences.items():
        if len(owners) > 1:
            failures.append(f"duplicate frame content sequence: {', '.join(owners)}")
    for required in ("Idle", "Dragging", "Falling"):
        if required not in role_owners:
            failures.append(f"missing required role {required}")
    return failures


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    candidates = [root.parent / "pet_animated" / "pet.json", root.parent / "pet_animated_1" / "pet.json"]
    available = [pet_json for pet_json in candidates if pet_json.exists()]
    if not available:
        print("NOT CONFIGURED optional release pet projects are excluded from the public source package")
        return 0
    any_failures = False
    for pet_json in available:
        failures = validate_pet(pet_json)
        if failures:
            any_failures = True
            print(f"FAIL {pet_json}")
            for failure in failures:
                print(f"- {failure}")
        else:
            print(f"PASS {pet_json}")
    return 1 if any_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
