#include "enemy_b_art.h"
#include <assert.h>
#include <stdio.h>
#include "enemy_b_pack.h"
int main(int argc,char **argv)
{
    uint8_t data[256]={0};size_t n=0;
    data[n++]=63; // transparent 64
    data[n++]=127;for(unsigned i=0;i<64;i++){data[n++]=0x34;data[n++]=0x12;}
    data[n++]=191;data[n++]=0x78;data[n++]=0x56;data[n++]=128;
    data[n++]=215;for(unsigned i=0;i<24;i++){data[n++]=0xBC;data[n++]=0x9A;data[n++]=32;}
    uint32_t rows[2*147]={0};rows[1]=(uint32_t)n;
    enemy_b_art_pack_t p={data,n,rows,2*147};
    uint16_t colors[216];uint8_t alpha[216];
    assert(enemy_b_decode_row(&p,0,0,colors,alpha,216));
    assert(colors[0]==0 && alpha[0]==0);
    assert(colors[64]==0x1234 && alpha[64]==255);
    assert(colors[128]==0x5678 && alpha[128]==128);
    assert(colors[215]==0x9ABC && alpha[215]==32);
    assert(!enemy_b_decode_row(&p,2,0,colors,alpha,216));
    assert(!enemy_b_decode_row(&p,0,146,colors,alpha,216));
    assert(!enemy_b_decode_row(&p,0,0,colors,alpha,215));
    p.data_size=1;assert(!enemy_b_decode_row(&p,0,0,colors,alpha,216));p.data_size=n;
    rows[1]=(uint32_t)n-1;assert(!enemy_b_decode_row(&p,0,0,colors,alpha,216));
    FILE *dump=argc==2 ? fopen(argv[1],"wb") : NULL;
    if(argc==2)assert(dump);
    for(unsigned frame=0;frame<2;frame++)for(unsigned y=0;y<146;y++) {
        assert(enemy_b_decode_row(&enemy_b_art_pack,frame,y,colors,alpha,216));
        if(dump){assert(fwrite(colors,sizeof(colors),1,dump)==1);assert(fwrite(alpha,sizeof(alpha),1,dump)==1);}
    }
    if(dump)fclose(dump);
    puts("Enemy B row packet types: PASS");
}
