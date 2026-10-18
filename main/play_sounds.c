#include "play_sounds.h"
#include <string.h>

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool play_sounds_bind(play_sounds_t *s, const uint8_t *pack, size_t size)
{
    memset(s, 0, sizeof(*s));
    if (!pack || size < 64 || memcmp(pack, "STFX", 4) || u32(pack+4) != 1 ||
        u32(pack+8) != size || u32(pack+12) != PLAY_SOUND_COUNT ||
        anniv_bgm_crc32(pack+32, size-32) != u32(pack+16)) return false;
    anniv_bgm_t clips[PLAY_SOUND_COUNT];
    size_t expected = 64;
    for (unsigned i = 0; i < PLAY_SOUND_COUNT; i++) {
        uint32_t offset = u32(pack+32+i*8), bytes = u32(pack+36+i*8);
        if (offset != expected || offset > size || bytes > size-offset ||
            !anniv_bgm_bind(&clips[i], pack+offset, bytes)) return false;
        expected += bytes;
    }
    if (expected != size) return false;
    memcpy(s->clips, clips, sizeof(clips));
    return true;
}

bool play_sounds_ready(const play_sounds_t *s) { return s->clips[0].data != NULL; }

int play_sounds_start(play_sounds_t *s)
{
    if (!play_sounds_ready(s)) return -1;
    unsigned index = s->next;
    s->stream = s->clips[index];
    anniv_bgm_sync(&s->stream, 1, true);
    s->remaining = s->stream.blocks;
    s->next = (index+1) % PLAY_SOUND_COUNT;
    return (int)index;
}

bool play_sounds_render(play_sounds_t *s, int16_t pcm[ANNIV_BGM_SAMPLES])
{
    if (!s->remaining || !anniv_bgm_render(&s->stream, pcm)) return false;
    s->remaining--;
    return true;
}

void play_sounds_stop(play_sounds_t *s)
{
    s->remaining = 0;
    s->stream.wanted = false;
    s->stream.gain = 0;
}
