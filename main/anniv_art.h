// Pure bounded decoder for the user-provided anniversary frames.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ANNIV_FRAME_W 216U
#define ANNIV_FRAME_H 136U
#define ANNIV_FRAME_COUNT 8U
#define ANNIV_FRAME_MS 200U
unsigned anniv_art_frame_at(uint32_t elapsed_ms);
typedef struct {
    const uint16_t *palette;
    size_t palette_count;
    const uint8_t *base;
    size_t base_size;
    const uint8_t *delta;
    size_t delta_size;
    const uint32_t *row_offsets;
    size_t row_count;
} anniv_art_pack_t;
bool anniv_art_decode_row(const anniv_art_pack_t *pack, unsigned frame, unsigned y,
                          uint16_t *out, size_t capacity);
