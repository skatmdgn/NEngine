#!/usr/bin/env python3
"""Validate the canonical NEngine checklist and prevent untracked completion.

Runs with Python stdlib on Windows and Ubuntu. This is a bookkeeping gate,
not a substitute for engine CTest or physical Windows/Android acceptance.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

TASK = re.compile(r"^- \[([ x])\] ([A-Z][A-Z0-9]*-\d{3}) (.+)$")
HEADING = re.compile(r"^## ([A-Z][A-Z0-9]*) — .+ \((\d+)/(\d+)\)$")
TOTAL = re.compile(r"총 (\d+)개")
DONE = re.compile(r"확인 (\d+)개")
OPEN = re.compile(r"미완료 (\d+)개")
PR = re.compile(r"\bPR\s*#\d+\b")


def parse_document(text: str) -> tuple[dict[str, tuple[bool, str]], dict[str, tuple[int, int]], list[str]]:
    tasks: dict[str, tuple[bool, str]] = {}
    sections: dict[str, tuple[int, int]] = {}
    errors: list[str] = []
    active = None
    for lineno, line in enumerate(text.splitlines(), 1):
        if line.startswith("## "):
            m = HEADING.fullmatch(line)
            active = m.group(1) if m else None
            if m:
                if active in sections:
                    errors.append(f"line {lineno}: duplicate section {active}")
                sections[active] = (int(m.group(2)), int(m.group(3)))
        if line.startswith("- ["):
            m = TASK.fullmatch(line)
            if not m:
                errors.append(f"line {lineno}: malformed checklist item")
                continue
            status, key, detail = m.groups()
            prefix = key.rsplit("-", 1)[0]
            if active != prefix:
                errors.append(f"line {lineno}: {key} is not in section {prefix}")
            if key in tasks:
                errors.append(f"line {lineno}: duplicate ID {key}")
            tasks[key] = (status == "x", detail)
            if status == "x" and "증거:" not in detail:
                errors.append(f"line {lineno}: checked {key} has no evidence")
            if not detail.strip():
                errors.append(f"line {lineno}: {key} has no acceptance criterion")
    if not tasks:
        errors.append("no checklist items found")
    for prefix, (expected_done, expected_total) in sections.items():
        subset = [v for key, v in tasks.items() if key.startswith(prefix + "-")]
        if (sum(v[0] for v in subset), len(subset)) != (expected_done, expected_total):
            errors.append(f"section {prefix} counts out of sync")
    keys_by_section: dict[str, list[int]] = {}
    for key in tasks:
        prefix, num = key.rsplit("-", 1)
        keys_by_section.setdefault(prefix, []).append(int(num))
    for prefix, numbers in keys_by_section.items():
        if sorted(numbers) != list(range(1, len(numbers) + 1)):
            errors.append(f"section {prefix} must have consecutive unique ID numbers")
    summary = (TOTAL.search(text), DONE.search(text), OPEN.search(text))
    if not all(summary):
        errors.append("top-level checklist summary is missing")
    else:
        declared = [int(x.group(1)) for x in summary]
        actual = [len(tasks), sum(v[0] for v in tasks.values()),
                  sum(not v[0] for v in tasks.values())]
        if declared != actual:
            errors.append(f"summary counts do not match: {declared} != {actual}")
    return tasks, sections, errors


def check_baseline(new: dict[str, tuple[bool, str]],
                   old: dict[str, tuple[bool, str]],
                   pr_title: str = "", pr_body: str = "") -> list[str]:
    errors: list[str] = []
    for key in old:
        if key not in new:
            errors.append(f"removed stable checklist ID {key}")
    completed = [key for key, (done, _) in new.items()
                 if done and (key not in old or not old[key][0])]
    for key in completed:
        detail = new[key][1]
        if not PR.search(detail) or not ("CI" in detail or "테스트" in detail or "검증" in detail):
            errors.append(f"newly completed {key} requires PR and test/CI evidence")
        if pr_title or pr_body:
            if key not in pr_title and key not in pr_body:
                errors.append(f"newly completed {key} is missing from PR title/body")
    return errors


def git_baseline(ref: str) -> str | None:
    result = subprocess.run(["git", "show", f"{ref}:docs/CHECKLIST.md"],
                            capture_output=True, text=True, encoding="utf-8",
                            errors="replace", check=False)
    if result.returncode != 0:
        # The very first checklist PR legitimately has no baseline document.
        ls = subprocess.run(["git", "cat-file", "-e", f"{ref}^{{commit}}"],
                            capture_output=True, check=False)
        if ls.returncode != 0:
            raise RuntimeError(f"baseline ref {ref} does not exist")
        return None
    return result.stdout


def validate(text: str, baseline: str | None = None, title: str = "",
             body: str = "") -> list[str]:
    current, _, errors = parse_document(text)
    if baseline is not None:
        older, _, old_errors = parse_document(baseline)
        if old_errors:
            errors.append("baseline malformed: " + "; ".join(old_errors))
        errors.extend(check_baseline(current, older, title, body))
    return errors



def parse_change_journal(raw: str) -> tuple[list[dict], list[str]]:
    """Parse append-only provenance records for stable task IDs."""
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as exc:
        return [], [f"invalid change journal JSON: {exc}"]
    if (not isinstance(value, dict) or value.get("version") != 1 or
            not isinstance(value.get("records"), list)):
        return [], ["change journal must contain version=1 and records list"]
    errors = []
    records = value["records"]
    from datetime import date
    for i, entry in enumerate(records):
        if not isinstance(entry, dict):
            errors.append(f"journal entry {i} is not an object")
            continue
        if entry.get("kind") not in ("add", "revise", "split", "merge"):
            errors.append(f"journal entry {i} has invalid kind")
        key = entry.get("id")
        if not isinstance(key, str) or not TASK.fullmatch("- [ ] " + key + " entry"):
            errors.append(f"journal entry {i} has invalid stable ID")
        reason = entry.get("reason")
        if not isinstance(reason, str) or len(reason.strip()) < 15:
            errors.append(f"journal entry {i} needs meaningful reason")
        try:
            date.fromisoformat(entry["date"])
        except (ValueError, TypeError, KeyError):
            errors.append(f"journal entry {i} needs ISO date")
        sources = entry.get("sources")
        if not isinstance(sources, list) or len(sources) != len(set(map(str, sources))):
            errors.append(f"journal entry {i} has invalid sources")
    return records, errors


def check_change_journal(current: dict, old: dict, records: list[dict],
                         previous: list[dict]) -> list[str]:
    """New IDs and revised criteria must have append-only auditable history."""
    errors = []
    if records[:len(previous)] != previous:
        errors.append("prior checklist change records were edited or deleted")
    additions = records[len(previous):]
    by_id: dict[str, list[dict]] = {}
    for entry in additions:
        if isinstance(entry, dict):
            by_id.setdefault(entry.get("id", ""), []).append(entry)
    altered = set()
    for key, (done, detail) in current.items():
        if key not in old:
            altered.add(key)
            matches = [r for r in by_id.get(key, [])
                       if r.get("kind") in ("add", "split", "merge")]
            if len(matches) != 1:
                errors.append(f"new checklist ID {key} requires add/split/merge provenance")
        elif detail.split(" — ", 1)[0] != old[key][1].split(" — ", 1)[0]:
            altered.add(key)
            matches = [r for r in by_id.get(key, []) if r.get("kind") == "revise"]
            if len(matches) != 1:
                errors.append(f"changed criteria {key} require revise provenance")
    for entry in additions:
        if not isinstance(entry, dict):
            continue
        key, kind, sources = entry.get("id"), entry.get("kind"), entry.get("sources")
        if key not in altered:
            errors.append(f"journal record for unchanged ID {key}")
        if kind in ("split", "merge"):
            minimum = 1 if kind == "split" else 2
            if not isinstance(sources, list) or len(sources) < minimum:
                errors.append(f"{key}: {kind} needs {minimum} source ID(s)")
                continue
            for source in sources:
                if source not in old or source not in current or source == key:
                    errors.append(f"{key}: invalid {kind} source {source}")
        elif isinstance(sources, list) and sources:
            errors.append(f"{key}: {kind} cannot claim source IDs")
    return errors


def git_optional_file(ref: str, path: str) -> str | None:
    command = subprocess.run(["git", "show", f"{ref}:{path}"],
                             capture_output=True, text=True, encoding="utf-8",
                             errors="replace", check=False)
    return command.stdout if command.returncode == 0 else None

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checklist", default="docs/CHECKLIST.md")
    parser.add_argument("--baseline-ref", default=None)
    args = parser.parse_args()
    text = Path(args.checklist).read_text(encoding="utf-8")
    ref = args.baseline_ref
    if ref is None and os.environ.get("GITHUB_EVENT_NAME") == "pull_request":
        ref = "origin/" + os.environ.get("GITHUB_BASE_REF", "main")
    baseline = git_baseline(ref) if ref else None
    title = body = ""
    if os.environ.get("GITHUB_EVENT_NAME") == "pull_request":
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
        title = event.get("pull_request", {}).get("title", "")
        body = event.get("pull_request", {}).get("body", "") or ""
    errors = validate(text, baseline, title, body)
    journal_file = Path("tools/checklist_changes.json")
    if journal_file.exists():
        records, journal_errors = parse_change_journal(journal_file.read_text(encoding="utf-8"))
        errors.extend(journal_errors)
        if baseline is not None:
            previous_raw = git_optional_file(ref, "tools/checklist_changes.json")
            previous, previous_errors = parse_change_journal(
                previous_raw if previous_raw is not None else '{"version":1,"records":[]}')
            errors.extend(previous_errors)
            newer, _, _ = parse_document(text)
            older, _, _ = parse_document(baseline)
            errors.extend(check_change_journal(newer, older, records, previous))
    else:
        errors.append("missing tools/checklist_changes.json")
    if errors:
        for error in errors:
            print("CHECKLIST ERROR:", error, file=sys.stderr)
        return 1
    tasks, _, _ = parse_document(text)
    n = sum(value[0] for value in tasks.values())
    print(f"Checklist OK: {len(tasks)} unique items, {n} checked, "
          f"{len(tasks) - n} remaining; baseline={ref or 'none'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
