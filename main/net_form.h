// main/net_form.h - Wi-Fi setup form parsing and helpers (pure C, host-testable).
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define NET_SSID_MAX 32
#define NET_PASS_MAX 64
#define NET_TZ_DEFAULT_MIN 480   // UTC+8

typedef struct {
    char ssid[NET_SSID_MAX + 1];
    char pass[NET_PASS_MAX + 1];
    int tz_min;                  // offset from UTC in minutes, east positive
} net_form_t;

typedef enum {
    NET_FORM_OK = 0,
    NET_FORM_NO_SSID,
    NET_FORM_SSID_TOO_LONG,
    NET_FORM_PASS_TOO_SHORT,     // WPA needs 8..63 characters (or none for open networks)
    NET_FORM_PASS_TOO_LONG,
    NET_FORM_BAD_TZ,
} net_form_err_t;

// Parse an application/x-www-form-urlencoded body with fields ssid, ssid_manual,
// pass and tz. A non-empty ssid_manual overrides the picked ssid.
net_form_err_t net_form_parse(const char *body, size_t len, net_form_t *out);
const char *net_form_error_text(net_form_err_t err);

// Escape <, >, &, " and ' for HTML text/attributes. Returns the output length;
// output is always NUL-terminated and truncated to fit.
size_t net_html_escape(const char *in, char *out, size_t n);

// POSIX TZ string for a fixed offset, e.g. 480 -> "UTC-8", 330 -> "UTC-5:30".
void net_tz_posix(int tz_min, char *buf, size_t n);
bool net_tz_valid(int tz_min);
