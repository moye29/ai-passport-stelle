// Button-driven conversation state. No microphone work happens in this model.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { CHAT_OFF, CHAT_CONNECTING, CHAT_BINDING, CHAT_RECORDING, CHAT_WAITING, CHAT_PLAYING, CHAT_ERROR } chat_phase_t;
typedef enum { CHAT_CMD_NONE, CHAT_CMD_START, CHAT_CMD_STOP_RECORDING, CHAT_CMD_CANCEL } chat_cmd_t;
typedef struct { chat_phase_t phase; } chat_model_t;
void chat_model_init(chat_model_t *c);
bool chat_model_active(const chat_model_t *c);
chat_cmd_t chat_model_click(chat_model_t *c);
void chat_model_set_phase(chat_model_t *c, chat_phase_t phase);
