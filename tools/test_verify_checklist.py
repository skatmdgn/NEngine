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



    def test_gov005_added_id_requires_explanation(self):
        old_text = sample()
        updated = sample().replace("총 1개", "총 2개").replace(
            "미완료 1개", "미완료 2개").replace("(0/1)", "(0/2)")
        updated += "- [ ] DEMO-002 New unmet acceptance — 수용: TEST\\n"
        old, _, _ = check.parse_document(old_text)
        new, _, _ = check.parse_document(updated)
        problems = check.check_change_journal(new, old, [], [])
        self.assertTrue(any("DEMO-002" in x for x in problems))
        audit = [{"id": "DEMO-002", "kind": "add", "date": "2026-10-22",
                  "reason": "User discovered a previously missing acceptance goal",
                  "sources": []}]
        self.assertEqual([], check.check_change_journal(new, old, audit, []))

    def test_gov006_split_requires_parent_and_keeps_prior_id(self):
        old_text = sample()
        updated = sample().replace("총 1개", "총 2개").replace(
            "미완료 1개", "미완료 2개").replace("(0/1)", "(0/2)")
        updated += "- [ ] DEMO-002 Smaller goal — 수용: TEST\\n"
        old, _, _ = check.parse_document(old_text)
        new, _, _ = check.parse_document(updated)
        log = [{"id": "DEMO-002", "kind": "split", "date": "2026-10-22",
                "reason": "Large acceptance split into independently measurable work",
                "sources": ["DEMO-001"]}]
        self.assertEqual([], check.check_change_journal(new, old, log, []))
        log[0]["sources"] = ["MISSING-001"]
        self.assertTrue(any("invalid split source" in e for e
                            in check.check_change_journal(new, old, log, [])))

    def test_gov006_merge_needs_two_sources(self):
        old = {"DEMO-001": (False, "A"), "DEMO-002": (False, "B")}
        new = dict(old, **{"DEMO-003": (False, "C")})
        records = [{"id": "DEMO-003", "kind": "merge", "date": "2026-10-22",
                    "reason": "Two tasks were found to have a shared acceptance",
                    "sources": ["DEMO-001"]}]
        self.assertTrue(any("needs 2 source" in e for e in
                            check.check_change_journal(new, old, records, [])))

    def test_gov006_acceptance_change_requires_revision_record(self):
        old = {"DEMO-001": (False, "Old behavior — 수용: TEST")}
        new = {"DEMO-001": (False, "New behavior — 수용: TEST")}
        self.assertTrue(check.check_change_journal(new, old, [], []))
        record = [{"id": "DEMO-001", "kind": "revise", "date": "2026-10-22",
                   "reason": "Clarified previously ambiguous acceptance criterion",
                   "sources": []}]
        self.assertEqual([], check.check_change_journal(new, old, record, []))

    def test_gov006_append_only_records(self):
        old = {"DEMO-001": (False, "Old")}
        previous = [{"id": "DEMO-001", "kind": "add", "date": "2026-10-20",
                     "reason": "Found a missing requirement", "sources": []}]
        self.assertTrue(any("prior checklist change records" in e for e
                            in check.check_change_journal(old, old, [], previous)))

    def test_gov005_audit_record_schema(self):
        valid = '{"version":1,"records":[{"id":"DEMO-002","kind":"add","date":"2026-10-22","reason":"A real new user requirement","sources":[]}]}'
        entries, errors = check.parse_change_journal(valid)
        self.assertEqual([], errors)
        self.assertEqual(1, len(entries))
        invalid = valid.replace("2026-10-22", "yesterday")
        self.assertTrue(check.parse_change_journal(invalid)[1])

if __name__ == "__main__":
    unittest.main()
