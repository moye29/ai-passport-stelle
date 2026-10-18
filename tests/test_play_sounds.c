#include "play_sounds.h"
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
        long bytes = ftell(in); assert(bytes > 64); rewind(in);
        uint8_t *data = malloc((size_t)bytes); assert(data);
        assert(fread(data, 1, (size_t)bytes, in) == (size_t)bytes); fclose(in);
        play_sounds_t s; assert(play_sounds_bind(&s, data, (size_t)bytes));
        FILE *out = fopen(argv[2], "wb"); assert(out);
        for (unsigned i = 0; i < 4; i++) {
            assert(play_sounds_start(&s) == (int)(i % 3));
            int16_t pcm[ANNIV_BGM_SAMPLES];
            while (play_sounds_render(&s, pcm)) assert(fwrite(pcm, sizeof(pcm), 1, out) == 1);
        }
        fclose(out); free(data);
    }
    uint8_t pack[64 + 3 * 156] = {0};
    memcpy(pack, "STFX", 4); put32(pack+4, 1); put32(pack+8, sizeof(pack)); put32(pack+12, 3);
    for (unsigned i = 0; i < 3; i++) {
        unsigned offset = 64 + i * 156;
        put32(pack+32+i*8, offset); put32(pack+36+i*8, 156);
        uint8_t *clip = pack + offset;
        memcpy(clip, "SBGM", 4); put32(clip+4, 1); put32(clip+8, 12000);
        put32(clip+12, 1); put32(clip+16, 124);
        unsigned value = (i+1)*1000;
        clip[32] = (uint8_t)value; clip[33] = (uint8_t)(value >> 8);
        put32(clip+20, anniv_bgm_crc32(clip+32, 124));
    }
    put32(pack+16, anniv_bgm_crc32(pack+32, sizeof(pack)-32));
    play_sounds_t sounds;
    int16_t pcm[ANNIV_BGM_SAMPLES];
    assert(play_sounds_bind(&sounds, pack, sizeof(pack)));
    assert(!play_sounds_render(&sounds, pcm));
    for (unsigned i = 0; i < 7; i++) {
        assert(play_sounds_start(&sounds) == (int)(i % 3));
        assert(play_sounds_render(&sounds, pcm));
        assert(pcm[0] == 0 && pcm[200] == (int)((i%3+1)*1000*200/320));
        assert(!play_sounds_render(&sounds, pcm)); // one clip, never a loop
    }
    assert(play_sounds_start(&sounds) == 1);
    play_sounds_stop(&sounds);
    assert(!play_sounds_render(&sounds, pcm));
    assert(play_sounds_start(&sounds) == 2); // cancellation does not reset rotation
    pack[sizeof(pack)-1] ^= 1;
    assert(!play_sounds_bind(&sounds, pack, sizeof(pack)));
    assert(play_sounds_start(&sounds) == -1);
    assert(!play_sounds_render(&sounds, pcm));
    assert(!play_sounds_bind(&sounds, pack, 10));
    puts("Play sounds: ordered 1/2/3 rotation, one-shot stop, cancellation and corrupt pack: PASS");
    return 0;
}
