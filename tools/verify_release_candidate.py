#!/usr/bin/env python3
"""Produce verifiable release-candidate provenance for binaries and CI reports.

All artifacts are read from the exact checked-out CI revision. The manifest
contains full SHA256s, CTest suite count, test report SHA and runner identity.
It is not a code-signature or proof of physical GPU/device execution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

SHA = re.compile(r"[a-f0-9]{40}\Z")

class EvidenceError(ValueError):
    pass


def digest(path):
    if path.is_symlink() or not path.is_file():
        raise EvidenceError(f"unsafe/missing artifact: {path}")
    result=hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(65536):
            result.update(chunk)
    return result.hexdigest()


def make_evidence(workspace, expected_sha, binary_paths, test_report, platform):
    if not isinstance(expected_sha,str) or not SHA.fullmatch(expected_sha):
        raise EvidenceError("commit SHA is invalid")
    actual=subprocess.run(["git","rev-parse","HEAD"],cwd=workspace,
                          text=True,capture_output=True,check=False)
    if actual.returncode or actual.stdout.strip()!=expected_sha:
        raise EvidenceError("checked-out commit differs from CI expected SHA")
    if not platform or not re.fullmatch(r"[A-Za-z0-9_-]{2,50}",platform):
        raise EvidenceError("invalid platform/runner identity")
    if not binary_paths:
        raise EvidenceError("at least one executable is required")
    root=workspace.resolve()
    artifacts=[]
    names=set()
    for item in binary_paths:
        resolved=item.resolve(strict=True)
        if not resolved.is_relative_to(root):
            raise EvidenceError("artifact is outside workspace")
        relative=resolved.relative_to(root).as_posix()
        if relative in names:
            raise EvidenceError("duplicate binary entry")
        names.add(relative)
        artifacts.append({"path":relative,"sha256":digest(item),"bytes":item.stat().st_size})
    report=test_report.resolve(strict=True)
    if not report.is_relative_to(root):
        raise EvidenceError("CTest report outside workspace")
    data=ET.parse(report).getroot()
    suites=[data] if data.tag=="testsuite" else data.findall("testsuite") if data.tag=="testsuites" else []
    cases=[x for suite in suites for x in suite.findall("testcase")]
    if not cases or any(t.find("failure") is not None or t.find("error") is not None or t.find("skipped") is not None for t in cases):
        raise EvidenceError("CTest JUnit suite missing or not fully passing")
    return {
        "schema":1,"kind":"NEngineReleaseCandidate",
        "git_sha":expected_sha,"platform":platform,
        "test_report":{"path":report.relative_to(root).as_posix(),
                       "sha256":digest(test_report),"passed":len(cases)},
        "executables":sorted(artifacts,key=lambda x:x["path"])
    }


def publish_manifest(output, evidence):
    if output.is_symlink():
        raise EvidenceError("refusing symlinked provenance target")
    if not output.parent.is_dir():
        raise EvidenceError("provenance output parent is missing")
    encoded=(json.dumps(evidence,indent=2,sort_keys=True)+"\n").encode("utf-8")
    temporary=None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent,prefix=".nengine-evidence-",
                                         delete=False) as stream:
            temporary=Path(stream.name)
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary,output)
        temporary=None
    finally:
        if temporary is not None: temporary.unlink(missing_ok=True)


def main(argv=None):
    ap=argparse.ArgumentParser(description="NEngine release candidate SHA and binary evidence")
    ap.add_argument("--workspace",type=Path,default=Path("."))
    ap.add_argument("--sha",required=True)
    ap.add_argument("--platform",required=True)
    ap.add_argument("--binary",required=True,action="append",type=Path)
    ap.add_argument("--test-report",required=True,type=Path)
    ap.add_argument("--output",required=True,type=Path)
    args=ap.parse_args(argv)
    try:
        data=make_evidence(args.workspace,args.sha,args.binary,args.test_report,args.platform)
        publish_manifest(args.output,data)
        print(f"RELEASE CANDIDATE PROVENANCE PASS: {data['git_sha']} "
              f"executables={len(data['executables'])} CTest={data['test_report']['passed']}")
        return 0
    except (OSError,EvidenceError,ET.ParseError) as exc:
        print(f"Release candidate provenance failed: {exc}",file=sys.stderr)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
