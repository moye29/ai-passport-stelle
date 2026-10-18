// main/app_model.h - Stelle companion state machine (pure C, no ESP-IDF/LVGL).
//
// Owns pages, focus timer and its per-day log, calendar, settings drafts, the network clock and screen
// state. The platform task feeds it inputs and a monotonic millisecond clock,
// then reads app_model_take_events() for side effects (chime, preview blip,
// persistence). Rendering never decides state.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "companion_model.h"
#include "focus_log.h"
#include "chat_model.h"
#include "chat_history.h"

#define APP_PREFS_VERSION 5   // v5 persists the daily Trailblaze destination; v2-v4 migrate
#define APP_TRAILBLAZE_COUNT 12U
#define APP_FOCUS_COUNTDOWN_MS (25U * 60U * 1000U)
#define APP_FOCUS_COUNTUP_MAX_MS ((99U * 60U + 59U) * 1000U)
#define APP_FOCUS_MIN_RECORD_MS (60U * 1000U)
#define APP_CONFIRM_TIMEOUT_MS 5000U
#define APP_NAV_VISIBLE 3   // nav buttons shown at once (of NAV_COUNT)
#define APP_BATTLE_HIT_MS 250U
#define APP_BATTLE_RECOVERY_MS 250U
#define APP_LOG_SAVE_MS (5U * 60U * 1000U)  // while focusing, persist the day log this often
// Honkai: Star Rail launched on 2023-04-26 10:00 (UTC+8); counted in local time.
#define APP_HSR_LAUNCH_YEAR 2023
#define APP_HSR_LAUNCH_MONTH 4
#define APP_HSR_LAUNCH_DAY 26
#define APP_HSR_LAUNCH_SECOND (10U * 3600U)

typedef enum {
    PAGE_HOME = 0,
    PAGE_FOCUS,
    PAGE_PLAY,
    PAGE_CALENDAR,     // month grid + focus time of the selected day
    PAGE_ANNIV,        // time since Honkai: Star Rail launched, with the Astral Express
    PAGE_SETTINGS,
    PAGE_ADJUST,
    PAGE_SETUP,        // Wi-Fi setup: phone joins the Passport hotspot
    PAGE_SCENE,        // background picker with a preview (draft = scene)
    PAGE_CHAT,
    PAGE_CHAT_HISTORY,
    PAGE_BATTLE,
} app_page_t;

typedef enum { NAV_FOCUS = 0, NAV_CALENDAR, NAV_CHAT, NAV_PLAY, NAV_BATTLE, NAV_SETTINGS, NAV_COUNT } app_nav_t;
typedef enum { BATTLE_READY = 0, BATTLE_ENCOUNTER, BATTLE_HIT, BATTLE_RECOVER } battle_stage_t;
typedef enum { IN_UP = 0, IN_DOWN, IN_OK, IN_HOLD_UP, IN_HOLD_DOWN, IN_HOLD_OK } app_input_t;
typedef enum { FOCUS_READY = 0, FOCUS_RUNNING, FOCUS_PAUSED, FOCUS_DONE } focus_state_t;
typedef enum { FOCUS_COUNTDOWN = 0, FOCUS_COUNTUP } focus_mode_t;
typedef enum { CONFIRM_NONE = 0, CONFIRM_STOP, CONFIRM_RESET } focus_confirm_t;
typedef enum { SET_BRIGHTNESS = 0, SET_VOLUME, SET_SCREEN_OFF, SET_WIFI, SET_SCENE, SET_COUNT } app_setting_t;
// Network time state, reported by the platform (no network = SYNC_OFF).
typedef enum { SYNC_OFF = 0, SYNC_RUNNING, SYNC_OK, SYNC_FAILED } time_sync_t;
typedef enum { SCENE_ROOM = 0, SCENE_TRAIN, SCENE_TRAILBLAZE, SCENE_COUNT } app_scene_t;
typedef enum { ROOM_DAY = 0, ROOM_DUSK, ROOM_NIGHT, ROOM_LATE } room_phase_t;

// Side effects for the platform layer, collected as bit flags.
enum {
    APP_EV_FOCUS_DONE = 1u << 0,  // play the completion chime
    APP_EV_VOLUME_PREVIEW = 1u << 1,  // short blip at the draft volume
    APP_EV_PREFS_CHANGED = 1u << 2,  // persist prefs (debounced by platform)
    APP_EV_LOG_CHANGED = 1u << 3,    // persist the focus log (session start/pause/end, every 5 min)
    APP_EV_CHAT_START = 1u << 4,
    APP_EV_CHAT_STOP_RECORDING = 1u << 5,
    APP_EV_CHAT_CANCEL = 1u << 6,
    APP_EV_KEY_SOUND = 1u << 7, // short, disposable UI feedback; never during voice
    APP_EV_PLAY_JUMP = 1u << 8, // reuse the completion chime, without completing Focus
    APP_EV_PLAY_BAT = 1u << 9, // next user-provided bat clip (audio worker owns rotation)
    APP_EV_ENERGY_CHANGED = 1u << 10,
    APP_EV_BATTLE_HIT = 1u << 11,
};

// Persisted blob. Validate with app_prefs_valid() before use.
typedef struct {
    uint16_t version;
    uint8_t brightness;      // 10..100, step 10
    uint8_t volume;          // 0..100, step 10
    uint8_t screen_off;      // index into app_screen_off_ms()
    uint8_t scene;           // app_scene_t
    uint8_t trailblaze_index;
    uint8_t trailblaze_chosen;
    int32_t trailblaze_day;  // 0 until local network time is known
} app_prefs_t;

typedef struct {
    focus_state_t state;
    focus_mode_t mode;
    focus_confirm_t confirm;
    uint32_t elapsed_ms;
    uint32_t last_ms;
    uint32_t confirm_ms;
    bool repeated_completion; // snapshot for this completion, including across midnight
} app_focus_t;

typedef struct {
    app_page_t page;
    app_nav_t nav;
    battle_stage_t battle_stage;
    uint8_t battle_enemy; // session-local A=0/B=1; never derived from the durable count
    uint32_t path_energy, energy_before, battle_started_ms;
    bool battle_save_failed, battle_storage_ready, battle_has_hit;
    bool battle_idle;
    uint32_t battle_idle_ms;
    uint8_t nav_first;     // first nav item in the visible window
    uint8_t setting_sel;
    uint8_t draft;         // value being adjusted on PAGE_ADJUST / PAGE_SCENE
    bool screen_off;
    bool display_greeting;
    uint32_t display_greeting_ms;
    bool clock_set;
    bool setup_from_settings;  // PAGE_SETUP was opened from Settings (else first boot)
    bool play_said;        // the last Play action's line stays while Play is open or highlighted
    bool anniv_paused;    // transient: each anniversary visit starts playing
    uint8_t anniv_volume; // temporary audition volume; discarded on exit
    uint32_t anniv_session; // distinguishes leave/re-enter even within one input batch
    uint32_t anniv_entered_ms;
    time_sync_t time_sync;
    uint32_t clock_s;      // seconds since local midnight
    uint32_t clock_ms;     // monotonic time of the last whole clock second
    uint32_t last_input_ms;
    uint32_t events;
    app_focus_t focus;
    app_prefs_t prefs;
    uint32_t scene_rng;     // platform seeds once from hardware entropy
    companion_model_t companion;
    // Calendar and the per-day focus log. The date is only known after a
    // network time sync; focus time before that waits in pend_* and is put
    // on that day once the date arrives.
    bool date_known;
    int32_t day;           // local day number (days since 1970-01-01)
    int32_t cal_day;       // day selected on the calendar
    uint32_t log_ms;       // focused milliseconds not yet added as whole seconds
    uint32_t pend_secs;    // focused before the date was known
    uint16_t pend_sessions;
    uint16_t pend_completions;
    uint32_t log_save_ms;  // running time since the log was last persisted
    focus_log_t log;
    chat_model_t chat;
    chat_history_t chat_history; // latest15 mapped Flash records; bounded12-entry RAM fallback
    uint16_t chat_history_scroll; // continuous vertical offset in native pixels
} app_model_t;

void app_model_init(app_model_t *m, uint32_t now_ms);
// Platform signals the first visible frame or a backlight transition from off.
// This transient sprite overlay does not change the current page/action.
void app_model_display_ready(app_model_t *m,uint32_t now_ms);
bool app_model_greeting_visible(const app_model_t *m,uint32_t now_ms);
void app_model_seed_scenes(app_model_t *m, uint32_t seed);
bool app_prefs_valid(const app_prefs_t *p);
void app_prefs_default(app_prefs_t *p);
// Read a stored blob of any known layout (v2/v3 with the old to-do list, v4/v5).
// Returns false when it is not a known layout.
bool app_prefs_from_blob(const uint8_t *raw, size_t size, app_prefs_t *out);
void app_model_load_log(app_model_t *m, const focus_log_t *log);
void app_model_load_prefs(app_model_t *m, const app_prefs_t *p);
// Platform acknowledges the synchronous worker-task NVS commit before rendering
// an award or playing its sound. Failure restores the last durable count.
void app_model_energy_saved(app_model_t *m, bool success);
// Wi-Fi setup page: shown at first boot (no saved network) or from Settings.
void app_model_begin_setup(app_model_t *m, bool from_settings);
void app_model_setup_finished(app_model_t *m);   // network saved: leave the page
// Network time arrived: seconds since local midnight and the local day number.
void app_model_set_time(app_model_t *m, uint32_t seconds_of_day, int32_t day, uint32_t now_ms);
// Focus today / on a day, including the part of a running session not yet logged.
focus_day_t app_focus_on_day(const app_model_t *m, int32_t day);
// Seconds since the Honkai: Star Rail launch (0 before the date is known).
uint64_t app_since_hsr_launch_s(const app_model_t *m);

void app_model_tick(app_model_t *m, uint32_t now_ms);
void app_model_handle(app_model_t *m, app_input_t input, uint32_t now_ms);
void app_model_handle_queued(app_model_t *m, app_input_t input, uint32_t received_ms, uint32_t now_ms);
uint32_t app_model_take_events(app_model_t *m);

uint32_t app_focus_display_s(const app_model_t *m);
bool app_focus_active(const app_model_t *m);
uint8_t app_backlight_percent(const app_model_t *m);
uint8_t app_volume_percent(const app_model_t *m);
uint32_t app_screen_off_ms(uint8_t index);
unsigned app_screen_off_count(void);
room_phase_t app_room_phase(const app_model_t *m);
// 0:00-8:00 (once the clock is known) idle Stelle is shown asleep in her box
// with the five-second sleep animation instead of standing idle — except while
// Play or Chat is selected in the Home nav or open, where she stands.
#define APP_SLEEP_END_HOUR 8U
bool app_companion_asleep(const app_model_t *m);
// Play is open, or highlighted in the Home nav.
bool app_play_in_focus(const app_model_t *m);
bool app_chat_in_focus(const app_model_t *m);
bool app_page_shows_room(app_page_t page);

