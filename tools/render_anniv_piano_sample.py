"""Audition six manually entered bars from Tokiku's Star Rail piano PDF.

The original version uses Tokiku's piano arrangement. The melody-clean revision
also checks frostwing98's ensemble piano part to separate melody from ornaments.
Neither version uses automatic audio transcription or downloaded MIDI.
Requires NumPy. This personal-use audition is not selected for firmware.
"""
import json
import argparse
from pathlib import Path
import struct
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/music/stelle-anniversary/piano-score-sample'
BPM = 66
RATE = 16000
PPQ = 480


def score(revision='piano'):
    # Beat positions are quarter notes; C4 = MIDI 60. One-sharp key signature.
    right, left = [], []

    def add(track, beat, duration, pitches, velocity):
        for pitch in pitches:
            track.append(dict(beat=beat, duration=duration, midi=pitch, velocity=velocity))

    for bar in range(5):
        for beat in range(4):
            add(left, bar * 4 + beat, 1, [62], 50) # written D4 pedal, quarter notes
    add(right, 7, 1, [74], 75) # bar 2: half rest + quarter rest, then D5
    for start in (8, 16):
        for i, pitch in enumerate((84, 86, 83)): # C6 D6 B5 eighth-note triplet
            add(right, start + i / 3, 1 / 3, [pitch], 82)
        add(right, start + 1, 2, [81], 76) # A5 half note
    add(right, 11, 1, [79], 72) # G5
    add(right, 12, 0.5, [78], 72) # written F5 -> F#5 from key signature
    add(right, 12.5, 0.5, [79], 75)
    add(right, 13, 1, [81], 80)
    # Beat 14 is a quarter rest.
    add(right, 15, 0.5, [81], 75)
    add(right, 15.5, 0.5, [74], 70)
    add(right, 19, 0.5, [81], 77)
    add(right, 19.5, 0.5, [88], 85) # E6
    # Bar 6: F#6, a C5 trill, octave D5/D6 and E5/E6 dyads.
    add(right, 20, 1, [90], 86)
    for i in range(8):
        add(right, 21 + i / 8, 1 / 8, [72 if i % 2 == 0 else 74], 66)
    add(right, 22, 1, [74, 86], 80)
    add(right, 23, 0.5, [74, 86], 82)
    add(right, 23.5, 0.5, [76, 88], 85)
    for beat in range(4):
        add(left, 20 + beat, 1, [60, 69], 56) # written C4/A4 dyads
    if revision in ('melody-clean', 'secondary-ducked', 'secondary-fixed80'):
        # Ensemble piano, page 7: bar 4 sustains A5 through beat 3;
        # bar 6 sustains F#6 for two beats. The C5 trill and low octave
        # doublings belong to secondary material, not this melody line.
        right = [n for n in right if n['beat'] < 20]
        for n in right:
            if n['beat'] == 13:
                n['duration'] = 2
        add(right, 20, 2, [90], 86)
        add(right, 22, 1, [86], 80)
        add(right, 23, 0.5, [86], 82)
        add(right, 23.5, 0.5, [88], 85)
        # User requested removal, rather than replacing the repeated dyads.
        left = [n for n in left if n['beat'] < 20]
    return right, left


def pulse(t, frequency, duty):
    # Finite Fourier synthesis keeps high-register chip leads free of aliasing.
    out = np.zeros(len(t))
    for harmonic in range(1, int(6200 / frequency) + 1):
        out += (2 / np.pi * np.sin(np.pi * harmonic * duty) / harmonic *
                np.cos(2 * np.pi * harmonic * frequency * t - np.pi * harmonic * duty))
    return out


def render(notes, hand, total):
    audio = np.zeros(total)
    for note in notes:
        start = round(note['beat'] * 60 / BPM * RATE)
        sustain = round(note['duration'] * 60 / BPM * RATE)
        release = min(round(0.045 * RATE), sustain // 3)
        count = sustain + release
        t = np.arange(count) / RATE
        frequency = 440 * 2 ** ((note['midi'] - 69) / 12)
        if hand == 'right':
            sound = 0.80 * pulse(t, frequency, 0.25) + 0.20 * np.sin(2 * np.pi * frequency * t)
            level = 0.66
        else:
            # Triangle core plus a quiet pulse edge; keep the piano's written register.
            sound = np.zeros(count)
            for h in range(1, int(6200 / frequency) + 1, 2):
                sound += (-1) ** ((h - 1) // 2) * np.sin(2 * np.pi * h * frequency * t) / h ** 2
            sound = 0.85 * sound + 0.15 * pulse(t, frequency, 0.5)
            level = 0.42
        env = 0.68 + 0.32 * np.exp(-t / 0.12)
        attack = min(64, sustain // 4)
        env[:attack] *= np.linspace(0, 1, attack)
        env[sustain:] *= np.linspace(1, 0, release)
        end = min(total, start + count)
        audio[start:end] += sound[:end-start] * env[:end-start] * level * note['velocity'] / 100
    return audio


def variable(value):
    out = [value & 127]
    value >>= 7
    while value:
        out.insert(0, (value & 127) | 128)
        value >>= 7
    return bytes(out)


def midi_track(events):
    events.sort(key=lambda event: (event[0], event[1][0] & 0xf0 != 0x80))
    data = bytearray()
    previous = 0
    for tick, message in events:
        data += variable(tick - previous) + message
        previous = tick
    data += b'\x00\xff\x2f\x00'
    return b'MTrk' + struct.pack('>I', len(data)) + data


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision', choices=('piano', 'melody-clean', 'secondary-ducked', 'secondary-fixed80'), default='piano')
    ap.add_argument('--output', type=Path, default=OUT)
    args = ap.parse_args()
    out_dir = args.output
    right, left = score(args.revision)
    assert max(n['beat'] + n['duration'] for n in right + left) == 24
    total = round((24 * 60 / BPM + 0.35) * RATE)
    audio = render(right, 'right', total) + render(left, 'left', total)
    reference = audio.copy()
    secondary_notes = []
    secondary_left = []
    if args.revision in ('secondary-ducked', 'secondary-fixed80'):
        original_right, original_left = score('piano')
        secondary_notes = [n for n in original_right if n['beat'] >= 20 and n['midi'] < 84]
        secondary_left = [n for n in original_left if args.revision == 'secondary-fixed80' or n['beat'] >= 20]
        secondary = render(secondary_notes + secondary_left, 'left', total)
        if args.revision == 'secondary-fixed80':
            audio = render(right, 'right', total)
            left = [] # all backing is already in secondary_left, including MIDI export
            secondary *= 0.80 # fixed gain for all backing, including the opening pedal
        else:
            beats = np.arange(total) / RATE * BPM / 60
            # The climax begins at beat 20 (18.18 s): subordinate the backing,
            # then smoothly reduce it from 55% to 20% over 1.5 beats.
            progress = np.clip((beats - 20) / 1.5, 0, 1)
            smooth = progress * progress * (3 - 2 * progress)
            secondary *= 0.55 - 0.35 * smooth
    else:
        secondary = np.zeros(total)
    # A quiet short echo adds space without altering any pitches or chord tones.
    delay = round(0.11 * RATE)
    audio[delay:] += 0.10 * audio[:-delay].copy()
    reference[delay:] += 0.10 * reference[:-delay].copy()
    # Keep the main melody at the same level as the melody-clean audition.
    scale = 18000 / max(np.max(np.abs(reference)), 0.001)
    secondary[delay:] += 0.10 * secondary[:-delay].copy()
    audio = (audio + secondary) * scale
    if np.max(np.abs(audio)) > 30000:
        raise ValueError('audition would exceed the playback headroom')
    pcm = np.round(audio).astype('<i2')
    out_dir.mkdir(parents=True, exist_ok=True)
    with wave.open(str(out_dir / 'star-rail-piano-chip-6bars.wav'), 'wb') as wav:
        wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        wav.writeframes(pcm.tobytes())
    tempo = round(60000000 / BPM)
    tracks = [midi_track([(0, b'\xff\x51\x03' + tempo.to_bytes(3, 'big'))])]
    for channel, hand in enumerate((right, left, secondary_notes, secondary_left)):
        events = []
        for n in hand:
            events += [(round(n['beat'] * PPQ), bytes([0x90 | channel, n['midi'], n['velocity']])),
                       (round((n['beat'] + n['duration']) * PPQ), bytes([0x80 | channel, n['midi'], 0]))]
        tracks.append(midi_track(events))
    (out_dir / 'star-rail-piano-6bars.mid').write_bytes(b'MThd' + struct.pack('>IHHH', 6, 1, len(tracks), PPQ) + b''.join(tracks))
    (out_dir / 'score-entry.json').write_text(json.dumps(dict(
        source='../scores/star-rail-tokiku-piano.pdf', arranger='Tokiku',
        source_url='https://tokiku-music.github.io/music/star_rail/star_rail.pdf',
        bars='1-6', bpm=BPM, key_signature='one sharp; G major / E minor',
        entry='manually entered from rendered piano score; no automatic audio transcription',
        revision=args.revision,
        secondary_reference=('../scores/star-rail-frostwing98-parts.pdf page 7, piano bars 4 and 6'
                             if args.revision != 'piano' else None),
        ornament=('bar 6 C5 trill, lower octave doublings and left-hand dyads restored as a separate subdued layer'
                  if args.revision in ('secondary-ducked', 'secondary-fixed80') else
                  'bar 6 secondary C5 trill, lower octave doublings and repeated left-hand dyads removed; F#6 sustained'
                  if args.revision == 'melody-clean' else
                  'bar 6 C5 trill interpreted as eight alternating C5/D5 notes over one quarter-note beat'),
        secondary_automation=(dict(start_beat=0, start_gain=0.80, end_gain=0.80, curve='constant',
                                   scope='all left-hand backing plus ending trill and lower octave notes',
                                   main_level='same scale as melody-clean', midi_note='MIDI contains notes only; WAV applies fixed backing gain')
                              if args.revision == 'secondary-fixed80' else
                              dict(start_beat=20, start_seconds=20*60/BPM, start_gain=0.55,
                                   end_beat=21.5, end_gain=0.20, curve='smoothstep',
                                   main_level='same scale as melody-clean', midi_note='MIDI contains notes only; WAV applies automation')
                              if args.revision == 'secondary-ducked' else None),
        timbre='25% pulse lead, triangle/pulse left hand; quiet short echo',
        status='audition only; not selected for firmware; manual entry requires listening review',
        right=right, left=left, secondary=secondary_notes, secondary_left=secondary_left,
    ), indent=2) + '\n', encoding='utf-8')
    print(f'Piano-score chip audition: {len(pcm)/RATE:.2f} s, bars 1-6 at {BPM} BPM')


if __name__ == '__main__':
    main()
