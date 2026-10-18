"""Verify actual audio, codec vectors and append-only image/audio placement."""
import json
from pathlib import Path
import struct
import sys
import unittest
import wave
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_anniv_bgm import advance, combine, encode


class AnniversaryPackTest(unittest.TestCase):
    def test_known_ima_vector_and_encoder(self):
        self.assertEqual(advance(0, 0, 7), (11, 8))
        self.assertEqual(advance(11, 8, 15), (-19, 16))
        self.assertEqual(advance(32767, 88, 7), (32767, 88))
        self.assertEqual(advance(-32768, 88, 15), (-32768, 88))
        pack, decoded = encode([1000] * 480)
        self.assertEqual(len(decoded), 480)
        self.assertEqual(decoded, [1000] * 480)
        self.assertEqual(len(pack), 32 + 124 * 2)

    def test_real_pack_and_audio_preview(self):
        folder = ROOT / 'assets/music/stelle-anniversary/selected'
        music = (folder / 'star-rail-chip-full.sbgm').read_bytes()
        manifest = json.loads((folder / 'manifest.json').read_text())
        self.assertEqual(music[:4], b'SBGM')
        self.assertEqual(zlib.crc32(music[32:]), struct.unpack_from('<I', music, 20)[0])
        self.assertGreater(manifest['seconds'], 60)
        self.assertLess(manifest['peak'], 30000)
        self.assertEqual(manifest['backing_gain'], 0.90)
        images = (ROOT / 'build/assets/stelle_assets.bin').read_bytes()
        result = combine(images, music)
        offset = (len(images) + 4095) & ~4095
        self.assertEqual(result[:len(images)], images)
        self.assertEqual(result[offset:], music)
        self.assertLessEqual(len(result), 0x200000)
        with wave.open(str(folder / 'star-rail-chip-device-preview.wav')) as w:
            self.assertEqual((w.getnchannels(), w.getsampwidth(), w.getframerate()), (1, 2, 16000))
            self.assertEqual(w.getnframes(), struct.unpack_from('<I', music, 12)[0] * 320)
        with self.assertRaises(ValueError):
            combine(images, music[:-1])
        damaged = bytearray(music); damaged[-1] ^= 1
        with self.assertRaises(ValueError):
            combine(images, damaged)


if __name__ == '__main__':
    unittest.main()
