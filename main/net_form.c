#include "net_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// URL-decode [s, s+len) into out (capacity n incl. NUL). Returns decoded length,
// or (size_t)-1 when it does not fit.
static size_t decode(const char *s, size_t len, char *out, size_t n)
{
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && i + 2 < len && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
            c = (char)(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        }
        if (o + 1 >= n) return (size_t)-1;
        out[o++] = c;
    }
    out[o] = '\0';
    return o;
}

// Find field `name` in the body; returns its raw value span or NULL.
static const char *field(const char *body, size_t len, const char *name, size_t *vlen)
{
    size_t nl = strlen(name);
    size_t i = 0;
    while (i < len) {
        size_t end = i;
        while (end < len && body[end] != '&') end++;
        if (end - i > nl && memcmp(body + i, name, nl) == 0 && body[i + nl] == '=') {
            *vlen = end - (i + nl + 1);
            return body + i + nl + 1;
        }
        i = end + 1;
    }
    return NULL;
}

bool net_tz_valid(int tz_min)
{
    return tz_min >= -12 * 60 && tz_min <= 14 * 60 && tz_min % 15 == 0;
}

net_form_err_t net_form_parse(const char *body, size_t len, net_form_t *out)
{
    memset(out, 0, sizeof(*out));
    out->tz_min = NET_TZ_DEFAULT_MIN;
    size_t vlen = 0;
    const char *v;
    char manual[NET_SSID_MAX + 2] = {0};

    if ((v = field(body, len, "ssid_manual", &vlen)) && vlen) {
        if (decode(v, vlen, manual, sizeof(manual)) == (size_t)-1 || strlen(manual) > NET_SSID_MAX)
            return NET_FORM_SSID_TOO_LONG;
    }
    if (manual[0]) {
        memcpy(out->ssid, manual, strlen(manual) + 1);
    } else if ((v = field(body, len, "ssid", &vlen)) && vlen) {
        if (decode(v, vlen, out->ssid, sizeof(out->ssid)) == (size_t)-1) return NET_FORM_SSID_TOO_LONG;
    }
    if (!out->ssid[0]) return NET_FORM_NO_SSID;

    if ((v = field(body, len, "pass", &vlen)) && vlen) {
        char pass[NET_PASS_MAX + 2];
        size_t n = decode(v, vlen, pass, sizeof(pass));
        if (n == (size_t)-1 || n > 63) return NET_FORM_PASS_TOO_LONG;
        if (n > 0 && n < 8) return NET_FORM_PASS_TOO_SHORT;
        memcpy(out->pass, pass, n + 1);
    }

    if ((v = field(body, len, "tz", &vlen)) && vlen) {
        char tz[8];
        if (decode(v, vlen, tz, sizeof(tz)) == (size_t)-1) return NET_FORM_BAD_TZ;
        char *endp = NULL;
        long t = strtol(tz, &endp, 10);
        if (!endp || *endp || !net_tz_valid((int)t)) return NET_FORM_BAD_TZ;
        out->tz_min = (int)t;
    }
    return NET_FORM_OK;
}

const char *net_form_error_text(net_form_err_t err)
{
    switch (err) {
    case NET_FORM_OK: return "OK";
    case NET_FORM_NO_SSID: return "Please choose or type a Wi-Fi name.";
    case NET_FORM_SSID_TOO_LONG: return "The Wi-Fi name is longer than 32 bytes.";
    case NET_FORM_PASS_TOO_SHORT: return "Wi-Fi passwords have at least 8 characters.";
    case NET_FORM_PASS_TOO_LONG: return "The password is longer than 63 characters.";
    case NET_FORM_BAD_TZ: return "Please pick a time zone.";
    }
    return "Invalid form.";
}

size_t net_html_escape(const char *in, char *out, size_t n)
{
    size_t o = 0;
    if (!n) return 0;
    for (; *in; in++) {
        const char *rep = NULL;
        switch (*in) {
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '&': rep = "&amp;"; break;
        case '"': rep = "&quot;"; break;
        case '\'': rep = "&#39;"; break;
        default: break;
        }
        size_t add = rep ? strlen(rep) : 1;
        if (o + add >= n) break;
        if (rep) memcpy(out + o, rep, add);
        else out[o] = *in;
        o += add;
    }
    out[o] = '\0';
    return o;
}

void net_tz_posix(int tz_min, char *buf, size_t n)
{
    // POSIX TZ offsets are west-positive: UTC+8 is written "UTC-8".
    int west = -tz_min;
    char sign = west < 0 ? '-' : '+';
    int a = abs(west);
    if (a % 60) snprintf(buf, n, "UTC%c%d:%02d", sign, a / 60, a % 60);
    else snprintf(buf, n, "UTC%c%d", sign, a / 60);
}
