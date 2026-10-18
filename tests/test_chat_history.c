#include "chat_history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    chat_history_t h;
    chat_history_init(&h);
    chat_history_add(&h, false, "hello");
    chat_history_add(&h, true, "world");
    assert(h.count == 2);
    assert(!chat_history_get(&h, 0)->assistant);
    assert(strcmp(chat_history_get(&h, 1)->text, "world") == 0);
    for (unsigned i = 0; i < CHAT_HISTORY_RAM_COUNT; i++) chat_history_add(&h, false, "next");
    assert(h.count == CHAT_HISTORY_RAM_COUNT);
    char long_text[CHAT_TEXT_BYTES + 20];
    for (unsigned i = 0; i + 3 < sizeof(long_text); i += 3) memcpy(long_text+i, "\xe4\xb8\xad", 3);
    long_text[sizeof(long_text)-3] = 0;
    chat_history_add(&h, true, long_text);
    const char *s = chat_history_get(&h, h.count-1)->text;
    assert(strlen(s) % 3 == 0);
    char wrapped[CHAT_WRAP_BYTES];
    assert(chat_history_wrap("one\ntwo\nthree", 10, wrapped, sizeof(wrapped)) == 3);
    assert(strcmp(wrapped, "one\ntwo\nthree") == 0);
    assert(chat_history_wrap("12345678901", 10, wrapped, sizeof(wrapped)) == 2);
    assert(strcmp(wrapped, "1234567890\n1") == 0);
    chat_history_init(&h);
    assert(chat_history_max_scroll(&h) == 0);
    chat_history_add(&h, false, "hello");
    chat_history_add(&h, true, "world");
    assert(chat_history_entry_top(&h, 1) == chat_history_entry_height(chat_history_get(&h,0)));
    assert(chat_history_max_scroll(&h) == 0);
    puts("bounded UTF-8 history and continuous layout: PASS");
}
