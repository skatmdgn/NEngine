#!/usr/bin/env python3
"""Compare Debug/Release CTest names/results from the same Git revision.

Both configurations must run the same test names and all must pass. The same
platform's configurations are compared; cross-OS results are separate.
"""
import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def read_results(path: Path):
    if not path.is_file() or path.stat().st_size>2*1024*1024:
        raise ValueError("missing or oversized CTest XML report")
    root=ET.parse(path).getroot()
    if root.tag not in ("testsuites","testsuite"):
        raise ValueError("expected CTest --output-junit testsuite(s) root")
    result={}
    for suite in ([root] if root.tag=="testsuite" else root.findall("testsuite")):
        for case in suite.findall("testcase"):
            name=case.get("name")
            if not name or name in result:
                raise ValueError(f"missing/duplicate CTest name: {name}")
            passed=not case.findall("failure") and not case.findall("error") and not case.findall("skipped")
            result[name]=bool(passed)
    if not result:raise ValueError("CTest report is empty")
    return result


def compare(debug, release):
    errors=[]
    if set(debug)!=set(release):
        errors.append("Debug/Release have different CTest suite names: "
                      f"Debug-only {sorted(set(debug)-set(release))}; "
                      f"Release-only {sorted(set(release)-set(debug))}")
    for name in sorted(set(debug)|set(release)):
        if not debug.get(name,False) or not release.get(name,False):
            errors.append(f"Debug/Release test did not pass: {name}")
    return errors


def main(argv=None):
    parser=argparse.ArgumentParser(description="NEngine Debug/Release test parity")
    parser.add_argument("--debug",type=Path,required=True)
    parser.add_argument("--release",type=Path,required=True)
    args=parser.parse_args(argv)
    try:
        errors=compare(read_results(args.debug),read_results(args.release))
        for error in errors:print(error,file=sys.stderr)
        if errors:return 1
        print("Debug/Release parity PASS: identical CTest groups all passed")
        return 0
    except (OSError,ValueError,ET.ParseError) as exc:
        print("CTest parity error: "+str(exc),file=sys.stderr)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
