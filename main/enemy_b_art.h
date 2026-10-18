// Bounded lossless RGB565 + alpha row decoder for the two Enemy B poses.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define ENEMY_B_W 216U
#define ENEMY_B_H 146U
#define ENEMY_B_FRAMES 2U
typedef struct {
    const uint8_t *data;
    size_t data_size;
    const uint32_t *rows;
    size_t row_count;
} enemy_b_art_pack_t;
bool enemy_b_decode_row(const enemy_b_art_pack_t *p,unsigned frame,unsigned y,
                       uint16_t *color,uint8_t *alpha,size_t capacity);
