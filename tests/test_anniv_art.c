#include "anniv_art.h"
#include <assert.h>
#include <stdio.h>
#include "anniv_pack.h"
#include <string.h>
int main(int argc,char **argv)
{
    assert(anniv_art_frame_at(0)==0);
    assert(anniv_art_frame_at(199)==0);
    assert(anniv_art_frame_at(200)==1);
    assert(anniv_art_frame_at(1599)==7);
    assert(anniv_art_frame_at(1600)==0);
    assert(anniv_art_frame_at(4800)==0);
    uint8_t base[ANNIV_FRAME_W*ANNIV_FRAME_H]={0};
    uint16_t palette[]={0x1234,0xABCD}, out[ANNIV_FRAME_W];
    uint8_t delta[]={0x80,1,127,86}; // one changed pixel, then 128+87 shared pixels
    uint32_t rows[ANNIV_FRAME_COUNT*(ANNIV_FRAME_H+1)]={0};
    rows[ANNIV_FRAME_H+2]=sizeof(delta);
    anniv_art_pack_t pack={palette,2,base,sizeof(base),delta,sizeof(delta),rows,sizeof(rows)/sizeof(rows[0])};
    assert(anniv_art_decode_row(&pack,0,0,out,ANNIV_FRAME_W));
    assert(out[0]==0x1234 && out[215]==0x1234);
    assert(anniv_art_decode_row(&pack,1,0,out,ANNIV_FRAME_W));
    assert(out[0]==0xABCD && out[1]==0x1234 && out[215]==0x1234);
    assert(!anniv_art_decode_row(&pack,8,0,out,216));
    assert(!anniv_art_decode_row(&pack,0,136,out,216));
    assert(!anniv_art_decode_row(&pack,0,0,out,215));
    pack.delta_size=1;assert(!anniv_art_decode_row(&pack,1,0,out,216));pack.delta_size=sizeof(delta);
    delta[1]=2;assert(!anniv_art_decode_row(&pack,1,0,out,216));delta[1]=1;
    rows[ANNIV_FRAME_H+2]=3;assert(!anniv_art_decode_row(&pack,1,0,out,216));
    FILE *dump=argc==2 ? fopen(argv[1],"wb") : NULL;
    if (argc==2) assert(dump);
    for(unsigned frame=0;frame<8;frame++) for(unsigned y=0;y<136;y++) {
        assert(anniv_art_decode_row(&anniv_art_pack,frame,y,out,216));
        if(dump) assert(fwrite(out,sizeof(out),1,dump)==1);
    }
    if(dump)fclose(dump);
    puts("Anniversary: eight 200ms frames, 1600ms wrap: PASS");
}
