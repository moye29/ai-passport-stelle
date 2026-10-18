#include "asset_pack.h"

#include <string.h>

#define HEADER_SIZE 32U

static lv_image_dsc_t s_desc[STELLE_PACK_IMAGES];
static bool s_ready;
static lv_image_dsc_t s_trailblaze[TRAILBLAZE_PACK_COUNT];
static bool s_trailblaze_ready;

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

uint32_t asset_pack_crc32(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static bool built;
    if (!built) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1U) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        built = true;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

asset_pack_result_t asset_pack_bind(const uint8_t *base, size_t len)
{
    s_ready = false;
    if (!base || len < STELLE_PACK_SIZE) return ASSET_PACK_MISSING;
    if (memcmp(base, "STLP", 4) != 0 || rd32(base + 4) != STELLE_PACK_VERSION || rd32(base + 8) != STELLE_PACK_SIZE ||
        rd32(base + 12) != STELLE_PACK_IMAGES || rd32(base + 16) != STELLE_PACK_CRC)
        return ASSET_PACK_BAD_HEADER;
    if (asset_pack_crc32(base + HEADER_SIZE, STELLE_PACK_SIZE - HEADER_SIZE) != STELLE_PACK_CRC) return ASSET_PACK_BAD_CRC;
    for (unsigned i = 0; i < STELLE_PACK_IMAGES; i++) {
        const stelle_pack_image_t *im = &stelle_pack_images[i];
        if (im->offset < HEADER_SIZE || im->offset + im->size > STELLE_PACK_SIZE) return ASSET_PACK_BAD_HEADER;
        lv_image_dsc_t *d = &s_desc[i];
        memset(d, 0, sizeof(*d));
        d->header.magic = LV_IMAGE_HEADER_MAGIC;
        d->header.cf = im->is_sprite ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
        d->header.w = im->w;
        d->header.h = im->h;
        d->header.stride = (uint32_t)im->w * 2U;
        d->data_size = im->size;
        d->data = base + im->offset;
    }
    s_ready = true;
    return ASSET_PACK_OK;
}

bool asset_pack_ready(void) { return s_ready; }

const char *asset_pack_result_name(asset_pack_result_t r)
{
    static const char *const names[] = {"ok", "missing", "bad header", "bad crc"};
    return r <= ASSET_PACK_BAD_CRC ? names[r] : "unknown";
}

stelle_frame_t stelle_frame_at(stelle_state_t state, uint32_t elapsed_ms)
{
    stelle_frame_t f = {NULL, 0, 0};
    if (!s_ready) return f;
    if ((unsigned)state >= STELLE_STATE_COUNT) state = (stelle_state_t)0;
    const stelle_pack_state_t *s = &stelle_pack_states[state];
    uint32_t t = s->cycle_ms ? elapsed_ms % s->cycle_ms : 0;
    unsigned i = 0;
    while (i + 1U < s->count && t >= s->durations[i]) t -= s->durations[i++];
    unsigned index = s->first + i;
    f.image = &s_desc[index];
    f.ox = stelle_pack_images[index].ox;
    f.oy = stelle_pack_images[index].oy;
    return f;
}

const lv_image_dsc_t *asset_room(unsigned scene, unsigned phase)
{
    unsigned scenes = sizeof(stelle_pack_rooms) / sizeof(stelle_pack_rooms[0]);
    unsigned phases = sizeof(stelle_pack_rooms[0]) / sizeof(stelle_pack_rooms[0][0]);
    if (!s_ready || scene >= scenes || phase >= phases) return NULL;
    return &s_desc[stelle_pack_rooms[scene][phase]];
}

const lv_image_dsc_t *asset_anniv(void)
{
    return s_ready ? &s_desc[STELLE_PACK_ANNIV] : NULL;
}

const lv_image_dsc_t *asset_trailblaze_preview(void)
{
    return s_ready ? &s_desc[STELLE_PACK_TRAILBLAZE_PREVIEW] : NULL;
}

asset_pack_result_t asset_trailblaze_bind(const uint8_t *base, size_t len)
{
    s_trailblaze_ready = false;
    if (!base || len < TRAILBLAZE_PACK_SIZE) return ASSET_PACK_MISSING;
    if (memcmp(base, "STLT", 4) || rd32(base + 4) != 1 || rd32(base + 8) != TRAILBLAZE_PACK_SIZE ||
        rd32(base + 12) != TRAILBLAZE_PACK_COUNT || rd32(base + 20) != TRAILBLAZE_PACK_W ||
        rd32(base + 24) != TRAILBLAZE_PACK_H) return ASSET_PACK_BAD_HEADER;
    if (asset_pack_crc32(base + HEADER_SIZE, TRAILBLAZE_PACK_SIZE - HEADER_SIZE) != rd32(base + 16))
        return ASSET_PACK_BAD_CRC;
    for (unsigned i = 0; i < TRAILBLAZE_PACK_COUNT; i++) {
        lv_image_dsc_t *d = &s_trailblaze[i];
        memset(d, 0, sizeof(*d));
        d->header.magic = LV_IMAGE_HEADER_MAGIC;
        d->header.cf = LV_COLOR_FORMAT_RGB565;
        d->header.w = TRAILBLAZE_PACK_W;
        d->header.h = TRAILBLAZE_PACK_H;
        d->header.stride = TRAILBLAZE_PACK_W * 2U;
        d->data_size = TRAILBLAZE_PACK_W * TRAILBLAZE_PACK_H * 2U;
        d->data = base + HEADER_SIZE + i * d->data_size;
    }
    s_trailblaze_ready = true;
    return ASSET_PACK_OK;
}

const lv_image_dsc_t *asset_trailblaze(unsigned index)
{
    return s_trailblaze_ready && index < TRAILBLAZE_PACK_COUNT ? &s_trailblaze[index] : NULL;
}
