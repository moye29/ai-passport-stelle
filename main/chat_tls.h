// ESP-IDF 5.5.3 compatible full-anchor CA callback; no trust bypass.
#pragma once
#include "esp_err.h"
esp_err_t chat_tls_attach(void *ssl_config);
