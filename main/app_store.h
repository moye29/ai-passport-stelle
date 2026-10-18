// main/app_store.h - NVS persistence: app_prefs_t (settings) and the per-day focus log.
// Call only from the platform task; writes block for a flash commit.
#pragma once

#include <stdbool.h>
#include "app_model.h"

bool app_store_open(void);                       // false: storage unavailable (UI says "Not saved")
bool app_store_load(app_prefs_t *out);           // false: nothing valid stored yet (older layouts are converted)
bool app_store_save(const app_prefs_t *prefs);   // false: write or commit failed
bool app_store_load_log(focus_log_t *out);       // false: no valid log yet
bool app_store_save_log(const focus_log_t *log);
// Missing key is a valid first-run zero; read/type failures must not overwrite it.
bool app_store_load_energy(uint32_t *out);
bool app_store_save_energy(uint32_t value);
