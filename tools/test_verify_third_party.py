"""Validate notice/license checks against deliberately malformed fixtures."""
import tempfile
from pathlib import Path
import unittest

import verify_third_party as third_party


NOTICE = """# Third-Party Notices
## stb

- Project: stb
- Upstream: nothings/stb
- Pinned commit: `2c980bb59875b0d32144a71867fbdebb2f77cd20`
- Component: `stb_image.h`
- License used by NEngine: MIT
- Vendored license text: `third_party/stb/LICENSE`
"""


class ThirdPartyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "third_party/stb").mkdir(parents=True)
        (self.root / "THIRD_PARTY_NOTICES.md").write_text(NOTICE)
        (self.root / "third_party/stb/LICENSE").write_text("MIT license text " * 30)
        (self.root / "third_party/stb/stb_image.h").write_text("// dependency")

    def test_good_dependency(self):
        self.assertEqual([], third_party.validate(self.root))

    def test_missing_license(self):
        (self.root / "third_party/stb/LICENSE").unlink()
        self.assertTrue(any("missing vendored license" in e
                            for e in third_party.validate(self.root)))

    def test_unknown_vendor(self):
        (self.root / "third_party/newvendor").mkdir()
        self.assertTrue(any("unlisted third-party dependency" in e
                            for e in third_party.validate(self.root)))

    def test_missing_pin(self):
        (self.root / "THIRD_PARTY_NOTICES.md").write_text(
            NOTICE.replace("2c980bb59875b0d32144a71867fbdebb2f77cd20", "123"))
        self.assertTrue(any("upstream commit" in e
                            for e in third_party.validate(self.root)))


if __name__ == "__main__":
    unittest.main()
