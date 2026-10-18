#ifdef NDEBUG
#undef NDEBUG
#endif
#include "asset_pack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t pack[TRAILBLAZE_PACK_SIZE]; // host fixture only
static void word(unsigned off, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) pack[off + i] = (uint8_t)(value >> (i * 8U));
}

int main(void)
{
    assert(asset_trailblaze_bind(NULL, 0) == ASSET_PACK_MISSING);
    assert(!asset_trailblaze(0));
    memcpy(pack, "STLT", 4);
    word(4, 1); word(8, sizeof(pack)); word(12, TRAILBLAZE_PACK_COUNT);
    word(20, TRAILBLAZE_PACK_W); word(24, TRAILBLAZE_PACK_H);
    word(16, asset_pack_crc32(pack + 32, sizeof(pack) - 32));
    assert(asset_trailblaze_bind(pack, sizeof(pack) - 1) == ASSET_PACK_MISSING);
    assert(asset_trailblaze_bind(pack, sizeof(pack)) == ASSET_PACK_OK);
    for (unsigned i = 0; i < TRAILBLAZE_PACK_COUNT; i++) {
        const lv_image_dsc_t *image = asset_trailblaze(i);
        assert(image && image->header.w == 216 && image->header.h == 146);
        assert(image->header.stride == 432 && image->data_size == 63072);
        assert(image->data == pack + 32 + i * 63072U);
    }
    assert(!asset_trailblaze(TRAILBLAZE_PACK_COUNT));
    pack[32] = 1;
    assert(asset_trailblaze_bind(pack, sizeof(pack)) == ASSET_PACK_BAD_CRC);
    assert(!asset_trailblaze(0));
    pack[32] = 0; word(20, 217);
    assert(asset_trailblaze_bind(pack, sizeof(pack)) == ASSET_PACK_BAD_HEADER);
    word(20, 216);
    assert(asset_trailblaze_bind(pack, sizeof(pack)) == ASSET_PACK_OK);
    puts("Trailblaze Flash binding, bounds and corruption recovery: PASS");
    return 0;
}
