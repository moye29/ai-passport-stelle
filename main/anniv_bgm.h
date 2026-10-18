// Bounded, allocation-free IMA playback at 12 kHz, resampled to the 16 kHz codec.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ANNIV_BGM_SAMPLES 320U // 20 ms at 16 kHz
typedef struct {
    const uint8_t *data;
    uint32_t blocks, block, session;
    unsigned gain;
    bool wanted;
} anniv_bgm_t;

uint32_t anniv_bgm_crc32(const uint8_t *data, size_t size);
bool anniv_bgm_bind(anniv_bgm_t *b, const uint8_t *pack, size_t size);
void anniv_bgm_sync(anniv_bgm_t *b, uint32_t session, bool wanted);
// False when stopped/paused/unavailable. Does not advance a paused stream.
bool anniv_bgm_render(anniv_bgm_t *b, int16_t pcm[ANNIV_BGM_SAMPLES]);
