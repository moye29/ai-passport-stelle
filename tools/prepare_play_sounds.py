"""Prepare the user's three MP3 Bat Swing clips without changing originals."""
import argparse
import array
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave
import zlib
from prepare_anniv_bgm import encode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/music/stelle-play-sounds'
DEVICE = SOURCE / 'device'


def pack_clips(clips):
    if len(clips) != 3:
        raise ValueError('exactly three clips are required')
    body = bytearray(32) # directory plus reserved padding, following the header
    offset = 64
    for i, clip in enumerate(clips):
        struct.pack_into('<II', body, i*8, offset, len(clip))
        body += clip
        offset += len(clip)
    return b'STFX' + struct.pack('<IIII',1,offset,3,zlib.crc32(body)) + bytes(12) + body


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ffmpeg')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    if args.check:
        manifest = json.loads((DEVICE / 'manifest.json').read_text())
        clips=[]
        for entry in manifest['clips']:
            source=(SOURCE / entry['source']).read_bytes()
            clip=(DEVICE / entry['device']).read_bytes()
            if hashlib.sha256(source).hexdigest() != entry['source_sha256']:
                raise ValueError('source changed: '+entry['source'])
            if hashlib.sha256(clip).hexdigest() != entry['device_sha256']:
                raise ValueError('device clip changed: '+entry['device'])
            clips.append(clip)
        if (DEVICE / 'play_sounds.bin').read_bytes() != pack_clips(clips):
            raise ValueError('stale Play sound pack')
        print('Play audio sources, hashes and ordered pack: PASS')
        return
    if not args.ffmpeg:
        ap.error('--ffmpeg is required to prepare audio')
    DEVICE.mkdir(parents=True,exist_ok=True)
    clips, records=[],[]
    for i in range(1,4):
        source=SOURCE / f'bat_swing_{i:02d}.mp3'
        raw=subprocess.check_output([args.ffmpeg,'-v','error','-i',str(source),
                                     '-ar','12000','-ac','1','-f','s16le','-'])
        pcm=array.array('h',raw)
        if len(pcm) < 240:
            raise ValueError('clip is too short: '+source.name)
        seconds=len(pcm)/12000
        # Preserve level and content, with only 4 ms edge ramps against clicks.
        fade=min(48,len(pcm)//4)
        for j in range(fade):
            pcm[j]=int(pcm[j]*j/fade)
            pcm[-1-j]=int(pcm[-1-j]*j/fade)
        # Delay the audible effect by exactly 200 ms without sleeping in the
        # button/model path or adding a blocking timer to the audio worker.
        pcm=array.array('h',[0]*2400)+pcm
        pcm.extend([0]*((-len(pcm))%240))
        packed, decoded=encode(pcm)
        device=DEVICE / f'bat_swing_{i:02d}.sbgm'
        device.write_bytes(packed)
        preview=array.array('h')
        for j in range(len(decoded)*4//3):
            at,fraction=divmod(j*3,4)
            value=int((decoded[at]*(4-fraction)+decoded[(at+1)%len(decoded)]*fraction)/4)
            preview.append(int(value*min(j,320)/320)) # same 20 ms start ramp as firmware
        with wave.open(str(DEVICE / f'bat_swing_{i:02d}-preview.wav'),'wb') as wav:
            wav.setparams((1,2,16000,0,'NONE','not compressed'))
            wav.writeframes(preview.tobytes())
        clips.append(packed)
        records.append(dict(source=source.name,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                            source_seconds=seconds,device=device.name,device_bytes=len(packed),
                            playback_seconds=len(decoded)/12000,peak=max(abs(v) for v in preview),
                            device_sha256=hashlib.sha256(packed).hexdigest()))
    result=pack_clips(clips)
    (DEVICE / 'play_sounds.bin').write_bytes(result)
    manifest=dict(order='01 -> 02 -> 03 -> 01',restart='first clip after reboot',
                  start_delay_ms=200,
                  gain='original source level, no normalization or pixel effect',edge_fade_ms=4,
                  format='12 kHz mono independent IMA blocks, 16 kHz playback',
                  source_rights='user-provided files; no redistribution license supplied',
                  bytes=len(result),clips=records)
    (DEVICE / 'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest))


if __name__ == '__main__':
    main()
