// RAM-only transport lifetime policy, independent of ESP-IDF and LVGL.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CHAT_CACHE_IDLE_MS 120000U
typedef struct { bool valid; uint32_t touched_ms; } chat_cache_t;
void chat_cache_touch(chat_cache_t *cache, uint32_t now);
void chat_cache_clear(chat_cache_t *cache);
bool chat_cache_reusable(const chat_cache_t *cache, uint32_t now,
                         bool foreground, bool network_up, bool mqtt_up);
bool chat_cache_accept_session(bool channel_open, const char *current, const char *message);
