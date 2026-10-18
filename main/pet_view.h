// main/pet_view.h - LVGL view of the Stelle companion. Call only with the LVGL lock held.
#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "app_model.h"
#include "net_status.h"
#include "chat_status.h"

typedef struct {
    int battery;        // 0..100, or < 0 when the fuel gauge is unavailable
    bool storage_ok;    // NVS writes succeed
    bool audio_ok;      // speaker path initialised
    bool charging;      // external power detected (see charge_detect.h)
    const net_status_t *net;  // Wi-Fi setup / time sync state; NULL = no network
    const chat_service_status_t *chat;
} pet_view_status_t;

lv_obj_t *pet_view_create(void);   // returns a new screen (not loaded)
// Sync the view with the model. Only changed objects are invalidated.
void pet_view_update(const app_model_t *m, const pet_view_status_t *status, uint32_t now_ms);
void pet_view_destroy(void);
