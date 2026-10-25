#!/usr/bin/env python3
"""Fail CI if any historically passing NEngine CTest suite is removed."""
import argparse
import json
import subprocess
import sys

BASELINE = frozenset({
    "NEngineCoreTests", "NEngineEditorTests", "NEngineAssetTests",
    "NEngineScriptingTests", "NEngineRenderTests", "NEngineInputTests",
    "NEnginePhysicsTests", "NEngineNavigationTests", "NEngineAudioTests",
    "NEngineUITests", "NEnginePlayerTests", "NEnginePlayerCLIIntegration",
})


def validate_ctest_tests(payload):
    if not isinstance(payload, dict) or not isinstance(payload.get("tests"), list):
        return ["ctest JSON has no tests array"]
    names = [test.get("name") for test in payload["tests"] if isinstance(test, dict)]
    problems = []
    if len(names) != len(set(names)):
        problems.append("duplicate CTest names")
    missing = sorted(BASELINE - set(names))
    if missing:
        problems.append("baseline test suites removed: " + ", ".join(missing))
    return problems


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    result = subprocess.run(
        ["ctest", "--test-dir", args.build_dir, "-C", args.config,
         "--show-only=json-v1"],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        check=False,
    )
    if result.returncode:
        print("CTest discovery failed: " + result.stderr, file=sys.stderr)
        return 1
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        print("Invalid CTest discovery JSON: " + str(exc), file=sys.stderr)
        return 1
    errors = validate_ctest_tests(payload)
    for error in errors:
        print("TEST MATRIX ERROR: " + error, file=sys.stderr)
    if errors:
        return 1
    print(f"CTest regression gate PASS: {len(payload['tests'])} suites, "
          f"{len(BASELINE)} baseline suites retained")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
