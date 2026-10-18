"""Check reproducible history-font coverage independently of the firmware build."""
from pathlib import Path
import gzip
import hashlib
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "assets/fonts/stelle-chat"

class ChatFontTests(unittest.TestCase):
    def test_source_identity_and_complete_basic_han_coverage(self):
        report = json.loads((BASE / "coverage.json").read_text(encoding="utf-8"))
        source = (BASE / "unifont-16.0.04.hex.gz").read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(), report["source_sha256"])
        self.assertEqual(report["missing"], [])
        glyphs = {int(line.split(":", 1)[0], 16) for line in gzip.decompress(source).decode("ascii").splitlines()}
        requested = {c for a, b in report["ranges"] for c in range(a, b+1)}
        self.assertTrue(set(range(0x4e00, 0xa000)) <= requested <= glyphs)
        self.assertEqual(len(requested), report["glyph_count"])
        self.assertIn("stelle_chat_font", (BASE / "stelle_chat_font.c").read_text(encoding="utf-8"))

if __name__ == "__main__":
    unittest.main()
