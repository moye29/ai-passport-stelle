#include "anniv_art.h"
#include <string.h>
unsigned anniv_art_frame_at(uint32_t elapsed_ms)
{
    return (elapsed_ms / ANNIV_FRAME_MS) % ANNIV_FRAME_COUNT;
}

bool anniv_art_decode_row(const anniv_art_pack_t *p, unsigned frame, unsigned y,
                          uint16_t *out, size_t capacity)
{
    if (!p || !out || !p->palette || !p->base || !p->palette_count || p->palette_count>256U ||
        frame>=ANNIV_FRAME_COUNT || y>=ANNIV_FRAME_H || capacity<ANNIV_FRAME_W ||
        p->base_size<ANNIV_FRAME_W*ANNIV_FRAME_H) return false;
    uint8_t indices[ANNIV_FRAME_W];
    memcpy(indices,p->base+y*ANNIV_FRAME_W,sizeof(indices));
    if (frame) {
        if (!p->row_offsets || !p->delta || p->row_count<ANNIV_FRAME_COUNT*(ANNIV_FRAME_H+1U)) return false;
        size_t row=frame*(ANNIV_FRAME_H+1U)+y;
        size_t at=p->row_offsets[row], end=p->row_offsets[row+1];
        if (at>end || end>p->delta_size) return false;
        unsigned x=0;
        while (at<end) {
            unsigned op=p->delta[at++], count=(op&127U)+1U;
            if (count>ANNIV_FRAME_W-x) return false;
            if (op&128U) {
                if (count>end-at) return false;
                memcpy(indices+x,p->delta+at,count);at+=count;
            }
            x+=count;
        }
        if (x!=ANNIV_FRAME_W) return false;
    }
    for (unsigned x=0;x<ANNIV_FRAME_W;x++) {
        if (indices[x]>=p->palette_count) return false;
        out[x]=p->palette[indices[x]];
    }
    return true;
}
