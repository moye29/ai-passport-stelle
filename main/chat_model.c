#include "chat_model.h"

void chat_model_init(chat_model_t *c) { c->phase = CHAT_OFF; }
bool chat_model_active(const chat_model_t *c)
{
    return c->phase != CHAT_OFF && c->phase != CHAT_ERROR;
}
void chat_model_set_phase(chat_model_t *c, chat_phase_t phase) { c->phase = phase; }
chat_cmd_t chat_model_click(chat_model_t *c)
{
    if (!chat_model_active(c)) { c->phase = CHAT_CONNECTING; return CHAT_CMD_START; }
    if (c->phase == CHAT_RECORDING) { c->phase = CHAT_WAITING; return CHAT_CMD_STOP_RECORDING; }
    c->phase = CHAT_OFF;
    return CHAT_CMD_CANCEL;
}
