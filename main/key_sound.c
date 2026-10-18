#include "key_sound.h"
#include <math.h>

// A small Flash table keeps the streamed bell inexpensive on the ESP32-C3.
static const int16_t bell_sine[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
};

static int32_t bell_sine_at(uint32_t phase)
{
    unsigned index = phase >> 24;
    int32_t a = bell_sine[index], b = bell_sine[(index + 1U) & 255U];
    return a + (b - a) * (int32_t)((phase >> 8) & 65535U) / 65536;
}

void key_sound_render(int16_t pcm[KEY_SOUND_SAMPLES])
{
    const float seconds = (float)(KEY_SOUND_SAMPLES - 1U) / KEY_SOUND_HZ;
    for (unsigned i = 0; i < KEY_SOUND_SAMPLES; i++) {
        float t = (float)i / (KEY_SOUND_SAMPLES - 1U);
        float attack = t < 0.15f ? t / 0.15f : 1.0f;
        float decay = (1.0f - t) * (1.0f - t);
        // A brief 2.2 -> 1.5 kHz glide with a faint overtone gives a light tap.
        float phase = 6.2831853f * seconds * (2200.0f * t - 350.0f * t * t);
        float wave = 0.85f * sinf(phase) + 0.15f * sinf(2.0f * phase);
        pcm[i] = (int16_t)(3200.0f * attack * decay * wave);
    }
}

void key_sound_bell_render(int16_t *pcm, unsigned count, unsigned offset)
{
    for (unsigned i = 0; i < count; i++) {
        if (offset >= KEY_SOUND_BELL_SAMPLES || i >= KEY_SOUND_BELL_SAMPLES - offset) {
            pcm[i] = 0;
            continue;
        }
        uint32_t index = offset + i;
        uint32_t remaining = (KEY_SOUND_BELL_SAMPLES - 1U - index) * 32767U /
                             (KEY_SOUND_BELL_SAMPLES - 1U);
        uint32_t squared = remaining * remaining / 32767U;
        uint32_t decay = squared * squared / 32767U;
        uint32_t attack = index < 128U ? index * 32767U / 128U : 32767U;
        int32_t gain = (int32_t)(3200U * attack / 32767U * decay / 32767U);
        // 1046.5 Hz bell with two quiet, inharmonic partials; unsigned phase wraps.
        int32_t wave = (78 * bell_sine_at(index * 280917705U) +
                        16 * bell_sine_at(index * 775332865U) +
                         6 * bell_sine_at(index * 1123670819U)) / 100;
        pcm[i] = (int16_t)(gain * wave / 32767);
    }
}

void key_sound_chime_render(int16_t *pcm, unsigned count, unsigned offset)
{
    static const uint32_t step[3] = {
        (uint32_t)((880ULL * 4294967296ULL + 8000ULL) / 16000ULL),
        (uint32_t)((1175ULL * 4294967296ULL + 8000ULL) / 16000ULL),
        (uint32_t)((1568ULL * 4294967296ULL + 8000ULL) / 16000ULL),
    };
    for (unsigned i = 0; i < count; i++) {
        if (offset >= KEY_SOUND_CHIME_SAMPLES || i >= KEY_SOUND_CHIME_SAMPLES-offset) {
            pcm[i] = 0;
            continue;
        }
        unsigned at = offset+i, part = at < 2240U ? 0 : at < 4480U ? 1 : 2;
        unsigned k = at - (part == 0 ? 0U : part == 1 ? 2240U : 4480U);
        unsigned total = part < 2 ? 2240U : 5120U;
        uint32_t attack = k < 160U ? k * 32767U / 160U : 32767U;
        uint32_t decay = (total-k) * 32767U / total;
        uint32_t gain = 5000U * (attack < decay ? attack : decay) / 32767U;
        pcm[i] = (int16_t)(bell_sine_at(k*step[part]) * (int32_t)gain / 32767);
    }
}
