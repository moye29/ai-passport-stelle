#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHAT_HISTORY_COUNT 15U
#define CHAT_HISTORY_RAM_COUNT 12U
#define CHAT_HISTORY_SAVED_COUNT CHAT_HISTORY_COUNT
#define CHAT_TEXT_BYTES 768U
#define CHAT_HISTORY_COLUMNS 24U
#define CHAT_WRAP_BYTES (CHAT_TEXT_BYTES * 2U)
#define CHAT_HISTORY_LINE_PX 18U
#define CHAT_HISTORY_ROLE_PX 18U
#define CHAT_HISTORY_GAP_PX 12U
#define CHAT_HISTORY_VIEW_PX 206U
typedef struct { bool assistant; char text[CHAT_TEXT_BYTES]; } chat_entry_t;
typedef struct {
    chat_entry_t entries[CHAT_HISTORY_RAM_COUNT]; // existing RAM fallback; do not enlarge
    const chat_entry_t *saved[CHAT_HISTORY_SAVED_COUNT]; // immutable mapped Flash entries
    uint8_t first, count;
    bool external;
    uint32_t revision;
} chat_history_t;
void chat_history_init(chat_history_t *h);
void chat_history_add(chat_history_t *h, bool assistant, const char *text);
const chat_entry_t *chat_history_get(const chat_history_t *h, unsigned index);
void chat_history_set_saved(chat_history_t *h,const chat_entry_t *const *entries,unsigned count);
size_t chat_utf8_copy(char *out, size_t size, const char *text);
// Fixed cells: ASCII 1, CJK/other Unicode 2. Returns total wrapped line count.
unsigned chat_history_wrap(const char *text, unsigned columns, char *out, size_t size);
unsigned chat_history_entry_height(const chat_entry_t *entry);
unsigned chat_history_entry_top(const chat_history_t *h, unsigned index);
unsigned chat_history_max_scroll(const chat_history_t *h);
