#include "chat_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    chat_model_t c;
    chat_model_init(&c);
    assert(c.phase == CHAT_OFF);
    assert(chat_model_click(&c) == CHAT_CMD_START);
    assert(c.phase == CHAT_CONNECTING);
    chat_model_set_phase(&c, CHAT_RECORDING);
    assert(chat_model_click(&c) == CHAT_CMD_STOP_RECORDING);
    assert(c.phase == CHAT_WAITING);
    chat_model_set_phase(&c, CHAT_PLAYING);
    assert(chat_model_click(&c) == CHAT_CMD_CANCEL);
    assert(c.phase == CHAT_OFF);
    assert(!chat_model_active(&c));
    puts("chat button lifecycle: PASS");
    return 0;
}
