"""Make a softly crushed, band-limited loop and independently seekable IMA blocks.

Requires an explicit FFmpeg executable; sources and derivatives stay in assets/music.
No network access, downloads or source replacement are performed by this tool.
"""
import argparse
import array
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave
import zlib

STEPS = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
         60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
         337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,
         1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,
         4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
         15289,16818,18500,20350,22385,24623,27086,29794,32767]
ADJUST = [-1,-1,-1,-1,2,4,6,8]
RATE = 12000
FILTER = ('highpass=f=110,lowpass=f=4300,'
          'acrusher=bits=7.5:mix=0.32:mode=lin:aa=1:samples=1.5,'
          'lowpass=f=4100')
HYBRID_FILTER = (
    'asplit=2[dry][wet];'
    '[dry]highpass=f=100,lowpass=f=5200,aresample=12000[clean];'
    '[wet]highpass=f=180,lowpass=f=4000,aresample=12000,'
    'acrusher=bits=5.5:mix=1:mode=lin:aa=0.7:samples=2,lowpass=f=2900[chip];'
    '[clean][chip]amix=inputs=2:weights=0.68 0.32:normalize=0,lowpass=f=5000'
)
STRONG_HYBRID_FILTER = HYBRID_FILTER.replace(
    'bits=5.5:mix=1:mode=lin:aa=0.7:samples=2',
    'bits=5:mix=1:mode=lin:aa=0.7:samples=2.5'
).replace('weights=0.68 0.32', 'weights=0.35 0.65')


def advance(value, index, code):
    step = STEPS[index]
    delta = (step >> 3) + ((step >> 2) if code & 1 else 0)
    delta += (step >> 1) if code & 2 else 0
    delta += step if code & 4 else 0
    value = max(-32768, min(32767, value + (-delta if code & 8 else delta)))
    return value, max(0, min(88, index + ADJUST[code & 7]))


def encode(pcm):
    if not pcm or len(pcm) % 240:
        raise ValueError('audio must have a whole number of 20 ms blocks')
    payload = bytearray()
    decoded = []
    index = 0
    for off in range(0, len(pcm), 240):
        value = pcm[off]
        payload += struct.pack('<hBB', value, index, 0)
        codes = bytearray(120)
        decoded.append(value)
        for i in range(1, 240):
            difference = pcm[off + i] - value
            code = 8 if difference < 0 else 0
            difference = abs(difference)
            step = STEPS[index]
            for bit, threshold in [(4, step), (2, step >> 1), (1, step >> 2)]:
                if difference >= threshold:
                    code |= bit
                    difference -= threshold
            value, index = advance(value, index, code)
            decoded.append(value)
            codes[(i - 1) // 2] |= code << (((i - 1) % 2) * 4)
        payload += codes
    head = b'SBGM' + struct.pack('<IIIII', 1, RATE, len(pcm) // 240, 124, zlib.crc32(payload)) + bytes(8)
    return head + payload, decoded


def combine(images, music, effects=b''):
    if len(images) < 32 or images[:4] != b'STLP' or struct.unpack_from('<I', images, 8)[0] != len(images):
        raise ValueError('invalid image resource pack')
    if len(music) < 32 or music[:4] != b'SBGM':
        raise ValueError('invalid anniversary audio')
    version, rate, count, block, crc = struct.unpack_from('<IIIII', music, 4)
    if (version, rate, block) != (1, RATE, 124) or not count or len(music) != 32 + count * block:
        raise ValueError('unsupported audio format/length')
    if zlib.crc32(music[32:]) != crc:
        raise ValueError('audio CRC mismatch')
    offset = (len(images) + 4095) & ~4095
    result = images + b'\xff' * (offset - len(images)) + music
    if effects:
        if len(effects) < 64 or effects[:4] != b'STFX':
            raise ValueError('invalid Play sound pack')
        version, total, count, crc = struct.unpack_from('<IIII', effects, 4)
        if (version, total, count) != (1, len(effects), 3) or zlib.crc32(effects[32:]) != crc:
            raise ValueError('invalid Play sound header/CRC')
        result += effects
    if len(result) > 0x200000:
        raise ValueError(f'audio and images exceed the 2 MiB assets partition: {len(result)}')
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path)
    ap.add_argument('--ffmpeg')
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--combine', type=Path, help='existing image pack to combine with --source SBGM')
    ap.add_argument('--effects', type=Path, help='append the three-clip Play sound pack after the music')
    ap.add_argument('--style', choices=('retro', 'hybrid', 'hybrid-strong'), default='retro')
    args = ap.parse_args()
    if args.combine:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        effects = args.effects.read_bytes() if args.effects else b''
        args.output.write_bytes(combine(args.combine.read_bytes(), args.source.read_bytes(), effects))
        return
    if not args.source or not args.ffmpeg:
        ap.error('--source and --ffmpeg are required for audio processing')
    selected_filter = {'retro': FILTER, 'hybrid': HYBRID_FILTER,
                       'hybrid-strong': STRONG_HYBRID_FILTER}[args.style]
    raw = subprocess.check_output([args.ffmpeg, '-v', 'error', '-i', str(args.source),
                                   '-af', selected_filter, '-ar', str(RATE), '-ac', '1', '-f', 's16le', '-'])
    samples = array.array('h', raw)
    if len(samples) < RATE * 4:
        raise ValueError('source too short')
    # Overlap a short tail with the head, then rotate the loop to that join.
    # The two joins are contiguous source samples, without a hard waveform jump.
    n = int(RATE * 0.8)
    cross = [round(samples[-n + i] * (1 - i / n) + samples[i] * i / n) for i in range(n)]
    loop = list(samples[n:-n]) + cross
    loop = loop[:len(loop) // 240 * 240]
    peak = max(abs(v) for v in loop)
    if not peak:
        raise ValueError('silent source')
    loop = [round(v * 18000 / peak) for v in loop]
    packed, decoded = encode(loop)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'star-rail-retro.sbgm').write_bytes(packed)
    # Preview uses the exact firmware's IMA decode and 12->16 kHz interpolation.
    preview = array.array('h')
    for i in range(len(decoded) * 4 // 3):
        at, fraction = divmod(i * 3, 4)
        a, b = decoded[at], decoded[(at + 1) % len(decoded)]
        preview.append(int((a * (4 - fraction) + b * fraction) / 4))
    with wave.open(str(args.output / 'star-rail-retro-preview.wav'), 'wb') as out:
        out.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
        out.writeframes(preview.tobytes())
    metadata = dict(source=args.source.name, source_sha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
                    style=args.style, filter=selected_filter, loop_crossfade_s=0.8, seconds=len(decoded)/RATE,
                    storage='independent 240-sample IMA ADPCM blocks, low nibble first',
                    source_rate=RATE, playback_rate=16000, bytes=len(packed), peak=max(abs(v) for v in preview),
                    audio_sha256=hashlib.sha256(packed).hexdigest())
    (args.output / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(metadata))


if __name__ == '__main__':
    main()
