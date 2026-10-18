#include "ui_copy.h"

#include <stdio.h>
#include <string.h>

static const char *const PHASE_LINES[] = {
    "Reach the end in your own way.",
    "Journeys never end.",
    "May this journey lead us starward.",
    "May this journey lead us starward.",
};
static const char *const PLAY_LINES[COMPANION_ACTION_COUNT] = {
    "Hi there, partner!",
    "Fighting!",
    "Mom, I have knees!",
    "Rules are made to be broken!",
};
#define S_HELLO      "Hey, you're a Trailblazer too?"
#define S_READY      "Ready? Let's go."
#define S_RUN_DOWN   "One thing at a time."
#define S_RUN_UP     "One thing at a time."
#define S_PAUSED     "Take a breath. I'll be here."
#define S_DONE       "Nice. What's next?"
#define S_DONE_AGAIN "Another one. Is that all?"
#define S_STOP       "Stop here? That's okay too."
#define S_RESET      "Again? Let's do it."
#define S_PLAY       "What are we playing?"
#define S_MORNING    "A new day. What to do?"

static const char *idle_speech(const app_model_t *m)
{
    if (app_companion_asleep(m)) return "Good night. Rest easy.";
    if (m->clock_set && m->clock_s >= 6U * 3600U && m->clock_s < 12U * 3600U)
        return S_MORNING;
    return PHASE_LINES[app_room_phase(m)];
}

#define H_SET_TIME   "Set up Wi-Fi to get the time"
#define H_READY_DOWN "Countdown / OK: start"
#define H_READY_UP   "Count up / OK: start"
#define H_RUNNING    "OK: pause / UP: stop"
#define H_PAUSED     "OK: resume / UP: reset"
#define H_DONE       "Complete! / OK: again"
#define H_STOP       "Stop? OK: yes / UP: no"
#define H_RESET      "Reset? OK: yes / UP: no"
#define H_PLAY       "Play with Stelle / OK: go"

const char *ui_copy_speech(const app_model_t *m)
{
    static const char *const battle[] = {"Stay alert.", "Batter up.", "Take this!", "Who's next?"};
    if (m->page == PAGE_BATTLE) return battle[m->battle_stage];
    if (m->page == PAGE_HOME && m->nav == NAV_BATTLE && m->battle_idle) return "I'm ready for fight.";
    const app_focus_t *f = &m->focus;
    const companion_model_t *c = &m->companion;
    if (app_chat_in_focus(m)) {
        if (m->chat.phase == CHAT_BINDING) return "Bind this device at xiaozhi.me";
        if (m->chat.phase == CHAT_RECORDING) return "I'm listening.";
        if (m->chat.phase == CHAT_PLAYING) return "A little reply for you.";
        if (m->chat.phase == CHAT_WAITING) return "Let me think for a moment.";
        if (m->chat.phase == CHAT_CONNECTING) return "Connecting to XiaoZhi...";
        if (m->chat.phase == CHAT_ERROR) return "Chat unavailable. Try again.";
        return idle_speech(m);
    }
    // After a Play action its line stays until another action plays or Play is left
    // (Play page, or Play still highlighted in the Home nav).
    if (m->play_said || (m->page == PAGE_PLAY && c->from_play && c->motion != COMPANION_MOTION_IDLE))
        return PLAY_LINES[c->last_action];
    if (m->page == PAGE_PLAY) return S_PLAY;
    if (f->confirm == CONFIRM_STOP) return S_STOP;
    if (f->confirm == CONFIRM_RESET) return S_RESET;
    if (f->state == FOCUS_DONE) return f->repeated_completion ? S_DONE_AGAIN : S_DONE;
    if (f->state == FOCUS_RUNNING) return f->mode == FOCUS_COUNTDOWN ? S_RUN_DOWN : S_RUN_UP;
    if (f->state == FOCUS_PAUSED) return S_PAUSED;
    if (m->page == PAGE_FOCUS) return S_READY;
    if (c->motion == COMPANION_MOTION_GREETING && !c->from_play) return S_HELLO;
    return idle_speech(m);
}

void ui_copy_mmss(uint32_t seconds, char *buf, size_t n)
{
    snprintf(buf, n, "%02u:%02u", (unsigned)(seconds / 60U % 100U), (unsigned)(seconds % 60U));
}

void ui_copy_clock(const app_model_t *m, uint32_t seconds, char *buf, size_t n)
{
    if (!m->clock_set) {
        snprintf(buf, n, "--:--");
        return;
    }
    snprintf(buf, n, "%02u:%02u", (unsigned)(seconds / 3600U % 24U), (unsigned)(seconds / 60U % 60U));
}

void ui_copy_info(const app_model_t *m, ui_info_t *out)
{
    const app_focus_t *f = &m->focus;
    memset(out, 0, sizeof(*out));
    if (m->page == PAGE_BATTLE || (m->page == PAGE_HOME && m->nav == NAV_BATTLE)) {
        out->sparkles = true;
        snprintf(out->big, sizeof(out->big), "Battle");
        const char *hint = m->page == PAGE_HOME ? "OK: enter" :
            !m->battle_storage_ready ? "Storage unavailable" :
            m->battle_save_failed ? "Save failed: try again" :
            m->battle_stage == BATTLE_HIT ? "Please wait." :
            m->battle_stage == BATTLE_ENCOUNTER ? (m->path_energy == UINT32_MAX ? "Counter full" : "OK: hit / Hold: exit / UP: reset") :
            m->battle_stage == BATTLE_RECOVER ? "Please wait." : "OK: start battle / Hold OK: exit";
        if (m->page == PAGE_HOME) hint = "OK: return to battle";
        snprintf(out->sub, sizeof(out->sub), "%s", hint);
        return;
    }
    if (app_chat_in_focus(m)) {
        const char *big = m->chat.phase == CHAT_RECORDING ? "Listening" : m->chat.phase == CHAT_PLAYING ? "Speaking" :
                          m->chat.phase == CHAT_CONNECTING ? "Connecting" : m->chat.phase == CHAT_BINDING ? "Bind device" :
                          m->chat.phase == CHAT_WAITING ? "Thinking" : "Chat off";
        snprintf(out->big, sizeof(out->big), "%s", big);
        snprintf(out->sub, sizeof(out->sub), "%s", !chat_model_active(&m->chat) ? "OK: start / Hold: history" : "OK: stop");
        return;
    }
    if (m->page == PAGE_PLAY) {
        snprintf(out->big, sizeof(out->big), "%s", companion_action_name(m->companion.selected));
        snprintf(out->sub, sizeof(out->sub), "%s", H_PLAY);
        out->arrows = out->sparkles = true;
        return;
    }
    out->big_is_digits = true;
    if (m->page == PAGE_FOCUS) {
        out->sparkles = true;
        ui_copy_mmss(app_focus_display_s(m), out->big, sizeof(out->big));
        const char *hint = f->confirm == CONFIRM_STOP  ? H_STOP
                         : f->confirm == CONFIRM_RESET ? H_RESET
                         : f->state == FOCUS_RUNNING   ? H_RUNNING
                         : f->state == FOCUS_PAUSED    ? H_PAUSED
                         : f->state == FOCUS_DONE      ? H_DONE
                         : f->mode == FOCUS_COUNTDOWN  ? H_READY_DOWN : H_READY_UP;
        snprintf(out->sub, sizeof(out->sub), "%s", hint);
        out->arrows = f->state == FOCUS_READY;
        return;
    }
    ui_copy_clock(m, m->clock_s, out->big, sizeof(out->big));
    char t[8];
    ui_copy_mmss(app_focus_display_s(m), t, sizeof(t));
    if (f->state == FOCUS_RUNNING)
        snprintf(out->sub, sizeof(out->sub), f->mode == FOCUS_COUNTDOWN ? "Focus %s left" : "Focus %s", t);
    else if (f->state == FOCUS_PAUSED) snprintf(out->sub, sizeof(out->sub), "Focus paused %s", t);
    else if (f->state == FOCUS_DONE) snprintf(out->sub, sizeof(out->sub), "Focus complete!");
    else if (!m->clock_set && m->time_sync == SYNC_RUNNING) snprintf(out->sub, sizeof(out->sub), "Syncing time over Wi-Fi...");
    else if (!m->clock_set && m->time_sync == SYNC_FAILED) snprintf(out->sub, sizeof(out->sub), "Offline / retrying later");
    else if (!m->clock_set) snprintf(out->sub, sizeof(out->sub), "%s", H_SET_TIME);
}

const char *ui_copy_setting_name(unsigned setting)
{
    static const char *const names[SET_COUNT] = {"Brightness", "Volume", "Auto off", "Wi-Fi", "Scene"};
    return setting < SET_COUNT ? names[setting] : "";
}

void ui_copy_setting_value(unsigned setting, unsigned value, char *buf, size_t n)
{
    if (setting == SET_SCREEN_OFF) {
        uint32_t ms = app_screen_off_ms((uint8_t)value);
        if (!ms) snprintf(buf, n, "Never");
        else if (ms < 60000U) snprintf(buf, n, "%lu s", (unsigned long)(ms / 1000U));
        else snprintf(buf, n, "%lu min", (unsigned long)(ms / 60000U));
    } else if (setting == SET_SCENE) {
        snprintf(buf, n, "%s", value == SCENE_TRAILBLAZE ? "Trailblaze" : value == SCENE_TRAIN ? "Train" : "Room");
    } else {
        snprintf(buf, n, "%u%%", value);
    }
}

static const char *const MONTHS[12] = {"January", "February", "March", "April", "May", "June",
                                       "July", "August", "September", "October", "November", "December"};

void ui_copy_month(int32_t day, char *buf, size_t n)
{
    int y, mo, d;
    date_from_day(day, &y, &mo, &d);
    snprintf(buf, n, "%s %d", MONTHS[(mo - 1) % 12], y);
}

void ui_copy_date(int32_t day, char *buf, size_t n)
{
    int y, mo, d;
    date_from_day(day, &y, &mo, &d);
    snprintf(buf, n, "%.3s %d", MONTHS[(mo - 1) % 12], d);
}

void ui_copy_duration(uint32_t secs, char *buf, size_t n)
{
    uint32_t h = secs / 3600U, mi = secs / 60U % 60U;
    if (h) snprintf(buf, n, "%lu h %lu min", (unsigned long)h, (unsigned long)mi);
    else if (mi) snprintf(buf, n, "%lu min", (unsigned long)mi);
    else snprintf(buf, n, "%lu s", (unsigned long)secs);
}

void ui_copy_since_launch(uint64_t secs, char *days, size_t dn, char *clock, size_t cn)
{
    unsigned long d = (unsigned long)(secs / 86400U);
    unsigned s = (unsigned)(secs % 86400U);
    snprintf(days, dn, "%lu", d);
    snprintf(clock, cn, "%02u:%02u:%02u", s / 3600U, s / 60U % 60U, s % 60U);
}
const char *ui_copy_nav_name(unsigned nav)
{
    static const char *const names[NAV_COUNT] = {
        [NAV_FOCUS]="Focus",[NAV_CALENDAR]="Days",[NAV_CHAT]="Chat",
        [NAV_PLAY]="Play",[NAV_BATTLE]="Battle",[NAV_SETTINGS]="Settings"
    };
    return nav < NAV_COUNT ? names[nav] : "";
}

size_t ui_copy_all_fixed(const char *const **out)
{
    static const char *const all[] = {
        "Reach the end in your own way.", "Journeys never end.", S_MORNING,
        "Good night. Rest easy.", "Hi there, partner!", "Fighting!", "May this journey lead us starward.",
        "Mom, I have knees!", "Rules are made to be broken!", S_HELLO, S_READY, S_RUN_DOWN, S_RUN_UP, S_PAUSED, S_DONE,
        S_STOP, S_RESET, S_PLAY, S_DONE_AGAIN,
    };
    *out = all;
    return sizeof(all) / sizeof(all[0]);
}

// Short, printable-ASCII form of a network name for the pixel font.
static void short_ssid(const char *in, char *out, size_t size)
{
    size_t o = 0, max = size - 1 < 14 ? size - 1 : 14;
    for (; *in && o < max; in++) {
        unsigned char c = (unsigned char)*in;
        if (c >= 0x80) {  // skip UTF-8 continuation bytes, one '?' per character
            if ((c & 0xC0) == 0x80) continue;
            c = '?';
        } else if (c < 0x20 || c > 0x7E) {
            c = '?';
        }
        out[o++] = (char)c;
    }
    if (*in && o >= 2) { out[o - 2] = '.'; out[o - 1] = '.'; }
    out[o] = '\0';
}

void ui_copy_setup_status(const net_status_t *n, char *buf, size_t size)
{
    char ssid[16];
    short_ssid(n->sta_ssid, ssid, sizeof(ssid));
    switch (n->phase) {
    case NET_PHASE_IDLE:
    case NET_PHASE_STARTING: snprintf(buf, size, "Starting hotspot..."); break;
    case NET_PHASE_PORTAL:
        snprintf(buf, size, "%s", n->phone_joined ? "Phone connected - fill the page" : "Waiting for your phone...");
        break;
    case NET_PHASE_JOINING: snprintf(buf, size, "Joining %s...", ssid); break;
    case NET_PHASE_JOIN_FAILED: snprintf(buf, size, "%s", n->message[0] ? n->message : "Could not connect. Try again."); break;
    case NET_PHASE_CONNECTING:
    case NET_PHASE_SYNCING: snprintf(buf, size, "Connected! Syncing time..."); break;
    case NET_PHASE_SYNCED: snprintf(buf, size, "All set!"); break;
    case NET_PHASE_OFFLINE: snprintf(buf, size, "Saved, but %s is offline", ssid); break;
    }
}

void ui_copy_wifi_value(const net_status_t *n, char *buf, size_t size)
{
    if (!n || !n->has_credentials) {
        snprintf(buf, size, "Not set");
        return;
    }
    switch (n->phase) {
    case NET_PHASE_CONNECTING:
    case NET_PHASE_SYNCING: snprintf(buf, size, "Syncing..."); break;
    case NET_PHASE_OFFLINE: snprintf(buf, size, "Offline"); break;
    case NET_PHASE_SYNCED: snprintf(buf, size, "Time synced"); break;
    default: snprintf(buf, size, "Saved"); break;
    }
}
