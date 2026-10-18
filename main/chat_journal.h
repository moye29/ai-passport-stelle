// Power-cut-safe record journal. No ESP-IDF/LVGL or heap allocation.
#pragma once
#include "chat_history.h"
#define CHAT_JOURNAL_SECTOR 4096U
#define CHAT_JOURNAL_SLOTS 32U
#define CHAT_JOURNAL_BYTES (CHAT_JOURNAL_SLOTS*CHAT_JOURNAL_SECTOR)
#define CHAT_JOURNAL_HEADER 32U
#define CHAT_JOURNAL_RECORD 804U
typedef struct { unsigned char record[CHAT_JOURNAL_RECORD]; } chat_journal_workspace_t;
typedef struct {
    void *ctx;
    bool (*read)(void *,unsigned,void *,unsigned);
    bool (*write)(void *,unsigned,const void *,unsigned);
    bool (*erase)(void *,unsigned,unsigned);
} chat_journal_io_t;
typedef struct {
    chat_journal_io_t io;
    chat_journal_workspace_t *work;
    uint32_t sequence[CHAT_HISTORY_SAVED_COUNT];
    uint8_t slots[CHAT_HISTORY_SAVED_COUNT];
    uint8_t count,next;
    bool ready;
} chat_journal_t;
bool chat_journal_open(chat_journal_t *,const chat_journal_io_t *,chat_journal_workspace_t *);
bool chat_journal_append(chat_journal_t *,const chat_entry_t *);
// Validate the complete record, including CRC, role, length and last-written marker.
bool chat_journal_record_valid(const unsigned char *,uint32_t *sequence);
bool chat_journal_slot_live(const chat_journal_t *,unsigned slot);
