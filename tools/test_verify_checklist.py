"""Regression tests for checklist bookkeeping invariants (stdlib only)."""
import unittest

import verify_checklist as check


def sample(done: bool = False, evidence: str = " — 증거: PR #30, 테스트 PASS, CI") -> str:
    status = "x" if done else " "
    return ("# Sample\n\n총 1개, 확인 " + ("1" if done else "0") +
            "개, 미완료 " + ("0" if done else "1") +
            "개\n\n## DEMO — Sample (" + ("1" if done else "0") +
            "/1)\n\n- [" + status + "] DEMO-001 Demo acceptance" +
            (evidence if done else " — 수용: TEST") + "\n")


class ChecklistTests(unittest.TestCase):
    def test_unchecked_valid(self):
        self.assertEqual([], check.validate(sample()))

    def test_checked_valid(self):
        self.assertEqual([], check.validate(sample(True)))

    def test_checked_requires_evidence(self):
        self.assertTrue(any("no evidence" in x for x in
                            check.validate(sample(True, evidence=""))))

    def test_duplicate_ids_rejected(self):
        text = sample() + "- [ ] DEMO-001 Duplicate item — 수용: TEST\n"
        self.assertTrue(any("duplicate ID" in x for x in check.validate(text)))

    def test_declared_summary_must_match(self):
        text = sample().replace("총 1개", "총 2개")
        self.assertTrue(any("summary counts" in x for x in check.validate(text)))

    def test_section_counts_must_match(self):
        text = sample().replace("(0/1)", "(1/1)")
        self.assertTrue(any("section DEMO counts" in x for x in check.validate(text)))

    def test_old_ids_cannot_be_deleted(self):
        old = sample()
        new = sample().replace("DEMO-001", "DEMO-002")
        self.assertTrue(any("removed stable" in x for x in
                            check.validate(new, old)))

    def test_new_completion_requires_pr_evidence(self):
        old = sample()
        new = sample(True, evidence=" — 증거: local only")
        self.assertTrue(any("requires PR" in x for x in
                            check.validate(new, old)))

    def test_new_completion_requires_pr_description_id(self):
        old = sample()
        new = sample(True)
        self.assertTrue(any("missing from PR" in x for x in
                            check.validate(new, old, "No linked ID", "Details only")))
        self.assertEqual([], check.validate(new, old, "DEMO-001 acceptance", ""))

    def test_malformed_checkbox_rejected(self):
        text = sample() + "- [y] DEMO-002 invalid\n"
        self.assertTrue(any("malformed" in x for x in check.validate(text)))


if __name__ == "__main__":
    unittest.main()
