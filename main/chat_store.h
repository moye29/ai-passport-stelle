// Platform-task-only Flash persistence. Do not call from LVGL or button callbacks.
#pragma once
#include "chat_journal.h"
bool chat_store_open(chat_history_t *,chat_journal_workspace_t *);
bool chat_store_append(chat_history_t *,const chat_entry_t *);
// Erase one non-live sector ahead while audio is idle; never touches recent15.
void chat_store_prepare(void);
