import unittest
import verify_checkout_provenance as provenance


def pr(sha="a"*40, base="b"*40):
    return {"pull_request":{"head":{"sha":sha},"base":{"sha":base}}}


class ProvenanceTests(unittest.TestCase):
    def test_matching_push(self):
        self.assertEqual([],provenance.check_context("push","a"*40,"a"*40,{}))
    def test_mismatched_checkout(self):
        self.assertTrue(provenance.check_context("push","a"*40,"b"*40,{}))
    def test_missing_pr_base(self):
        errors=provenance.check_context("pull_request","a"*40,"a"*40,
                                       {"pull_request":{"head":{"sha":"a"*40}}})
        self.assertTrue(any("base" in e for e in errors))
    def test_pr_head_valid(self):
        self.assertEqual([],provenance.check_context("pull_request","a"*40,
                                                     "a"*40,pr()))
    def test_invalid_shas(self):
        self.assertTrue(provenance.check_context("pull_request","a"*40,
                                                  "a"*40,pr("a"*39)))
    def test_unexpected_event(self):
        self.assertTrue(provenance.check_context("schedule","a"*40,"a"*40,{}))


if __name__=="__main__":
    unittest.main()
