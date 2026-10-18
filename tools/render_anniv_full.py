"""Render the complete score-led chip arrangement with fixed 90% backing.

The opening six-bar reference is retained. Remaining piano figures are arranged
into a foreground line and background; dense ornaments are condensed. Ensemble
parts are used as a melody reference, not as a downloaded MIDI source.
"""
import array
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import argparse
import wave
import numpy as np
from render_anniv_piano_sample import score, render, midi_track, BPM, RATE, PPQ
from prepare_anniv_bgm import encode, combine

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/music/stelle-anniversary/selected'


def full_score():
    lead, _ = score('melody-clean')
    original, backing = score('piano')
    backing += [n for n in original if n['beat'] >= 20 and n['midi'] < 84]
    # Bar starts preserve the piano arrangement's two 7/8 measures (9 and 10).
    starts = {7:24, 8:28, 9:32, 10:35.5, 11:39, 12:43, 13:47,
              14:51, 15:55, 16:59, 17:63, 18:67}

    def add(track, bar, beat, duration, notes, velocity=72):
        for pitch in notes:
            track.append(dict(beat=starts[bar] + beat, duration=duration,
                              midi=pitch, velocity=velocity))

    def phrase(track, bar, beat, pitches, duration, velocity=72):
        for i, pitch in enumerate(pitches):
            add(track, bar, beat + i * duration, duration, [pitch], velocity)

    # Continuation of the high theme and its response, piano bars 7-8.
    phrase(lead, 7, 0, [89,91,88], 1/3, 82)
    add(lead, 7, 1, 1, [86], 78)
    add(lead, 7, 3, 1, [84], 75)
    phrase(lead, 8, 0, [83,84], .5, 76)
    add(lead, 8, 1, 2, [86], 78)
    add(backing, 7, 0, 1, [34,46], 46)
    phrase(backing, 7, 0, [77,79,76], 1/3, 48)
    phrase(backing, 7, 1, [65,67,64], 1/3, 49)
    add(backing, 7, 1, 1, [74], 48)
    add(backing, 7, 2, 1, [29,41], 45)
    phrase(backing, 7, 3, [60,64], .5, 48)
    add(backing, 8, 0, 1, [34,46], 46)
    add(backing, 8, 0, .5, [71,74], 48)
    add(backing, 8, .5, .5, [72], 48)
    add(backing, 8, 1, 2, [74], 46)
    phrase(backing, 8, 1, [65,67,65], 1/3, 48)
    add(backing, 8, 2, .5, [67], 48)
    phrase(backing, 8, 3, [62,59], .5, 45)

    # Piano flourish: retain its pitches, simplify the ornamental grouping.
    add(lead, 9, .75, .25, [83], 72)
    phrase(lead, 9, 1, [84,86,88,90,91,93], 1/6, 73)
    add(lead, 9, 2, .5, [93], 72)
    phrase(lead, 9, 2.5, [86,88,86,84], .25, 71)
    add(backing, 9, 0, 1, [53,57,60], 43)
    phrase(backing, 9, 1, [81,79,78], 1/3, 42)
    phrase(backing, 9, 2, [74,72,71], .5, 43)
    add(lead, 10, 0, 3, [74], 68)
    phrase(backing, 10, 0, [84,86,84,83,84,83,81,79,81,79,78,76,78,76,78,76], .125, 47)
    phrase(backing, 10, 2, [74,72], .25, 47)
    add(backing, 10, 2.5, .5, [74], 46)
    add(backing, 10, 0, 1/3, [54,57,62], 43)
    add(backing, 10, 1/3, 1/3, [55], 43)
    add(backing, 10, 2/3, 7/3, [57], 43)

    # Sustained foreground over the piano's six-note figures. The ensemble
    # melodic line provides the response across bars 11-14.
    add(lead, 11, 0, 2, [69], 74)
    phrase(lead, 11, 2, [72,74,71], 1/3, 75)
    add(lead, 11, 3, 1, [69], 72)
    add(lead, 12, 0, 2, [69], 72)
    phrase(lead, 12, 2, [69,71,67], 1/3, 74)
    add(lead, 12, 3, 1, [69], 74)
    add(lead, 13, 0, 2, [69], 74)
    phrase(lead, 13, 2, [72,74,71], 1/3, 75)
    phrase(lead, 13, 3, [69,72], .5, 76)
    phrase(lead, 14, 0, [78,79,81,79,78,76], 1/3, 78)
    phrase(lead, 14, 2, [69,78,81], 1/3, 79)
    phrase(lead, 14, 3, [86,88], .5, 82)
    for bar in (11,12):
        for beat in range(4):
            pattern = [93,81,86,81,86,81]
            if bar == 11 and beat == 2:
                pattern = [72,81,86,81,86,81]
            phrase(backing, bar, beat, pattern, 1/6, 43)
    for beat in range(4):
        phrase(backing, 13, beat, [91,79,84,79,84,79], 1/6, 43)
    for beat, first in enumerate((78,79,81)):
        phrase(backing, 14, beat, [first,90,86,81,86,78], 1/6, 45)
    phrase(backing, 14, 3, [74,76], .5, 46)
    # Condensed left-hand support avoids crowding the foreground chip voice.
    for bar, bass, upper in [(11,48,57),(12,50,57),(13,53,60)]:
        add(backing, bar, 0, 2, [bass], 44)
        add(backing, bar, 2, 2, [upper], 44)
    add(backing, 14, 0, 1, [29,41], 43)
    add(backing, 14, 1, 1, [43], 44)
    add(backing, 14, 2, 1, [45,48], 44)
    add(backing, 14, 3, 1, [48,60], 46)

    # Final return and piano coda, including written F/B naturals/flats.
    phrase(lead, 15, 0, [89,91,88], 1/3, 84)
    add(lead, 15, 1, 1, [86], 80)
    add(lead, 15, 2, 1, [78], 76)
    add(lead, 15, 3, 1, [84], 76)
    phrase(backing, 15, 0, [77,79,76], 1/3, 49)
    add(backing, 15, 1, 1, [74,79], 48)
    add(backing, 15, 3, 1, [72], 47)
    add(backing, 15, 0, 1, [34,46], 45)
    phrase(backing, 15, 1, [79,77,72], 1, 47)
    phrase(lead, 16, 0, [83,84], .5, 77)
    add(lead, 16, 1, 1, [86], 78)
    add(lead, 16, 2.5, .5, [74], 72)
    phrase(lead, 16, 3, [72,71,69], 1/3, 70)
    add(backing, 16, 0, .5, [71,74], 46)
    add(backing, 16, .5, .5, [72], 46)
    add(backing, 16, 1, 1, [74], 46)
    add(backing, 16, 0, 1, [31,43], 44)
    phrase(backing, 16, 1, [65,67,65], 1/3, 46)
    add(backing, 16, 2, .5, [67], 46)
    phrase(lead, 17, 1, [79,81], .5, 71)
    add(lead, 17, 2, 1, [82], 73)
    add(lead, 17, 3, 1, [86], 74)
    add(backing, 17, 0, 1, [51,55,58,63,75], 40)
    add(backing, 17, 0, 2, [39,46], 43)
    add(backing, 17, 2, 2, [55], 43)
    phrase(lead, 18, 0, [62,66,69,74], .25, 62)
    phrase(lead, 18, 1, [81,86], .5, 69)
    add(lead, 18, 2, 4, [81,93], 70) # interpreted held fermata, then a release
    add(backing, 18, 0, 6, [50], 43)
    return lead, backing


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ffmpeg', required=True)
    args = ap.parse_args()
    lead, backing = full_score()
    end_beats = max(n['beat'] + n['duration'] for n in lead + backing)
    total = ((round((end_beats * 60 / BPM + .35) * RATE)+319)//320)*320
    melody = render(lead, 'right', total)
    accompaniment = render(backing, 'left', total) * .90
    # Reuse exactly the reference gain of the accepted six-bar audition.
    ref_lead, ref_left = score('melody-clean')
    reference = render(ref_lead, 'right', total) + render(ref_left, 'left', total)
    delay = round(.11 * RATE)
    reference[delay:] += .10 * reference[:-delay].copy()
    scale = 18000 / np.max(np.abs(reference))
    audio = melody + accompaniment
    audio[delay:] += .10 * audio[:-delay].copy()
    audio *= scale
    if np.max(np.abs(audio)) > 30000:
        raise ValueError('arrangement exceeds the selected playback headroom')
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / 'star-rail-chip-full-unfaded.wav'), 'wb') as out:
        out.setparams((1,2,RATE,0,'NONE','not compressed'))
        out.writeframes(np.round(audio).astype('<i2').tobytes())
    # Fade the final combined melody and backing together, exactly two seconds.
    audio[-2*RATE:] *= np.linspace(1,0,2*RATE)
    pcm = np.round(audio).astype('<i2')
    wav_path = OUT / 'star-rail-chip-full.wav'
    with wave.open(str(wav_path), 'wb') as out:
        out.setparams((1,2,RATE,0,'NONE','not compressed'))
        out.writeframes(pcm.tobytes())
    raw = subprocess.check_output([args.ffmpeg, '-v','error','-i',str(wav_path),
                                   '-ar','12000','-ac','1','-f','s16le','-'])
    samples = array.array('h', raw)
    samples[0] = 0 # remove the resampler's one-LSB edge before the loop restart
    samples.extend([0] * ((-len(samples)) % 240))
    packed, decoded = encode(samples)
    (OUT / 'star-rail-chip-full.sbgm').write_bytes(packed)
    # This is the actual device decode, with no new bitcrusher or note changes.
    x = np.asarray(decoded, dtype=np.int32)
    pos = np.arange(len(x) * 4 // 3) * 3
    at, fraction = pos // 4, pos % 4
    replay = np.trunc((x[at] * (4-fraction) + x[(at+1) % len(x)] * fraction)/4).astype('<i2')
    with wave.open(str(OUT / 'star-rail-chip-device-preview.wav'), 'wb') as out:
        out.setparams((1,2,RATE,0,'NONE','not compressed'))
        out.writeframes(replay.tobytes())
    tracks = [midi_track([(0,b'\xff\x51\x03'+round(60000000/BPM).to_bytes(3,'big'))])]
    for channel, notes in enumerate((lead,backing)):
        events=[]
        for n in notes:
            events.extend([(round(n['beat']*PPQ),bytes([0x90|channel,n['midi'],n['velocity']])),
                           (round((n['beat']+n['duration'])*PPQ),bytes([0x80|channel,n['midi'],0]))])
        tracks.append(midi_track(events))
    (OUT / 'star-rail-chip-full.mid').write_bytes(b'MThd'+struct.pack('>IHHH',6,1,len(tracks),PPQ)+b''.join(tracks))
    image_pack = (ROOT / 'build/assets/stelle_assets.bin').read_bytes()
    combined = combine(image_pack, packed)
    metadata=dict(bpm=BPM, seconds=len(decoded)/12000, bytes=len(packed),
                  combined_asset_bytes=len(combined), partition_free=0x200000-len(combined),
                  source='Tokiku piano PDF, frostwing98 ensemble parts as melodic reference',
                  adaptation='opening bars 1-6; remaining bars manually adapted, dense inner figures condensed; held final fermata',
                  backing_gain=.90, backing_automation='constant; no ducking', fade_out_seconds=2,
                  scale=scale, peak=int(np.max(np.abs(replay.astype(np.int32)))),
                  ending='natural note release and short breath before returning to opening pedal',
                  audio_sha256=hashlib.sha256(packed).hexdigest(), lead=lead, backing=backing)
    (OUT / 'manifest.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in metadata.items() if k not in ('lead','backing')}))


if __name__ == '__main__':
    main()
