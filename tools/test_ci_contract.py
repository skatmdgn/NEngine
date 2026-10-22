"""CI baseline regression gate tests, stdlib only."""
import unittest

import verify_test_matrix as matrix


class TestCtestMatrix(unittest.TestCase):
    def test_historical_baseline_accepted(self):
        payload = {"tests": [{"name": n} for n in sorted(matrix.BASELINE)]}
        self.assertEqual([], matrix.validate_ctest_tests(payload))

    def test_missing_historical_suite_rejected(self):
        payload = {"tests": [{"name": n} for n in matrix.BASELINE
                             if n != "NEngineCoreTests"]}
        self.assertTrue(any("NEngineCoreTests" in x
                            for x in matrix.validate_ctest_tests(payload)))

    def test_extra_new_suite_is_accepted(self):
        payload = {"tests": [{"name": n} for n in matrix.BASELINE] +
                   [{"name": "NewFeatureTests"}]}
        self.assertEqual([], matrix.validate_ctest_tests(payload))

    def test_duplicate_and_malformed_suite_rejected(self):
        payload = {"tests": [{"name": n} for n in matrix.BASELINE] +
                   [{"name": "NEngineCoreTests"}]}
        self.assertTrue(any("duplicate" in x
                            for x in matrix.validate_ctest_tests(payload)))
        self.assertTrue(matrix.validate_ctest_tests({"other": []}))


if __name__ == "__main__":
    unittest.main()
