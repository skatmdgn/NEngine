#!/usr/bin/env python3
"""Check that vendored dependency directories, pinned SHA, notices and licenses agree."""
import argparse
from pathlib import Path
import re
import sys

PIN = re.compile(r"Pinned commit:\s*`([0-9a-f]{40})`")
REQUIRED = ("Project:", "Upstream:", "Component:", "License used by NEngine:",
            "Vendored license text:")


def validate(repo):
    root = Path(repo)
    notice = root / "THIRD_PARTY_NOTICES.md"
    if not notice.is_file():
        return ["missing THIRD_PARTY_NOTICES.md"]
    text = notice.read_text(encoding="utf-8")
    vendor = root / "third_party"
    directories = sorted(p for p in vendor.iterdir() if p.is_dir()) if vendor.is_dir() else []
    errors = []
    if not directories:
        errors.append("no vendored dependency directories")
    for dep in directories:
        # Some upstream component headings intentionally differ from the vendor folder.
        section_name = {"stb": "stb_image"}.get(dep.name, dep.name)
        section = re.search(
            r"(?ms)^## " + re.escape(section_name) + r"\s*\n(.*?)(?=^## |\Z)", text)
        if not section:
            errors.append(f"unlisted third-party dependency: {dep.name}")
            continue
        info = section.group(1)
        for required in REQUIRED:
            if required not in info:
                errors.append(f"{dep.name}: missing {required}")
        if not PIN.search(info):
            errors.append(f"{dep.name}: upstream commit must be 40 hex chars")
        licenses = [p for p in dep.iterdir()
                    if p.is_file() and p.name.upper().startswith("LICENSE")]
        if not licenses:
            errors.append(f"{dep.name}: missing vendored license")
        for license_file in licenses:
            if len(license_file.read_bytes()) < 100:
                errors.append(f"{dep.name}: truncated or empty license")
        if "Vendored license text: `third_party/" + dep.name + "/LICENSE`" not in info:
            errors.append(f"{dep.name}: notice license path does not match")
        if not any(p.is_file() and p.name != "LICENSE" for p in dep.iterdir()):
            errors.append(f"{dep.name}: dependency source missing")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    args = parser.parse_args()
    errors = validate(args.repo)
    for error in errors:
        print("THIRD PARTY ERROR: " + error, file=sys.stderr)
    if not errors:
        print("Vendored dependencies, pinned commits, notices and licenses: PASS")
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
