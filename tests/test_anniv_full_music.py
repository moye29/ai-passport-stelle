"""Check opening-score stability, playback headroom and loop endpoints."""
import array
import hashlib
import json
from pathlib import Path
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]
MUSIC = ROOT / 'assets/music/stelle-anniversary'


def read_wav(path):
    with wave.open(str(path)) as wav:
        if (wav.getframerate(), wav.getnchannels(), wav.getsampwidth()) != (16000, 1, 2):
            raise ValueError('unexpected playback format')
        return wav.readframes(wav.getnframes())


class FullMusicTest(unittest.TestCase):
    def test_opening_six_bars_match_reference(self):
        reference = json.loads((ROOT / 'tests/fixtures/audio/opening-score.json').read_text())
        complete = json.loads((MUSIC / 'selected/manifest.json').read_text())
        self.assertEqual([n for n in complete['lead'] if n['beat'] < 24], reference['right'])
        self.assertEqual([n for n in complete['backing'] if n['beat'] < 24],
                         reference['secondary_left']+reference['secondary'])

    def test_whole_mix_fades_only_in_last_two_seconds(self):
        selected = MUSIC / 'selected'
        faded = array.array('h', read_wav(selected / 'star-rail-chip-full.wav'))
        original = array.array('h', read_wav(selected / 'star-rail-chip-full-unfaded.wav'))
        fade = 2*16000
        self.assertEqual(len(faded), len(original))
        self.assertEqual(faded[:-fade], original[:-fade])
        for j in range(0, fade, 17):
            expected = original[-fade+j] * (fade-1-j)/(fade-1)
            self.assertLessEqual(abs(faded[-fade+j]-expected), 1)
        self.assertEqual(faded[-1], 0)

    def test_stored_music_matches_manifest_and_has_clean_loop_join(self):
        selected = MUSIC / 'selected'
        manifest = json.loads((selected / 'manifest.json').read_text())
        packed = (selected / 'star-rail-chip-full.sbgm').read_bytes()
        self.assertEqual(hashlib.sha256(packed).hexdigest(), manifest['audio_sha256'])
        self.assertEqual(len(packed), manifest['bytes'])
        self.assertEqual(manifest['backing_gain'], 0.9)
        self.assertEqual(manifest['fade_out_seconds'], 2)
        self.assertEqual(manifest['backing_automation'], 'constant; no ducking')
        self.assertGreater(manifest['partition_free'], 0)
        self.assertGreater(manifest['seconds'], 60)
        pcm = array.array('h', read_wav(selected / 'star-rail-chip-device-preview.wav'))
        self.assertLess(max(abs(v) for v in pcm), 30000)
        self.assertEqual(pcm[0], 0)
        self.assertTrue(all(v == 0 for v in pcm[-160:]))
        # Decoded last->first samples have no amplitude discontinuity.
        self.assertEqual(pcm[-1], pcm[0])


if __name__ == '__main__':
    unittest.main()
