"""QA-013: fail CI when the advertised C# SDK example drifts from its release."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


class SdkDocsContractTests(unittest.TestCase):
    def test_version_and_links_follow_source(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        version = re.search(r"project\(NEngine VERSION (\d+\.\d+\.\d+)", cmake)
        self.assertIsNotNone(version)
        root = (ROOT / "README.md").read_text(encoding="utf-8")
        guide = (ROOT / "samples/managed/README.md").read_text(encoding="utf-8")
        self.assertIn(version.group(1) + "-dev", root)
        self.assertIn(version.group(1) + "-dev", guide)
        self.assertIn("samples/managed/README.md", root)
        self.assertIn("NavQuickstart.cs", guide)
        self.assertIn("not", guide.lower())

    def test_managed_example_matches_generated_csharp_api(self):
        source = (ROOT / "samples/managed/NavQuickstart.cs").read_text(encoding="utf-8")
        generated = (ROOT / "engine/scripting/src/managed_project.cpp").read_text(encoding="utf-8")
        for method in ("class NavMeshAgent", "SetDestination(",
                       "class WaitForSeconds", "StartCoroutine(",
                       "StopCoroutine(", "class Behaviour"):
            with self.subTest(method=method):
                self.assertIn(method, generated)
        for method in ("GetComponent<NavMeshAgent>()", "StartCoroutine(",
                       "StopCoroutine(", "new WaitForSeconds(", "SetDestination("):
            with self.subTest(method=method):
                self.assertIn(method, source)
        tests = (ROOT / "tests/scripting_tests.cpp").read_text(encoding="utf-8")
        self.assertIn("NavQuickstart.cs", tests)
        self.assertIn("managed build integration project generation succeeds", tests)

    def test_docs_do_not_claim_android_or_hardware_acceptance(self):
        guide = (ROOT / "samples/managed/README.md").read_text(encoding="utf-8")
        self.assertIn("**does not**", guide)
        self.assertIn("APK", guide)
        self.assertIn("not a production-ready 1.0", guide)


if __name__ == "__main__":
    unittest.main()
