// Platform service: all HTTPS, MQTT, UDP and PCM work runs outside the UI task.
#pragma once
#include "chat_model.h"
#include "chat_history.h"
#include "chat_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

bool chat_service_init(SemaphoreHandle_t audio_mutex);
void chat_service_set_foreground(bool active);
void chat_service_command(chat_cmd_t cmd);
void chat_service_get_status(chat_service_status_t *out);
bool chat_service_take_text(chat_entry_t *out);
