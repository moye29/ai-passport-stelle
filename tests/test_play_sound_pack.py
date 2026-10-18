"""User MP3 provenance, three-clip order and combined resource bounds."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest
import wave
import zlib
import array

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from prepare_anniv_bgm import combine
from prepare_play_sounds import pack_clips


class PlaySoundPackTest(unittest.TestCase):
    def test_actual_clips_and_order(self):
        source=ROOT/'assets/music/stelle-play-sounds'
        device=source/'device'
        manifest=json.loads((device/'manifest.json').read_text())
        self.assertEqual(manifest['start_delay_ms'],200)
        self.assertEqual([e['source'] for e in manifest['clips']],
                         [f'bat_swing_{n:02d}.mp3' for n in (1,2,3)])
        clips=[]
        for n,e in enumerate(manifest['clips'],1):
            self.assertEqual(hashlib.sha256((source/e['source']).read_bytes()).hexdigest(), e['source_sha256'])
            data=(device/e['device']).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(),e['device_sha256'])
            self.assertEqual(data[:4],b'SBGM')
            self.assertEqual(zlib.crc32(data[32:]),struct.unpack_from('<I',data,20)[0])
            with wave.open(str(device/f'bat_swing_{n:02d}-preview.wav')) as wav:
                self.assertEqual((wav.getnchannels(),wav.getsampwidth(),wav.getframerate()),(1,2,16000))
                self.assertEqual(wav.getnframes(),struct.unpack_from('<I',data,12)[0]*320)
                prefix=array.array('h',wav.readframes(3200))
                self.assertTrue(all(v==0 for v in prefix))
            clips.append(data)
        self.assertEqual((device/'play_sounds.bin').read_bytes(),pack_clips(clips))

    def test_audio_pack_preserves_artwork_and_music_without_repartitioning(self):
        images=(ROOT/'build/assets/stelle_assets.bin').read_bytes()
        music=(ROOT/'assets/music/stelle-anniversary/selected/star-rail-chip-full.sbgm').read_bytes()
        effects=(ROOT/'assets/music/stelle-play-sounds/device/play_sounds.bin').read_bytes()
        result=combine(images,music,effects)
        offset=(len(images)+4095)&~4095
        self.assertEqual(result[:len(images)],images)
        self.assertEqual(result[offset:offset+len(music)],music)
        self.assertEqual(result[offset+len(music):],effects)
        self.assertLessEqual(len(result),0x200000)
        broken=bytearray(effects);broken[-1]^=1
        with self.assertRaises(ValueError): combine(images,music,broken)


if __name__=='__main__': unittest.main()
