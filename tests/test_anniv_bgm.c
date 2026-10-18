#include "anniv_bgm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

int main(int argc, char **argv)
{
    if (argc == 3) {
        FILE *in = fopen(argv[1], "rb"); assert(in);
        assert(fseek(in, 0, SEEK_END) == 0);
        long bytes = ftell(in); assert(bytes > 32);
        rewind(in);
        uint8_t *data = malloc((size_t)bytes); assert(data);
        assert(fread(data, 1, (size_t)bytes, in) == (size_t)bytes); fclose(in);
        anniv_bgm_t b; assert(anniv_bgm_bind(&b, data, (size_t)bytes));
        anniv_bgm_sync(&b, 1, true); b.gain = ANNIV_BGM_SAMPLES;
        FILE *out = fopen(argv[2], "wb"); assert(out);
        for (uint32_t i = 0; i < b.blocks; i++) {
            int16_t pcm[ANNIV_BGM_SAMPLES]; assert(anniv_bgm_render(&b, pcm));
            assert(fwrite(pcm, sizeof(pcm), 1, out) == 1);
        }
        fclose(out); free(data);
    }
    uint8_t pack[32 + 2 * 124] = {0};
    memcpy(pack, "SBGM", 4);
    put32(pack + 4, 1); put32(pack + 8, 12000);
    put32(pack + 12, 2); put32(pack + 16, 124);
    // Independent blocks with a constant non-zero predictor and tiny deltas.
    pack[32] = 0xe8; pack[33] = 3;
    pack[156] = 0xd0; pack[157] = 7;
    put32(pack + 20, anniv_bgm_crc32(pack + 32, sizeof(pack) - 32));
    anniv_bgm_t b = {0}; int16_t pcm[ANNIV_BGM_SAMPLES];
    assert(anniv_bgm_bind(&b, pack, sizeof(pack)));
    assert(!anniv_bgm_render(&b, pcm));
    anniv_bgm_sync(&b, 1, true);
    assert(anniv_bgm_render(&b, pcm) && b.block == 1);
    assert(pcm[0] == 0 && pcm[200] > 0);
    assert(anniv_bgm_render(&b, pcm) && b.block == 0); // wraps forever
    anniv_bgm_sync(&b, 1, false);
    assert(anniv_bgm_render(&b, pcm)); // short fade out
    uint32_t paused_at = b.block;
    assert(!anniv_bgm_render(&b, pcm) && b.block == paused_at);
    anniv_bgm_sync(&b, 1, true);
    assert(b.block == paused_at); // resume retains position
    assert(anniv_bgm_render(&b, pcm));
    anniv_bgm_sync(&b, 2, true);
    assert(b.block == 0 && b.gain == 0); // a new visit always starts at zero
    pack[34] = 89; // invalid IMA step index, even with a matching CRC
    put32(pack + 20, anniv_bgm_crc32(pack + 32, sizeof(pack) - 32));
    assert(!anniv_bgm_bind(&b, pack, sizeof(pack)));
    assert(!anniv_bgm_render(&b, pcm));
    pack[34] = 0; // stale CRC
    assert(!anniv_bgm_bind(&b, pack, sizeof(pack)));
    assert(!anniv_bgm_bind(&b, pack, 12));
    puts("Anniversary BGM: loop, pause/resume, restart, fades and corrupt resource: PASS");
    return 0;
}
