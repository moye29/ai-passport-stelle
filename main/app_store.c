#include "app_store.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

#define NAMESPACE "stelle"
#define KEY "prefs2"
#define KEY_LOG "flog"

static nvs_handle_t s_handle;
static bool s_open;

bool app_store_open(void)
{
    // Never erase NVS here: if it cannot be mounted the UI says "Not saved"
    // and keeps working from RAM, leaving stored data for recovery.
    esp_err_t err = nvs_flash_init();
    s_open = err == ESP_OK && nvs_open(NAMESPACE, NVS_READWRITE, &s_handle) == ESP_OK;
    return s_open;
}

bool app_store_load(app_prefs_t *out)
{
    if (!s_open) return false;
    uint8_t raw[64];  // larger than every known layout (older ones carried the to-do list)
    size_t size = sizeof(raw);
    if (nvs_get_blob(s_handle, KEY, raw, &size) != ESP_OK) return false;
    return app_prefs_from_blob(raw, size, out);
}

bool app_store_save(const app_prefs_t *prefs)
{
    if (!s_open) return false;
    return nvs_set_blob(s_handle, KEY, prefs, sizeof(*prefs)) == ESP_OK && nvs_commit(s_handle) == ESP_OK;
}

bool app_store_load_log(focus_log_t *out)
{
    if (!s_open) return false;
    size_t size = sizeof(*out);
    if (nvs_get_blob(s_handle, KEY_LOG, out, &size) != ESP_OK || size != sizeof(*out)) return false;
    return focus_log_valid(out);
}

bool app_store_save_log(const focus_log_t *log)
{
    if (!s_open) return false;
    return nvs_set_blob(s_handle, KEY_LOG, log, sizeof(*log)) == ESP_OK && nvs_commit(s_handle) == ESP_OK;
}

bool app_store_load_energy(uint32_t *out)
{
    if (!s_open) return false;
    esp_err_t err = nvs_get_u32(s_handle, "path_energy", out);
    if (err == ESP_ERR_NVS_NOT_FOUND) { *out = 0; return true; }
    return err == ESP_OK;
}

bool app_store_save_energy(uint32_t value)
{
    return s_open && nvs_set_u32(s_handle, "path_energy", value) == ESP_OK && nvs_commit(s_handle) == ESP_OK;
}
