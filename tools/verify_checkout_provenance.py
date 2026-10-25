#!/usr/bin/env python3
"""Bind CI test/checklist evidence to the exact checked-out commit.

For pull_request, Actions checks out a synthetic merge commit. Verify it
contains both the PR head and base, and that its checklist is exactly the
PR head's checklist. This prevents a mismatch between tested source and
newly checked acceptance IDs. For push, HEAD must equal workflow GITHUB_SHA.
"""
import json
import os
from pathlib import Path
import subprocess
import sys


def check_context(event_name, action_sha, checkout_sha, event):
    problems = []
    if not action_sha or action_sha != checkout_sha:
        problems.append("workflow GITHUB_SHA and checkout HEAD differ")
    if event_name == "pull_request":
        pr = event.get("pull_request") if isinstance(event, dict) else None
        head = pr.get("head", {}) if isinstance(pr, dict) else {}
        base = pr.get("base", {}) if isinstance(pr, dict) else {}
        for label, obj in (("head", head), ("base", base)):
            sha = obj.get("sha") if isinstance(obj, dict) else None
            if not isinstance(sha, str) or len(sha) != 40 or not all(
                ch in "0123456789abcdef" for ch in sha.lower()
            ):
                problems.append(f"PR {label} must carry full Git SHA")
    elif event_name != "push":
        problems.append(f"unexpected CI event: {event_name}")
    return problems


def git(*args):
    result = subprocess.run(["git", *args], capture_output=True, check=False)
    return result.returncode, result.stdout.strip()


def main():
    sha = os.environ.get("GITHUB_SHA", "")
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")
    path = os.environ.get("GITHUB_EVENT_PATH", "")
    if not path or not Path(path).is_file():
        print("Missing GitHub Actions event payload", file=sys.stderr)
        return 1
    try:
        event = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print(f"Malformed CI event: {exc}", file=sys.stderr)
        return 1
    rc, checkout = git("rev-parse", "HEAD")
    if rc:
        print("Cannot determine checkout SHA", file=sys.stderr)
        return 1
    problems = check_context(event_name, sha, checkout.decode(), event)
    if not problems and event_name == "pull_request":
        pr = event["pull_request"]
        for label in ("head", "base"):
            commit = pr[label]["sha"]
            if git("merge-base", "--is-ancestor", commit, checkout.decode())[0]:
                problems.append(f"checkout does not contain PR {label} SHA")
        rc, head_doc = git("show", f"{pr['head']['sha']}:docs/CHECKLIST.md")
        if rc or head_doc != Path("docs/CHECKLIST.md").read_bytes().strip():
            problems.append("verified checklist is not from PR exact head SHA")
    for problem in problems:
        print("PROVENANCE ERROR: " + problem, file=sys.stderr)
    if not problems:
        print(f"Exact checkout evidence PASS: {event_name} SHA={checkout.decode()}")
    return int(bool(problems))


if __name__ == "__main__":
    raise SystemExit(main())
