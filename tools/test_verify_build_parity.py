import tempfile
import unittest
from pathlib import Path
from verify_build_parity import read_results, compare

def fixture(names):
    inner="".join(f'<testcase name="{name}"/>' for name in names)
    return '<testsuites><testsuite name="NEngine">'+inner+'</testsuite></testsuites>'


class BuildParityTests(unittest.TestCase):
    def test_identical_results(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"results.xml"
            p.write_text(fixture(["A","B"]))
            d=read_results(p)
            self.assertEqual({"A":True,"B":True},d)
            self.assertEqual([],compare(d,d))
    def test_missing_suite_detected(self):
        self.assertTrue(compare({"A":True,"B":True},{"A":True}))
    def test_any_failure_detected(self):
        self.assertTrue(compare({"A":True},{"A":False}))
    def test_duplicate_names_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"results.xml"
            p.write_text(fixture(["A","A"]))
            with self.assertRaises(ValueError):read_results(p)
    def test_nonempty_report_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"results.xml"
            p.write_text(fixture([]))
            with self.assertRaises(ValueError):read_results(p)


if __name__=="__main__":unittest.main()
