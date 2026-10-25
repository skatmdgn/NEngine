import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock
import json

from verify_release_candidate import (
    EvidenceError, make_evidence, publish_manifest, digest
)

SHA="c"*40


class CandidateEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)
        (self.root/"build").mkdir()
        self.exe=self.root/"build"/"NEnginePlayer.exe"
        self.exe.write_bytes(b"binary")
        self.report=self.root/"ctest.xml"
        self.report.write_text(
            '<testsuite tests="2"><testcase name="Core"/><testcase name="Player"/></testsuite>'
        )

    def evidence(self,sha=SHA):
        with patch("verify_release_candidate.subprocess.run",
                   return_value=Mock(returncode=0,stdout=SHA+"\n")):
            return make_evidence(self.root,sha,[self.exe],self.report,"windows-x64")

    def test_matching_SHA_executable_and_test_log(self):
        ev=self.evidence()
        self.assertEqual(ev["git_sha"],SHA)
        self.assertEqual(ev["test_report"]["passed"],2)
        self.assertEqual(ev["test_report"]["sha256"],hashlib.sha256(self.report.read_bytes()).hexdigest())
        self.assertEqual(ev["executables"][0]["sha256"],hashlib.sha256(b"binary").hexdigest())
        dest=self.root/"release-candidate.json"
        publish_manifest(dest,ev)
        self.assertEqual(json.loads(dest.read_text())["git_sha"],SHA)
        self.assertFalse(list(self.root.glob(".nengine-evidence-*")))

    def test_mismatched_commit_refused(self):
        with self.assertRaisesRegex(EvidenceError,"differs"):
            self.evidence("d"*40)

    def test_test_failure_refuses_release_manifest(self):
        self.report.write_text(
            '<testsuite><testcase name="Broken"><failure>failed</failure></testcase></testsuite>')
        with self.assertRaisesRegex(EvidenceError,"not fully passing"):
            self.evidence()

    def test_symlinked_binary_and_duplicate_rejected(self):
        link=self.root/"build"/"link.exe"
        try:link.symlink_to(self.exe)
        except OSError:self.skipTest("symlinks unavailable")
        with patch("verify_release_candidate.subprocess.run",
                   return_value=Mock(returncode=0,stdout=SHA+"\n")):
            with self.assertRaises(EvidenceError):
                make_evidence(self.root,SHA,[link],self.report,"windows-x64")
            with self.assertRaisesRegex(EvidenceError,"duplicate"):
                make_evidence(self.root,SHA,[self.exe,self.exe],self.report,"windows-x64")

    def test_atomic_manifest_refuses_symlink(self):
        target=self.root/"original"
        target.write_text("KEEP")
        link=self.root/"evidence"
        try:link.symlink_to(target)
        except OSError:self.skipTest("symlinks unavailable")
        with self.assertRaises(EvidenceError):
            publish_manifest(link,self.evidence())
        self.assertEqual(target.read_text(),"KEEP")


if __name__=="__main__":unittest.main()
