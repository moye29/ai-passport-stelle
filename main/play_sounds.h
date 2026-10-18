// Pure bounded playback of the user's three sequential Bat Swing clips.
#pragma once
#include "anniv_bgm.h"

#define PLAY_SOUND_COUNT 3U
typedef struct {
    anniv_bgm_t clips[PLAY_SOUND_COUNT], stream;
    uint32_t remaining;
    unsigned next;
} play_sounds_t;

bool play_sounds_bind(play_sounds_t *s, const uint8_t *pack, size_t size);
bool play_sounds_ready(const play_sounds_t *s);
// Start exactly one clip and advance once. Returns 0..2, or -1 if unavailable.
int play_sounds_start(play_sounds_t *s);
bool play_sounds_render(play_sounds_t *s, int16_t pcm[ANNIV_BGM_SAMPLES]);
void play_sounds_stop(play_sounds_t *s);
