#include "chat_journal.h"
#include <string.h>

#define MAGIC 0x53485443U
#define COMMIT 0x54494D43U
_Static_assert(sizeof(chat_entry_t)==769,"Flash entry layout must remain byte-sized role plus768 text bytes");
_Static_assert(CHAT_JOURNAL_SLOTS>CHAT_HISTORY_SAVED_COUNT,"Never erase a live record");
static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t v)
{ for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8)); }
static uint32_t crc_step(uint32_t crc,const unsigned char *p,unsigned n)
{
    while(n--) { crc^=*p++;for(unsigned k=0;k<8;k++)crc=(crc>>1)^((crc&1)?0xEDB88320U:0); }
    return crc;
}
static uint32_t crc(const unsigned char *p)
{ return ~crc_step(crc_step(~0U,p,16),p+CHAT_JOURNAL_HEADER,sizeof(chat_entry_t)); }
bool chat_journal_record_valid(const unsigned char *p,uint32_t *seq)
{
    if(get32(p)!=MAGIC || get32(p+4)!=1 || get32(p+28)!=COMMIT || get32(p+16)!=crc(p))return false;
    unsigned len=get32(p+12);
    const unsigned char *entry=p+CHAT_JOURNAL_HEADER;
    if(entry[0]>1 || !len || len>=CHAT_TEXT_BYTES || entry[1+len]!=0 ||
       memchr(entry+1,0,len)!=NULL)return false;
    if(seq)*seq=get32(p+8);
    return true;
}
static bool newer(uint32_t a,uint32_t b) { return a!=b && (uint32_t)(a-b)<0x80000000U; }
static void remember(chat_journal_t *j,unsigned slot,uint32_t seq)
{
    if(j->count==CHAT_HISTORY_SAVED_COUNT && !newer(seq,j->sequence[0]))return;
    if(j->count==CHAT_HISTORY_SAVED_COUNT) {
        memmove(j->slots,j->slots+1,CHAT_HISTORY_SAVED_COUNT-1);
        memmove(j->sequence,j->sequence+1,(CHAT_HISTORY_SAVED_COUNT-1)*sizeof(uint32_t));j->count--;
    }
    unsigned i=j->count;
    while(i && newer(j->sequence[i-1],seq)) {
        j->sequence[i]=j->sequence[i-1];j->slots[i]=j->slots[i-1];i--;
    }
    j->sequence[i]=seq;j->slots[i]=(uint8_t)slot;j->count++;
}
bool chat_journal_open(chat_journal_t *j,const chat_journal_io_t *io,chat_journal_workspace_t *work)
{
    memset(j,0,sizeof(*j));j->io=*io;j->work=work;
    for(unsigned slot=0;slot<CHAT_JOURNAL_SLOTS;slot++) {
        if(!io->read(io->ctx,slot*CHAT_JOURNAL_SECTOR,work->record,CHAT_JOURNAL_RECORD))return false;
        uint32_t seq;if(chat_journal_record_valid(work->record,&seq))remember(j,slot,seq);
    }
    j->next=j->count ? (uint8_t)((j->slots[j->count-1]+1)%CHAT_JOURNAL_SLOTS) : 0;
    j->ready=true;return true;
}
bool chat_journal_slot_live(const chat_journal_t *j,unsigned slot)
{ for(unsigned i=0;i<j->count;i++)if(j->slots[i]==slot)return true;return false; }
bool chat_journal_append(chat_journal_t *j,const chat_entry_t *entry)
{
    if(!j->ready || !entry->text[0] || chat_journal_slot_live(j,j->next))return false;
    unsigned slot=j->next,off=slot*CHAT_JOURNAL_SECTOR;
    uint32_t seq=j->count ? j->sequence[j->count-1]+1U : 1U;
    // Mark state unavailable before I/O: callers must reopen after any failure.
    j->ready=false;
    if(!j->io.erase(j->io.ctx,off,CHAT_JOURNAL_SECTOR))return false;
    unsigned char *p=j->work->record;memset(p,255,CHAT_JOURNAL_RECORD);
    put32(p,MAGIC);put32(p+4,1);put32(p+8,seq);
    unsigned char *payload=p+CHAT_JOURNAL_HEADER;memset(payload,0,sizeof(chat_entry_t));payload[0]=entry->assistant?1:0;
    size_t len=chat_utf8_copy((char *)payload+1,CHAT_TEXT_BYTES,entry->text);
    put32(p+12,(uint32_t)len);put32(p+16,crc(p));
    if(!j->io.write(j->io.ctx,off,p,CHAT_JOURNAL_RECORD))return false;
    unsigned char committed[4];put32(committed,COMMIT);
    if(!j->io.write(j->io.ctx,off+28,committed,4) ||
       !j->io.read(j->io.ctx,off,p,CHAT_JOURNAL_RECORD) || !chat_journal_record_valid(p,NULL))return false;
    remember(j,slot,seq);j->next=(uint8_t)((slot+1)%CHAT_JOURNAL_SLOTS);j->ready=true;
    return true;
}
