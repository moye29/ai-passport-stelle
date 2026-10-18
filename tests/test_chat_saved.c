#include "chat_history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static chat_entry_t entries[CHAT_HISTORY_SAVED_COUNT];
int main(void)
{
    chat_history_t h;chat_history_init(&h);
    const chat_entry_t *refs[CHAT_HISTORY_SAVED_COUNT];
    for (unsigned i=0;i<CHAT_HISTORY_SAVED_COUNT;i++) {
        snprintf(entries[i].text,sizeof(entries[i].text),"Saved %u",i);
        entries[i].assistant=i%2;refs[i]=&entries[i];
    }
    chat_history_set_saved(&h,refs,CHAT_HISTORY_SAVED_COUNT);
    assert(h.count==15 && h.external);
    assert(chat_history_get(&h,14)==&entries[14]);
    assert(chat_history_max_scroll(&h)>0);
    unsigned before=h.revision;
    chat_history_add(&h,false,"RAM fallback");
    assert(!h.external && h.count==12 && h.revision>before);
    assert(strcmp(chat_history_get(&h,0)->text,"Saved 4")==0);
    assert(strcmp(chat_history_get(&h,11)->text,"RAM fallback")==0);
    assert(strcmp(entries[14].text,"Saved 14")==0);
    puts("15 Flash references, continuous layout and bounded12-entry RAM fallback: PASS");
}
