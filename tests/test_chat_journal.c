#include "chat_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char flash[CHAT_JOURNAL_BYTES],snapshot[CHAT_JOURNAL_BYTES];
static int budget=-1;
static bool rd(void *p,unsigned off,void *out,unsigned n)
{ (void)p;if(off+n>sizeof(flash))return false;memcpy(out,flash+off,n);return true; }
static bool wr(void *p,unsigned off,const void *src,unsigned n)
{
    (void)p;const unsigned char *s=src;
    for(unsigned i=0;i<n;i++) { if(budget==0)return false;if(budget>0)budget--;flash[off+i]&=s[i]; }
    return true;
}
static bool er(void *p,unsigned off,unsigned n)
{
    (void)p;for(unsigned i=0;i<n;i++) { if(budget==0)return false;if(budget>0)budget--;flash[off+i]=255; }
    return true;
}
static const chat_journal_io_t io={NULL,rd,wr,er};
static chat_journal_workspace_t work;
static chat_journal_t journal;
static void append(unsigned i)
{
    chat_entry_t e={.assistant=i%2};snprintf(e.text,sizeof(e.text),"Message %u",i);
    assert(chat_journal_append(&journal,&e));
}
static const chat_entry_t *entry(unsigned i)
{ return (const chat_entry_t *)(const void *)(flash+journal.slots[i]*CHAT_JOURNAL_SECTOR+CHAT_JOURNAL_HEADER); }
int main(void)
{
    memset(flash,255,sizeof(flash));assert(chat_journal_open(&journal,&io,&work));assert(journal.count==0);
    for(unsigned i=1;i<=64;i++)append(i);
    assert(journal.count==15 && strcmp(entry(0)->text,"Message 50")==0);
    assert(chat_journal_open(&journal,&io,&work));assert(strcmp(entry(14)->text,"Message 64")==0);
    memcpy(snapshot,flash,sizeof(flash));
    // Cut at every programmed byte, plus cuts inside erase. Previous15 remain.
    for(unsigned cut=0;cut<=CHAT_JOURNAL_RECORD+4;cut++) {
        memcpy(flash,snapshot,sizeof(flash));budget=-1;assert(chat_journal_open(&journal,&io,&work));
        budget=CHAT_JOURNAL_SECTOR+(int)cut;
        chat_entry_t e={0};strcpy(e.text,"New committed record");
        bool saved=chat_journal_append(&journal,&e);budget=-1;
        assert(chat_journal_open(&journal,&io,&work));assert(journal.count==15);
        assert(strcmp(entry(14)->text,saved ? "New committed record" : "Message 64")==0);
        assert(strcmp(entry(0)->text,saved ? "Message 51" : "Message 50")==0);
    }
    for(unsigned cut=0;cut<CHAT_JOURNAL_SECTOR;cut+=257) {
        memcpy(flash,snapshot,sizeof(flash));budget=-1;assert(chat_journal_open(&journal,&io,&work));
        budget=(int)cut;chat_entry_t e={0};strcpy(e.text,"torn erase");assert(!chat_journal_append(&journal,&e));budget=-1;
        assert(chat_journal_open(&journal,&io,&work));assert(strcmp(entry(14)->text,"Message 64")==0);
    }
    memcpy(flash,snapshot,sizeof(flash));assert(chat_journal_open(&journal,&io,&work));
    flash[journal.slots[14]*CHAT_JOURNAL_SECTOR+CHAT_JOURNAL_HEADER+8]^=1;
    assert(chat_journal_open(&journal,&io,&work));assert(strcmp(entry(14)->text,"Message 63")==0);
    // Seed a near-wrap counter without billions of writes; the seed has no
    // physical record and disappears at the next simulated reboot.
    memset(flash,255,sizeof(flash));assert(chat_journal_open(&journal,&io,&work));
    journal.count=1;journal.slots[0]=31;journal.sequence[0]=UINT32_MAX-3;journal.next=0;
    for(unsigned i=100;i<105;i++)append(i);
    assert(chat_journal_open(&journal,&io,&work));assert(journal.count==5);
    assert(strcmp(entry(0)->text,"Message 100")==0 && strcmp(entry(4)->text,"Message 104")==0);
    chat_entry_t unicode={.assistant=true};
    for(unsigned i=0;i<765;i+=3)memcpy(unicode.text+i,"\xe4\xb8\xad",3);
    assert(chat_journal_append(&journal,&unicode));assert(chat_journal_open(&journal,&io,&work));
    assert(strlen(entry(journal.count-1)->text)==765);
    puts("Flash journal:15 retained, wrap, reboot, every torn-program byte, torn erase and CRC recovery: PASS");
}
