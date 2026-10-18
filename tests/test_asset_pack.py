"""Checks for the resource-partition asset pack (tools/make_asset_pack.py).

- the pack and its C index are current, and the pack fits the 2 MB partition
- header fields and CRC match what the firmware compiles in
- every cropped sprite, put back at its crop offset, equals its original
  frame pixel-for-pixel (the crop only removes fully transparent pixels)
- every room equals the room painted from the source sheet
"""
import json
import re
import struct
import subprocess
import sys
import unittest
import zlib
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "build/assets/stelle_assets.bin"
INDEX_H = ROOT / "main/generated/pack/stelle_pack.h"
INDEX_C = ROOT / "main/generated/pack/stelle_pack.c"
DEVICE = ROOT / "assets/images/stelle-companion/device"
sys.path.insert(0, str(ROOT / "tools"))
from ui_assets import rooms  # noqa: E402


def define(name):
    return int(re.search(rf"#define {name} (0x[0-9A-F]+|\d+)U?", INDEX_H.read_text()).group(1), 0)


def images():
    rows = re.findall(r"\{(\d+)U, (\d+)U, (\d+), (\d+), (-?\d+), (-?\d+), ([01])\}", INDEX_C.read_text())
    return [tuple(int(v) for v in r) for r in rows]


def decode(blob, off, w, h, sprite):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for i in range(w * h):
        v = blob[off + 2 * i] | blob[off + 2 * i + 1] << 8
        r, g, b = (v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3
        a = blob[off + 2 * w * h + i] if sprite else 255
        px[i % w, i // w] = (r, g, b, a)
    return img


def quantize(img):
    """Original RGBA as the pack stores it: RGB565, colour 0 where transparent."""
    out = Image.new("RGBA", img.size)
    src, dst = img.load(), out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = src[x, y]
            dst[x, y] = ((r >> 3) << 3, (g >> 2) << 2, (b >> 3) << 3, a) if a else (0, 0, 0, 0)
    return out


class AssetPackTest(unittest.TestCase):
    def test_pack_is_current(self):
        result = subprocess.run([sys.executable, str(ROOT / "tools/make_asset_pack.py"), "--check"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_header_and_crc(self):
        blob = PACK.read_bytes()
        magic, ver, total, count, crc = struct.unpack_from("<4sIIII", blob, 0)
        self.assertEqual(magic, b"STLP")
        self.assertEqual(ver, define("STELLE_PACK_VERSION"))
        self.assertEqual(total, len(blob))
        self.assertEqual(total, define("STELLE_PACK_SIZE"))
        self.assertLessEqual(total, 0x200000, "pack must fit the 2 MB resource partition")
        self.assertEqual(count, define("STELLE_PACK_IMAGES"))
        self.assertEqual(crc, define("STELLE_PACK_CRC"))
        self.assertEqual(zlib.crc32(blob[32:]) & 0xFFFFFFFF, crc)

    def test_sprites_restore_exactly(self):
        blob = PACK.read_bytes()
        table = images()
        manifest = json.loads((DEVICE / "sprites.json").read_text())
        cw, ch = manifest["cell"]
        frames = [rel for s in manifest["states"] for rel in s["frames"]]
        sprites = [t for t in table if t[6] == 1]
        self.assertEqual(len(sprites), len(frames))
        for (off, size, w, h, ox, oy, _), rel in zip(sprites, frames):
            self.assertEqual(size, w * h * 3)
            self.assertEqual(w % 4, 0)
            self.assertTrue(0 <= ox and ox + w <= cw and 0 <= oy and oy + h <= ch, rel)
            full = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
            full.paste(decode(blob, off, w, h, True), (ox, oy))
            want = quantize(Image.open(DEVICE / rel).convert("RGBA"))
            self.assertEqual(full.tobytes(), want.tobytes(), rel)

    def test_duplicate_sleep_frames_share_storage_without_dropping_frames(self):
        manifest = json.loads((DEVICE / "sprites.json").read_text())
        start = 0
        for state in manifest["states"]:
            if state["name"] == "sleep": break
            start += len(state["frames"])
        rows = images()[start:start+8]
        self.assertEqual(len(rows), 8)
        self.assertEqual(rows[1][0], rows[3][0])
        self.assertEqual(rows[1][0], rows[7][0])
        self.assertEqual(rows[2][0], rows[4][0])
        self.assertEqual(rows[2][0], rows[6][0])

    def test_rooms_match_the_sheet(self):
        blob = PACK.read_bytes()
        table = images()
        room_rows = [t for t in table if t[6] == 0][: len(rooms.SCENES) * len(rooms.PHASES)]
        self.assertEqual(len([t for t in table if t[6] == 0]), len(rooms.SCENES) * len(rooms.PHASES) + 2)  # anniversary + picker art
        i = 0
        for scene in rooms.SCENES:
            for phase in rooms.PHASES:
                off, size, w, h, *_ = room_rows[i]
                i += 1
                self.assertEqual((w, h, size), (rooms.W, rooms.H, rooms.W * rooms.H * 2))
                got = decode(blob, off, w, h, False).convert("RGB")
                want = quantize(rooms.paint(phase, scene).convert("RGBA")).convert("RGB")
                self.assertEqual(got.tobytes(), want.tobytes(), f"{scene}/{phase}")

    def test_trailblaze_preview_preserves_full_illustration(self):
        off, _, w, h, *_ = images()[define("STELLE_PACK_TRAILBLAZE_PREVIEW")]
        source = Image.open(ROOT / "assets/images/stelle-companion/source/trailblaze-preview.png").convert("RGB")
        fitted = ImageOps.pad(source, (216, 146), method=Image.Resampling.LANCZOS, color=(0, 0, 0))
        actual = decode(PACK.read_bytes(), off, w, h, False)
        self.assertEqual(actual.tobytes(), quantize(fitted.convert("RGBA")).tobytes())


if __name__ == "__main__":
    unittest.main()
