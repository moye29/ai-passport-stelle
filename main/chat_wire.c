#include "chat_wire.h"
#include <string.h>
bool chat_wire_hex16(const char *text, uint8_t out[16])
{
    if (strlen(text)!=32) return false;
    uint8_t result[16];
    for (unsigned i=0;i<16;i++) {
        unsigned a=0;
        for (unsigned j=0;j<2;j++) {
            unsigned char c=(unsigned char)text[2*i+j];
            unsigned v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;
            if (v>15) return false;
            a=a*16+v;
        }
        result[i]=(uint8_t)a;
    }
    memcpy(out,result,16); return true;
}
void chat_wire_header(uint8_t out[16],const uint8_t nonce[16],uint16_t size,uint32_t sequence)
{
    memcpy(out,nonce,16);
    out[2]=(uint8_t)(size>>8); out[3]=(uint8_t)size;
    out[12]=(uint8_t)(sequence>>24); out[13]=(uint8_t)(sequence>>16);
    out[14]=(uint8_t)(sequence>>8); out[15]=(uint8_t)sequence;
}
bool chat_wire_accept(const uint8_t *p,size_t bytes,uint32_t previous,uint32_t *sequence)
{
    if (bytes<16 || p[0]!=1) return false;
    size_t len=(size_t)p[2]*256+p[3];
    uint32_t seq=(uint32_t)p[12]<<24 | (uint32_t)p[13]<<16 | (uint32_t)p[14]<<8 | p[15];
    if (len==0 || bytes!=16+len || seq<=previous) return false;
    *sequence=seq; return true;
}
