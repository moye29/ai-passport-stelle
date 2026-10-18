#include "anniv_bgm.h"
#include <string.h>

static const int steps[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
    60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
    337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,
    1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,
    4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int adjust[8] = {-1,-1,-1,-1,2,4,6,8};
static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int predictor(const uint8_t *p)
{
    unsigned v = p[0] | (unsigned)p[1] << 8;
    return v < 32768 ? (int)v : (int)v - 65536;
}

uint32_t anniv_bgm_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc ^ 0xffffffffU;
}

bool anniv_bgm_bind(anniv_bgm_t *b, const uint8_t *pack, size_t size)
{
    memset(b, 0, sizeof(*b));
    if (!pack || size < 32 || memcmp(pack, "SBGM", 4) || u32(pack + 4) != 1 ||
        u32(pack + 8) != 12000 || u32(pack + 16) != 124) return false;
    uint32_t count = u32(pack + 12);
    if (!count || count > (size - 32) / 124 || size != 32 + (size_t)count * 124) return false;
    if (anniv_bgm_crc32(pack + 32, size - 32) != u32(pack + 20)) return false;
    for (uint32_t i = 0; i < count; i++)
        if (pack[32 + (size_t)i * 124 + 2] > 88 || pack[32 + (size_t)i * 124 + 3]) return false;
    b->data = pack + 32;
    b->blocks = count;
    return true;
}

void anniv_bgm_sync(anniv_bgm_t *b, uint32_t session, bool wanted)
{
    if (session != b->session) {
        b->session = session;
        b->block = 0;
        b->gain = 0;
    }
    b->wanted = wanted && b->data != NULL;
}

bool anniv_bgm_render(anniv_bgm_t *b, int16_t pcm[ANNIV_BGM_SAMPLES])
{
    if (!b->data || (!b->wanted && !b->gain)) return false;
    const uint8_t *p = b->data + (size_t)b->block * 124;
    int value = predictor(p), index = p[2];
    int16_t source[241];
    source[0] = (int16_t)value;
    for (unsigned i = 1; i < 240; i++) {
        unsigned shift = ((i - 1) & 1U) * 4U;
        unsigned code = (p[4 + (i - 1) / 2] >> shift) & 15U;
        int step = steps[index], delta = step >> 3;
        if (code & 1) delta += step >> 2;
        if (code & 2) delta += step >> 1;
        if (code & 4) delta += step;
        value += (code & 8) ? -delta : delta;
        if (value < -32768) value = -32768;
        if (value > 32767) value = 32767;
        index += adjust[code & 7];
        if (index < 0) index = 0;
        if (index > 88) index = 88;
        source[i] = (int16_t)value;
    }
    b->block = (b->block + 1U) % b->blocks;
    source[240] = (int16_t)predictor(b->data + (size_t)b->block * 124);
    unsigned target = b->wanted ? ANNIV_BGM_SAMPLES : 0;
    for (unsigned i = 0; i < ANNIV_BGM_SAMPLES; i++) {
        unsigned pos = i * 3U, at = pos / 4U, fraction = pos % 4U;
        int sample = (source[at] * (int)(4 - fraction) + source[at + 1] * (int)fraction) / 4;
        pcm[i] = (int16_t)(sample * (int)b->gain / (int)ANNIV_BGM_SAMPLES);
        if (b->gain < target) b->gain++;
        else if (b->gain > target) b->gain--;
    }
    return true;
}
