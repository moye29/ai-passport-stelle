// Plain C status shared by firmware and native preview.
#pragma once
#include "chat_model.h"
typedef struct {
    chat_phase_t phase;
    char code[16];
    char hint[48];
} chat_service_status_t;
