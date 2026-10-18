#include "chat_history.h"
#include <string.h>

static unsigned utf8_size(unsigned char c)
{
    return c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
}
size_t chat_utf8_copy(char *out, size_t size, const char *text)
{
    if (!size) return 0;
    size_t i = 0;
    while (text[i]) {
        unsigned n = utf8_size((unsigned char)text[i]);
        if (i+n >= size) break;
        unsigned j = 1;
        while (j < n && text[i+j] && ((unsigned char)text[i+j] & 0xc0) == 0x80) j++;
        if (j != n) break;
        memcpy(out+i, text+i, n);
        i += n;
    }
    out[i] = 0;
    return i;
}
void chat_history_init(chat_history_t *h) { memset(h, 0, sizeof(*h)); }
void chat_history_add(chat_history_t *h, bool assistant, const char *text)
{
    if (!text || !*text) return;
    if (h->external) {
        const chat_entry_t *keep[CHAT_HISTORY_RAM_COUNT];
        unsigned n=h->count<CHAT_HISTORY_RAM_COUNT ? h->count : CHAT_HISTORY_RAM_COUNT;
        for(unsigned i=0;i<n;i++)keep[i]=h->saved[h->count-n+i];
        for(unsigned i=0;i<n;i++)memcpy(&h->entries[i],keep[i],sizeof(chat_entry_t));
        h->first=0;h->count=(uint8_t)n;h->external=false;
    }
    unsigned slot = (h->first + h->count) % CHAT_HISTORY_RAM_COUNT;
    if (h->count == CHAT_HISTORY_RAM_COUNT) h->first = (h->first + 1) % CHAT_HISTORY_RAM_COUNT;
    else h->count++;
    h->entries[slot].assistant = assistant;
    chat_utf8_copy(h->entries[slot].text, CHAT_TEXT_BYTES, text);
    h->revision++;
}
const chat_entry_t *chat_history_get(const chat_history_t *h, unsigned index)
{
    return index < h->count ? (h->external ? h->saved[index] : &h->entries[(h->first + index) % CHAT_HISTORY_RAM_COUNT]) : NULL;
}
void chat_history_set_saved(chat_history_t *h,const chat_entry_t *const *entries,unsigned count)
{
    if(count>CHAT_HISTORY_SAVED_COUNT)count=CHAT_HISTORY_SAVED_COUNT;
    for(unsigned i=0;i<count;i++)h->saved[i]=entries[i];
    h->count=(uint8_t)count;h->first=0;h->external=true;h->revision++;
}
unsigned chat_history_wrap(const char *text, unsigned columns, char *out, size_t size)
{
    if (!size || !columns) return 0;
    size_t o = 0;
    unsigned row = 0, col = 0;
    for (size_t i = 0; text[i]; ) {
        unsigned n = utf8_size((unsigned char)text[i]);
        unsigned w = n == 1 ? 1 : 2;
        bool newline = text[i] == '\n';
        if (!newline && col && col + w > columns) {
            if (o+1 < size) out[o++] = '\n';
            row++; col = 0;
        }
        if (o+n < size) {
            memcpy(out+o, text+i, n); o += n;
        }
        if (newline) { row++; col = 0; }
        else col += w;
        i += n;
    }
    out[o] = 0;
    return row + 1;
}
unsigned chat_history_entry_height(const chat_entry_t *entry)
{
    char unused[1];
    unsigned lines=chat_history_wrap(entry->text,CHAT_HISTORY_COLUMNS,unused,sizeof(unused));
    return CHAT_HISTORY_ROLE_PX + lines*CHAT_HISTORY_LINE_PX - 2U + CHAT_HISTORY_GAP_PX;
}
unsigned chat_history_entry_top(const chat_history_t *h, unsigned index)
{
    unsigned top=0;
    for (unsigned i=0;i<index && i<h->count;i++) top+=chat_history_entry_height(chat_history_get(h,i));
    return top;
}
unsigned chat_history_max_scroll(const chat_history_t *h)
{
    unsigned height=chat_history_entry_top(h,h->count);
    if (height>=CHAT_HISTORY_GAP_PX) height-=CHAT_HISTORY_GAP_PX;
    return height>CHAT_HISTORY_VIEW_PX ? height-CHAT_HISTORY_VIEW_PX : 0;
}
