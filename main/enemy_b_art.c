#include "enemy_b_art.h"

bool enemy_b_decode_row(const enemy_b_art_pack_t *p,unsigned frame,unsigned y,
                       uint16_t *color,uint8_t *alpha,size_t capacity)
{
    if(!p || !p->data || !p->rows || !color || !alpha || frame>=ENEMY_B_FRAMES ||
       y>=ENEMY_B_H || capacity<ENEMY_B_W || p->row_count<ENEMY_B_FRAMES*(ENEMY_B_H+1U))return false;
    size_t row=frame*(ENEMY_B_H+1U)+y,at=p->rows[row],end=p->rows[row+1];
    if(at>end || end>p->data_size)return false;
    unsigned x=0;
    while(at<end) {
        unsigned op=p->data[at++],kind=op>>6,count=(op&63U)+1U;
        if(count>ENEMY_B_W-x)return false;
        size_t bytes=kind==0 ? 0 : kind==1 ? count*2U : kind==2 ? 3U : count*3U;
        if(bytes>end-at)return false;
        for(unsigned i=0;i<count;i++) {
            if(kind==0){color[x]=0;alpha[x]=0;}
            else {
                size_t pos=at+(kind==2 ? 0U : i*(kind==1 ? 2U : 3U));
                color[x]=(uint16_t)((unsigned)p->data[pos]|(unsigned)p->data[pos+1]<<8);
                alpha[x]=kind==1 ? 255U : p->data[pos+2];
            }
            x++;
        }
        at+=bytes;
    }
    return x==ENEMY_B_W;
}
