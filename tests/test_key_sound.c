#include "key_sound.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void)
{
    static int16_t chime[9600], chunked[9600];
    assert(KEY_SOUND_CHIME_SAMPLES == 9600);
    key_sound_chime_render(chime, 9600, 0);
    for (unsigned off = 0; off < 9600; off += 160) key_sound_chime_render(chunked+off, 160, off);
    for (unsigned i = 0; i < 9600; i++) {
        assert(chime[i] == chunked[i]);
        unsigned start = i < 2240 ? 0 : i < 4480 ? 2240 : 4480;
        unsigned total = i < 4480 ? 2240 : 5120;
        unsigned k = i-start;
        float hz = i < 2240 ? 880.0f : i < 4480 ? 1175.0f : 1568.0f;
        float attack = (float)k / 160.0f, decay = (float)(total-k) / total;
        float env = attack < decay ? attack : decay;
        int reference = (int)(5000.0f * env * sinf(6.2831853f * hz * k / 16000));
        int error = chime[i]-reference;
        assert(error >= -8 && error <= 8);
    }
    puts("Integer streamed chime: original pitch/timing/envelope and exact chunk equivalence: PASS");
    static int16_t bell[KEY_SOUND_BELL_SAMPLES], streamed[KEY_SOUND_BELL_SAMPLES];
    key_sound_bell_render(bell, KEY_SOUND_BELL_SAMPLES, 0);
    assert(KEY_SOUND_BELL_SAMPLES == 1920);
    assert(bell[0] == 0 && bell[KEY_SOUND_BELL_SAMPLES - 1] == 0);
    long bell_early = 0, bell_late = 0;
    for (unsigned off = 0; off < KEY_SOUND_BELL_SAMPLES; off += 160) {
        key_sound_bell_render(streamed + off, 160, off);
        for (unsigned i = off; i < off + 160; i++) {
            assert(streamed[i] == bell[i]);
            int value = bell[i] < 0 ? -bell[i] : bell[i];
            assert(value <= 3200);
            if (i < KEY_SOUND_BELL_SAMPLES / 4) bell_early += value;
            if (i >= 3 * KEY_SOUND_BELL_SAMPLES / 4) bell_late += value;
        }
    }
    assert(bell_early > 100000 && bell_early > bell_late * 4);
    int16_t beyond[8];
    key_sound_bell_render(beyond, 8, KEY_SOUND_BELL_SAMPLES);
    for (unsigned i = 0; i < 8; i++) assert(beyond[i] == 0);
    int16_t pcm[KEY_SOUND_SAMPLES];
    key_sound_render(pcm);
    assert(KEY_SOUND_SAMPLES == 192); // 12 ms at the existing 16 kHz codec rate
    assert(pcm[0] == 0 && pcm[KEY_SOUND_SAMPLES - 1] == 0);
    unsigned early_crossings = 0, late_crossings = 0;
    long early_energy = 0, late_energy = 0;
    int peak = 0;
    for (unsigned i = 0; i < KEY_SOUND_SAMPLES; i++) {
        int magnitude = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (magnitude > peak) peak = magnitude;
        if (i < KEY_SOUND_SAMPLES / 3) early_energy += magnitude;
        if (i >= 2 * KEY_SOUND_SAMPLES / 3) late_energy += magnitude;
        if (i && ((pcm[i] > 0 && pcm[i-1] < 0) || (pcm[i] < 0 && pcm[i-1] > 0))) {
            if (i < KEY_SOUND_SAMPLES / 2) early_crossings++;
            else late_crossings++;
        }
    }
    assert(peak > 500 && peak < 3200); // audible envelope without clipping
    assert(early_energy > late_energy * 2); // light, decaying tap
    assert(early_crossings > late_crossings); // pitch falls, instead of a fixed buzzer
    puts("12 ms key feedback: silent endpoints, safe level, decay and falling pitch: PASS");
    puts("Single bell: 120 ms, safe level, short decay and exact 10 ms streaming: PASS");
    return 0;
}
