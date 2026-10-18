// main/asset_pack.h - Stelle's images from the resource partition (pure C + LVGL types).
//
// The pack (tools/make_asset_pack.py) holds the cropped sprite frames and the
// room backgrounds. The platform maps it read-only (esp_partition_mmap on the
// device, a file in the native preview) and binds it here once; the view then
// asks for frames and rooms. Nothing is copied: image data stays in Flash.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "stelle_pack.h"

typedef enum {
    ASSET_PACK_OK = 0,
    ASSET_PACK_MISSING,       // no partition / file, or too small
    ASSET_PACK_BAD_HEADER,    // magic, version or length do not match this firmware
    ASSET_PACK_BAD_CRC,       // contents corrupted or from another build
} asset_pack_result_t;

typedef struct {
    const lv_image_dsc_t *image;  // NULL when the pack is not bound
    int16_t ox, oy;               // where the cropped frame sits inside the full cell
} stelle_frame_t;

// Validate `len` bytes at `base` (header + CRC against the compiled-in index)
// and build the image descriptors. Safe to call again; on failure all lookups
// return NULL until a valid pack is bound.
asset_pack_result_t asset_pack_bind(const uint8_t *base, size_t len);
bool asset_pack_ready(void);
const char *asset_pack_result_name(asset_pack_result_t r);

stelle_frame_t stelle_frame_at(stelle_state_t state, uint32_t elapsed_ms);
const lv_image_dsc_t *asset_room(unsigned scene, unsigned phase);
const lv_image_dsc_t *asset_anniv(void);  // pixel-art Astral Express (anniversary page)
const lv_image_dsc_t *asset_trailblaze_preview(void);
// Separate uncompressed destination pack; descriptors point straight into Flash.
#define TRAILBLAZE_PACK_COUNT 12U
#define TRAILBLAZE_PACK_W 216U
#define TRAILBLAZE_PACK_H 146U
#define TRAILBLAZE_PACK_SIZE (32U + TRAILBLAZE_PACK_COUNT * TRAILBLAZE_PACK_W * TRAILBLAZE_PACK_H * 2U)
asset_pack_result_t asset_trailblaze_bind(const uint8_t *base, size_t len);
const lv_image_dsc_t *asset_trailblaze(unsigned index);

// CRC-32 (IEEE 802.3), exposed for tests.
uint32_t asset_pack_crc32(const uint8_t *data, size_t len);
