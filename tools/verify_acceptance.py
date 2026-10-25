#!/usr/bin/env python3
"""Validate the provenance and structure of optional physical-device test reports.

Passing this validator is NOT proof a device test happened; zero reports are valid
and every product acceptance item still requires real tester-supplied evidence.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys

SHA = re.compile(r"^[0-9a-f]{40}$")
SAFE = re.compile(r"^[A-Za-z0-9._/-]+$")
TASK = re.compile(r"^[A-Z0-9]+-\d{3}$")


def validate_report(data, platform, root):
    errors = []
    if not isinstance(data, dict):
        return ["report must be JSON object"]
    if data.get("schema_version") != 1 or data.get("platform") != platform:
        errors.append("invalid schema_version or platform")
    if not isinstance(data.get("commit_sha"), str) or not SHA.fullmatch(data["commit_sha"]):
        errors.append("commit_sha must be a full Git SHA")
    if not isinstance(data.get("test_id"), str) or not TASK.fullmatch(data["test_id"]):
        errors.append("test_id must be a checklist ID")
    if data.get("result") not in ("PASS", "FAIL"):
        errors.append("result must be PASS or FAIL")
    if not isinstance(data.get("tester"), str) or len(data["tester"].strip()) < 2:
        errors.append("tester must identify a real tester")
    timestamp = data.get("timestamp_utc")
    try:
        dt = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
        if dt.utcoffset() != timezone.utc.utcoffset(dt):
            raise ValueError("not UTC")
    except (AttributeError, TypeError, ValueError):
        errors.append("timestamp_utc must be an ISO-8601 UTC timestamp")
    h = data.get("hardware")
    if not isinstance(h, dict):
        errors.append("hardware must be an object")
        h = {}
    needed = ("os", "gpu", "driver", "audio_device", "input_device") if platform == "windows" else (
        "os", "manufacturer", "model", "abi", "api_level", "gpu")
    for key in needed:
        value = h.get(key)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"hardware.{key} must be nonempty")
    if platform == "windows" and h.get("os") not in ("Windows 10", "Windows 11"):
        errors.append("hardware.os must be Windows 10 or Windows 11")
    if platform == "android" and isinstance(h.get("os"), str) and not h["os"].startswith("Android "):
        errors.append("hardware.os must start with Android ")
    steps = data.get("steps")
    if not isinstance(steps, list) or not steps:
        errors.append("steps must be a nonempty list")
    else:
        for i, step in enumerate(steps):
            if not isinstance(step, dict) or any(
                not isinstance(step.get(k), str) or not step[k].strip()
                for k in ("action", "expected", "observed")
            ) or step.get("result") not in ("PASS", "FAIL"):
                errors.append(f"steps[{i}] requires action/expected/observed/result")
    if data.get("result") == "PASS" and isinstance(steps, list) and any(
        isinstance(step, dict) and step.get("result") != "PASS" for step in steps):
        errors.append("PASS report cannot contain a failed step")
    artifacts = data.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        errors.append("artifacts must link at least one real file and sha256")
    else:
        for i, item in enumerate(artifacts):
            if not isinstance(item, dict):
                errors.append(f"artifacts[{i}] must be object")
                continue
            path = item.get("path")
            expected_hash = item.get("sha256")
            if (not isinstance(path, str) or not SAFE.fullmatch(path)
                or not path.startswith("evidence/") or ".." in Path(path).parts):
                errors.append(f"artifacts[{i}] has invalid relative path")
                continue
            if not isinstance(expected_hash, str) or not re.fullmatch(r"[a-f0-9]{64}", expected_hash):
                errors.append(f"artifacts[{i}] has invalid sha256")
                continue
            resolved = (root / path).resolve()
            try:
                resolved.relative_to(root.resolve())
            except ValueError:
                errors.append(f"artifacts[{i}] escapes repository")
                continue
            if not resolved.is_file():
                errors.append(f"artifacts[{i}] missing file")
            elif hashlib.sha256(resolved.read_bytes()).hexdigest() != expected_hash:
                errors.append(f"artifacts[{i}] sha256 mismatch")
    return errors


def validate_tree(root):
    errors, count = [], 0
    for platform in ("windows", "android"):
        folder = root / "evidence" / "hardware" / platform
        if not folder.exists():
            continue
        for path in sorted(folder.glob("*.json")):
            count += 1
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (ValueError, UnicodeError) as exc:
                errors.append(f"{path}: bad JSON: {exc}")
                continue
            errors.extend(f"{path}: {e}" for e in validate_report(data, platform, root))
    return errors, count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    args = parser.parse_args()
    errors, count = validate_tree(Path(args.repo))
    for error in errors:
        print("DEVICE ACCEPTANCE ERROR: " + error, file=sys.stderr)
    if not errors:
        print(f"Device evidence format PASS: {count} report(s); "
              "0 reports does not establish hardware acceptance")
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
