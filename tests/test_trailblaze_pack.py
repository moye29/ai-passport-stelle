"""Destination pack contract, using temporary synthetic images, never shipped art."""
import struct
import json
import sys
import tempfile
import unittest
import zlib
import hashlib
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from make_trailblaze_pack import build
from prepare_trailblaze import outputs
import prepare_trailblaze as converter


class TrailblazePackTest(unittest.TestCase):
    def test_check_accepts_reencoded_pngs_with_identical_pixels_and_updated_hashes(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, device = Path(tmp) / "source", Path(tmp) / "device"
            source.mkdir(); device.mkdir()
            for i in range(12):
                Image.new("RGB", (216, 146), (i * 16, 128, 248)).save(source / f"{i:02}.png")
            prepared = outputs(source)
            for name, data in prepared.items(): (device / name).write_bytes(data)
            image = Image.open(device / "00.png").copy()
            image.save(device / "00.png", compress_level=0)
            self.assertNotEqual(prepared["00.png"], (device / "00.png").read_bytes())
            with self.assertRaisesRegex(ValueError, "manifest.json"):
                converter.check(source, device)
            manifest = json.loads(prepared["manifest.json"])
            manifest["images"][0]["device_sha256"] = hashlib.sha256((device / "00.png").read_bytes()).hexdigest()
            (device / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            converter.check(source, device)

    def test_check_rejects_changed_pixels_even_with_matching_file_hashes(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, device = Path(tmp) / "source", Path(tmp) / "device"
            source.mkdir(); device.mkdir()
            for i in range(12):
                Image.new("RGB", (216, 146), "blue").save(source / f"{i:02}.png")
            prepared = outputs(source)
            for name, data in prepared.items(): (device / name).write_bytes(data)
            Image.new("RGB", (216, 146), "red").save(device / "00.png")
            changed = (device / "00.png").read_bytes()
            manifest = json.loads(prepared["manifest.json"])
            manifest["images"][0]["device_sha256"] = hashlib.sha256(changed).hexdigest()
            (device / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "00.png"):
                converter.check(source, device)
            self.assertEqual((device / "00.png").read_bytes(), changed)

    def test_explicit_pool_preserves_indices_when_extra_originals_are_added(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)
            names = [f"Location {i:02}.png" for i in range(12)]
            for name in names + ["Extra artwork.png"]:
                Image.new("RGB", (216, 146), "blue").save(source / name)
            (source / "scenes.json").write_text(json.dumps(names), encoding="utf-8")
            converted = outputs(source)
            manifest = json.loads(converted["manifest.json"])
            self.assertEqual([item["source"] for item in manifest["images"]], names)
            self.assertTrue((source / "Extra artwork.png").is_file())

    def test_approved_conversion_keeps_sources_and_crops_about_centre(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)
            for i in range(12):
                image = Image.new("RGB", (864, 146), "red")
                image.paste((0, 255, 0), (216, 0, 648, 146))
                image.save(source / f"Location {i:02}.png")
            before = {p.name: p.read_bytes() for p in source.glob("*.png")}
            converted = outputs(source)
            import io
            native = Image.open(io.BytesIO(converted["00.png"]))
            self.assertEqual(native.size, (216, 146))
            self.assertEqual(native.getextrema(), ((0, 0), (255, 255), (0, 0)))
            self.assertEqual(before, {p.name: p.read_bytes() for p in source.glob("*.png")})

    def test_missing_or_wrong_size_art_is_rejected_without_processing(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)
            with self.assertRaisesRegex(ValueError, "missing"):
                build(source)
            for i in range(12):
                Image.new("RGB", (216, 146)).save(source / f"{i:02}.png")
            Image.new("RGB", (432, 292)).save(source / "03.png")
            with self.assertRaisesRegex(ValueError, "no automatic cropping"):
                build(source)

    def test_twelve_native_images_are_uncompressed_and_fit(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)
            for i in range(12):
                Image.new("RGB", (216, 146), (i * 16, 128, 248)).save(source / f"{i:02}.png")
            blob = build(source)
            self.assertEqual(blob[:4], b"STLT")
            version, size, count, crc, width, height, reserved = struct.unpack_from("<7I", blob, 4)
            self.assertEqual((version, size, count, width, height, reserved), (1, 756896, 12, 216, 146, 0))
            self.assertEqual(crc, zlib.crc32(blob[32:]))
            self.assertLessEqual(len(blob), 0xC0000)
            self.assertEqual(blob[32:34], struct.pack("<H", (32 << 5) | 31))


if __name__ == "__main__":
    unittest.main()
