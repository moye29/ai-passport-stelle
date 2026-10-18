#include "chat_store.h"
#include <string.h>
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_timer.h"

static const esp_partition_t *s_partition;
static const unsigned char *s_map;
static esp_partition_mmap_handle_t s_mapping;
static chat_journal_t s_journal;
static unsigned s_prepare;
static bool s_ok;
static const char *TAG="chat_store";

static bool read_bytes(void *ctx,unsigned off,void *out,unsigned n)
{ return esp_partition_read(ctx,off,out,n)==ESP_OK; }
static bool write_bytes(void *ctx,unsigned off,const void *in,unsigned n)
{ return esp_partition_write(ctx,off,in,n)==ESP_OK; }
static bool blank_sector(unsigned slot)
{
    const unsigned char *p=s_map+slot*CHAT_JOURNAL_SECTOR;
    for(unsigned i=0;i<CHAT_JOURNAL_SECTOR;i++)if(p[i]!=255)return false;
    return true;
}
static bool erase_sector(void *ctx,unsigned off,unsigned n)
{
    if(off%CHAT_JOURNAL_SECTOR || n!=CHAT_JOURNAL_SECTOR || off>=CHAT_JOURNAL_BYTES)return false;
    return blank_sector(off/CHAT_JOURNAL_SECTOR) || esp_partition_erase_range(ctx,off,n)==ESP_OK;
}
static void publish(chat_history_t *history)
{
    const chat_entry_t *refs[CHAT_HISTORY_SAVED_COUNT];
    for(unsigned i=0;i<s_journal.count;i++)
        refs[i]=(const chat_entry_t *)(const void *)(s_map+s_journal.slots[i]*CHAT_JOURNAL_SECTOR+CHAT_JOURNAL_HEADER);
    chat_history_set_saved(history,refs,s_journal.count);
}
bool chat_store_open(chat_history_t *history,chat_journal_workspace_t *work)
{
    s_ok=false;
    s_partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,(esp_partition_subtype_t)0x41,"chatlog");
    if(!s_partition || s_partition->size<CHAT_JOURNAL_BYTES)return false;
    const void *map=NULL;
    if(esp_partition_mmap(s_partition,0,CHAT_JOURNAL_BYTES,ESP_PARTITION_MMAP_DATA,&map,&s_mapping)!=ESP_OK)return false;
    s_map=map;
    chat_journal_io_t io={(void *)s_partition,read_bytes,write_bytes,erase_sector};
    if(!chat_journal_open(&s_journal,&io,work)) {
        esp_partition_munmap(s_mapping);s_map=NULL;return false;
    }
    s_ok=true;s_prepare=s_journal.next;publish(history);
    ESP_LOGI(TAG,"restored %u saved messages",(unsigned)s_journal.count);
    return true;
}
bool chat_store_append(chat_history_t *history,const chat_entry_t *entry)
{
    if(!entry || !entry->text[0])return true;
    if(!s_ok)return false;
    int64_t started=esp_timer_get_time();
    if(!chat_journal_append(&s_journal,entry)) {
        // Keep existing Flash references valid. Main materializes a bounded RAM
        // fallback; this boot does not retry erasing after an uncertain write.
        s_ok=false;ESP_LOGW(TAG,"message save failed; prior committed records retained");return false;
    }
    s_prepare=s_journal.next;publish(history);
    ESP_LOGI(TAG,"committed count=%u in %lld us",(unsigned)s_journal.count,(long long)(esp_timer_get_time()-started));
    return true;
}
void chat_store_prepare(void)
{
    if(!s_ok)return;
    // One sector per idle platform tick spreads erase latency between turns.
    unsigned slot=s_prepare;s_prepare=(s_prepare+1)%CHAT_JOURNAL_SLOTS;
    if(chat_journal_slot_live(&s_journal,slot) || blank_sector(slot))return;
    if(esp_partition_erase_range(s_partition,slot*CHAT_JOURNAL_SECTOR,CHAT_JOURNAL_SECTOR)!=ESP_OK)
        ESP_LOGW(TAG,"idle history sector preparation failed");
}
