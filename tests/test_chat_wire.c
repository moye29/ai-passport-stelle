#include "chat_wire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    uint8_t nonce[16]={1}, packet[20]={0}, key[16];
    assert(chat_wire_hex16("00112233445566778899aAbBcCdDeEfF", key));
    assert(key[15]==255);
    assert(!chat_wire_hex16("00112233445566778899aabbccddeezz", key));
    assert(!chat_wire_hex16("001122", key));
    chat_wire_header(packet, nonce, 4, 1);
    uint32_t seq=0;
    assert(chat_wire_accept(packet, 20, 0, &seq) && seq==1);
    assert(!chat_wire_accept(packet, 20, 1, &seq));
    assert(!chat_wire_accept(packet, 19, 0, &seq));
    assert(!chat_wire_accept(packet, 15, 0, &seq));
    packet[0]=2; assert(!chat_wire_accept(packet,20,0,&seq));
    puts("official UDP header, truncation, ordering and key validation: PASS");
}
