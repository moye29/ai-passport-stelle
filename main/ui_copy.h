// main/ui_copy.h - every user-visible string, chosen from model state (pure C).
//
// Keeping copy out of the LVGL view lets host tests check page/state wording
// and lets the native preview verify that the longest strings fit.
#pragma once

#include <stddef.h>
#include "app_model.h"
#include "net_status.h"

#define UI_COPY_MAX 48

typedef struct {
    char big[12];            // large digits or title line
    char sub[UI_COPY_MAX];   // hint line under it ("" = none)
    bool big_is_digits;      // digits font (else title font)
    bool arrows;             // show the UP/DOWN choice arrows beside `big`
    bool sparkles;           // focus/play style (gold sparkles) vs clock style
} ui_info_t;

const char *ui_copy_speech(const app_model_t *m);
void ui_copy_info(const app_model_t *m, ui_info_t *out);
void ui_copy_mmss(uint32_t seconds, char *buf, size_t n);
void ui_copy_clock(const app_model_t *m, uint32_t seconds, char *buf, size_t n);
void ui_copy_setting_value(unsigned setting, unsigned value, char *buf, size_t n);
const char *ui_copy_setting_name(unsigned setting);
// Calendar and anniversary text (English, ASCII only).
void ui_copy_month(int32_t day, char *buf, size_t n);          // "October 2026"
void ui_copy_date(int32_t day, char *buf, size_t n);           // "Oct 16"
void ui_copy_duration(uint32_t secs, char *buf, size_t n);     // "1 h 25 min" / "12 min" / "40 s"
void ui_copy_since_launch(uint64_t secs, char *days, size_t dn, char *clock, size_t cn);
const char *ui_copy_nav_name(unsigned nav);

// Wi-Fi setup page status line, and the value shown on the Settings Wi-Fi tile.
// Network names are reduced to printable ASCII (the pixel font has no CJK).
void ui_copy_setup_status(const net_status_t *n, char *buf, size_t size);
void ui_copy_wifi_value(const net_status_t *n, char *buf, size_t size);

// Every fixed string the view can show, for fit checks in the preview harness.
size_t ui_copy_all_fixed(const char *const **out);
