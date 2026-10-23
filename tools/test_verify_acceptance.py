"""Windows/Android physical-device report schema and tamper regression tests."""
import hashlib
from pathlib import Path
import tempfile
import unittest

import verify_acceptance as acceptance


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        path = self.root / "evidence" / "artifacts" / "test.log"
        path.parent.mkdir(parents=True)
        path.write_text("Observed physical test transcript")
        self.report = {
            "schema_version": 1,
            "platform": "windows",
            "commit_sha": "a" * 40,
            "test_id": "EDIT-004",
            "result": "PASS",
            "tester": "Manual QA",
            "timestamp_utc": "2026-10-22T07:30:00Z",
            "hardware": {"os": "Windows 11", "gpu": "Test GPU",
                         "driver": "123.1", "audio_device": "Test audio",
                         "input_device": "Test keyboard"},
            "steps": [{"action": "Launch", "expected": "Opens",
                       "observed": "Opens", "result": "PASS"}],
            "artifacts": [{"path": "evidence/artifacts/test.log",
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}],
        }

    def validate(self, platform="windows"):
        return acceptance.validate_report(self.report, platform, self.root)

    def test_good_windows(self):
        self.assertEqual([], self.validate())

    def test_good_android(self):
        self.report["platform"] = "android"
        self.report["hardware"] = {"os": "Android 15", "manufacturer": "Example",
                                   "model": "ExamplePhone", "abi": "arm64-v8a",
                                   "api_level": "35", "gpu": "Example GPU"}
        self.assertEqual([], self.validate("android"))

    def test_wrong_platform(self):
        self.assertTrue(self.validate("android"))

    def test_bad_sha_and_timestamp(self):
        self.report["commit_sha"] = "no"
        self.report["timestamp_utc"] = "tomorrow"
        self.assertTrue(any("commit_sha" in e for e in self.validate()))
        self.assertTrue(any("timestamp_utc" in e for e in self.validate()))

    def test_missing_driver(self):
        self.report["hardware"].pop("driver")
        self.assertTrue(any("hardware.driver" in e for e in self.validate()))

    def test_pass_cannot_contain_fail(self):
        self.report["steps"][0]["result"] = "FAIL"
        self.assertTrue(any("PASS report cannot" in e for e in self.validate()))

    def test_missing_or_tampered_artifact(self):
        self.report["artifacts"][0]["sha256"] = "b" * 64
        self.assertTrue(any("mismatch" in e for e in self.validate()))
        self.report["artifacts"][0]["path"] = "evidence/../elsewhere"
        self.assertTrue(any("invalid relative path" in e for e in self.validate()))

    def test_no_reports_are_not_a_claim_of_device_success(self):
        errors, count = acceptance.validate_tree(self.root)
        self.assertEqual(([], 0), (errors, count))


if __name__ == "__main__":
    unittest.main()
